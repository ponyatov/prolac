#ifndef MODULE_HH
#define MODULE_HH
#include "type.hh"
#include "namespace.hh"
#include "ruleset.hh"
class Relation;
class Expr;
class Module;
class Compiler;
class Protomodule;

extern PermString all_string;
extern PermString allstatic_string;
extern PermString constructor_string;


class ModuleNames: public Namespace, public Type {

  Prototype *_prototype;
  Namespace *_parent_space;
  Module *_module;
  bool _own_module;
  
  HashMap<PermString, int> _using;
  bool _explicit_using;
  
  HashMap<RuleID, int> _inliners;
  HashMap<ModuleID, int> _module_inliners;
  
  bool _from_equation;
  ModuleNames *_trivial_equation;
  
  PointerType *_pointer_type;
  
  mutable GenTracker _gen_track;
  
 public:
  
  ModuleNames(PermString, Namespace *, Protomodule *, const Landmark &);
  ModuleNames(const ModuleNames &, Prototype *);
  
  void force_equation(PermString, Namespace *);
  void force_trivial_equation(PermString, Namespace *, ModuleNames *);
  void copy_inliners(const ModuleNames &);

  Prototype *prototype() const		{ return _prototype; }
  Module *module() const		{ return _module; }
  ModuleNames *base_modnames() const;
  
  Feature *feature(PermString n) const	{ return find(n); }
  
  bool allow_import() const;
  bool allow_ancestor() const;

  PointerType *make_pointer(const Landmark &) const;
  
  bool conforms_to(const Type *) const;
  bool same_type(const Type *) const;
  Node *convert_to(Node *, const Landmark &, bool = false) const;
  Node *convert_pointer_to(Node *, ModuleNames *, const Landmark &, bool = false) const;
  Node *cast_pointer_to(Node *, ModuleNames *, const Landmark &) const;

  int size() const;
  int align() const;
  
  Rule *find_rule(ModuleID, int) const;
  Rule *find_rule(const Rule *) const;
  Rule *find_rule(const RuleRef *) const;
  
  void resolve1_parent(ModuleNames *);
  
  Type *header_type(Type *) const;
  inline ModuleNames *header_module(ModuleNames *) const;
  
  // USING
  
  bool is_using_empty(bool = false) const;
  bool is_using(PermString n) const	{ return _using[n] != 0; }
  void set_using(PermString, bool);
  void clear_using(bool);
  void set_using_all_static();
  
  bool has_implicit_feature(PermString, bool want_static) const;
  
  // INLINE LEVELS
  
  int inline_level(Rule *) const;
  void set_inline_level(Rule *, int);
  void clear_inline(ModuleID, int);
  
  // WRITE, GEN, CAST
  
  void write(Writer &) const;
  void write_super(Writer &) const;
  void write_full(Writer &, bool ns, bool rs);
  int gen_parts(Writer &, PermString &) const;
  void gen_prototype(Writer &) const;
  void gen_create(Writer &) const;
  PermString gen_name() const;
  
  ModuleNames *cast_modnames()			{ return this; }
  const ModuleNames *cast_modnames() const	{ return this; }
  Module *cast_module() const			{ return _module; }
  
};


class Module {
  
  Protomodule *_protomodule;
  ModuleNames *_base_modnames;
  ModuleNames *_default_modnames;
  Module *_receiver_class;
  Module *_parent_class;
  
  PermString _gen_name;
  PermString _object_name;
  
  HashMap<ModuleID, int> _ancestor_map;
  Vector<Ruleset *> _rsets;
  
  HashMap<FieldID, int> _field_map;
  Vector<Field *> _fields;
  int _anc_field_count;
  Field *_self_field;
  Field *_self_vtbl_field;

  Vector<Field *> _all_slots;
  int _size;
  int _align;
  
  HashMap<ModuleID, int> _module_map;
  Vector<ModuleNames *> _filtered_modules;
  
  bool _need_struct;
  Vector<Module *> _anc_layout;
  mutable GenTracker _gen_track;
  
  friend class ModuleFrob;
  
  Module(const Module &);
  
  bool merge_rulesets(int, Ruleset *);
  bool merge_fields(int, Field *);
  void merge_receiver_class(Module *);
  
  void gen_slots(Writer &) const;
  void gen_static_slots(Writer &) const;
  
  Field *self_vtbl_field();
  
 public:
  
  Module(Protomodule *, ModuleNames *);
  Field *make_self();
  
  // BASIC

  Protomodule *protomodule() const	{ return _protomodule; }
  ModuleNames *base_modnames() const	{ return _base_modnames; }
  ModuleNames *default_modnames() const	{ return _default_modnames; }
  Type *base_type() const		{ return _base_modnames; }
  PermString basename() const		{ return _base_modnames->basename(); }
  ModuleID actual() const		{ return this; }
  ModuleID receiver_class() const;
  ModuleID parent_class() const		{ return _parent_class; }
  
  operator const Landmark &() const	{ return _base_modnames->landmark(); }
  const Landmark &landmark() const	{ return _base_modnames->landmark(); }
  
  // VARIOUS PARTS
  
  bool has_ancestor(ModuleID mid) const	{ return _ancestor_map[mid] >= 0; }
  bool has_ancestor(Module *m) const	{ return has_ancestor(m->actual()); }

  bool each_ancestor(int &, ModuleID &) const;
  
  Ruleset *find_ruleset(ModuleID) const;
  Rule *find_rule(ModuleID, int) const;
  Rule *find_rule(const Rule *) const;
  Rule *find_rule(const RuleRef *) const;
  
  Field *self() const			{ return _self_field; }
  Ruleset *self_ruleset() const;
  Rule *constructor_rule() const;
  
  // TYPE
  
  int size() const			{ return _size; }
  int align() const			{ return _align; }
  
  void check_override_types();
  
  ModuleNames *header_module(ModuleNames *) const;
  
  void grep_my_fields(Vector<Field *> &) const;
  void grep_my_rules(Vector<Rule *> &) const;
  
  // RESOLVE
  
  void add_field(Field *);
  
  bool resolve1_parent(Module *);
  bool resolve1_self();
  bool resolve1_rule(Rule *, RuleRef *);
  void resolve1_constructor(Rule *, RuleRef *);
  bool resolve1_field(Field *);
  
  bool resolve2_parent(Module *);
  void resolve2_module_map();
  void insert_module_map(ModuleNames *);
  
  bool resolve3_implicit_rule(int);
  
  void layout1();
  void layout2();
  void set_default_modnames(ModuleNames *);
  
  // GEN

  void mark_receiver_class(Module *);
  PermString gen_module_name() const	{ return _gen_name; }
  void mark_gen() const			{ _gen_track.mark(); }
  
  void gen(Writer &) const;		// from Type
  PermString gen_object_name() const	{ return _object_name; }
  void gen_slot_types(Writer &) const;

  void gen_prototype(Writer &) const;
  void gen_struct(Writer &) const;
  void gen_vtbl_proto(Writer &) const;
  void gen_vtbl(Writer &, ModuleNames *) const;
  void gen_assign_vtbl(Compiler *, Node *) const;
  PermString gen_self_vtbl_name();
  
  // WRITE
  
  void write(Writer &) const;
  void write_rulesets(Writer &) const;
  
  const Module *cast_module() const	{ return this; }
  
};


inline ModuleNames *
ModuleNames::base_modnames() const
{
  return _module->base_modnames();
}

inline ModuleNames *
ModuleNames::header_module(ModuleNames *mn) const
{
  if (mn->_module == _module)
    return (ModuleNames *)this;
  else
    return _module->header_module(mn);
}

inline Rule *
ModuleNames::find_rule(const RuleRef *rref) const
{
  return _module->find_rule(rref->origin(), rref->ruleindex());
}

inline Rule *
ModuleNames::find_rule(ModuleID origin, int ruleindex) const
{
  return _module->find_rule(origin, ruleindex);
}

inline Ruleset *
Module::self_ruleset() const
{
  assert(_rsets.back()->origin() == actual());
  return _rsets.back();
}

inline Rule *
Module::constructor_rule() const
{
  return self_ruleset()->rule(0);
}

inline ModuleID
Module::receiver_class() const
{
  return _receiver_class ? _receiver_class : this;
}

inline Rule *
ModuleNames::find_rule(const Rule *rule) const
{
  return _module->find_rule(rule);
}

inline void
Module::mark_receiver_class(Module *m)
{
  if (!_receiver_class || _receiver_class == m)
    _receiver_class = m;
  else
    merge_receiver_class(m);
}

inline Writer &
operator<<(Writer &w, const Module *m)
{
  m->write(w);
  return w;
}

inline Writer &
operator<<(Writer &w, ModuleID mid)
{
  mid->write(w);
  return w;
}

#endif
