#ifndef RULESET_HH
#define RULESET_HH
#include <lcdf/vector.hh>
#include "idcapsule.hh"
#include "gentrack.hh"
class Type;
class Rule;
class Namespace;
class ModuleNames;
class Writer;

class Ruleset {
  
  ModuleID _origin;
  ModuleID _actual;
  
  Vector<Rule *> _rules;
  Ruleset *_parent;
  Ruleset *_sibling;
  Ruleset *_child;
  
  mutable bool _any_dynamic: 1;
  mutable bool _any_dynamic_done: 1;
  mutable Type *_gen_type;
  mutable GenTracker _gen_track;
  
  bool calculate_any_dynamic() const;
  
 public:
  
  Ruleset(ModuleID);
  Ruleset(Ruleset *parent, ModuleID);
  
  Ruleset *parent() const			{ return _parent; }
  Ruleset *sibling() const			{ return _sibling; }
  Ruleset *child() const			{ return _child; }
  ModuleID origin() const			{ return _origin; }
  ModuleID actual() const			{ return _actual; }
  bool any_dynamic() const;
  int count() const				{ return _rules.size(); }
  
  Rule *rule(int i) const			{ return _rules[i]; }
  Rule *parent_rule(int i) const;
  
  int append(Rule *);
  void fill_in(int, Rule *);
  void override(int, Rule *);
  
  void check_types() const;
  
  Type *gen_type() const;
  void gen_struct(Writer &) const;
  void gen_item_name(Writer &) const;
  void gen_item_proto(Writer &) const;
  void gen_item(Writer &, ModuleNames *) const;
  
  void write_full(Writer &);
  
};

inline bool
Ruleset::any_dynamic() const
{
  return (_any_dynamic_done ? _any_dynamic : calculate_any_dynamic());
}

#endif
