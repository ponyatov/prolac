#ifndef PROTOTYPE_HH
#define PROTOTYPE_HH
#include "module.hh"
#include "modfrob.hh"
class Protomodule;
class Protofrob;
class Rule;
class ListExpr;
class NodeOptimizer;
class Program;


class Prototype: public Feature {
  
  ModuleNames *_modnames;
  Module *_module;
  Namespace *_parent_space;
  int _resolve_level;
  
 public:
  
  Prototype(PermString, Namespace *, const Landmark &);
  
  Module *module() const		{ return _module; }
  ModuleNames *modnames() const		{ return _modnames; }
  virtual ModuleNames *outer_modnames() const;
  Type *base_type() const		{ return _modnames; }
  Namespace *parent_space() const	{ return _parent_space; }
  
  Prototype *install_fake_modnames();
  void set_modnames(ModuleNames *mn);
  
  bool begin_resolve(int, bool *);
  void end_resolve(int);
  
  virtual bool resolve1() = 0;	// namespace
  virtual bool resolve2() = 0;	// fields
  virtual bool resolve3() = 0;	// implicit rule path
  virtual bool resolve4() = 0;	// rule bodies
  virtual bool resolve5() = 0;	// create inline levels, count own exceptions
  virtual void resolve6(bool &) = 0; // combine exceptions
  virtual void resolve7() = 0;	// check exceptions, copy inline levels
  
  virtual void grep_rules(Vector<Rule *> &) const = 0;
  virtual Prototype *skim_frob() const		{ return (Prototype *)this; }
  
  void write(Writer &) const;
  
  Prototype *cast_prototype()			{ return this; }
  virtual Protomodule *cast_protomodule()	{ return 0; }
  virtual Protofrob *cast_protofrob()		{ return 0; }
  
};


class Protomodule: public Prototype {

  Program *_program;
  
  Vector<Expr *> _parent_exprs;
  Vector<PermString> _parent_names;
  
  Vector<Expr *> _import_exprs;
  Vector<PermString> _import_names;
  
  Vector<Prototype *> _parents;
  Vector<Field *> _import_fields;
  Vector<Prototype *> _imports;
  int _real_import_count;
  
  Vector<Rule *> _rules;
  Vector<RuleRef *> _rulerefs;
  int _real_rule_count;
  Rule *_implicit_ctor_parent;
  int _ctor_rulei;
  
  Vector<Expr *> _rule_parameters;
  Vector<Expr *> _rule_return_types;
  Vector<Expr *> _rule_bodies;
  
  Vector<Field *> _slots;
  Vector<Expr *> _slot_types;
  Vector<Expr *> _slot_offsets;
  
  Vector<Field *> _implicit_path;
  Vector<int> _implicit_epochs;
  
  Protofrob *_pre_frobbed;
  Protofrob *_frobbed;
  Expr *_after_frob;
  
  void resolve0();
  void resolve1_implicit_constructor();
  void resolve1_make_implicit_rules(int);
  bool resolve2_rule(int);
  bool resolve2_slot(int);
  void resolve3_make_implicit_path(Module *, Vector<Module *> &);
  Node *resolve4_constructor_body(Rule *, Expr *);
  bool resolve4_rule(int); 
  bool resolve4_implicit_rule(int);
  bool resolve4_slot(int);
  bool resolve5_rule(int); 
  
  void do_rule_parameters(Rule *, ListExpr *, bool);

  void def1(PermString, Namespace *, Feature *, const char *context);
  void def1_header(PermString, Namespace *, Field *) const;
  
 public:
  
  Protomodule(PermString, Namespace *, Program *, const Landmark &);
  
  void add_parent(PermString, Expr *);
  void add_import(PermString, Expr *);
  Field *add_import_prototype(Prototype *, const Landmark &);
  
  int add_slot(PermString, Namespace *, const Landmark &, bool is_static,
	       Expr *type_expr);
  void set_slot_offset(int num, Expr *e)	{ _slot_offsets[num] = e; }
  
  Rule *add_rule(PermString, Namespace *, const Landmark &,
		 Expr *paramt, Expr *rett, Expr *body);
  void add_constructor(const Landmark &, Expr *paramt, Expr *body);

  void add_exception(PermString, Namespace *, const Landmark &);
  
  void set_after_frob(Expr *);
  
  Protofrob *frobbed() const			{ return _frobbed; }
  ModuleNames *outer_modnames() const;
  const Vector<Prototype *> &parents() const	{ return _parents; }
  
  void def1(PermString, Namespace *, Feature *);
  void def1(Namespace *n, Feature *f)		{ def1(f->basename(), n, f); }
  
  bool resolve1();
  bool resolve2();
  bool resolve3();
  bool resolve4();
  bool resolve5();
  void resolve6(bool &);
  void resolve7();

  void resolve_rule(Rule *);
  void resolve_rule_type(Rule *);
  
  bool resolve1_rule_override(RuleRef *mine, RuleRef *anc);
  void resolve1_add_implicit_rule(PermString, const Landmark &);
  
  void grep_rules(Vector<Rule *> &) const;
  
  Protomodule *cast_protomodule()		{ return this; }
  
};


class Protofrob: public Prototype {
  
  Prototype *_actual;
  Vector<ModuleFrob> _frobs;
  
 public:
  
  Protofrob(Prototype *, Namespace *, const Landmark &);
  
  bool any_frobs() const;
  ModuleNames *base_modnames() const		{ return _actual->modnames(); }
  
  void add_frob(const ModuleFrob &f)		{ _frobs.push_back(f); }
  
  bool resolve1();
  bool resolve2();
  bool resolve3();
  bool resolve4();
  bool resolve5();
  void resolve6(bool &);
  void resolve7();
  
  void grep_rules(Vector<Rule *> &) const;

  Prototype *skim_frob() const			{ return _actual; }
  Protofrob *cast_protofrob()			{ return this; }
  
};


class Protoequate: public Prototype {
  
  Expr *_expr;
  Prototype *_proto;
  
 public:
  
  Protoequate(PermString, Namespace *, Expr *, const Landmark &);
  
  bool resolve1();
  bool resolve2();
  bool resolve3();
  bool resolve4();
  bool resolve5();
  void resolve6(bool &);
  void resolve7();
  
  void grep_rules(Vector<Rule *> &) const;
  
};

#endif
