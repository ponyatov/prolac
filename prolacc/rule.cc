#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "rule.hh"
#include "ruleset.hh"
#include "expr.hh"
#include "error.hh"
#include "node.hh"
#include "prototype.hh"


Rule::Rule(PermString bn, ModuleID mid, Namespace *ns, const Landmark &l)
  : _basename(bn), _origin(mid), _actual(mid), _contextsp(ns), _namesp(0),
    _return_type(0), _body(0),
    _is_exception(false), _is_static(false), _implicit(false),
    _undefined_implicit(false), _leaf(true),
    _constructor(false), _inlining(false), _compiled(false),
    _dyn_dispatch(false), _receiver_classed(false), _callable(false),
    _landmark(l)
{
}

Rule *
Rule::base_rule() const
{
  return version_in(_origin);
}

Rule *
Rule::version_in(ModuleID m) const
{
  Ruleset *rs = m->find_ruleset(_origin);
  return (rs ? rs->rule(_ruleindex) : 0);
}

void
Rule::make_undefined_implicit()
{
  // mark as compiled so we don't try to create its body later
  _undefined_implicit = _implicit = _compiled = true;
  set_return_type(any_type);
}

/*****
 * parameters
 **/

PermString
Rule::param_name(int i) const
{
  return _param[i]->temporary();
}

Type *
Rule::param_type(int i) const
{
  return _param[i]->type();
}

int
Rule::add_param(PermString name, const Landmark &lm)
{
  if (!_namesp)
    // Make our own namespace.
    _namesp = new Namespace("<parameterspace>", _contextsp, landmark());
  
  ParamNode *p = new ParamNode(name, _param.size(), 0, lm);
  _namesp->def(name, new NodeRef(name, p));
  int s = _param.size();
  _param.push_back(p);
  return s;
}

void
Rule::set_param_type(int pnum, Type *t)
{
  _param[pnum]->set_type(t);
}

bool
Rule::check_type(Rule *against) const
{
  // prevent extra errors: always OK to override undefined implicit rules
  if (undefined_implicit())
    return true;
  
  int errors = 0;
  
  if (_param.size() != against->_param.size()) {
    if (!errors++)
      error(*this, "type conflict in overriding `%r' method:", this);
    error(*this, "  wrong number of parameters");
  } else
    for (int i = 0; i < _param.size(); i++)
      if (!against->param_type(i)->conforms_to( param_type(i) )) {
	if (!errors++)
	  error(*this, "type conflict in overriding `%r' method:", this);
	error(*this, "  parameter %d type `%t' conflicts with `%t'",
	      i+1, param_type(i), against->param_type(i));
      }
  
  if (!_return_type->conforms_to( against->_return_type )) {
    if (!errors++)
      error(*this, "type conflict in overriding `%r' method:", this);
    error(*this, "  return type `%t' conflicts with `%t'", _return_type,
	  against->_return_type);
  }
  
  return errors == 0;
}


void
Rule::make_override(Rule *overriding)
{
  assert(!overriding->_is_static && !_is_static);
  overriding->_leaf = false;
}

bool Rule::report_circular_dependency_error = true;


Node *
Rule::make_body()
{
  if (_inlining) {
    // In some situations -- specifically, when we're looking at the contents
    // of a code block -- we don't want to report a circular dependency.
    if (report_circular_dependency_error)
      error(*this, "circular method dependency on `%r'", this);
    return 0;
  }
  set_inlining(true);
  _actual->protomodule()->resolve_rule(this);
  set_inlining(false);
  _compiled = true;
  return _body;
}


Type *
Rule::make_type()
{
  if (!return_type())
    _actual->protomodule()->resolve_rule_type(this);
  return return_type();
}


/*****
 * exceptions
 **/

void
Rule::make_exception(Exception *e)
{
  _is_exception = _is_static = _compiled = true;
  _body = new ExceptionNode(e, *this);
  _return_type = _body->type();
  _all_except += e;
}

Exception *
Rule::exception_value() const
{
  if (_is_exception)
    return _body->cast_exception()->exception();
  else
    return 0;
}

void
Rule::add_exception(Exception *e)
{
  _all_except += e;
}

bool
Rule::merge_exceptions(const ExceptionSet &o)
{
  bool changed = !(o <= _all_except);
  _all_except += o;
  return changed;
}


/*****
 * gen
 **/

void
Rule::receiver_class_analysis()
{
  _receiver_classed = true;
  if (_body)
    _body->receiver_class_analysis(constructor());
}

void
Rule::callable_analysis()
{
  _callable = true;
  if (_body)
    _body->callable_analysis();
}

void
Rule::gen_name(Writer &w) const
{
  if (_contextsp)
    _contextsp->gen_subname(w);
  else
    _actual->base_modnames()->gen_subname(w);
  w << _basename << "__" << _actual->gen_module_name();
}

void
Rule::gen_prototype(Writer &w, bool is_proto) const
{
  if (is_proto && !gen_proto_if())
    return;
  
  if (can_throw())
    int_type->gen(w);
  else
    _return_type->gen(w);
  w << (is_proto ? " " : "\n");
  gen_name(w);
  w << "(";
  
  //if (!is_proto) {
  if (!is_static()) {
    receiver_class()->gen(w);
    w << " *This" << (_param.size() ? ", " : "");
  }
  
  for (int i = 0; i < _param.size(); i++) {
    if (i) w << ", ";
    param_type(i)->gen_object(w, param_name(i));
  }
  //}
  
  w << ")" << (is_proto ? ";\n" : "\n");
}

void
Rule::gen_vtbl_name(Writer &w) const
{
  if (_contextsp)
    _contextsp->gen_subname(w);
  else
    _actual->base_modnames()->gen_subname(w);
  w << _basename;
}

void
Rule::gen_vtbl_member_decl(Writer &w) const
{
  _return_type->gen(w);
  w << " (*";
  gen_vtbl_name(w);
  w << ")(";

#if 1
  if (!is_static()) {
    _origin->gen(w);
    w << (_param.size() ? " *, " : " *");
  }
  
  for (int i = 0; i < _param.size(); i++) {
    if (i) w << ", ";
    _param[i]->type()->gen(w);
  }
#endif
  
  w << ");\n";
}


/*****
 * write
 **/

void
Rule::write_name(Writer &w) const
{
    if (_contextsp)
	_contextsp->write_sub(w);
    w << basename();
}

void
Rule::write(Writer &w) const
{
    _actual->base_modnames()->write(w);
    w << ".";
    write_name(w);
}

void
Rule::write_all(Writer &w) const
{
  w << ruleindex() << " / " << basename();
  
  if (_param.size()) {
    w << " (";
    for (int i = 0; i < _param.size(); i++) {
      if (i) w << ", ";
      w << _param[i];
    }
    w << ")";
  }
  
  if (_return_type != void_type)
    w << " :> " << _return_type;
  
  w << " / ";

  if (_implicit)
    w << "implicit ";
  if (_is_static)
    w << "static ";
  if (_leaf)
    w << "leaf ";
  
  if (_actual)
    w << _actual;
  
  if (_body)
    w << wmindent(5) << wmendl << _body << wmindent(-5);
  
  w << wmendl;
}
