#ifndef MODFROB_HH
#define MODFROB_HH
#include "landmark.hh"
#include "operator.hh"
#include <lcdf/vector.hh>
class ModuleNames;
class Module;
class Namespace;
class Expr;
class Prototype;
class Rule;


class ModuleFrob {
  
  Operator _kind;
  Landmark _landmark;

  union {
    PermString::Capsule psc;
    Expr *e;
    Rule *rule;
    Module *module;
    Vector<Rule *> *rule_vec;
  } _v1;
  union {
    Expr *e;
    int i;
  } _v2;
  
  enum Is {
    isBlank = 0,
    isExpr,
    isRule,
    isRuleVec,
    isPermString,
    isInt,
    isModule,
  };
  
  Is _is1;
  Is _is2;
  
  static PermString resolve_identifier(Expr *, const char *);
  static int resolve_integer_constant(Expr *, Namespace *, const char *);
  
  Namespace *copy_namespace_parents(Namespace *, Namespace *) const;
  
  bool hide_handle_match(Namespace *, int, Module *, ModuleNames *, const char *) const;
  bool hide_whole_namespace(Namespace *, ModuleNames *) const;
  bool hide_find_matches(Namespace *, const char *, Module *, ModuleNames *) const;
  bool do_hide(ModuleNames *) const;
  bool do_show(ModuleNames *, bool) const;
  bool do_redo_self_ref(ModuleNames *) const;
  bool do_using(ModuleNames *, bool isu) const;
  void inline_namespace(ModuleNames *, Namespace *, Vector<Rule *> &) const;
  bool do_inline_1(ModuleNames *);
  bool do_inline_5(ModuleNames *, Namespace *) const;
  
 public:
  
  ModuleFrob()				: _kind(opNullFrob) { }
  ModuleFrob(Operator, Expr *, Expr *, const Landmark &);
  ModuleFrob(Operator, PermString, const Landmark &);
  ModuleFrob(Operator, PermString, int, const Landmark &);
  ModuleFrob(Operator, Rule *, int, const Landmark &);
  // XXX need ~ModuleFrob
  
  Operator kind() const			{ return _kind; }
  
  bool resolve1(ModuleNames *);
  bool resolve5(ModuleNames *, Namespace *) const;
  bool full_resolve(ModuleNames *, Namespace *);
  
  operator const Landmark &() const	{ return _landmark; }
  
};

#endif
