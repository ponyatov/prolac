#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "optimize.hh"
#include "node.hh"
#include "prototype.hh"
#include "module.hh"
#include "error.hh"
#include "field.hh"
#include "operator.hh"
#include "target.hh"
#include "codeblock.hh"
#include "compiler.hh"
#include "writer.hh"


static PermString::Initializer initializer;
PermString star_string = "*";

Betweenliner Node::cur_betweenliner;
short Betweenliner::last_outline_epoch = 0;


Betweenliner::Betweenliner()
    : _inline_level(-1), _outline_level(-1), _outline_epoch(0)
{
}


Betweenliner::Betweenliner(bool is_inline, int level, Betweenliner prev)
{
    if (is_inline) {
	_outline_epoch = prev._outline_epoch;
	_outline_level = prev._outline_level;
	_inline_level = level;
    
    } else {
	// check for `short' wrap-around (should never happen, but inexpensive)
	if (last_outline_epoch == -1)
	    ++last_outline_epoch;
	_outline_epoch = ++last_outline_epoch;
	_outline_level = level;
	_inline_level = prev._inline_level;
    }
}


Betweenliner &
Betweenliner::change_outline(Betweenliner b)
{
    _outline_epoch = b._outline_epoch;
    _outline_level = b._outline_level;
    return *this;
}


PrototypeNode::PrototypeNode(Prototype *p, bool afterfrob, const Landmark &l)
    : Node(p->base_type(), l), _proto(p), _afterfrob(afterfrob)
{
}


NamespaceNode::NamespaceNode(Node *of, Namespace *ns, const Landmark &l)
    : Node(namespace_type, l), _object(of), _namesp(ns)
{
}

NamespaceNode::NamespaceNode(Node *of, const NamespaceNode &n)
    : Node(n), _object(of), _namesp(n._namesp)
{
}


FieldNode::FieldNode(Node *fof, Field *f, bool is_static, const Landmark &l)
    : Node(f->type(), l), _object(fof), _field(f), _is_static(is_static)
{
    assert(f);
    // This assertion is still true, but useless -- it was used to catch a
    // specific bug
    assert(!fof || !fof->cast_literal() || !fof->simple_type());
}

FieldNode::FieldNode(Node *fof, const FieldNode &n)
    : Node(n), _object(fof), _field(n._field), _is_static(n._is_static)
{
}


CallNode::CallNode(Node *of, Rule *rule, Type *return_type, bool fixed_rule,
		   Betweenliner b, const Landmark &l)
    : Node(return_type, b, l), _call_of(of), _rule(rule),
      _fixed_rule(fixed_rule), _tail_recursion(false)
{
    /*if (_call_of && _call_of->fixed_run_time_type()) {
	errwriter << "1 " << this << wmendl;
	_fixed_rule = true;}*/
}

CallNode::CallNode(Node *of, const Vector<Node *> &params, const CallNode &n)
    : Node(n), _call_of(of), _rule(n._rule), _params(params),
      _fixed_rule(n._fixed_rule), _tail_recursion(n._tail_recursion)
{
    /*    if (_call_of && _call_of->fixed_run_time_type()) {
	errwriter << "2 " << this << wmendl;
	_fixed_rule = true;}*/
}

void
CallNode::set_call_of(Node *n)
{
    assert(!_call_of);
    _call_of = n;
    /*if (_call_of && _call_of->fixed_run_time_type())
    _fixed_rule = true;*/
}

Rule *
CallNode::fixed_rule() const
{
    if (_fixed_rule)
	return _rule;
    if (_call_of && _call_of->fixed_run_time_type())
	return _rule->version_in(_call_of->type()->cast_module());
    Rule *r = _rule->version_in(_rule->receiver_class());
    return (r->leaf() ? r : 0);
}

ConstructorNode::ConstructorNode(Node *of, Rule *rule,
				 Betweenliner b, const Landmark &l)
  : CallNode(of, rule, of->type(), false, b, l)
{
  assert(rule->constructor());
  //errwriter << of << "\n";
}

ConstructorNode::ConstructorNode(Node *of, const Vector<Node *> &params,
				 const ConstructorNode &node)
  : CallNode(of, params, node)
{
  //change_type(of->type());
}

ConstructorNode *
ConstructorNode::make_implicit(Node *object, Module *parent,
			       Betweenliner b, const Landmark &l)
{
  Rule *ctor_rule = parent->constructor_rule();
  if (ctor_rule->param_count() > 0) {
    error(l, "too few arguments to `%m.constructor'", parent);
    return 0;
  }
  return new ConstructorNode(object, ctor_rule, b, l);
}


LetNode::LetNode(const Landmark &lm)
  : Node(0, lm), _body(0)
{
}


DeclarationNode::DeclarationNode(Node *var, Node *val, const Landmark &l)
  : Node(void_type, l), _variable(var), _value(val)
{
}

DeclarationNode::DeclarationNode(Node *var, Node *val,
				 const DeclarationNode &n)
  : Node(n), _variable(var), _value(val)
{
}


int VariableNode::uniqueifier = 1;

VariableNode::VariableNode(PermString name, Type *t, const Landmark &l)
  : Node(t, l), _name(name)
{
  make_temporary(permprintf("%p_%d", name.capsule(), uniqueifier++));
}


ParamNode::ParamNode(PermString name, int num, Type *t, const Landmark &l)
  : VariableNode(name, t, l), _number(num), _assigned(false)
{
  make_temporary(name);
}


BinaryNode::BinaryNode(Node *left, Operator op, Node *right, Type *type,
		       const Landmark &l)
  : Node(type, l), _op(op), _left(left), _right(right)
{
  assert(_left && _right);
}

BinaryNode::BinaryNode(Node *left, Node *right, const BinaryNode &n)
  : Node(n), _op(n._op), _left(left), _right(right)
{
}


SemistrictNode::SemistrictNode(Node *left, Operator op, Node *right,
			       Type *type, const Landmark &l)
  : BinaryNode(left, op, right, type, l)
{
}

SemistrictNode::SemistrictNode(Node *left, Node *right,
			       const SemistrictNode &n)
  : BinaryNode(left, right, n)
{
}


EffectNode::EffectNode(Node *left, Operator op, Node *right,
		       Type *type, const Landmark &l)
  : BinaryNode(left, op, right, type, l)
{
}

EffectNode::EffectNode(Node *left, Node *right, const EffectNode &n)
  : BinaryNode(left, right, n)
{
}


ConditionalNode::ConditionalNode(Node *test, Node *yes, Node *no, Type *type,
				 const Landmark &l)
  : Node(type, l), _test(test), _yes(yes), _no(no)
{
  assert(_test && _yes && _no);
}

ConditionalNode::ConditionalNode(Node *test, Node *yes, Node *no,
				 const ConditionalNode &n)
  : Node(n), _test(test), _yes(yes), _no(no)
{
}


UnaryNode::UnaryNode(Node *child, Operator op, Type *type, Betweenliner b,
		     const Landmark &l)
  : Node(type, b, l), _op(op), _child(child)
{
  assert(_child);
}

UnaryNode::UnaryNode(Node *child, Operator op, Type *type)
  : Node(type, child->betweenliner(), child->landmark()), _op(op),
    _child(child)
{
  assert(_child);
}

UnaryNode::UnaryNode(Node *child, const UnaryNode &n)
  : Node(n), _op(n._op), _child(child)
{
}


CastNode::CastNode(Node *ch, Type *type, Betweenliner b, const Landmark &l)
  : Node(type, b, l), _child(ch)
{
  assert(_child);
}

CastNode::CastNode(Node *ch, Type *type)
  : Node(type, ch->betweenliner(), ch->landmark()), _child(ch)
{
  assert(_child);
}

CastNode::CastNode(Node *ch, const CastNode &n)
  : Node(n), _child(ch)
{
}


TestNode::TestNode(Node *t)
  : Node(void_type, t->betweenliner(), t->landmark()), _test(t)
{
}


ExceptionNode::ExceptionNode(const Landmark &l)
  : Node(any_type, l), _exception(0)
{
}

ExceptionNode::ExceptionNode(Exception *e, const Landmark &l)
  : Node(any_type, l), _exception(e)
{
  make_temporary("__except_id");
}

CatchNode::CatchNode(Node *child, ExceptionSet eset, bool catch_all,
		     const Landmark &l)
  : Node(bool_type, l), _child(child), _eset(eset), _catch_all(catch_all)
{
  assert(_child);
}

CatchNode::CatchNode(Node *child, const CatchNode &n)
  : Node(n), _child(child), _eset(n._eset), _catch_all(n._catch_all)
{
}


CodeNode::CodeNode(CodeBlock *code)
  : Node(bool_type, code->landmark()), _code(code), _self(0), _have_params(0)
{
}

CodeNode::CodeNode(Node *s, const Vector<Node *> &params, const CodeNode &c)
  : Node(c), _code(c._code), _self(s), _params(params), _have_params(1)
{
}


VtblNode::VtblNode(Module *m, Node *o, const Landmark &l)
  : Node(m->base_modnames(), l), _vtbl_from(m), _object(o)
{
}


SelfNode::SelfNode(Type *t, const Landmark &lm)
  : Node(t, lm), _super(false)
{
}

SelfNode::SelfNode(Type *t, bool is_super, const Landmark &lm)
  : Node(t, lm), _super(is_super)
{
}


LabelNode::LabelNode(Node *ch)
  : Node(*ch), _child(ch), _target(0)
{
  assert(_child);
}


VariableNode *
exceptid_node()
{
  static VariableNode *v;
  if (!v) {
    v = new VariableNode("__except_id", int_type, Landmark());
    v->make_temporary("__except_id");
  }
  return v;
}

LiteralNode *
zero_node(const Landmark &lm)
{
  return new LiteralNode(int_type, 0L, lm);
}

LiteralNode *
true_node(const Landmark &lm)
{
  return new LiteralNode(bool_type, 1L, lm);
}

LiteralNode *
false_node(const Landmark &lm)
{
  return new LiteralNode(bool_type, 0L, lm);
}

/*****
 * Node etc.
 **/

void
Node::set_type(Type *t)
{
  assert(!_type);
  _type = t;
}

void
Node::change_type(Type *t)
{
  assert(_type);
  _type = t;
}


/*****
 * LetNode etc.
 **/

void
LetNode::set_body(Node *body)
{
  assert(!_body);
  _body = body;
  set_type(_body->type());
}


/*****
 * prototype node
 **/

void
PrototypeNode::resolve1()
{
  _proto->resolve1();
  if (!type()) {
    Type *t = _proto->base_type();
    if (!t)
      t = void_type;
    set_type(t);
  }
}


/*****
 * feature
 **/

Feature *
Node::feature(PermString n) const
{
  return type()->feature(n);
}

Feature *
NamespaceNode::feature(PermString n) const
{
  return _namesp->find(n); 
}

Feature *
FieldNode::feature(PermString n) const
{
  return _field->type()->feature(n);
}


/*****
 * simple_type
 * lvalue
 **/

bool
FieldNode::simple_type() const
{
  return (!_object || _object->simple_type()) && _field->is_type();
}


bool
FieldNode::fixed_run_time_type() const
{
  return !type()->cast_pointer();
}

bool
VariableNode::fixed_run_time_type() const
{
  return !type()->cast_pointer();
}

bool
UnaryNode::fixed_run_time_type() const
{
  return _child->fixed_run_time_type();
}


bool
FieldNode::lvalue() const
{
  return _field->lvalue();
}

bool
UnaryNode::lvalue() const
{
  return _op == opDeref;
}

bool
SelfNode::lvalue() const
{
  return true;
}


bool
FieldNode::interpolatable() const
{
  return _field->is_slot();
}

bool
UnaryNode::interpolatable() const
{
  return (_op == opDeref || _op == opAddress) && _child->interpolatable();
}

bool
CastNode::interpolatable() const
{
  return _child->interpolatable();
}

bool
LiteralNode::interpolatable() const
{
  return true;
}


Node *
ParamNode::state_wrap()
{
  _assigned = true;
  return this;
}


/*****
 * make_deref, make_address
 **/

Node *
Node::make_deref() const
{
  return type()->make_deref((Node *)this);
}

Node *
UnaryNode::make_deref() const
{
  if (_op == '&')
    return _child;
  else
    return Node::make_deref();
}

Node *
Node::make_address() const
{
  return type()->make_address((Node *)this, landmark());
}

Node *
UnaryNode::make_address() const
{
  if (_op == opDeref)
    return _child;
  else
    return Node::make_address();
}


/*****
 * write
 **/

static WriterManip wmi = wmpushindent;
static WriterManip wmni = wmpopindent;

void
Node::write(Writer &w) const
{
  w << "(\?\?\?)";
}

void
PrototypeNode::write(Writer &w) const
{
  w << "(proto " << _proto->basename() << ")";
}

void
CallNode::write(Writer &w) const
{
    bool is_constructor = ((Node *)this)->cast_constructor();
    w << (is_constructor ? "(constructor " : "(call ") << wmi;
  
    if (_call_of)
	w << _call_of << ':' << _call_of->type();
    else
	w << "*";

    w << wmnodesep;
  
    if (_rule)
	w << _rule; //->basename();
    else
	w << "*";
  
    if (_tail_recursion)
	w << "!TR";
    if (_fixed_rule)
	w << "!F";
  
    for (int i = 0; i < _params.size(); i++)
	w << wmnodesep << _params[i];
  
    w << wmni << ")";
}

void
LetNode::write(Writer &w) const
{
    w << "(le" << wmi << "t [" << wmi;
  
    for (int i = 0; i < _items.size(); i++) {
	if (i)
	    w << wmnodesep;
	w << _items[i];
    }
  
    w << "]\n" << wmni << _body << ")" << wmni;
}

void
FieldNode::write(Writer &w) const
{
  w << "(. ";
  
  if (_object)
    w << _object;
  else
    w << "*";
  
  w << " " << _field->basename() << ")";
}

void
VariableNode::write(Writer &w) const
{
    w << "(v " << temporary() << ':' << type() << ")";
}

void
DeclarationNode::write(Writer &w) const
{
  w << "(decl " << _variable << " [" << temporary() << "]";
  if (_value)
    w << wmi << wmnodesep << _value << wmni;
  w << ")";
}

void
ParamNode::write(Writer &w) const
{
  w << (_assigned ? "(param!" : "(param ") << _number << " " << name() << ")";
}

void
BinaryNode::write(Writer &w) const
{
  w << "(bin " << wmi << _op << " " << _left
    << wmnodesep << _right << wmni << ")";
}

void
SemistrictNode::write(Writer &w) const
{
  w << "(ss " << wmi << _op << wmnodesep << _left
    << wmnodesep // << "[" << _op << "] "
    << _right << wmni << ")";
}


void
ConditionalNode::write(Writer &w) const
{
  w << "(cond " << wmi << _test << wmnodesep
    << "? " << _yes
    << ": " << _no << wmni << ")";
}


void
UnaryNode::write(Writer &w) const
{
  w << "(una " << wmi << _op << " " << _child << wmni << ")";
}

void
CastNode::write(Writer &w) const
{
  w << "(->" << wmi << type() << " " << _child << wmni << ")";
}

void
TestNode::write(Writer &w) const
{
  w << "(test " << wmi << _test << wmni << ")";
}

void
ExceptionNode::write(Writer &w) const
{
  if (_exception) w << "(" << _exception << ")";
  else w << "(rethrow exception)";
}

void
CatchNode::write(Writer &w) const
{
    w << "(catch " << wmi;
    if (_catch_all)
	w << "all";
    else
	w << _eset;
    w << wmnodesep << _child << wmni << ")";
}

void
LiteralNode::write(Writer &w) const
{
  w << "(lit " << _literal << ")";
}

void
CodeNode::write(Writer &w) const
{
  w << "{code}";
}

void
VtblNode::write(Writer &w) const
{
    w << "(vtbl ";
    if (_object)
	w << wmi << _object << wmni << ")";
    else
	w << "*)";
}

void
SelfNode::write(Writer &w) const
{
  w << (_super ? "super" : "self");
}

void
LabelNode::write(Writer &w) const
{
  w << "(label " << _child << ")";
}


void
FieldNode::write_short(Writer &w) const
{
  w << "[field " << _field->basename() << "]";
}

void
BinaryNode::write_short(Writer &w) const
{
  w << "[bin " << _op << "]";
}

void
SemistrictNode::write_short(Writer &w) const
{
  w << "[ss " << _op << "]";
}

void
ConditionalNode::write_short(Writer &w) const
{
  w << "[?:]";
}

void
UnaryNode::write_short(Writer &w) const
{
  w << "[una " << _op << "]";
}

void
CastNode::write_short(Writer &w) const
{
  w << "[->" << type() << "]";
}

void
TestNode::write_short(Writer &w) const
{
  w << "[test]";
}

void
CatchNode::write_short(Writer &w) const
{
  w << "[catch ";
  if (_catch_all) w << "all";
  else w << _eset;
  w << "]";
}

void
LabelNode::write_short(Writer &w) const
{
  w << "[label]";
}


/*****
 * clone
 **/

Node *
Node::clone(const Landmark &) const
{
  assert(0 && "compiler bug: can't clone that node");
  return 0;
}

PrototypeNode *
PrototypeNode::clone(const Landmark &lm) const
{
  assert(0 && "compiler bug: cloning PrototypeNode");
  return 0;
}

VariableNode *
VariableNode::clone(const Landmark &lm) const
{
  return new VariableNode(name(), type(), lm);
}

ParamNode *
ParamNode::clone(const Landmark &lm) const
{
  return new ParamNode(name(), _number, type(), lm);
}

LiteralNode *
LiteralNode::clone(const Landmark &lm) const
{
  return new LiteralNode(Literal(_literal, lm));
}

SelfNode *
SelfNode::clone(const Landmark &lm) const
{
  return new SelfNode(type(), _super, lm);
}


/*****
 * optimize
 **/

Node *
Node::optimize(NodeOptimizer *) const
{
  return (Node *)this;
}


Node *
NamespaceNode::optimize(NodeOptimizer *opt) const
{
  const NamespaceNode *result = this;
  Node *newob = _object ? _object->optimize(opt) : opt->do_self();
  if (newob != _object)
    result = new NamespaceNode(newob, *this);
  return (Node *)result;
}


Node *
FieldNode::optimize(NodeOptimizer *opt) const
{
  const FieldNode *result = this;
  Node *newob = _object ? _object->optimize(opt) : opt->do_self();
  if (newob != _object)
    result = new FieldNode(newob, *this);
  return opt->do_field(result);
}


Node *
CallNode::optimize(NodeOptimizer *opt) const
{
  Node *new_call_of = _call_of ? _call_of->optimize(opt) : opt->do_self();
  bool need_new = new_call_of != _call_of;
  
  Vector<Node *> new_params;
  for (int i = 0; i < _params.size(); i++) {
    Node *p = _params[i]->optimize(opt);
    new_params.push_back(p);
    if (p != _params[i])
      need_new = true;
  }
  
  const CallNode *result;
  if (need_new)
    result = new CallNode(new_call_of, new_params, *this);
  else
    result = this;
  
  return opt->do_call(result);
}

Node *
ConstructorNode::optimize(NodeOptimizer *opt) const
{
  Node *new_call_of = _call_of ? _call_of->optimize(opt) : opt->do_self();
  bool need_new = new_call_of != _call_of;
  
  Vector<Node *> new_params;
  for (int i = 0; i < _params.size(); i++) {
    Node *p = _params[i]->optimize(opt);
    new_params.push_back(p);
    if (p != _params[i])
      need_new = true;
  }
  
  const ConstructorNode *result;
  if (need_new)
    result = new ConstructorNode(new_call_of, new_params, *this);
  else
    result = this;
  
  return opt->do_constructor(result);
}


Node *
LetNode::optimize(NodeOptimizer *opt) const
{
  LetNode *result = new LetNode(landmark());
  result->_items = _items;
  bool needed = false;
  
  for (int i = 0; i < _items.size(); i++) {
    Node *newi = _items[i]->optimize(opt);
    if (newi != _items[i]) needed = true;
    result->_items[i] = newi;
  }
  
  Node *body = _body->optimize(opt);
  if (body != _body) needed = true;
  result->set_body(body);
  
  if (needed)
    return opt->do_let(result);
  else {
    delete result;
    return opt->do_let(this);
  }
}


Node *
DeclarationNode::optimize(NodeOptimizer *opt) const
{
  const DeclarationNode *result = this;
  Node *variable = _variable->optimize(opt);
  assert(variable == _variable);
  Node *newval = _value ? _value->optimize(opt) : 0;
  if (newval != _value)
    result = new DeclarationNode(_variable, newval, *this);
  return (Node *)result;
}


Node *
ParamNode::optimize(NodeOptimizer *opt) const
{
  return opt->do_param(this);
}


Node *
BinaryNode::optimize(NodeOptimizer *opt) const
{
  const BinaryNode *result = this;
  Node *newl = _left->optimize(opt);
  Node *newr = _right->optimize(opt);
  
  if (newl != _left || newr != _right)
    result = new BinaryNode(newl, newr, *this);
  
  return opt->do_binary(result);
}


Node *
SemistrictNode::optimize(NodeOptimizer *opt) const
{
  const SemistrictNode *result = this;
  Node *newl = _left->optimize(opt);
  Node *newr = _right->optimize(opt);
  
  if (newl != _left || newr != _right)
    result = new SemistrictNode(newl, newr, *this);
  
  return opt->do_semistrict(result);
}

Node *
EffectNode::optimize(NodeOptimizer *opt) const
{
  const EffectNode *result = this;
  Node *newl = _left->optimize(opt);
  Node *newr = _right->optimize(opt);
  
  if (newl != _left || newr != _right)
    result = new EffectNode(newl, newr, *this);
  
  return opt->do_effect(result);
}

Node *
UnaryNode::optimize(NodeOptimizer *opt) const
{
  const UnaryNode *result = this;
  Node *newl = _child->optimize(opt);
  if (newl != _child)
    result = new UnaryNode(newl, *this);
  return opt->do_unary(result);
}

Node *
TestNode::optimize(NodeOptimizer *opt) const
{
  const TestNode *result = this;
  Node *newc = _test->optimize(opt);
  if (newc != _test)
    result = new TestNode(newc);
  return (Node *)result;
}

Node *
CastNode::optimize(NodeOptimizer *opt) const
{
  const CastNode *result = this;
  Node *new_child = _child->optimize(opt);
  if (new_child != _child)
    result = new CastNode(new_child, *this);
  return opt->do_cast(result);
}

Node *
ConditionalNode::optimize(NodeOptimizer *opt) const
{
  const ConditionalNode *result = this;
  Node *new_test = _test->optimize(opt);
  Node *new_yes = _yes->optimize(opt);
  Node *new_no = _no->optimize(opt);
  
  if (new_test != _test || new_yes != _yes || new_no != _no)
    result = new ConditionalNode(new_test, new_yes, new_no, *this);
  
  return opt->do_conditional(result);
}

Node *
ExceptionNode::optimize(NodeOptimizer *opt) const
{
  return opt->do_exception(this);
}

Node *
CatchNode::optimize(NodeOptimizer *opt) const
{
  const CatchNode *result = this;
  Node *newc = _child->optimize(opt);
  if (newc != _child)
    result = new CatchNode(newc, *this);
  return opt->do_catch(result);
}

Node *
CodeNode::optimize(NodeOptimizer *opt) const
{
  if (!_have_params) return opt->do_code(this);

  // otherwise, go through all objects and stuff and optimize them
  Node *new_self = _self ? _self->optimize(opt) : opt->do_self();
  bool need_new = new_self != _self;
  
  Vector<Node *> new_params;
  for (int i = 0; i < _params.size(); i++) {
    Node *p = _params[i]->optimize(opt);
    new_params.push_back(p);
    if (p != _params[i])
      need_new = true;
  }
  
  const CodeNode *result;
  if (need_new)
    result = new CodeNode(new_self, new_params, *this);
  else
    result = this;
  
  return opt->do_code(result);
}

Node *
VtblNode::optimize(NodeOptimizer *opt) const
{
  const VtblNode *result = this;
  Node *newobj = _object ? _object->optimize(opt) : opt->do_self();
  if (newobj != _object)
    result = new VtblNode(_vtbl_from, newobj, landmark());
  return (Node *)result;
}

Node *
SelfNode::optimize(NodeOptimizer *opt) const
{
  return opt->do_self(this);
}

Node *
LabelNode::optimize(NodeOptimizer *opt) const
{
  const LabelNode *result = this;
  Node *newc = _child->optimize(opt);
  if (newc != _child)
    result = new LabelNode(newc);
  return opt->do_label(result);
}


/*****
 * compile
 **/

static Target *tail_recursion_label;


Target *
Node::compile(Compiler *, bool, Target *)
{
    assert(0 && "don't know how to compile that");
    return 0;
}


Target *
FieldNode::compile(Compiler *c, bool value_used, Target *ts)
{
    Target *last = new Target(this, value_used, ts);
    if (_object)
	last = _object->compile(c, true, last);
    return last;
}

Target *
CallNode::compile(Compiler *c, bool value_used, Target *ts)
{
    Target *last;
    if (_tail_recursion)
	last = tail_recursion_label;
    else
	last = new Target(this, value_used, ts);
    for (int i = _params.size() - 1; i >= 0; i--)
	last = _params[i]->compile(c, true, last);
    if (_call_of)
	last = _call_of->compile(c, true, last);
    return last;
}

Target *
ExceptionNode::compile(Compiler *c, bool, Target *)
{
    if (_exception)
	return new Target(this, true, c->exception_handler());
    else
	return c->exception_handler();
}

Target *
LetNode::compile(Compiler *c, bool value_used, Target *ts)
{
    Target *last = _body->compile(c, value_used, ts);
    for (int i = _items.size() - 1; i >= 0; i--)
	last = _items[i]->compile(c, false, last);
    return last;
}

Target *
DeclarationNode::compile(Compiler *c, bool value_used, Target *ts)
{
    assert(!value_used);
    // Need to compile the variable in too to ensure that the compiler will
    // notice its temporary. It won't actually generate any code, since it's
    // a simple value and its value isn't used.
    Target *variable = _variable->compile(c, false, ts);
    Target *last = new Target(this, false, variable);
    if (_value)
	last = _value->compile(c, true, last);
    return last;
}

Target *
VariableNode::compile(Compiler *, bool value_used, Target *ts)
{
    return new Target(this, value_used, ts);
}

Target *
BinaryNode::compile(Compiler *c, bool value_used, Target *ts)
{
    Target *result = new Target(this, value_used, ts);
    Target *second = _right->compile(c, true, result);
    Target *first = _left->compile(c, true, second);
    return first;
}

Target *
Node::compile_test(Node *&n, Compiler *c, Target *tyes, Target *tno)
{
    TestNode *test = n->cast_test();
    if (!test) {
	test = new TestNode(n);
	n = test;
    }
    return test->compile_test(c, tyes, tno);
}

Target *
SemistrictNode::compile(Compiler *c, bool value_used, Target *ts)
{
    Target *first;
    Target *second;
    switch ((int)_op) {
    
      case ',':
	second = _right->compile(c, value_used, ts);
	first = _left->compile(c, false, second); // YES/NO
	break;
    
      case opLogAnd:
      case opLogOr: {
	  Target *tyes, *tno;
	  if (ts->gen_code() == gcTest) {
	      tyes = ts->succeed();
	      tno = ts->fail();
	  } else {
	      tyes = new Target(this, value_used, ts, 0, gcYes);
	      tno = new Target(this, value_used, ts, 0, gcNo);
	      Target *tright = _op == opLogAnd ? tyes : tno;
	      tright->set_betweenliner(_right->betweenliner());
	  }
     
	  second = compile_test(_right, c, tyes, tno);
	  
	  Target *go_yes = _op == opLogAnd ? second : tyes;
	  Target *go_no = _op == opLogAnd ? tno : second;
	  first = compile_test(_left, c, go_yes, go_no);
     
	  break;
      }
   
      case opArrow: {
	  Target *tyes, *tno;
	  if (ts->gen_code() == gcTest) {
	      tyes = ts->succeed();
	      tno = ts->fail();
	  } else {
	      tyes = new Target(this, value_used, ts, 0, gcYes);
	      tno = new Target(this, value_used, ts, 0, gcNo);
	      Target *tright = _op == opLogAnd ? tyes : tno;
	      tright->set_betweenliner(_right->betweenliner());
	  }
     
	  second = _right->compile(c, false, tyes);
	  first = compile_test(_left, c, second, tno);
	  
	  break;
      }
   
      default:
	assert(0);
	return 0;
    
    }
  
    return first;
}

Target *
ConditionalNode::compile(Compiler *c, bool value_used, Target *ts)
{
    // A small bug here resulted in very incorrect code. Need to call
    // _yes->compile() and _no->compile() with value_used instead of always
    // assuming true.
    Target *yes_result = new Target(this, value_used, ts, 0, gcYes);
    Target *yes = _yes->compile(c, value_used, yes_result);
  
    Target *no_result = new Target(this, value_used, ts, 0, gcNo);
    Target *no = _no->compile(c, value_used, no_result);
  
    Target *result = compile_test(_test, c, yes, no);
    return result;
}

Target *
UnaryNode::compile(Compiler *c, bool value_used, Target *ts)
{
    Target *result = new Target(this, value_used, ts);
    assert(_child->type());
    Target *first = _child->compile(c, _child->type() != void_type, result);
    return first;
}

Target *
CastNode::compile(Compiler *c, bool value_used, Target *ts)
{
    Target *first;
    /* YES/NO REMOVAL happened */
  
    if (type() == bool_type) // FIXME
	first = _child->compile(c, value_used, ts);
    else {
	bool to_void = type() != void_type;
	Target *result = new Target(this, value_used, ts);
	first = _child->compile(c, to_void, result);
    }
  
    return first;
}

Target *
TestNode::compile(Compiler *c, bool, Target *ts)
{
    assert(0);
    return compile_test(c, ts, 0);
}

Target *
TestNode::compile_test(Compiler *c, Target *tyes, Target *tno)
{
    Target *test = new Target(this, false, tyes, tno, gcTest);
    Target *first = _test->compile(c, true, test);
    return first;
}

Target *
CatchNode::compile(Compiler *c, bool value_used, Target *ts)
{
    Target *tcaught, *tuncaught;
    if (ts->gen_code() == gcTest) {
	tcaught = ts->succeed();
	tuncaught = ts->fail();
    } else if (!value_used) {
	tcaught = tuncaught = ts;
    } else {
	tcaught = new Target(this, value_used, ts, 0, gcYes);
	tuncaught = new Target(this, value_used, ts, 0, gcNo);
    }
  
    // Make handler target
    Target *old_handler = c->exception_handler();
    Target *handler = old_handler;
    if (_catch_all)
	handler = tcaught;
    else {
	VariableNode *exceptid = exceptid_node();
	for (Exception *e = _eset.element(0); e; e = _eset.element(e)) {
	    Node *comparison = new BinaryNode
		(exceptid, opEq,
		 new LiteralNode(int_type, e->exception_id(), *this),
		 bool_type, *this);
	    handler = compile_test(comparison, c, tcaught, handler);
	}
    }
  
    c->set_exception_handler(handler);
    Target *first = _child->compile(c, false, tuncaught);
    c->set_exception_handler(old_handler);
    
    return first;
}

Target *
LiteralNode::compile(Compiler *, bool value_used, Target *ts)
{
    if (ts->gen_code() == gcTest) {
	if (vlong())
	    return ts->succeed();
	else
	    return ts->fail();
    } else
	return new Target(this, value_used, ts);
}


Target *
CodeNode::compile(Compiler *, bool value_used, Target *ts)
{
    return new Target(this, value_used, ts);
}


Target *
VtblNode::compile(Compiler *c, bool value_used, Target *ts)
{
    assert(!value_used);
    Target *last = new Target(this, false, ts);
    if (_object)
	return _object->compile(c, true, last);
    else
	return last;
}


Target *
SelfNode::compile(Compiler *, bool value_used, Target *ts)
{
    return new Target(this, value_used, ts);
}


Target *
LabelNode::compile(Compiler *c, bool value_used, Target *ts)
{
    Target *last_tail_recursion_label = tail_recursion_label;
    Target *ours = new Target(this, value_used, 0);
    tail_recursion_label = ours;
  
    Target *ret = new Target(this, value_used, ts);
    Target *first = _child->compile(c, value_used, ret);
    ours->make_alias(first);
    first->set_label_name("LOOP");
  
    tail_recursion_label = last_tail_recursion_label;
    return ours;
}


/*****
 * traverse
 **/

void
Node::traverse(TraverseHook, void *)
{
}

void
FieldNode::traverse(TraverseHook f, void *v)
{
  if (_object)
    (_object->*f)(v);
}

void
CallNode::traverse(TraverseHook f, void *v)
{
  if (_call_of)
    (_call_of->*f)(v);
  for (int i = 0; i < _params.size(); i++)
    (_params[i]->*f)(v);
}

void
LetNode::traverse(TraverseHook f, void *v)
{
  for (int i = 0; i < _items.size(); i++)
    (_items[i]->*f)(v);
  (_body->*f)(v);
}

void
DeclarationNode::traverse(TraverseHook f, void *v)
{
  (_variable->*f)(v);
  if (_value)
    (_value->*f)(v);
}

void
BinaryNode::traverse(TraverseHook f, void *v)
{
  (_left->*f)(v);
  (_right->*f)(v);
}

void
UnaryNode::traverse(TraverseHook f, void *v)
{
  (_child->*f)(v);
}

void
CastNode::traverse(TraverseHook f, void *v)
{
  (_child->*f)(v);
}

void
TestNode::traverse(TraverseHook f, void *v)
{
  (_test->*f)(v);
}

void
ConditionalNode::traverse(TraverseHook f, void *v)
{
  (_test->*f)(v);
  (_yes->*f)(v);
  (_no->*f)(v);
}

void
CatchNode::traverse(TraverseHook f, void *v)
{
  (_child->*f)(v);
}

void
VtblNode::traverse(TraverseHook f, void *v)
{
  if (_object)
    (_object->*f)(v);
}

void
LabelNode::traverse(TraverseHook f, void *v)
{
  (_child->*f)(v);
}


/*****
 * mark_usage
 **/

void
Node::decr_usage(void *)
{
  /* Check for case when this Node has actually been optimized away and doesn't
     appear as a Fork. If so, it hasn't had its usage incremented -- so don't
     decrement it; rather, decrement its children with mark_usage. */
  if (_usage > 0)
    _usage--;
  else
    mark_usage();
}


/*****
 * fix_outline
 **/

struct NodeOutlineFixer {
  
  bool _novel_ok;
  Betweenliner _new_outline;

  NodeOutlineFixer(bool ok, Betweenliner b) : _novel_ok(ok), _new_outline(b){}
  int outline_epoch() const	{ return _new_outline.outline_epoch(); }
  
};

void
Node::fix_outline()
{
  NodeOutlineFixer nof(false, Betweenliner());
  fix_outline(&nof);
}

void
Node::fix_outline(void *v)
{
  NodeOutlineFixer *nof = (NodeOutlineFixer *)v;
  
  if (outline_epoch() != nof->outline_epoch() && !nof->_novel_ok)
    _betweenliner.change_outline(nof->_new_outline);
  
  NodeOutlineFixer newnof(false, _betweenliner);
  traverse(&Node::fix_outline, &newnof);
}

void
SemistrictNode::fix_outline(void *v)
{
  NodeOutlineFixer *nof = (NodeOutlineFixer *)v;
  
  if (outline_epoch() != nof->outline_epoch() && !nof->_novel_ok)
    set_betweenliner( betweenliner().change_outline(nof->_new_outline) );
  
  NodeOutlineFixer left_nof(false, betweenliner());
  _left->fix_outline(&left_nof);
  
  NodeOutlineFixer right_nof(true, betweenliner());
  _right->fix_outline(&right_nof);
}

void
ConditionalNode::fix_outline(void *v)
{
  NodeOutlineFixer *nof = (NodeOutlineFixer *)v;
  
  if (outline_epoch() != nof->outline_epoch() && !nof->_novel_ok)
    set_betweenliner( betweenliner().change_outline(nof->_new_outline) );
  
  NodeOutlineFixer test_nof(false, betweenliner());
  _test->fix_outline(&test_nof);
  
  NodeOutlineFixer branch_nof(true, betweenliner());
  _yes->fix_outline(&branch_nof);
  _no->fix_outline(&branch_nof);
}


/*****
 * receiver_class_analysis
 **/

void
Node::receiver_class_analysis(bool in_constructor)
{
  receiver_class_analysis(in_constructor ? this : 0);
}

void
Node::receiver_class_analysis(void *in_constructor)
{
  traverse(&Node::receiver_class_analysis, in_constructor);
}

static void
receiver_class_analysis_children(Rule *r)
{
  Ruleset *base_ruleset = r->actual()->find_ruleset(r->origin());
  int ruleindex = r->ruleindex();
  for (Ruleset *rs = base_ruleset->child(); rs; rs = rs->sibling()) {
    r = rs->rule(ruleindex);
    if (!r->receiver_classed()) {
      r->receiver_class_analysis();
      receiver_class_analysis_children(r);
    }
  }
}

void
CallNode::receiver_class_analysis(void *in_constructor)
{
    if (_rule->constructor()
	&& (!in_constructor || (_call_of && !_call_of->cast_self()))) {
	int i = 0;
	ModuleID ancestor;
	Module *actual = _rule->actual();
	while (actual->each_ancestor(i, ancestor))
	    ancestor->mark_receiver_class(actual);
    }
  
    if (!_rule->receiver_classed()) {
	_rule->receiver_class_analysis();
	if (!_fixed_rule)
	    receiver_class_analysis_children(_rule);
    }
  
    traverse(&Node::receiver_class_analysis, in_constructor);
}


/*****
 * callable_analysis
 **/

void
Node::callable_analysis(void *)
{
  traverse(&Node::callable_analysis, 0);
}

static void
callable_analysis_children(Rule *r)
{
    Ruleset *base_ruleset = r->actual()->find_ruleset(r->origin());
    int ruleindex = r->ruleindex();
    for (Ruleset *rs = base_ruleset->child(); rs; rs = rs->sibling()) {
	r = rs->rule(ruleindex);
	if (!r->callable()) {
	    r->callable_analysis();
	    callable_analysis_children(r);
	}
    }
}

void
CallNode::callable_analysis(void *)
{
    Rule *r = (_fixed_rule ? _rule : _rule->version_in(_rule->receiver_class()));
    if (!r->callable())
	r->callable_analysis();
    if (!r->leaf() && !_fixed_rule && !r->dyn_dispatch()) {
	r->mark_dynamic_dispatch();
	callable_analysis_children(r);
    }
    traverse(&Node::callable_analysis, 0);
}


/*****
 * gen_prototypes
 **/

void
Node::gen_prototypes(Compiler *c)
{
    gen_prototypes((void *)c);
}

void
Node::gen_prototypes(void *v)
{
    traverse(&Node::gen_prototypes, v);
}

void
CallNode::gen_prototypes(void *v)
{
    Compiler *c = (Compiler *)v;
    Rule *rule = (_fixed_rule ? _rule : _rule->version_in(_rule->receiver_class()));
    if (_fixed_rule || rule->leaf()) {
	rule->mark_gen();
	rule->gen_prototype(c->proto_out, true);
    }
    traverse(&Node::gen_prototypes, v);
}


/*****
 * count_operations
 **/

int
Node::count_operations()
{
  int val = 0;
  count_operations(&val);
  return val;
}

void
Node::count_operations(void *v)
{
  int *ip = (int *)v;
  (*ip)++;
  traverse(&Node::count_operations, v);
}

void
VariableNode::count_operations(void *)
{
  // no work done at runtime
}

void
CastNode::count_operations(void *v)
{
  // no work done at runtime
  traverse(&Node::count_operations, v);
}

void
SemistrictNode::count_operations(void *v)
{
  if (op() != ',') {		// no work done at runtime for ,
    int *ip = (int *)v;
    (*ip)++;
  }
  traverse(&Node::count_operations, v);
}

void
SelfNode::count_operations(void *)
{
  // no work done at runtime
}


/*****
 * mark_tail_recursions
 **/

static int tail_recursion_count;

int
Node::mark_tail_recursions(Rule *rule)
{
  tail_recursion_count = 0;
  mark_tail_recursions((void *)rule);
  return tail_recursion_count;
}

void
Node::mark_tail_recursions(void *)
{
}

void
CallNode::mark_tail_recursions(void *v)
{
  Rule *rule = (Rule *)v;
  if (rule == _rule && !_call_of) {
    set_tail_recursion(true);
    tail_recursion_count++;
  }
}

void
LetNode::mark_tail_recursions(void *v)
{
  _body->mark_tail_recursions(v);
}

void
SemistrictNode::mark_tail_recursions(void *v)
{
  if (op() == ',' || op() == opLogOr || op() == opLogAnd)
    _right->mark_tail_recursions(v);
}

void
ConditionalNode::mark_tail_recursions(void *v)
{
  _yes->mark_tail_recursions(v);
  _no->mark_tail_recursions(v);
}

void
LabelNode::mark_tail_recursions(void *v)
{
  _child->mark_tail_recursions(v);
}


/*****
 * mark_param_usage
 **/

void
Node::mark_param_usage(Vector<int> &usage)
{
  mark_param_usage((void *)&usage);
}

void
Node::mark_param_usage(void *v)
{
  traverse(&Node::mark_param_usage, v);
}

void
ParamNode::mark_param_usage(void *v)
{
  Vector<int> *usage = (Vector<int> *)v;
  (*usage)[_number]++;
}


/*****
 * call_object
 **/

Node *
Node::call_object() const
{
  return (Node *)this;
}

Node *
NamespaceNode::call_object() const
{
  return _object ? _object->call_object() : 0;
}

Node *
FieldNode::call_object() const
{
  if (_field->is_ancestor())
    return _object ? _object->call_object() : 0;
  else
    return (Node *)this;
}

/*****
 * simple_value
 **/

Node *
Node::simple_value() const
{
  return 0;
}

Node *
FieldNode::simple_value() const
{
  if (!_object || _object->simple_value() == _object)
    return (Node *)this;
  else
    return 0;
}

Node *
LetNode::simple_value() const
{
  return _body->simple_value();
}

Node *
VariableNode::simple_value() const
{
  return (Node *)this;
}

Node *
PrototypeNode::simple_value() const
{
  return (Node *)this;
}

Node *
NamespaceNode::simple_value() const
{
  if (!_object || _object->simple_value() == _object)
    return (Node *)this;
  else
    return 0;
}

Node *
DeclarationNode::simple_value() const
{
  return _variable->simple_value();
}

Node *
ConstructorNode::simple_value() const
{
  if (call_of())
    return call_of()->simple_value();
  else
    return 0;			// XXX
}

Node *
SemistrictNode::simple_value() const
{
  if (_op == ',')
    return _right->simple_value();
  else
    return 0;
}

Node *
EffectNode::simple_value() const
{
  return _left->simple_value();
}

Node *
UnaryNode::simple_value() const
{
  switch ((int)_op) {
    
   case opDeref:
    if (_child->simple_value() == _child)
      return (Node *)this;
    else
      return 0;

   case opPreIncr:
   case opPreDecr:
    return _child->simple_value();
    
   default:
    return 0;
    
  }
}

Node *
CastNode::simple_value() const
{
  if (_child->simple_value() == _child)
    return (Node *)this;
  else
    return 0;
}

Node *
LiteralNode::simple_value() const
{
  assert(type() != code_type);
  return (Node *)this;
}

Node *
CodeNode::simple_value() const
{
  return (Node *)this;
}

Node *
SelfNode::simple_value() const
{
  return (Node *)this;
}


/*****
 * GEN
 **/

/*****
 * gen_declare_temp, gen_value_temp
 **/

void
Node::gen_declare_temp(Compiler *c)
{
  assert(_temporary);
  type()->gen_object(c->out, _temporary);
  c->out << "; /* ";
  write_short(c->out);
  c->out << " */\n";
}

void
ParamNode::gen_declare_temp(Compiler *)
{
  /* do nothing, since the parameter was already defined in the function
     prototype */
}


GenContext
Node::gen_value_temp(Compiler *c)
{
  assert(_temporary);
  c->out << _temporary;
  return gctxNone;
}


/*****
 * gen_state_temp
 **/

void
Node::gen_state_temp(Compiler *c)
{
  if (must_gen_state_real()) {
    gen_state_real(c);
#ifdef CHECK_GEN
    _n_gen_state++;
#endif
  }
  c->out << _temporary << " = (";
  gen_value_real(c, gctxNone);
  c->out << ");\n";
#ifdef CHECK_GEN
  _n_gen_value++;
#endif
}

void
VariableNode::gen_state_temp(Compiler *)
{
  /* do nothing */
}


/*****
 * global nodes
 **/

static SelfNode *self_node = new SelfNode(0, Landmark());
Type *SelfNode::current_self_type;


/*****
 * gen_state_real
 **/

void
Node::gen_state_real(Compiler *)
{
}

void
DeclarationNode::gen_state_real(Compiler *c)
{
    if (_value) {
	_variable->gen_value(c);
	c->out << " = (";
	_value->gen_value(c);
	c->out << "); /*decl*/\n";
    } else if (Module *m = _variable->type()->cast_module())
	m->gen_assign_vtbl(c, _variable);
}

void
EffectNode::gen_state_real(Compiler *c)
{
    c->out << "(";
    _left->gen_value(c);
    c->out << ") " << _op << " (";
    _right->gen_value(c);
    c->out << ");\n";
}

void
CodeNode::gen_state_real(Compiler *c)
{
    ParamReplacer p(_self, _params);
    _code->gen(c, &p);
}

void
VtblNode::gen_state_real(Compiler *c)
{
    _vtbl_from->gen_assign_vtbl(c, _object ? _object : self_node);
}


/*****
 * gen_value
 **/

GenContext
Node::gen_value_real(Compiler *, GenContext)
{
    return gctxNone;
}

GenContext
FieldNode::gen_value_real(Compiler *c, GenContext)
{
    _field->gen_value(c, _object);
    return gctxNone;
}

GenContext
VariableNode::gen_value_real(Compiler *c, GenContext)
{
    c->out << temporary();
    return gctxNone;
}

GenContext
LetNode::gen_value_real(Compiler *c, GenContext)
{
    _body->gen_value(c);
    return gctxNone;
}

GenContext
CallNode::gen_value_real(Compiler *c, GenContext)
{
    Node *call_of = _call_of ? _call_of : self_node;

    // Find actual rule using receiver class analysis information
    Rule *fixed = fixed_rule();
    if (fixed) {
	fixed->gen_name(c->out);
	fixed->mark_gen();	// we will need to output this rule as a fn
    } else {
	assert(call_of->simple_value());
	c->out << "((";
	call_of->gen_value(c);
	c->out << ")." << _rule->origin()->gen_self_vtbl_name() << "->"
	       << _rule->basename() << ")" << "/*DYN*/";
    }
  
    c->out << "(";
    if (!_rule->is_static()) {
	// Generate `This' argument, including cast, if necessary.
	Module *rule_self = _rule->receiver_class();
	Type *call_of_type = call_of->type();
	if (!call_of_type) call_of_type = SelfNode::current_self_type;
	if (rule_self != call_of_type->cast_module())
	    c->out << "(" << rule_self->gen_object_name() << "*)";
	
	c->out << "&(";
	call_of->gen_value(c);
	c->out << (_params.size() ? "), " : ")");
    }
    for (int i = 0; i < _params.size(); i++) {
	if (i) c->out << ", ";
	_params[i]->gen_value(c);
    }
    c->out << ")";
  
    return gctxNone;
}

GenContext
BinaryNode::gen_value_real(Compiler *c, GenContext)
{
    _op.gen_binary_value(c, _left, _right);
    return gctxNone;
}

GenContext
SemistrictNode::gen_value_real(Compiler *c, GenContext)
{
    switch ((int)_op) {
    
      case ',':
	_right->gen_value(c);
	break;
    
      case opLogAnd:
      case opLogOr:
      case opArrow: {
	  switch (gen_code()) {
       
	    case gcYes:
	      c->out << "1";
	      break;
       
	    case gcNo:
	      c->out << "0";
	      break;
       
	    default:
	      assert(0);
       
	  }
	  break;
      }
   
      default:
	assert(0);
    
    }
    return gctxNone;
}

GenContext
EffectNode::gen_value_real(Compiler *c, GenContext)
{
    _left->gen_value_real(c, gctxNone);
    return gctxNone;
}

GenContext
ConditionalNode::gen_value_real(Compiler *c, GenContext)
{
    switch (gen_code()) {
    
      case gcYes:
	_yes->gen_value(c);
	break;
    
      case gcNo:
	_no->gen_value(c);
	break;
    
      default:
	assert(0);
    
    }
    return gctxNone;
}

GenContext
UnaryNode::gen_value_real(Compiler *c, GenContext gctx)
{
    switch ((int)_op) {
    
      case opCopy:
	_child->gen_value(c);
	break;
    
      case opPostIncr:
      case opPostDecr:
	c->out << "(";
	_child->gen_value(c);
	c->out << ")" << _op;
	break;
    
      case opDeref:
	if (gctx == gctxMember) {
	    _child->gen_value(c);
	    return gctxMember;
	} else
	    goto normal_case;
    
      default:
      normal_case:
	c->out << _op << "(";
	_child->gen_value(c);
	c->out << ")";
	break;
    
    }
    return gctxNone;
}

GenContext
CastNode::gen_value_real(Compiler *c, GenContext gctx)
{
    if (type() == bool_type) {
	c->out << "(";
	_child->gen_value(c);
	c->out << ") != 0";
    
    } else if (type() == void_type) {
	// Don't bother printing anything out at all!
	c->out << "/* NOTHING */";
	if (!_child->cast_catch()) _child->gen_value(c);
    
    } else if (_child->type() == bool_type) {
	// C does the right thing.
	_child->gen_value(c);
	
    } else if (type()->cast_pointer() && gctx == gctxPointerCast)
	_child->gen_value(c, gctxPointerCast);
  
    else {
	c->out << "(";
	type()->gen(c->out);
	c->out << ")(";
	GenContext new_ctx = (type()->cast_pointer() ? gctxPointerCast : gctxNone);
	_child->gen_value(c, new_ctx);
	c->out << ")";
    }
    return gctxNone;
}

GenContext
TestNode::gen_value_real(Compiler *c, GenContext)
{
    _test->gen_value(c);
    return gctxNone;
}

GenContext
ExceptionNode::gen_value_real(Compiler *c, GenContext)
{
    assert(_exception);
    c->out << _exception->exception_id() << " /* " << _exception << " */";
    return gctxNone;
}

GenContext
CatchNode::gen_value_real(Compiler *c, GenContext)
{
    switch (gen_code()) {
    
      case gcYes:
	c->out << "1 /* exception */";
	break;
    
      case gcNo:
	c->out << "0 /* no exception */";
	break;
    
      default:
	assert(0);
    
    }
    return gctxNone;
}

GenContext
CodeNode::gen_value_real(Compiler *c, GenContext)
{
    c->out << "true";
    return gctxNone;
}

GenContext
LiteralNode::gen_value_real(Compiler *c, GenContext)
{
    _literal.gen(c->out);
    return gctxNone;
}

GenContext
LabelNode::gen_value_real(Compiler *c, GenContext)
{
    _child->gen_value(c);
    return gctxNone;
}

GenContext
SelfNode::gen_value_real(Compiler *c, GenContext gctx)
{
    bool need_cast = type() && type() != current_self_type
	&& gctx != gctxPointerCast;
    if (need_cast) {
	Module *current_self_module = current_self_type->cast_module();
	Module *module = type()->cast_module();
	if (gctx == gctxMember)
	    need_cast = !current_self_module->has_ancestor(module);
	else
	    need_cast = current_self_module != module;
    }
  
    if (!need_cast) {
	c->out << (gctx == gctxMember ? "This" : "*This");
    } else {
	c->out << (gctx == gctxMember ? "(/*!*/" : "*(");
	c->out << "/*KAST"; current_self_type->gen(c->out); c->out << "*/";
	type()->gen(c->out);
	c->out << "*)This";
    }
  
    return (gctx == gctxMember ? gctxMember : gctxNone);
}


/*****
 * gen_value_string
 **/

PermString
Node::gen_value_string() const
{
    PermString null;
    return _temporary ? _temporary : null;
}

PermString
SelfNode::gen_value_string() const
{
    return "(*This)";
}


/*****
 * check_gen
 **/

#ifdef CHECK_GEN

void
Node::clear_gen_counts(void *)
{
  _n_gen_state = _n_gen_value = _n_uses = 0;
  traverse(&Node::clear_gen_counts, 0);
}

void
Node::count_uses(void *)
{
  _n_uses++;  
  traverse(&Node::count_uses, 0);
}

void
Node::check_gen_counts(void *)
{
  if (must_gen_state_real()) {
    if (_n_gen_state != _n_uses)
      error(*this, "node %p %N: %d gen_states", this, this, _n_gen_state);
  }
  
  if (must_gen_value()) {
    if (_n_gen_value != _n_uses)
      error(*this, "node %p %N: %d gen_values", this, this, _n_gen_value);
  }
  
  traverse(&Node::check_gen_counts, 0);
}

#endif
