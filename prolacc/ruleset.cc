#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ruleset.hh"
#include "rule.hh"
#include "module.hh"
#include "compiler.hh"
#include <cstdio>

Ruleset::Ruleset(ModuleID origin)
  : _origin(origin), _actual(origin),
    _parent(0), _sibling(0), _child(0),
    _any_dynamic_done(false),
    _gen_type(0)
{
}

Ruleset::Ruleset(Ruleset *parent, ModuleID module)
  : _origin(parent->_origin), _actual(module), _rules(parent->_rules),
    _parent(parent), _sibling(0), _child(0),
    _any_dynamic_done(false),
    _gen_type(0)
{
  if (parent->_child) 
    _sibling = parent->_child;
  parent->_child = this;
}

Rule *
Ruleset::parent_rule(int i) const
{
  return _parent ? _parent->rule(i) : 0;
}

int
Ruleset::append(Rule *r)
{
  assert(!_parent);
  int s = _rules.size();
  _rules.push_back(r);
  return s;
}

void
Ruleset::fill_in(int index, Rule *r)
{
  assert(!_parent && !_rules[index]);
  _rules[index] = r;
}

void
Ruleset::override(int num, Rule *r)
{
  assert(_parent && _actual == r->actual());
  r->make_override( _rules[num] );
  _rules[num] = r;
}

void
Ruleset::check_types() const
{
  if (!_parent) return;
  for (int i = 0; i < _rules.size(); i++) {
    Rule *r = _rules[i];
    Rule *pr = _parent->_rules[i];
    if (pr != r)
      r->check_type(pr);
  }
}

void
Ruleset::write_full(Writer &w)
{
  w << _origin << " :: ";
  
  for (Ruleset *rs = this; rs; rs = rs->_parent)
    w << rs->_actual << " ";
  
  w << "::=\n";
  
  for (int i = 0; i < _rules.size(); i++)
    _rules[i]->write_all(w);
}

bool
Ruleset::calculate_any_dynamic() const
{
  _any_dynamic_done = true;
  Ruleset *root = _origin->find_ruleset(_origin);
  for (int i = 0; i < _rules.size(); i++)
    if (root->_rules[i]->dyn_dispatch())
      return (_any_dynamic = true);
  return (_any_dynamic = false);
}

/*****
 * gen
 **/

Type *
Ruleset::gen_type() const
{
  if (!_gen_type) {
    if (_parent)
      _gen_type = _parent->gen_type();
    else
      _gen_type = new VtblType(_origin);
  }
  return _gen_type;
}

void
Ruleset::gen_struct(Writer &w) const
{
  assert(_origin == _actual);
  if (any_dynamic()) {
    w << gen_type() << " {\n" << wmindent(2);
    
    for (int i = 0; i < _rules.size(); i++)
      if (!_rules[i]->leaf()) {
	_rules[i]->gen_vtbl_member_decl(w);
      }
    
    w << wmindent(-2) << "};\n\n";
  }
}

void
Ruleset::gen_item_name(Writer &w) const
{
  w << _actual->gen_module_name() << "__vtbl_";
  w << _origin->gen_module_name();
}

void
Ruleset::gen_item_proto(Writer &w) const
{
  if (!_gen_track.protogen_if())
    return;
  
  w << "extern " << gen_type() << " ";
  gen_item_name(w);
  w << ";\n";
}

void
Ruleset::gen_item(Writer &w, ModuleNames *) const
{
  if (!_gen_track.gen_if())
    return;
  
  Ruleset *root = _origin->find_ruleset(_origin);
  
  w << gen_type() << " ";
  gen_item_name(w);
  w << " = { ";
  
  int count = 0;
  for (int i = 0; i < _rules.size(); i++)
    if (!root->_rules[i]->leaf()) {
      if (count++) w << ", ";
      if (_rules[i]->dyn_dispatch())
	_rules[i]->gen_name(w);
      else
	//w << "evil_func /* no dynamic dispatch */";
	w << "0 /*nodyn*/";
    }
  
  w << " };\n";
}
