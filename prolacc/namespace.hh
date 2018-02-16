#ifndef NAMESPACE_HH
#define NAMESPACE_HH
#include <lcdf/permstr.hh>
#include <lcdf/vector.hh>
#include <lcdf/hashmap.hh>
#include "feature.hh"
class ModuleNames;
class Protomodule;

class ConcreteNamespace {
  
  PermString _name;
  HashMap<PermString, int> _h;
  Vector<Feature *> _f;
  Vector<PermString> _n;
  int _refcount;
  
  ConcreteNamespace(const ConcreteNamespace &);
  
 public:
  
  ConcreteNamespace(PermString n);
  ConcreteNamespace *clone(Namespace *new_parent) const;
  
  void use()				{ _refcount++; }
  void unuse()				{ if (!--_refcount) delete this; }
  bool uniquely_referenced() const	{ return _refcount == 1; }
  
  PermString name() const		{ return _name; }
  
  Feature *find(PermString n) const	{ return _f[ _h[n] ]; }
  int find_index(PermString n) const	{ return _h[n]; }
  PermString find_name(Feature *f) const;
  
  bool def(PermString n, Feature *f);
  bool undef(PermString n);
  void clear();
  bool rename(PermString old, PermString nuu);
  
  int featcount() const			{ return _f.size(); }
  Feature *feat(int i) const;
  PermString featname(int i) const;
  
  void name_resolve_parent(Namespace *, Protomodule *, ModuleNames *) const;
  void name_resolve_internal(Namespace *, Protomodule *) const;
  
  void write_full(Writer &) const;
  
};

class Namespace: public Feature {
  
  Namespace *_parent;
  ConcreteNamespace *_cns;
  
  Namespace &operator=(const Namespace &) { assert(0); return *this; }
  
 public:
  
  Namespace(PermString, const Landmark &);
  Namespace(PermString, Namespace *parent, const Landmark &);
  Namespace(const Namespace &ns);
  Namespace(const Namespace &ns, Namespace *);
  ~Namespace()				{ _cns->unuse(); }
  
  Namespace *parent() const		{ return _parent; }
  ConcreteNamespace *concrete() const	{ return _cns; }
  void change_concrete(ConcreteNamespace *);
  PermString concrete_name() const	{ return _cns->name(); }
  
  Feature *find(PermString n) const	{ return _cns->find(n); }
  int find_index(PermString n) const	{ return _cns->find_index(n); }
  Feature *search(PermString n) const;
  PermString find_name(Feature *f) const { return _cns->find_name(f); }
  
  int featcount() const			{ return _cns->featcount(); }
  Feature *feat(int i) const;
  PermString featname(int i) const;
  
  bool def(PermString n, Feature *f) const { return _cns->def(n, f); }
  bool def_reparent(PermString n, Feature *f, bool is_const) const;
  bool undef(PermString n) const	{ return _cns->undef(n); }
  void clear() const			{ _cns->clear(); }
  
  void make_unique();
  
  void name_resolve_parent(Namespace *, Protomodule *, ModuleNames *) const;
  void name_resolve_internal(Namespace *, Protomodule *) const;
  
  RuleRef *find_ruleref() const;
  
  PermString gen_name() const;
  PermString gen_subname() const;
  void gen_subname(Writer &) const;

  void write(Writer &) const;
  void write_sub(Writer &) const;
  void write_full(Writer &) const;
  PermString description() const	{ return "namespace"; }
  
  Namespace *cast_namespace()		{ return this; }
  
};


inline
ConcreteNamespace::ConcreteNamespace(PermString n)
  : _name(n), _h(0), _refcount(0)
{
  _f.push_back((Feature *)0);
  _n.push_back(PermString());
}

inline Feature *
ConcreteNamespace::feat(int featureindex) const
{
  assert(featureindex > 0 && featureindex < _f.size());
  return _f[featureindex];
}

inline PermString
ConcreteNamespace::featname(int featureindex) const
{
  assert(featureindex > 0 && featureindex < _f.size());
  return _n[featureindex];
}

inline
Namespace::Namespace(PermString n, const Landmark &l)
  : Feature(n, ModuleID(0), l), _parent(0),
    _cns(new ConcreteNamespace(n))
{
  _cns->use();
}

inline
Namespace::Namespace(PermString n, Namespace *parent, const Landmark &l)
  : Feature(n, parent->origin(), l), _parent(parent),
    _cns(new ConcreteNamespace(n))
{
  _cns->use();
}

inline
Namespace::Namespace(const Namespace &ns, Namespace *new_parent)
  : Feature(ns), _parent(new_parent), _cns(ns._cns)
{
  _cns->use();
}

inline Feature *
Namespace::feat(int featureindex) const
{
  return _cns->feat(featureindex);
}

inline PermString
Namespace::featname(int i) const
{
  return _cns->featname(i);
}

extern PermString inaccessible_string;

#endif
