#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "optimize.hh"
#include "module.hh"
#include "rule.hh"
#include "ruleset.hh"
#include "error.hh"
#include "operator.hh"
#include "writer.hh"
#include "field.hh"

Node *
NodeOptimizer::to_static_constant(const Node *input, Module *me)
{
  Node *me_node;
  if (me)
    me_node = new SelfNode(me->base_type(), me->landmark());
  else
    me_node = 0;
  
  InlineOptimizer inliner(me_node, inlineYes, inlinePath);
  Node *n = input->optimize(&inliner);
  
  ConstOptimizer conster;
  n = n->optimize(&conster);
  return n;
}

bool
NodeOptimizer::to_integer_constant(const Node *input, Module *me, int &value)
{
  Node *n = to_static_constant(input, me);
  if (n)
    if (LiteralNode *lit = n->cast_literal()) {
      value = lit->vlong();
      return true;
    }
  return false;
}


/*****
 * InlineOptimizer
 **/

InlineOptimizer::InlineOptimizer(Node *self, int min_lev, int max_lev)
  : _self(self), _self_type(self ? self->type() : 0),
    _pass_self(0), _pass_self_type(0),
    _min_inline_level(min_lev), _max_inline_level(max_lev), _inlining(false)
{
}

InlineOptimizer::InlineOptimizer(Node *self, Type *self_type,
				 Type *pass_self_type,
				 int min_lev, int max_lev,
				 const Vector<Node *> &param)
  : _self(self), _self_type(self_type),
    _pass_self(0), _pass_self_type(pass_self_type),
    _min_inline_level(min_lev), _max_inline_level(max_lev), _inlining(true),
    _param(param)
{
}


Node *
InlineOptimizer::temporarize_self(Node *self, Node *&comma) const
{
  Node *value = self->simple_value();
  if (value == self)
    return value;
  
  if (!value) {
    if (!self->lvalue()) {
      value = new VariableNode("_SELF", self->type(), *self);
      self = new DeclarationNode(value, self, *self);
    } else {
      Node *self_ptr = self->make_address();
      VariableNode *var = new VariableNode("_THIS", self_ptr->type(), *self);
      self = new DeclarationNode(var, self_ptr, *self);
      value = var->make_deref();
    }
  }
  
  if (comma)
    comma = new SemistrictNode(comma, ',', self, self->type(), *comma);
  else
    comma = self;
  
  return value;
}

Node *
InlineOptimizer::temporarize(Node *param, ParamNode *formal, Node *&comma)
  const
{
  Node *value = param->simple_value();
  if (value == param && !formal->assigned())
    return value;
  
  if (!value || formal->assigned()) {
    value = new VariableNode(formal->name(), param->type(), param->landmark());
    param = new DeclarationNode(value, param, param->landmark());
  }
  
  if (comma)
    comma = new SemistrictNode(comma, ',', param, param->type(), *comma);
  else
    comma = param;
  
  return value;
}

Node *
InlineOptimizer::change_object_type(Node *ob, Type *to_type) const
{
  if (ob->type()->same_type(to_type))
    return ob;
  if (SelfNode *self = ob->cast_self())
    return new SelfNode(to_type, self->super(), *self);
  return ob;
}

Node *
InlineOptimizer::do_call(const CallNode *old_call)
{
  Node *ob = old_call->call_of();
  if (!ob) ob = _self;
  if (!ob) return 0;
  
  // Use the self type we've figured out if the object is `self'. This self
  // type may be more derived than the object's type.
  Type *ob_type = ob->cast_self() ? _self_type : ob->type();
  ModuleNames *mn = ob_type->cast_modnames();
  if (!mn)
    return (Node *)old_call;
  
  Rule *rule;
  if (!(rule = old_call->fixed_rule()))
    rule = mn->find_rule(old_call->origin(), old_call->ruleindex());
  assert(rule);
  
  int module_level = mn->inline_level(rule);
  int call_level = old_call->inline_level();
  
  // Choose the inline level. Call, if specified; otherwise module, if
  // specified; otherwise minimum level. Take _min_inline_level and
  // _max_inline_level into account.
  int level = (call_level < 0 ? module_level : call_level);
  if (_min_inline_level == inlinePath
      || (_min_inline_level == inlineYes && level == inlineDefault))
    level = _min_inline_level;
  if (level > _max_inline_level)
    level = _max_inline_level;
  //if (_min_inline_level < inlineYes && rule->basename() == "fin_wait_1")
  //errwriter << rule << " " << level << " (" << call_level << "/" << module_level << "/" << (Type *)mn << " " << (void*)mn << " " << (void *)(mn->module()->base_modnames()) << ")" << " [" << _min_inline_level << "/" << _max_inline_level << "]" << wmendl;
  
  Node *body = rule->body();
  if (body && level >= inlineYes && !rule->inlining()
      && (rule->leaf() || old_call->fixed_rule())) {
    // Generate new variables to ensure that parameter expressions aren't
    // fully generated more than once.
    Node *call = 0;
    
    Type *pass_self_type = rule->actual()->base_modnames();
    if (!rule->is_static()) {
      ob = change_object_type(ob, pass_self_type);
      ob = temporarize_self(ob, call);
    }
    Vector<Node *> param = old_call->parameters();
    for (int i = 0; i < param.size(); i++)
      param[i] = temporarize(param[i], rule->param(i), call);
    
    // Find the next inline level.
    int next_min_level = (level == inlinePath ? inlineYes : inlineNo);
    if (next_min_level < _min_inline_level) next_min_level = _min_inline_level;
    
    // Use ob_type as the new self type, which may be more derived
    // than the type we've cast `ob' to above.
    InlineOptimizer newopt(ob, ob_type, pass_self_type,
			   next_min_level, _max_inline_level, param);
    rule->set_inlining(true);
    body = body->optimize(&newopt);
    rule->set_inlining(false);
    // Don't forget to change the type of `body' back to what it was before.
    body->change_type(old_call->type());

    if (call == 0)
      return body;
    else
      return new SemistrictNode(call, ',', body, body->type(), *body);
  }
  
  return (Node *)old_call;
}


Node *
InlineOptimizer::do_param(const ParamNode *p)
{
  if (_inlining)
    return _param[ p->number() ];
  else
    return (Node *)p;
}

Node *
InlineOptimizer::do_code(const CodeNode *c)
{
  if (_inlining && !c->have_params())
    return new CodeNode(_self, _param, *c);
  else
    return (Node *)c;
}

Node *
InlineOptimizer::do_self(const Node *n)
{
  if (!_inlining)
    return (Node *)n;
  if (n == 0 || _pass_self_type == _self->type())
    return _self;
  
  // Try to change type of `_self' object to `_pass_self_type' to reduce
  // complaints from C compiler.
  if (!_pass_self)
    if (UnaryNode *un = _self->cast_unary()) {
      if (un->op() == opDeref) {
	Node *new_pointer = new CastNode
	  (un->child(), _pass_self_type->make_pointer(),
	   un->betweenliner(), *un);
	_pass_self = new UnaryNode(new_pointer, *un);
      }
    }
  
  return _pass_self ? _pass_self : _self;
}


/*****
 * ConstOptimizer
 **/

Node *
ConstOptimizer::do_binary(const BinaryNode *b)
{
  LiteralNode *l = b->left()->cast_literal();
  LiteralNode *r = b->right()->cast_literal();
  if (!l || !r)
    return (Node *)b;
  
  Type *t = Type::common_type(l->type(), r->type());
  if (!t || !t->cast_arithmetic())
    return (Node *)b;
  
  long lv = l->vlong();
  long rv = r->vlong();
  
  switch ((int)b->op()) {
    
   case '+':
    return new LiteralNode(t, lv + rv, *b);
    
   case '-':
    return new LiteralNode(t, lv - rv, *b);
    
   case '*':
    return new LiteralNode(t, lv * rv, *b);
    
   case '/':
    return new LiteralNode(t, lv / rv, *b);
    
   case '%':
    return new LiteralNode(t, lv % rv, *b);
    
   case '&':
    return new LiteralNode(t, lv & rv, *b);
    
   case '^':
    return new LiteralNode(t, lv ^ rv, *b);

   case '|':
    return new LiteralNode(t, lv | rv, *b);
    
   case opLeftShift:
    return new LiteralNode(t, lv << rv, *b);
    
   case opRightShift:
    return new LiteralNode(t, lv >> rv, *b);

   case opEq:
    return new LiteralNode(t, lv == rv, *b);
    
   case opNotEq:
    return new LiteralNode(t, lv != rv, *b);
    
   case opLt:
    return new LiteralNode(t, lv < rv, *b);
    
   case opLeq:
    return new LiteralNode(t, lv <= rv, *b);
    
   case opGt:
    return new LiteralNode(t, lv > rv, *b);
    
   case opGeq:
    return new LiteralNode(t, lv >= rv, *b);
    
   default:
    return (Node *)b;
    
  }
}

Node *
ConstOptimizer::do_semistrict(const SemistrictNode *ss)
{
  LiteralNode *l = ss->left()->cast_literal();
  Node *r = ss->right();
  if (!l)
    return (Node *)ss;
  
  if (ss->op() == ',')
    // `l' might not be an integral constant; don't call vlong()
    return r;
  
  long lv = l->vlong();
  switch ((int)ss->op()) {
    
   case opLogAnd:
    if (lv)
      return r;
    else
      return new LiteralNode(bool_type, 0L, *ss);
    
   case opLogOr:
    if (lv)
      return new LiteralNode(bool_type, 1L, *ss);
    else
      return r;
    
   case opArrow:
    if (lv) {
      Node *true_node = new LiteralNode(bool_type, 1L, *ss);
      return
	do_semistrict(new SemistrictNode(r, ',', true_node, bool_type, *ss));
    } else
      return new LiteralNode(bool_type, 0L, *ss);
    
   default:
    assert(0);
    return (Node *)ss;
    
  }
}

Node *
ConstOptimizer::do_unary(const UnaryNode *u)
{
  if (u->op() == '+')
    return u->child();
  
  LiteralNode *l = u->child()->cast_literal();
  if (!l)
    return (Node *)u;
  
  Type *t = l->type();
  if (u->op() == '!' && t == bool_type)
    return new LiteralNode(t, !l->vlong(), *u);
  
  if (!t || !t->cast_arithmetic())
    return (Node *)u;
  
  long lv = l->vlong();
  
  switch ((int)u->op()) {
    
   case '-':
    return new LiteralNode(t, -lv, *u);

   default:
    return (Node *)u;
    
  }
}

Node *
ConstOptimizer::do_cast(const CastNode *cast)
{
  LiteralNode *c = cast->child()->cast_literal();
  if (!c)
    return (Node *)cast;
  
  Type *t = c->type();
  if (!t || (!t->cast_arithmetic() && t != bool_type))
    return (Node *)cast;
  
  long lv = c->vlong();
  if (cast->type() == bool_type)
    lv = lv != 0;
  
  return new LiteralNode(cast->type(), lv, *cast);
}

Node *
ConstOptimizer::do_conditional(const ConditionalNode *cond)
{
  LiteralNode *c = cond->test()->cast_literal();
  if (!c)
    return (Node *)cond;
  
  if (c->vlong())
    return cond->yes();
  else
    return cond->no();
}


/*****
 * ParentFixer
 **/

/* ParentFixer is called on constructor bodies only. Its job is to make proper
   constructors -- to make sure that constructors call their parents'
   constructors and such. */

ParentFixer::ParentFixer(ModuleNames *s, const Landmark &l)
  : _module(s->module()), _self(new SelfNode(s, l)),
    _parent_map(-1), _slot_map(-1),
    _landmark(l)
{
  _module->grep_my_fields(_fields);
  _constructed.assign(_fields.size(), 0);
  for (int i = 0; i < _fields.size(); i++)
    if (_fields[i]->is_slot() && _fields[i]->origin() == _module
	&& !_fields[i]->is_static()) {
      if (_fields[i]->module())
	_slot_map.insert(_fields[i], i);
      else
	_slot_map.insert(_fields[i], -2);
    }
}

void
ParentFixer::add_parent(Module *m)
{
  for (int i = 0; i < _fields.size(); i++)
    if (_fields[i]->is_ancestor() && _fields[i]->module() == m)
      _parent_map.insert(m, i);
}

Node *
ParentFixer::do_call(const CallNode *call)
{
  if (!call->rule()->constructor())
    return (Node *)call;
  
  int index = -1;
  if (Node::is_self(call->call_of())) {
    Module *m = call->rule()->origin();
    index = _parent_map[m];
    if (index < 0 && !_module->has_ancestor(m))
      error(*call, "`%m' is not an ancestor of `%m'", m, _module);
    else if (index < 0)
      error(*call, "`%m' is not a direct parent of `%m'", m, _module);
    
  } else if (FieldNode *fn = call->call_of()->cast_field()) {
    if (!Node::is_self(fn->field_of()))
      return (Node *)call;
    Field *f = fn->field();
    index = _slot_map[f];
    if (index == -1)
      error(*call, "`%f' is a field of `%m' not `%m'", f, f->origin().obj(), _module);
    else if (index == -2)
      error(*call, "field `%f' of type `%t' can't be constructed", f, f->type());
    
  } else
    return (Node *)call;
  
  if (index >= 0) {
    _constructed[index]++;
    return (Node *)call;
  } else
    return 0;
}

Node *
ParentFixer::add_implicits(Node *n)
{
  for (int i = 0; i < _fields.size(); i++)
    if (_constructed[i] > 1)
      error(*n, "constructor for `%f' called multiple times", _fields[i]);
  
    else if (_constructed[i] == 0) {
      Field *f = _fields[i];
      if (f->is_import()
	  || (f->is_slot() && _slot_map[f] < 0)
	  || (f->is_ancestor() && _parent_map[f->module()] < 0))
	continue;
      
      Node *subnode = f->is_ancestor() ? _self
	: new FieldNode(_self, _fields[i], false, _landmark);
      Node *con = ConstructorNode::make_implicit
	(subnode, _fields[i]->module(), Node::cur_betweenliner, _landmark);
      if (!con) {
	if (f->is_slot())
	  error(_landmark, "(in initialization of `%m.%f')", _module, f);
	return 0;
      }
      
      n = new BinaryNode
	(con, ',', n, n->type(), _landmark);
    }
  
  return n;
}


/*****
 * ConstructorFixer
 **/

/* ConstructorFixer fixes constructors so they don't really return their
   objects. It's easier to do this after Expr::resolve so that Expr::resolve
   doesn't have to deal with the comma nodes that often result from these
   transformations. (Parts of Expr::resolve must often check a Node to see if
   it was a constructor; it'd be annoying to have to always look for a comma
   node with a constructor LHS rather than just checking for a constructor.)

   ConstructorFixer also determines if the function body has recursion. */

ConstructorFixer::ConstructorFixer(Rule *r)
  : _body_of(r), _recursion(false)
{
}

Node *
ConstructorFixer::do_constructor(const ConstructorNode *const_con)
{
  ConstructorNode *con = (ConstructorNode *)const_con;
  Node *value = con->simple_value();
  con->change_type(void_type);
  return new SemistrictNode(con, ',', value, value->type(), con->landmark());
}

Node *
ConstructorFixer::do_call(const CallNode *const_call)
{
  if (const_call->rule() == _body_of)
    _recursion = true;
  return (Node *)const_call;
}


/*****
 * TailRecursionFixer
 **/

TailRecursionFixer::TailRecursionFixer(Rule *r)
  : _body_of(r), _defining_this_param(-1), _use_new_params(false)
{
}

Node *
TailRecursionFixer::do_call(const CallNode *call)
{
  if (!call->tail_recursion())
    return (Node *)call;
  Rule *rule = call->rule();
  
  // Go through the parameters. If the parameter on the next iteration
  // isn't equal to the parameter on the current iteration, change the
  // parameter to an assignment.
  // Care is needed to deal with cases like
  // f(x, y) ::= ... f(y, x) ...
  // Naive code generation would generate
  // ... x = y; y = x; goto label_representing_f; ...
  // Obviously we don't want this.
  
  // STEP 1. Go through all parameters P_i and mark all those where:
  // (1) P_i != P'_i    [P_i is value now, P'_i is value in next iteration]
  // (2) there exists a P_j where P'_j depends on P_i.
  // Set any_usage = true if such a parameter exists.
  Vector<int> param_usage(call->param_count(), 0);
  for (int i = 0; i < call->param_count(); i++) {
    int self_usage = param_usage[i];
    call->param(i)->mark_param_usage(param_usage);
    param_usage[i] = self_usage;
  }
  bool any_usage = false;
  for (int i = 0; i < call->param_count(); i++)
    if (call->param(i) == rule->param(i))
      param_usage[i] = 0;
    else if (param_usage[i])
      any_usage = true;

  // STEP 2. If any_usage is true, we need to compile the call into a LET of
  // the form ``let var_p_0 = P_0, ..., var_p_n = P_n in
  //              P_0 = ...(refers to var_p_i instead of P_i)...,
  //              P_1 = ...,
  //              call(P_0, ..., P_n) end''.
  // But we only need bindings for those P_i's who meet the criteria listed
  // above in step (1). Prepare the _new_params variable with the "var_p_i"
  // bindings for later optimization.
  LetNode *let = 0;
  if (any_usage) {
    let = new LetNode(*call);
    _new_params.clear();
    for (int i = 0; i < call->param_count(); i++)
      if (param_usage[i]) {
	VariableNode *var =
	  new VariableNode(rule->param_name(i), rule->param_type(i), *call);
	_new_params.push_back(var);
	DeclarationNode *decl =
	  new DeclarationNode(var, rule->param(i), *call);
	let->add_param(decl);
      } else
	_new_params.push_back(rule->param(i));
  }

  // STEP 3. For each parameter which has changed, replace it with an
  // assignment of the form ``P_i = value''. If necessary, optimize ``value''
  // first to replace P_i's with var_p_i's, as discussed above.
  Vector<Node *> call_params;
  _use_new_params = true;
  for (int i = 0; i < call->param_count(); i++) {
    Node *param = rule->param(i);
    Node *value = call->param(i);
    if (any_usage) value = value->optimize(this);
    if (param == value)
      call_params.push_back(param);
    else {
      EffectNode *assign =
	new EffectNode(param, opAssign, value, param->type(), *call);
      param->state_wrap();
      call_params.push_back(assign);
    }
  }
  _use_new_params = false;

  // We're done!
  CallNode *new_call = new CallNode(0, call_params, *call);
  if (let) {
    let->set_body(new_call);
    return let;
  } else
    return new_call;
}


Node *
TailRecursionFixer::do_param(const ParamNode *param)
{
  if (!_use_new_params)
    return (Node *)param;
  else
    return _new_params[param->number()];
}


/*****
 * ExceptionCounter
 **/

ExceptionCounter::ExceptionCounter(ExceptionSet &eset)
  : _eset(eset)
{
}

Node *
ExceptionCounter::do_call(const CallNode *call)
{
  _eset += call->rule()->all_exceptions();
  return (Node *)call;
}

Node *
ExceptionCounter::do_exception(const ExceptionNode *en)
{
  if (en->exception())
    _eset += en->exception();
  return (Node *)en;
}

Node *
ExceptionCounter::do_catch(const CatchNode *catchn)
{
  if (catchn->catch_all())
    _eset = ExceptionSet();
  else
    _eset -= catchn->exception_set();
  return (Node *)catchn;
}


/*****
 * ExceptionLocator
 **/

ExceptionLocator::ExceptionLocator(Exception *ex)
  : _exception(ex), _context(0), _explicit_throw(false)
{
}

Node *
ExceptionLocator::do_call(const CallNode *call)
{
  if (!_explicit_throw && !_context &&
      call->rule()->all_exceptions().contains(_exception))
    _context = call->rule();
  return (Node *)call;
}

Node *
ExceptionLocator::do_exception(const ExceptionNode *en)
{
  if (en->exception() == _exception)
    _explicit_throw = true;
  return (Node *)en;
}

Node *
ExceptionLocator::do_catch(const CatchNode *catchn)
{
  if (catchn->catch_all() || catchn->exception_set().contains(_exception)) {
    _context = 0;
    _explicit_throw = false;
  }
  return (Node *)catchn;
}


/*****
 * CallFixer
 **/

/* CallFixer is used to fix calls into their canonical form. Canonical form
   means the `self' of each call is a simple value (so we can take its
   address). It also handles exceptions. */

Node *
CallFixer::do_call(const CallNode *const_call)
{
  CallNode *call = (CallNode *)const_call;
  Node *of = call->call_of();
  Node *of_value = (of ? of->simple_value() : 0);
  bool of_self = !of || of->cast_self();
  
  Node *preceding = 0;
  
  if (of_self && !call->fixed_rule()) {
    Rule *r = _self->find_rule(call->rule());
    call = new CallNode(of, call->parameters(), *call);
    call->change_rule(r);
    
  } else if (of_self || of->temporary()) {
    /* do nothing */
    
  } else if (!of_value) {
    Node *new_of;
    DeclarationNode *decl;
    if (!of->lvalue()) {
      VariableNode *var = new VariableNode("_self_T", of->type(), *of);
      decl = new DeclarationNode(var, of, *of);
      new_of = var;
    } else {
      Node *of_ptr = of->make_address();
      VariableNode *var = new VariableNode("_this_T", of_ptr->type(), *of_ptr);
      decl = new DeclarationNode(var, of_ptr, *of_ptr);
      new_of = var->make_deref();
    }
    call = new CallNode(new_of, call->parameters(), *call);
    preceding = decl;
    
  } else if (of != of_value) {
    call = new CallNode(of_value, call->parameters(), *call);
    preceding = of;
  }
  
  // Having got the simple value, go ahead and look for exceptions
  Node *call_value = call;
  if (call->rule()->can_throw()) {
    Node *result = exceptid_node();
    // store result of call in `exceptid_node'
    call->make_temporary(result->temporary());
    BinaryNode *test_lz = new BinaryNode
      (call, '<', zero_node(*call), bool_type, *call);
    Node *check_result = new ConditionalNode
      (test_lz, new ExceptionNode(*call), result, call->type(), *call);
    call_value = check_result;
  }
  
  // Now return the built-up thing
  if (preceding)
    return new SemistrictNode
      (preceding, ',', call_value, call_value->type(), *call_value);
  else
    return call_value;
}


/*****
 * ParamReplacer
 **/

Node *
ParamReplacer::do_param(const ParamNode *p)
{
  if (_param.size())
    return _param[ p->number() ];
  else
    return (Node *)p;
}

Node *
ParamReplacer::do_self(const Node *)
{
  return _self;
}
