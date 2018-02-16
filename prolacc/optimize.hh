#ifndef OPTIMIZE_HH
#define OPTIMIZE_HH
#include "node.hh"

class NodeOptimizer {
  
 public:
  
  virtual ~NodeOptimizer()				{ }

  virtual Node *do_call(const CallNode *n)		{ return (Node *)n; }
  virtual Node *do_constructor(const ConstructorNode *n){ return do_call(n); }
  virtual Node *do_param(const ParamNode *n)		{ return (Node *)n; }
  virtual Node *do_binary(const BinaryNode *n)		{ return (Node *)n; }
  virtual Node *do_semistrict(const SemistrictNode *n)	{ return (Node *)n; }
  virtual Node *do_effect(const EffectNode *n)		{ return (Node *)n; }
  virtual Node *do_unary(const UnaryNode *n)		{ return (Node *)n; }
  virtual Node *do_field(const FieldNode *n)		{ return (Node *)n; }
  virtual Node *do_cast(const CastNode *n)		{ return (Node *)n; }
  virtual Node *do_conditional(const ConditionalNode *n){ return (Node *)n; }
  virtual Node *do_let(const LetNode *n)		{ return (Node *)n; }
  virtual Node *do_exception(const ExceptionNode *n)	{ return (Node *)n; }
  virtual Node *do_catch(const CatchNode *n)		{ return (Node *)n; }
  virtual Node *do_code(const CodeNode *n)		{ return (Node *)n; }
  virtual Node *do_label(const LabelNode *n)		{ return (Node *)n; }
  virtual Node *do_self(const Node *n = 0)		{ return (Node *)n; }
  
  static Node *to_static_constant(const Node *, Module *);
  static bool to_integer_constant(const Node *, Module *, int &);
  
};


class InlineOptimizer: public NodeOptimizer {
  
  Node *_self;
  Type *_self_type;
  Node *_pass_self;
  Type *_pass_self_type;
  
  int _min_inline_level;
  int _max_inline_level;
  bool _inlining;
  
  Vector<Node *> _param;
  
  InlineOptimizer(Node *, Type *, Type *, int, int, const Vector<Node *> &);
  Node *temporarize_self(Node *, Node *&) const;
  Node *temporarize(Node *, ParamNode *, Node *&) const;
  Node *change_object_type(Node *, Type *) const;
  
 public:
  
  InlineOptimizer(Node *, int, int);
  
  Node *do_call(const CallNode *);
  Node *do_param(const ParamNode *);
  Node *do_code(const CodeNode *);
  Node *do_self(const Node * = 0);
  
};


class ConstOptimizer: public NodeOptimizer {
  
 public:
  
  ConstOptimizer()			{ }
  
  Node *do_binary(const BinaryNode *);
  Node *do_semistrict(const SemistrictNode *);
  Node *do_unary(const UnaryNode *);
  Node *do_cast(const CastNode *);
  Node *do_conditional(const ConditionalNode *);
  
};


/* these optimization passes are done as part of resolution */
   
class ParentFixer: public NodeOptimizer {
  
  Module *_module;
  Node *_self;
  HashMap<ModuleID, int> _parent_map;
  HashMap<FieldID, int> _slot_map;
  Vector<Field *> _fields;
  Vector<int> _constructed;
  Landmark _landmark;
  
 public:
  
  ParentFixer(ModuleNames *, const Landmark &);
  void add_parent(Module *);
  
  Node *do_call(const CallNode *);
  
  Node *add_implicits(Node *);
  
};

class ConstructorFixer: public NodeOptimizer {
  
  ModuleNames *_mn;
  Rule *_body_of;
  bool _recursion;
  
 public:
  
  ConstructorFixer(Rule *);
  bool recursion() const		{ return _recursion; }
  
  Node *do_call(const CallNode *);
  Node *do_constructor(const ConstructorNode *);
  
};

class TailRecursionFixer: public NodeOptimizer {
  
  Rule *_body_of;
  int _defining_this_param;
  bool _use_new_params;
  Vector<Node *> _new_params;
  
 public:
  
  TailRecursionFixer(Rule *r);
  
  Node *do_call(const CallNode *);
  Node *do_param(const ParamNode *);
  
};

class ExceptionCounter: public NodeOptimizer {
  
  ExceptionSet &_eset;
  
 public:
  
  ExceptionCounter(ExceptionSet &);
  
  Node *do_call(const CallNode *);
  Node *do_exception(const ExceptionNode *);
  Node *do_catch(const CatchNode *);
  
};

class ExceptionLocator: public NodeOptimizer {
  
  Exception *_exception;
  Rule *_context;
  bool _explicit_throw;
  
 public:
  
  ExceptionLocator(Exception *);
  Rule *context() const			{ return _context; }
  
  Node *do_call(const CallNode *);
  Node *do_exception(const ExceptionNode *);
  Node *do_catch(const CatchNode *);
  
};


/* these happen at a later stage, during compilation */

class CallFixer: public NodeOptimizer {
  
  ModuleNames *_self;
  
 public:
  
  CallFixer(ModuleNames *s)		: _self(s) { }
  
  Node *do_call(const CallNode *);
  
};


class ParamReplacer: public NodeOptimizer {
  
  Node *_self;
  Vector<Node *> _param;
  
 public:
  
  ParamReplacer(Node *, const Vector<Node *> &);
  
  Node *do_param(const ParamNode *);
  Node *do_self(const Node * = 0);
  
};


inline
ParamReplacer::ParamReplacer(Node *s, const Vector<Node *> &p)
  : _self(s), _param(p)
{
}

#endif
