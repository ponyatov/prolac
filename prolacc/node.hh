#ifndef NODE_HH
#define NODE_HH
#include "type.hh"
#include "namespace.hh"
#include "rule.hh"
#include "literal.hh"
#include "operator.hh"
class PrototypeNode;
class NamespaceNode;
class VariableNode;
class SemistrictNode;
class FieldNode;
class CallNode;
class ConstructorNode;
class UnaryNode;
class BinaryNode;
class CastNode;
class TestNode;
class ExceptionNode;
class CatchNode;
class LiteralNode;
class SelfNode;
class Prototype;
class Rule;
class NodeOptimizer;
class Compiler;
class Target;
class CodeBlock;

extern PermString star_string;


enum GenCode {
    gcNormal = 0,
    gcTest,
    gcYes,
    gcNo,
};

enum GenContext {
    gctxNone = 0,
    gctxMember,
    gctxPointerCast,
};

class Betweenliner { public:

    Betweenliner();
    Betweenliner(bool is_inline, int, Betweenliner);
  
    int inline_level() const		{ return _inline_level; }
    int outline_level() const		{ return _outline_level; }
    int outline_epoch() const		{ return _outline_epoch; }
  
    Betweenliner &change_outline(Betweenliner);

  private:
  
    signed char _inline_level;
    signed char _outline_level;
    short _outline_epoch;
  
    static short last_outline_epoch;
  
};


class Node {
  
  Type *_type;
  Landmark _landmark;
  
  Betweenliner _betweenliner;
  int _usage;
  PermString _temporary;
  GenCode _gen_code;
  
  GenContext gen_value_temp(Compiler *);

#ifdef CHECK_GEN
  int _n_gen_state;
  int _n_gen_value;
  int _n_uses;
#endif
  
 public:
  
  static Betweenliner cur_betweenliner;
  
  Node(Type *, Betweenliner, const Landmark &);
  Node(Type *, const Landmark &);
  Node(const Node &);
  virtual ~Node()			{ }
  
  operator const Landmark &() const	{ return _landmark; }
  const Landmark &landmark() const	{ return _landmark; }
  
  Type *type() const			{ return _type; }
  void set_type(Type *);
  void change_type(Type *);
  
  virtual bool is_static() const		{ return false; }
  virtual bool simple_type() const		{ return false; }
  virtual bool fixed_run_time_type() const	{ return false; }
  virtual bool lvalue() const			{ return false; }
  virtual bool interpolatable() const		{ return false; }
  virtual Node *state_wrap()			{ return this; }
  virtual Node *simple_value() const;
  virtual Node *call_object() const;
  
  Node *type_convert(Type *, const Landmark &);
  Node *type_cast(Type *, const Landmark &);
  Node *make_cast(const Type *cast_to, const Landmark &lm) const;
  virtual Node *make_deref() const;
  virtual Node *make_address() const;
  
  virtual Feature *feature(PermString n) const;
  virtual Node *object() const		{ return (Node *)this; }
  
  typedef void (Node::*TraverseHook)(void *);
  virtual void traverse(TraverseHook, void *);
  
  int count_operations();
  virtual void count_operations(void *);
  
  // USAGE AND TEMPORARIES
  
  void reset_usage()			{ _usage = 0; }
  void incr_usage()			{ _usage++; }
  void decr_usage(void * = 0);
  void mark_usage()			{ traverse(&Node::decr_usage, 0); }
  bool unequal_usage() const		{ return _usage > 0; }
  
  PermString temporary() const		{ return _temporary; }
  void make_temporary();
  void make_temporary(PermString t)	{ _temporary = t; }

  // ANALYSIS
  
  void receiver_class_analysis(bool in_constructor);
  virtual void receiver_class_analysis(void *);
  
  void callable_analysis()		{ callable_analysis(0); }
  virtual void callable_analysis(void *);
  
  int mark_tail_recursions(Rule *);
  virtual void mark_tail_recursions(void *);
  void mark_param_usage(Vector<int> &);
  virtual void mark_param_usage(void *);
  
  // COMPILING
  
  int inline_level() const	{ return _betweenliner.inline_level(); }
  int outline_level() const	{ return _betweenliner.outline_level(); }
  int outline_epoch() const	{ return _betweenliner.outline_epoch(); }
  
  Betweenliner betweenliner() const		{ return _betweenliner; }
  void set_betweenliner(Betweenliner b)		{ _betweenliner = b; }
  
  void fix_outline();
  void gen_prototypes(Compiler *);
  virtual void fix_outline(void *);
  virtual void gen_prototypes(void *);
  
  virtual Node *optimize(NodeOptimizer *) const;
  virtual Target *compile(Compiler *, bool, Target *);
  
  Target *compile_test(Node *&, Compiler *, Target *, Target *);
  
  // GENERATING
  
  void set_gen_code(GenCode gc)			{ _gen_code = gc; }
  GenCode gen_code() const			{ return _gen_code; }
  
  virtual void gen_declare_temp(Compiler *);
  
  virtual bool must_gen_state() const;
  virtual bool must_gen_state_real() const	{ return false; }
  void gen_state(Compiler *);
  virtual void gen_state_real(Compiler *);
  virtual void gen_state_temp(Compiler *);
  
  GenContext gen_value(Compiler *, GenContext = gctxNone);
  virtual GenContext gen_value_real(Compiler *, GenContext);
  virtual bool must_gen_value() const		{ return false; }
  virtual PermString gen_value_string() const;
  
#ifdef CHECK_GEN
  void clear_gen_counts()		{ clear_gen_counts(0); }
  void clear_gen_counts(void *);
  void check_gen_counts()		{ count_uses(0); check_gen_counts(0); }
  void check_gen_counts(void *);
  void count_uses(void *);
#endif
  
  // PRINTING, CLONING, CASTING
  
  virtual void write(Writer &) const;
  virtual void write_short(Writer &w)		{ write(w); }
  
  virtual Node *clone(const Landmark &)	const;
  
  virtual PrototypeNode *cast_prototype()	{ return 0; }
  virtual NamespaceNode *cast_namespace()	{ return 0; }
  virtual FieldNode *cast_field()		{ return 0; }
  virtual CallNode *cast_call()			{ return 0; }
  virtual ConstructorNode *cast_constructor()	{ return 0; }
  virtual UnaryNode *cast_unary()		{ return 0; }
  virtual BinaryNode *cast_binary()		{ return 0; }
  virtual SemistrictNode *cast_semistrict()	{ return 0; }
  virtual CastNode *cast_cast()			{ return 0; }
  virtual TestNode *cast_test()			{ return 0; }
  virtual ExceptionNode *cast_exception()	{ return 0; }
  virtual CatchNode *cast_catch()		{ return 0; }
  virtual LiteralNode *cast_literal()		{ return 0; }
  virtual SelfNode *cast_self()			{ return 0; }
  static bool is_self(const Node *n);
  
};


class PrototypeNode: public Node {
  
  Prototype *_proto;
  bool _afterfrob;
  Vector<Node *> _ctor_args;
  
 public:
  
  PrototypeNode(Prototype *, bool afterfrob, const Landmark &);
  
  bool in_afterfrob() const		{ return _afterfrob; }
  
  void add_ctor_arg(Node *n)		{ _ctor_args.push_back(n); }
  
  Prototype *prototype() const		{ return _proto; }
  void resolve1();
  
  bool is_static() const		{ return true; }
  bool simple_type() const		{ return true; }
  Node *simple_value() const;
  
  void write(Writer &) const;
  PrototypeNode *clone(const Landmark &) const;
  
  PrototypeNode *cast_prototype()	{ return this; }
  
};


class NamespaceNode: public Node {
  
  Node *_object;
  Namespace *_namesp;
  
 public:
  
  NamespaceNode(Node *, Namespace *, const Landmark &);
  NamespaceNode(Node *, const NamespaceNode &);
  
  Namespace *namesp() const		{ return _namesp; }
  
  bool is_static() const;
  Feature *feature(PermString n) const;
  Node *object() const			{ return _object; }
  Node *simple_value() const;
  Node *call_object() const;
  
  Node *optimize(NodeOptimizer *) const;

  NamespaceNode *cast_namespace()	{ return this; }
  
};


class FieldNode: public Node {
  
  Node *_object;
  Field *_field;
  bool _is_static;
  
 public:
  
  FieldNode(Node *, Field *, bool staticp, const Landmark &);
  FieldNode(Node *, const FieldNode &);
  
  Field *field() const			{ return _field; }
  Node *field_of() const		{ return _object; }
  
  //ModuleNames *modnames() const;
  Feature *feature(PermString n) const;
  
  bool is_static() const		{ return _is_static; }
  bool simple_type() const;
  bool fixed_run_time_type() const;
  bool lvalue() const;
  bool interpolatable() const;
  Node *simple_value() const;
  Node *call_object() const;
  
  void traverse(TraverseHook, void *);
  Node *optimize(NodeOptimizer *) const;
  
  Target *compile(Compiler *, bool, Target *);
  GenContext gen_value_real(Compiler *, GenContext);
  
  void write(Writer &) const;
  void write_short(Writer &) const;
  
  FieldNode *cast_field()		{ return this; }
  
};


class CallNode : public Node { public:
  
    CallNode(Node *, Rule *, Type *, bool, Betweenliner, const Landmark &);
    CallNode(Node *, const Vector<Node *> &, const CallNode &);
  
    void add_param(Node *n)		{ _params.push_back(n); }
    int param_count() const		{ return _params.size(); }
    Node *param(int i) const		{ return _params[i]; }
    const Vector<Node *> &parameters() const { return _params; }
  
    Node *call_of() const		{ return _call_of; }
    void set_call_of(Node *n);
    Rule *rule() const			{ return _rule; }
    Rule *fixed_rule() const;
    void change_rule(Rule *r) 		{ _rule = r; }
    ModuleID origin() const		{ return _rule->origin(); }
    int ruleindex() const		{ return _rule->ruleindex(); }
    bool tail_recursion() const		{ return _tail_recursion; }
    void set_tail_recursion(bool tr)	{ _tail_recursion = tr; }
  
    void traverse(TraverseHook, void *);
    void receiver_class_analysis(void *);
    void callable_analysis(void *);
    void mark_tail_recursions(void *);
    void gen_prototypes(void *);
    Node *optimize(NodeOptimizer *) const;

    Target *compile(Compiler *, bool, Target *);
    GenContext gen_value_real(Compiler *, GenContext);
    bool must_gen_value() const		{ return !_tail_recursion; }
  
    void write(Writer &) const;
  
    CallNode *cast_call()			{ return this; }
  
  protected:
  
    Node *_call_of;
    Rule *_rule;
    Vector<Node *> _params;
    bool _fixed_rule: 1;
    bool _tail_recursion: 1;
  
};


class ConstructorNode: public CallNode { public:
  
  ConstructorNode(Node *, Rule *, Betweenliner, const Landmark &);
  ConstructorNode(Node *, const Vector<Node *> &, const ConstructorNode &);
  
  static ConstructorNode *make_implicit(Node *object, Module *parent,
					Betweenliner, const Landmark &);
  
  bool simple_type() const		{ return true; }
  Node *simple_value() const;
  
  Node *optimize(NodeOptimizer *) const;
  
  ConstructorNode *cast_constructor()	{ return this; }
  
};


class LetNode: public Node {
  
  Vector<Node *> _items;
  Node *_body;
  
 public:
  
  LetNode(const Landmark &);
  
  void add_param(Node *p)		{ _items.push_back(p); }
  void set_body(Node *);
  
  Node *simple_value() const;
  
  void traverse(TraverseHook, void *);
  void mark_tail_recursions(void *);
  Node *optimize(NodeOptimizer *) const;
  
  Target *compile(Compiler *, bool, Target *);
  GenContext gen_value_real(Compiler *, GenContext);
  
  void write(Writer &) const;
  
};


class DeclarationNode: public Node {
  
  Node *_variable;
  Node *_value;
  
 public:
  
  DeclarationNode(Node *, Node *, const Landmark &);
  DeclarationNode(Node *, Node *, const DeclarationNode &);
  
  Node *simple_value() const;
  
  void traverse(TraverseHook, void *);
  Node *optimize(NodeOptimizer *) const;
  
  Target *compile(Compiler *, bool, Target *);
  bool must_gen_state_real() const	{ return true; }
  void gen_state_real(Compiler *);
  
  void write(Writer &) const;
  
};


class VariableNode: public Node {
  
  PermString _name;

  static int uniqueifier;
  
 public:
  
  VariableNode(PermString, Type *, const Landmark &);
  
  PermString name() const		{ return _name; }
  
  bool lvalue() const			{ return true; }
  bool fixed_run_time_type() const;
  Node *simple_value() const;
  void count_operations(void *);
  
  Target *compile(Compiler *, bool, Target *);
  bool must_gen_state() const		{ return false; }
  void gen_state_temp(Compiler *);
  GenContext gen_value_real(Compiler *, GenContext);
  
  void write(Writer &) const;
  VariableNode *clone(const Landmark &) const;
  
};


class ParamNode: public VariableNode {
  
  int _number;
  bool _assigned;
  
 public:
  
  ParamNode(PermString, int, Type *, const Landmark &);
  
  int number() const			{ return _number; }
  bool assigned() const			{ return _assigned; }
  
  Node *state_wrap();
  void mark_param_usage(void *);
  
  Node *optimize(NodeOptimizer *) const;
  ParamNode *clone(const Landmark &) const;
  
  void gen_declare_temp(Compiler *);
  
  void write(Writer &) const;

};


class BinaryNode: public Node {
 protected:
  
  Operator _op;
  Node *_left;
  Node *_right;
  
 public:
  
  BinaryNode(Node *, Operator, Node *, Type *, const Landmark &);
  BinaryNode(Node *, Node *, const BinaryNode &);
  
  Operator op() const			{ return _op; }
  Node *left() const			{ return _left; }
  Node *right() const			{ return _right; }
  void set_right(Node *n)		{ _right = n; }
  
  void traverse(TraverseHook, void *);
  Node *optimize(NodeOptimizer *) const;
  
  Target *compile(Compiler *, bool, Target *);
  GenContext gen_value_real(Compiler *, GenContext);
  
  void write(Writer &) const;
  void write_short(Writer &) const;
  
  BinaryNode *cast_binary()		{ return (BinaryNode *)this; }
  
};


class SemistrictNode: public BinaryNode {
    
 public:
  
  SemistrictNode(Node *, Operator, Node *, Type *, const Landmark &);
  SemistrictNode(Node *, Node *, const SemistrictNode &);
  
  Node *simple_value() const;
  void count_operations(void *);
  
  Node *optimize(NodeOptimizer *) const;
  
  void fix_outline(void *);
  void mark_tail_recursions(void *);
  
  Target *compile(Compiler *, bool, Target *);
  GenContext gen_value_real(Compiler *, GenContext);
  
  void write(Writer &) const;
  void write_short(Writer &) const;
  
  SemistrictNode *cast_semistrict()		{ return this; }
  
};


class EffectNode: public BinaryNode {
  
 public:
  
  EffectNode(Node *, Operator, Node *, Type *, const Landmark &);
  EffectNode(Node *, Node *, const EffectNode &);
  
  Node *simple_value() const;
  
  Node *optimize(NodeOptimizer *) const;
  
  bool must_gen_state_real() const	{ return true; }
  void gen_state_real(Compiler *);
  GenContext gen_value_real(Compiler *, GenContext);
  
};


class ConditionalNode: public Node {
  
  Node *_test;
  Node *_yes;
  Node *_no;
  
 public:
  
  ConditionalNode(Node *, Node *, Node *, Type *, const Landmark &);
  ConditionalNode(Node *, Node *, Node *, const ConditionalNode &);
  
  Node *test() const			{ return _test; }
  Node *yes() const			{ return _yes; }
  Node *no() const			{ return _no; }
  
  void traverse(TraverseHook, void *);
  Node *optimize(NodeOptimizer *) const;
  
  void fix_outline(void *);
  void mark_tail_recursions(void *);
  
  Target *compile(Compiler *, bool, Target *);
  GenContext gen_value_real(Compiler *, GenContext);
  
  void write(Writer &) const;
  void write_short(Writer &) const;
  
};


class UnaryNode: public Node {
  
  Operator _op;
  Node *_child;
  
 public:
  
  UnaryNode(Node *, Operator, Type *, Betweenliner, const Landmark &);
  UnaryNode(Node *, Operator, Type *);
  UnaryNode(Node *, const UnaryNode &);
  
  Operator op() const			{ return _op; }
  Node *child() const			{ return _child; }
  
  bool lvalue() const;
  bool fixed_run_time_type() const;
  bool interpolatable() const;
  Node *simple_value() const;
  
  Node *make_deref() const;
  Node *make_address() const;
  
  void traverse(TraverseHook, void *);
  Node *optimize(NodeOptimizer *) const;
  
  Target *compile(Compiler *, bool, Target *);
  bool must_gen_value() const		{ return _op.side_effects(); }
  GenContext gen_value_real(Compiler *, GenContext);
  
  void write(Writer &) const;
  void write_short(Writer &) const;
  
  UnaryNode *cast_unary()		{ return this; }
  
};


class CastNode: public Node {
  
  Node *_child;
  
 public:
  
  CastNode(Node *, Type *, Betweenliner, const Landmark &);
  CastNode(Node *, Type *);
  CastNode(Node *, const CastNode &);
  
  Node *child() const			{ return _child; }

  Node *simple_value() const;
  bool interpolatable() const;
  void count_operations(void *);

  void traverse(TraverseHook, void *);
  Node *optimize(NodeOptimizer *) const;
  
  Target *compile(Compiler *, bool, Target *);
  GenContext gen_value_real(Compiler *, GenContext);
  
  void write(Writer &) const;
  void write_short(Writer &) const;
  
  CastNode *cast_cast()			{ return this; }
  
};


class TestNode: public Node {
  
  Node *_test;
  
 public:
  
  TestNode(Node *);
  
  void traverse(TraverseHook, void *);
  Node *optimize(NodeOptimizer *) const;
  
  Target *compile(Compiler *, bool, Target *);
  Target *compile_test(Compiler *, Target *, Target *);
  GenContext gen_value_real(Compiler *, GenContext);
  
  void write(Writer &) const;
  void write_short(Writer &) const;

  TestNode *cast_test()			{ return this; }
};


class ExceptionNode: public Node {
  
  Exception *_exception;
  
 public:
  
  ExceptionNode(const Landmark &);
  ExceptionNode(Exception *, const Landmark &);
  
  Exception *exception() const		{ return _exception; }
  
  Node *optimize(NodeOptimizer *) const;
  
  Target *compile(Compiler *, bool, Target *);
  GenContext gen_value_real(Compiler *, GenContext);
  void write(Writer &) const;
  
  ExceptionNode *cast_exception()	{ return this; }
  
};

class CatchNode: public Node {

  Node *_child;
  ExceptionSet _eset;
  bool _catch_all;

 public:
  
  CatchNode(Node *, ExceptionSet, bool, const Landmark &);
  CatchNode(Node *, const CatchNode &);

  ExceptionSet exception_set() const	{ return _eset; }
  bool catch_all() const		{ return _catch_all; }
  Node *child() const			{ return _child; }
  
  void traverse(TraverseHook, void *);
  Node *optimize(NodeOptimizer *) const;
  
  Target *compile(Compiler *, bool, Target *);
  GenContext gen_value_real(Compiler *, GenContext);
  
  void write(Writer &) const;
  void write_short(Writer &) const;
  
  CatchNode *cast_catch()		{ return this; }
  
};


class LiteralNode: public Node {
  
  Literal _literal;
  
 public:
  
  LiteralNode(const Literal &);
  LiteralNode(Type *, const Landmark &);
  LiteralNode(Type *, long, const Landmark &);
  
  long vlong() const			{ return _literal.vlong(); }
  
  bool simple_type() const		{ return _literal.is_type(); }
  bool interpolatable() const;
  Node *simple_value() const;
  
  Target *compile(Compiler *, bool, Target *);
  GenContext gen_value_real(Compiler *, GenContext);
  
  void write(Writer &) const;
  LiteralNode *clone(const Landmark &) const;
  
  LiteralNode *cast_literal()		{ return this; }
  
};


class CodeNode: public Node {
  
  CodeBlock *_code;
  Node *_self;
  Vector<Node *> _params;
  bool _have_params;
  
 public:
  
  CodeNode(CodeBlock *);
  CodeNode(Node *, const Vector<Node *> &, const CodeNode &);
  
  Node *simple_value() const;
  bool have_params() const		{ return _have_params; }
  
  Node *optimize(NodeOptimizer *) const;
  Target *compile(Compiler *, bool, Target *);
  bool must_gen_state_real() const	{ return true; }
  void gen_state_real(Compiler *);
  GenContext gen_value_real(Compiler *, GenContext);
  
  void write(Writer &) const;
  
};


class VtblNode: public Node {

  Module *_vtbl_from;
  Node *_object;
  
 public:
  
  VtblNode(Module *, Node *, const Landmark &);
  
  void traverse(TraverseHook, void *);
  Node *optimize(NodeOptimizer *) const;
  
  Target *compile(Compiler *, bool, Target *);
  bool must_gen_state_real() const	{ return true; }
  void gen_state_real(Compiler *);
  
  void write(Writer &) const;
  
};


class SelfNode : public Node { public:
  
    SelfNode(Type *, const Landmark &);
    SelfNode(Type *, bool, const Landmark &);
  
    bool super() const			{ return _super; }
  
    bool lvalue() const;
    Node *simple_value() const;
    void count_operations(void *);
  
    Node *optimize(NodeOptimizer *) const;
    Target *compile(Compiler *, bool, Target *);
  
    static Type *current_self_type;
    GenContext gen_value_real(Compiler *, GenContext);
    PermString gen_value_string() const;
  
    void write(Writer &) const;
    SelfNode *clone(const Landmark &) const;

    SelfNode *cast_self()		{ return this; }

  private:
  
    bool _super;

};


class LabelNode: public Node {
  
  Node *_child;
  Target *_target;
  
 public:
  
  LabelNode(Node *);
  
  void traverse(TraverseHook, void *);
  Node *optimize(NodeOptimizer *) const;
  Target *compile(Compiler *, bool, Target *);
  
  void mark_tail_recursions(void *);
  
  GenContext gen_value_real(Compiler *, GenContext);
  //PermString gen_value_string() const;
  
  void write(Writer &) const;
  void write_short(Writer &) const;
  
};


VariableNode *exceptid_node();
LiteralNode *zero_node(const Landmark &);
LiteralNode *true_node(const Landmark &);
LiteralNode *false_node(const Landmark &);


inline
Node::Node(Type *t, Betweenliner b, const Landmark &l)
  : _type(t), _landmark(l), _betweenliner(b)
#ifdef CHECK_GEN
  , _n_gen_state(0), _n_gen_value(0)
#endif
{
}

inline
Node::Node(Type *t, const Landmark &l)
  : _type(t), _landmark(l), _betweenliner(cur_betweenliner)
#ifdef CHECK_GEN
  , _n_gen_state(0), _n_gen_value(0)
#endif
{
}

inline
Node::Node(const Node &n)
  : _type(n._type), _landmark(n._landmark), _betweenliner(n._betweenliner)
#ifdef CHECK_GEN
  , _n_gen_state(0), _n_gen_value(0)
#endif
{
}

inline bool
Node::is_self(const Node *n)
{
  return !n || ((Node *)n)->cast_self();
}

inline
LiteralNode::LiteralNode(const Literal &lit)
  : Node(lit.type(), lit.landmark()), _literal(lit)
{
}

inline
LiteralNode::LiteralNode(Type *type, const Landmark &lm)
  : Node(type, lm), _literal(Literal(type, lm))
{
}

inline
LiteralNode::LiteralNode(Type *type, long value, const Landmark &lm)
  : Node(type, lm), _literal(Literal(type, value, lm))
{
}

inline void
Node::make_temporary()
{
  if (!_temporary) _temporary = star_string;
}

inline Node *
Node::type_convert(Type *into, const Landmark &lm)
{
  return into->convert_to(this, lm);
}

inline Node *
Node::type_cast(Type *into, const Landmark &lm)
{
  return into->cast_to(this, lm);
}

inline Node *
Node::make_cast(const Type *to, const Landmark &lm) const
{
  if (to == _type)
    return (Node *)this;
  else
    return new CastNode((Node *)this, (Type *)to, betweenliner(), lm);
}

inline bool
NamespaceNode::is_static() const
{
  return _object ? _object->is_static() : true;
}

inline bool
Node::must_gen_state() const
{
  return _temporary || must_gen_state_real();
}

inline void
Node::gen_state(Compiler *c)
{
  if (_temporary) {
    gen_state_temp(c);
  } else {
#ifdef CHECK_GEN
    _n_gen_state++;
#endif
    gen_state_real(c);
  }
}

inline GenContext
Node::gen_value(Compiler *c, GenContext gctx)
{
  if (_temporary) {
    return gen_value_temp(c);
  } else {
#ifdef CHECK_GEN
    _n_gen_value++;
#endif
    return gen_value_real(c, gctx);
  }
}

inline Writer &
operator<<(Writer &w, const Node *n)
{
  if (n) n->write(w);
  else w << "(null)";
  return w;
}

#endif
