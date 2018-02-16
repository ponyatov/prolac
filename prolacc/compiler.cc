#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "compiler.hh"
#include "optimize.hh"
#include "location.hh"
#include "fork.hh"
#include "rule.hh"
#include "error.hh"
#include "module.hh"

Compiler::Compiler(Writer &w, Writer &pw, int max_inline_level)
  : _max_inline_level(max_inline_level), out(w), proto_out(pw)
{
}

void
Compiler::compile(Rule *rule, bool debug_node, bool debug_target,
		  bool debug_loc)
{
  if (!rule->gen_if())
    return;
  
  _blocks.clear();
  _temporaries.clear();
  _marked_rules.clear();
  _exception_handler = _rethrow_handler = 0;
  
  _body_root = rule->body();
  if (!_body_root) {
    rule->gen_prototype(out, false);
    out << "{\n  assert(0 && \"method `" << rule
	<< "' does not exist\");\n}\n";
    return;
  }
  
  if (debug_node)
    errwriter << rule << "\n" << wmexpandnodes(1) << _body_root
	      << wmexpandnodes(0) << "\n";
  
  // Optimize the body
  _gen_modnames = rule->receiver_class()->default_modnames();
  Node *self = new SelfNode(_gen_modnames, *rule);
  InlineOptimizer inliner(self, inlineNo, _max_inline_level);
  _body_root = _body_root->optimize(&inliner);
  
  ConstOptimizer conster;
  _body_root = _body_root->optimize(&conster);
  
  // Fix calls up. This includes exception handling
  CallFixer call_fixer(_gen_modnames);
  _body_root = _body_root->optimize(&call_fixer);
  
  Type *return_type = rule->return_type();
  if (return_type == void_type && rule->can_throw()) {
    _body_root = new SemistrictNode
      (_body_root, ',', zero_node(*rule), int_type, *rule);
    return_type = int_type;
  }
  
  _body_root = new UnaryNode(_body_root, opCopy, return_type,
			     _body_root->betweenliner(), *_body_root);
  _body_root->fix_outline();
  _body_root->gen_prototypes(this);
#ifdef CHECK_GEN
  _body_root->clear_gen_counts();
#endif
  
  if (debug_node)
    errwriter << rule << "\n" << wmexpandnodes(1) << _body_root
	      << wmexpandnodes(0) << "\n";
  
  Target *target = _body_root->compile(this, false, 0);
  
  if (debug_target)
    target->print();
  
  // Make Forks from the target.
  target->count_enter();
  target->connect(0, this);
  
  // Optimize If locations.
  for (int i = 0; i < _blocks.size(); i++)
    _blocks[i]->better_logical();
  
  // No-one can jump directly to block 0, so don't consider it.
  for (int i = 1; i < _blocks.size(); i++)
    _blocks[i]->decide_direct();
  
  int labelno = 0;
  for (int i = 0; i < _blocks.size(); i++)
    if (_blocks[i]->need_label())
      _blocks[i]->set_label(++labelno);
  
  if (debug_loc) {
    for (int i = 0; i < _blocks.size(); i++)
      _blocks[i]->print();
  }
  
  for (int i = 0; i < _blocks.size(); i++)
    _blocks[i]->make_temporaries();
  
  // Find all temporaries.
  HashMap<PermString, int> temp_hash(-1);
  for (int i = 0; i < _blocks.size(); i++) {
    Vector<Fork *> forks;
    _blocks[i]->grep_forks(forks);
    for (int j = 0; j < forks.size(); j++) {
      Node *n = forks[j]->node();
      PermString tempnam = n->temporary();
      
      if (tempnam == star_string) {
	tempnam = permprintf("_T_%d", _temporaries.size() + 1);
	n->make_temporary(tempnam);
      }
      
      if (tempnam && temp_hash[tempnam] < 0) {
	int i = _temporaries.size();
	_temporaries.push_back(n);
	temp_hash.insert(tempnam, i);
      }
    }
  }
  
  // Make sure the exception temporary has the correct type, rather than the
  // nominal return type of one of the functions that could return an
  // exception
  PermString exceptid_name = exceptid_node()->temporary();
  for (int i = 0; i < _temporaries.size(); i++)
    if (_temporaries[i]->temporary() == exceptid_name)
      _temporaries[i]->change_type(any_type);
  
  rule->gen_prototype(out, false);
  gen();

#ifdef CHECK_GEN
  set_error_context("While compiling `%r':", rule);
  _body_root->check_gen_counts();
  reset_error_context();
#endif
}


void
Compiler::gen()
{
  out << "{\n" << wmindent(2);
  
  for (int t = 0; t < _temporaries.size(); t++)
    _temporaries[t]->gen_declare_temp(this);
  
  SelfNode::current_self_type = _gen_modnames;
  _blocks[0]->unreach();
  for (int i = 0; i < _blocks.size(); i++)
    if (!_blocks[i]->reached())
      _blocks[i]->gen(this);
  
  out << wmindent(-2) << "}\n";
}

void
Compiler::make_rethrow_handler()
{
  _rethrow_handler =
    exceptid_node()->compile(this, true, 0);
}

void
Compiler::set_exception_handler(Target *new_handler)
{
  if (new_handler == _rethrow_handler)
    _exception_handler = 0;
  else
    _exception_handler = new_handler;
}

Target *
Compiler::exception_handler()
{
  if (_exception_handler)
    return _exception_handler;
  if (!_rethrow_handler)
    make_rethrow_handler();
  return _rethrow_handler;
}
