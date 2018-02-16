#ifndef RULE_HH
#define RULE_HH
#include "feature.hh"
#include "exception.hh"
#include <lcdf/vector.hh>
#include "gentrack.hh"
#include "module.hh"
class Expr;
class Node;
class ParamNode;

enum InlineLevel {
  
  inlineNo = 0,
  inlineDefault = 1,
  inlineYes = 2,
  inlinePath = 3,
  
  inlineImplicitRule = inlineYes,
  inlineSimpleConstructor = inlineYes,
  inlineTrivial = inlineYes,
  
  inlineTrivialNodeCount = 5,
  
};


class Rule {
  
  PermString _basename;
  ModuleID _origin;
  ModuleID _actual;
  int _ruleindex;
  
  Namespace *_contextsp;
  Namespace *_namesp;
  
  Vector<Node *> _param;
  Type *_return_type;
  Node *_body;
  
  ExceptionSet _all_except;
  
  bool _is_exception: 1;
  bool _is_static: 1;
  bool _implicit: 1;
  bool _undefined_implicit: 1;
  bool _leaf: 1;
  bool _constructor: 1;
  bool _inlining: 1;
  bool _compiled: 1;
  bool _dyn_dispatch: 1;
  bool _receiver_classed: 1;
  bool _callable: 1;
  
  mutable GenTracker _gen_track;
  Landmark _landmark;
  
 public:
  
  Rule(PermString bn, ModuleID actual, Namespace *ns, const Landmark &);
  
  PermString basename() const		{ return _basename; }
  ModuleID origin() const		{ return _origin; }
  ModuleID actual() const		{ return _actual; }
  int ruleindex() const			{ return _ruleindex; }
  Rule *base_rule() const;
  Rule *version_in(ModuleID) const;
  
  operator const Landmark &() const	{ return _landmark; }
  const Landmark &landmark() const	{ return _landmark; }
  
  Namespace *contextsp() const		{ return _contextsp; }
  Namespace *namesp() const		{ return _namesp?_namesp:_contextsp; }
  void set_contextsp(Namespace *n)	{ if (n) _contextsp = n; }
  
  bool is_exception() const		{ return _is_exception; }
  bool is_static() const		{ return _is_static; }
  bool implicit() const			{ return _implicit; }
  bool undefined_implicit() const	{ return _undefined_implicit; }
  bool leaf() const			{ return _leaf; }
  bool constructor() const		{ return _constructor; }
  bool inlining() const			{ return _inlining; }
  bool compiled() const			{ return _compiled; }
  bool dyn_dispatch() const		{ return base_rule()->_dyn_dispatch; }
  bool receiver_classed() const		{ return _receiver_classed; }
  bool callable() const			{ return _callable; }
  
  void set_origin(ModuleID o, int ri)	{ _origin = o; _ruleindex = ri; }
  void set_static(bool s)		{ _is_static = s; }
  void make_implicit()			{ _implicit = true; }
  void make_undefined_implicit();
  void make_constructor()		{ _constructor = true; }
  void set_inlining(bool i)		{ _inlining = i; }
  void mark_dynamic_dispatch()		{ base_rule()->_dyn_dispatch = true; }
  
  void receiver_class_analysis();
  void callable_analysis();
  
  void make_override(Rule *r);
  
  // TYPE
  
  bool check_type(Rule *) const;
  
  Type *return_type() const		{ return _return_type; }
  void set_return_type(Type *t)		{ _return_type = t; }
  Type *make_type();
  
  // PARAMETERS
  
  ParamNode *param(int i) const		{ return (ParamNode *)_param[i]; }
  int param_count() const		{ return _param.size(); }
  PermString param_name(int i) const;
  Type *param_type(int i) const;
  
  int add_param(PermString, const Landmark &);
  void set_param_type(int, Type *);
  
  // BODY
  
  Node *make_body();
  Node *body()			{ return _compiled ? _body : make_body(); }
  void set_body(Node *n)	{ _body = n; _compiled = true; }
  static bool report_circular_dependency_error;

  // EXCEPTIONS

  void make_exception(Exception *);
  Exception *exception_value() const;
  
  ExceptionSet all_exceptions() const	{ return _all_except; }
  bool can_throw() const		{ return _all_except; }
  void add_exception(Exception *);
  bool merge_exceptions(const ExceptionSet &);
  
  // GEN
  
  //void mark_actual(ModuleID);
  void mark_gen()			{ _gen_track.mark(); }
  ModuleID receiver_class() const;
  bool need_gen() const			{ return _gen_track.need(); }
  bool gen_done() const			{ return _gen_track.done(); }
  bool gen_proto_if() const		{ return _gen_track.pregen_if(); }
  bool gen_if() const			{ return _gen_track.gen_if(); }
  
  void gen_name(Writer &) const;
  void gen_prototype(Writer &, bool is_proto) const;
  void gen_vtbl_name(Writer &) const;
  void gen_vtbl_member_decl(Writer &) const;
  
  // WRITE
  
  void write(Writer &w) const;
  void write_name(Writer &w) const;
  void write_all(Writer &w) const;
  
};


inline ModuleID
Rule::receiver_class() const
{
  return _actual->receiver_class();
}

inline Writer &
operator<<(Writer &w, const Rule *r)
{
  r->write(w);
  return w;
}

#endif
