#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "namespace.hh"
#include "prototype.hh"
#include "error.hh"
#include "node.hh"
#include "field.hh"
#include <cassert>

static PermString::Initializer initializer;
PermString inaccessible_string = "<inaccessible>";


ConcreteNamespace::ConcreteNamespace(const ConcreteNamespace &cn)
  : _name(cn._name),
    _h(cn._h), _f(cn._f), _n(cn._n),
    _refcount(0)
{
}


ConcreteNamespace *
ConcreteNamespace::clone(Namespace *new_parent) const
{
  ConcreteNamespace *ncn = new ConcreteNamespace(*this);
  for (int i = 1; i < _f.size(); i++)
    if (Namespace *nested = _f[i]->cast_namespace())
      ncn->_f[i] = new Namespace(*nested, new_parent);
  return ncn;
}


bool
ConcreteNamespace::def(PermString name, Feature *f)
{
  //f->use();
  int i = _h[name];
  if (i) {
    //_f[i]->unuse();
    _f[i] = f;
    return false;
  } else {
    i = _f.size();
    _f.push_back(f);
    _n.push_back(name);
    _h.insert(name, i);
    return true;
  }
}


bool
ConcreteNamespace::undef(PermString name)
{
  int i = _h[name];
  if (!i) return false;
  //_f[i]->unuse();
  _h.insert(name, 0);
  
  // Now, move the last element in the _f and _n arrays in the place of i.
  Feature *lastf = _f.back();
  _f.pop_back();
  PermString lastn = _n.back();
  _n.pop_back();
  if (i < _f.size()) {
    _f[i] = lastf;
    _n[i] = lastn;
    _h.insert(lastn, i);
  }
  return true;
}


void
ConcreteNamespace::clear()
{
  _h.clear();
  _f.clear();
  _n.clear();
  _f.push_back((Feature *)0);
  _n.push_back(PermString());
}


bool
ConcreteNamespace::rename(PermString old, PermString nuu)
{
  int i = _h[old];
  if (!i || _h[nuu]) return false;
  
  _n[i] = nuu;
  _h.insert(old, 0);
  _h.insert(nuu, i);
  return true;
}


Feature *
Namespace::search(PermString name) const
{
  const Namespace *ns = this;
  while (ns) {
    Feature *f = ns->find(name);
    if (f) return f;
    ns = ns->parent();
  }
  return 0;
}


PermString
ConcreteNamespace::find_name(Feature *f) const
{
  for (int i = 1; i < _f.size(); i++)
    if (_f[i] == f)
      return _n[i];
  return PermString();
}


void
Namespace::name_resolve_parent(Namespace *result, Protomodule *proto,
			       ModuleNames *modnames_p) const
{
  _cns->name_resolve_parent(result, proto, modnames_p);
}

void
Namespace::name_resolve_internal(Namespace *result, Protomodule *proto) const
{
  _cns->name_resolve_internal(result, proto);
}

inline bool
Namespace::def_reparent(PermString n, Feature *f, bool is_const) const
{
  if (Namespace *namesp = f->cast_namespace())
    if (is_const)
      return _cns->def(n, new Namespace(*namesp, (Namespace *)this));
    else
      namesp->_parent = (Namespace *)this;
  
  return _cns->def(n, f);
}


void
ConcreteNamespace::name_resolve_parent(Namespace *result,
				       Protomodule *proto,
				       ModuleNames *modnames_p) const
{
  for (int i = 1; i < _f.size(); i++) {
    Feature *f_p = _f[i];
    PermString name = _n[i];
    Feature *f_r = result->find(name);
    
    // No problem if the same feature is inherited twice under the same name.
    if (f_p == f_r)
      continue;
    
    // Don't include imports (the self-reference is taken care of in modfrob).
    Field *field_p = f_p->cast_field();
    if (field_p && (field_p->is_import() || field_p->is_ancestor()))
      continue;
    
    // Just define it if it's not already defined.
    if (f_r == 0) {
      result->def_reparent(name, f_p, true);
      continue;
    }
    
    // Now, act depending on what f is.
    if (f_p->cast_ruleref()) {
      if (name == constructor_string)
	continue;
      
    } else if (field_p) {
      if (Field *field_r = f_r->cast_field()) {
	if (field_p->is_ancestor() && field_r->is_ancestor()
	    && field_p->module() == field_r->module())
	  continue;
      }
      
    } else if (Namespace *ns_p = f_p->cast_namespace()) {
      if (Namespace *ns_r = f_r->cast_namespace()) {
	ns_r->make_unique();
	ns_p->name_resolve_parent(ns_r, proto, modnames_p);
      }

    } else
      assert(0);
    
    // If we get here, there's been an error.
    error(*f_p, "name conflict on `%s'", name.c_str());
    error(*f_p, "  %-F inherited from `%m'", f_p, f_p->origin().obj());
    error(*f_r, "  %-F inherited from `%m'", f_r, f_r->origin().obj());
  }
}


void
ConcreteNamespace::name_resolve_internal(Namespace *result,
					 Protomodule *proto) const
{
  for (int i = 1; i < _f.size(); i++) {
    Feature *f_i = _f[i];
    PermString name = _n[i];
    Feature *f_r = result->find(name);
    
    RuleRef *rule_i = f_i->cast_ruleref();
    if (rule_i)
      rule_i->set_contextsp(result);
    
    // Just define it if it's not already defined.
    if (f_r == 0) {
      // We want to just change the parent pointer. That way, any rules in the
      // namespace & its children will end up with the right context namespace
      // even though we didn't pass over their RuleRefs!
      result->def_reparent(name, f_i, false);
      continue;
    }
    
    // Now, act depending on what f is.
    if (rule_i) {
      if (RuleRef *rule_r = f_r->cast_ruleref()) {
	// an override; set the destination feature's rule index
	if (!proto->resolve1_rule_override(rule_i, rule_r))
	  result->def(name, rule_i);
	continue;
      }
      
    } else if (f_i->cast_field()) {
      // This is always an error: slot overriding something else.
      // This empty block's here so we don't hit assert(0) for unknown feature.
      
    } else if (Namespace *ns_i = f_i->cast_namespace()) {
      if (Namespace *ns_r = f_r->cast_namespace()) {
	ns_r->make_unique();
	ns_i->name_resolve_internal(ns_r, proto);
	continue;
	
      } else if (Field *field_r = f_r->cast_field())
	error(*ns_i, "cannot override parts of %-f `%s'", field_r,
	      name.c_str());
      
    } else
      assert(0);
    
    // If we get here, there's been an error. Only report the error if we
    // haven't already.
    if (f_r->origin() != proto->module()) {
      error(*f_i, "name conflict on %-F `%s'", f_i, name.c_str());
      error(*f_r, "  %-F inherited from `%m'", f_r, f_r->origin().obj());
    }
    // Don't define over something we already defined (which would probably be
    // a parent or import).
    if (f_r->origin() != f_i->origin())
      result->def_reparent(name, f_i, false);
  }
}


void
Namespace::change_concrete(ConcreteNamespace *new_cns)
{
  new_cns->use();
  _cns->unuse();
  _cns = new_cns;
}


void
Namespace::make_unique()
{
  if (!_cns->uniquely_referenced())
    change_concrete(_cns->clone(this));
}


RuleRef *
Namespace::find_ruleref() const
{
  if (Feature *f = find(concrete_name()))
    return f->cast_ruleref();
  return 0;
}


/*****
 * gen
 * print
 **/

void
Namespace::write(Writer &w) const
{
  if (_parent && _parent->concrete_name()) {
    _parent->write(w);
    w << '.';
  }
  w << concrete_name();
}

void
Namespace::write_sub(Writer &w) const
{
  if (_parent && _parent->concrete_name()) {
    _parent->write_sub(w);
    w << concrete_name() << '.';
  }
}

void
ConcreteNamespace::write_full(Writer &w) const
{
  for (int i = 1; i < featcount(); i++) {
    w << _n[i] << " ::= ";
    _f[i]->write_full(w);
    w << wmendl;
  }
}


void
Namespace::write_full(Writer &w) const
{
  w << "namespace ";
  write(w);
  w << " {\n" << wmindent(2);
  _cns->write_full(w);
  w << wmindent(-2) << "}";
}


PermString
Namespace::gen_name() const
{
  if (_parent)
    if (PermString parent_name = _parent->gen_name())
      return permprintf("%p__%p", parent_name.capsule(),
			concrete_name().capsule());
  return concrete_name();
}


PermString
Namespace::gen_subname() const
{
  if (_parent) {
    if (PermString parent_name = _parent->gen_subname())
      return permprintf("%p__%p", parent_name.capsule(),
			concrete_name().capsule());
    else
      return concrete_name();
  } else
    return PermString();
}


void
Namespace::gen_subname(Writer &w) const
{
  if (_parent) {
    _parent->gen_subname(w);
    w << concrete_name() << "__";
  }
}
