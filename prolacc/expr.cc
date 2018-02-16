#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "expr.hh"
#include "operator.hh"
#include "error.hh"
#include "prototype.hh"
#include "type.hh"
#include "node.hh"
#include "field.hh"
#include "ruleset.hh"
#include "optimize.hh"
#include "writer.hh"
#include "codeblock.hh"
#include "declarat.hh"

static PermString::Initializer initializer;
PermString all_string = "all";
PermString allstatic_string = "allstatic";
PermString constructor_string = "constructor";


/*****
 * your favorite constructors
 **/

LetExpr::LetExpr(Expr *params, Expr *body)
  : Expr(*params), _params(params), _body(body)
{
}

BinaryExpr::BinaryExpr(Expr *left, Operator op, Expr *right, Expr *opt)
  : Expr(*left), _op(op), _left(left), _right(right), _optional(opt)
{
}


FunctionishExpr::FunctionishExpr(Operator op, ListExpr *l)
  : Expr(*l), _op(op), _args(l)
{
}


MemberExpr::MemberExpr(Expr *left, Operator op, PermString right)
  : Expr(*left), _op(op), _left(left), _right(right)
{
}

MemberExpr::MemberExpr(PermString right, const Landmark &lm)
  : Expr(lm), _op('.'), _left(0), _right(right)
{
}


UnaryExpr::UnaryExpr(Operator op, Expr *left, Expr *opt)
  : Expr(*left), _op(op), _left(left), _optional(opt)
{
}

ConditionalExpr::ConditionalExpr(Expr *t, Expr *y, Expr *n)
  : Expr(*t), _test(t), _yes(y), _no(n)
{
}

CodeExpr::CodeExpr(CodeBlock *code)
  : Expr(code->landmark()), _code(code)
{
}

SelfExpr::SelfExpr(bool super, const Landmark &lm)
  : Expr(lm), _super(super)
{
}


/*****
 * (potpourri)
 **/


/*****
 * build_list
 **/

void
Expr::build_list(Vector<Expr *> &l, bool) const
{
  l.push_back((Expr *)this);
}

void
UnaryExpr::build_list(Vector<Expr *> &l, bool top) const
{
  if (_op == '(' && top)
    _left->build_list(l, false);
  else
    Expr::build_list(l, false);
}

void
BinaryExpr::build_list(Vector<Expr *> &l, bool) const
{
  if (_op == ',') {
    _left->build_list(l, false);
    _right->build_list(l, false);
  } else
    Expr::build_list(l, false);
}

void
ListExpr::build_list(Vector<Expr *> &l, bool) const
{
  for (int i = 0; i < _l.size(); i++)
    l.push_back(_l[i]);
}


/*****
 * declaration
 **/

void
Expr::declaration(Declaration *dt) const
{
  dt->fail();
}

void
MemberExpr::declaration(Declaration *dt) const
{
  if (_left)
    _left->declaration(dt);
  dt->set_name(_right);
  dt->implicit_type((Expr *)this);
}

void
BinaryExpr::declaration(Declaration *dt) const
{
  switch ((int)_op) {
    
   case opTypeDecl:
    _left->declaration(dt);
    dt->force_name();
    dt->set_type(_right);
    break;
    
   case opAssign:
    _left->declaration(dt);
    dt->set_value(_right);
    break;
    
   case '(':
   case opCall:
    _left->declaration(dt);
    dt->set_type((Expr *)this);
    break;
    
   case opHide:
   case opShow:
   case opRename:
   case opUsing:
   case opNotusing:
   case opNoinline2:
   case opDefaultinline2:
   case opInline2:
   case opPathinline2:
    _left->declaration(dt);
    dt->implicit_type((Expr *)this);
    break;
    
  }
}

void
IdentExpr::declaration(Declaration *dt) const
{
  dt->set_name(_ident);
  dt->implicit_type((Expr *)this);
}


/*****
 * parse_name
 **/

bool
Expr::name_ok(PermString name, const Landmark &lm, const char *context)
{
  if (name == all_string || name == allstatic_string
      || name == constructor_string) {
    error(lm, "invalid name in %s", context);
    return false;
  } else
    return true;
}


PermString
Expr::parse_name(Namespace *&, ModuleNames *&, const char *context, int)
  const
{
    error(*this, "name expression required in %s", context);
    return PermString();
}

PermString
BinaryExpr::parse_name(Namespace *&namesp, ModuleNames *&ancestor,
		       const char *context, int flags) const
{
#if 0
  if (_op != opCastParen)
  // check for top-level feature reference, `.X'
  if (_left == 0) {
    while (namesp->parent()) namesp = namesp->parent();
    return _right;
  }
  
  PermString name = _left->parse_name(namesp, ancestor, context, flags);
  if (!name)
    return 0;
  else if (!name_ok(name, *this, context))
    return 0;
  
  if (Feature *f = namesp->find(name)) {
    if (Namespace *ns = f->cast_namespace()) {
      namesp = ns;
      if ((flags & parseUniqueify) && !ancestor)
	namesp->make_unique();
      return _right;
      
    } else if (Field *field = f->cast_field()) {
      if (field->is_ancestor()) {
	ancestor = field->modnames();
	namesp = ancestor;
	assert(ancestor);
	return _right;
      }
      
    } else if (Prototype *proto = f->cast_prototype()) {
      ancestor = proto->outer_modnames();
      namesp = ancestor;
      return _right;
    }
    
  } else if (flags & (parseInitial | parseCreate)) {
    Namespace *new_namesp = new Namespace(name, namesp, *this);
    namesp->def(name, new_namesp);
    namesp = new_namesp;
    return _right;
  }
  
#endif
  return Expr::parse_name(namesp, ancestor, context, flags);
}

PermString
MemberExpr::parse_name(Namespace *&namesp, ModuleNames *&ancestor,
		       const char *context, int flags) const
{
  // check for top-level feature reference, `.X'
  if (_left == 0) {
    while (namesp->parent()) namesp = namesp->parent();
    return _right;
  }
  
  PermString name = _left->parse_name(namesp, ancestor, context, flags);
  if (!name)
    return PermString();
  else if (!name_ok(name, *this, context))
    return PermString();
  
  if (Feature *f = namesp->find(name)) {
    if (Namespace *ns = f->cast_namespace()) {
      namesp = ns;
      if ((flags & parseUniqueify) && !ancestor)
	namesp->make_unique();
      return _right;
      
    } else if (Field *field = f->cast_field()) {
      if (field->is_ancestor()) {
	ancestor = field->modnames();
	namesp = ancestor;
	assert(ancestor);
	return _right;
      }
      
    } else if (Prototype *proto = f->cast_prototype()) {
      ancestor = proto->outer_modnames();
      namesp = ancestor;
      return _right;
    }
    
  } else if (flags & (parseInitial | parseCreate)) {
    Namespace *new_namesp = new Namespace(name, namesp, *this);
    namesp->def(name, new_namesp);
    namesp = new_namesp;
    return _right;
  }
  
  error(*this, "bad name component `%s' in %s", name.c_str(), context);
  return PermString();
}

PermString
IdentExpr::parse_name(Namespace *&, ModuleNames *&, const char *, int) const
{
  return _ident;
}


PermString
Expr::create_name_text(const char *context) const
{
  error(*this, "name expression required in %s", context);
  return PermString();
}

PermString
BinaryExpr::create_name_text(const char *context) const
{
  return Expr::create_name_text(context);
}

PermString
MemberExpr::create_name_text(const char *context) const
{
    // check for top-level feature reference, `.X'
    if (_op != '.') {
	error(*this, "name expression required in %s", context);
	return PermString();
    } else if (!_left)
	return permprintf(".%p", _right.capsule());
    else if (PermString left_str = _left->create_name_text(context))
	return permprintf("%p.%p", left_str.capsule(), _right.capsule());
    else
	return PermString();
}

PermString
IdentExpr::create_name_text(const char *) const
{
    return _ident;
}

/*****
 * resolve
 **/

Node *
Expr::resolve(ModuleNames *, Namespace *, ResolveArgs) const
{
  assert(0 && "compiler bug: can't resolve this expr");
  return 0;
}

Node *
Expr::resolve_feature(ModuleNames *original_m, ModuleNames *m, Node *object,
		      Feature *f, ResolveArgs res) const
{
    assert(f);
  
    if (RuleRef *rref = f->cast_ruleref()) {
	// Now that we've got the feature we need, we can strip any ancestors
	// off the object, which were useful only for finding the right
	// namespace.
	object = (object ? object->call_object() : 0);
	SelfNode *self_object = (object ? object->cast_self() : 0);
	bool object_is_super = self_object && self_object->super();
    
	// If the rule was really found through `self' or `super', then
	// replace it with the real rule from self.
	Rule *rule = 0;
	if (object_is_super) {
	    // it was found through super; the `super' object already has the
	    // correct type
	    rule = object->type()->cast_module()->find_rule(rref);
	    object = 0;		// change it into `self'
      
	} else if (!object || self_object) {
	    rule = original_m->find_rule(rref);
	    object = 0;		// remove `self'; change to 0
      
	} else if (Module *m = object->type()->cast_module())
	    rule = m->find_rule(rref);

	assert(rule && "Can't find rule");

	if (rule->undefined_implicit())
	    // never report static vs. dynamic errors
	    ;
	else if (res.is_static() && !rule->is_static()) {
	    error(*this, "dynamic method `%r' called in static context", rule);
	    return 0;
	} else if (!res.is_static() && rule->is_static() && object) {
	    FieldNode *field = object->cast_field();
	    if (!field || !field->field()->allow_static()) {
		error(*this, "static method `%r' called in dynamic context", rule);
		return 0;
	    }
	}

	rule->make_type();
	if (!res.is_call() && rule->param_count() > 0) {
	    error(*this, "too few arguments to method `%*r'", rule);
	    return 0;
	}

	Type *ret_type = original_m->header_type(rule->return_type());
	return new CallNode
	    (object, rule, ret_type, object_is_super,
	     Node::cur_betweenliner, *this);

    } else if (Field *field = f->cast_field()) {
	assert(m);
	if (res.is_static() && !field->allow_static()) {
	    if (!object || object->simple_type()) {
		error(*this, "dynamic %-f `%f' in static context", field, field);
		return 0;
	    }
	} else if (!res.is_static() && field->is_static() && object) {
	    FieldNode *objf = object->cast_field();
	    if (!objf || !objf->field()->allow_static()) {
		error(*this, "static %-f `%f' in dynamic context", field, field);
		return 0;
	    }
	}
    
	if (field->is_slot() || res.allow_type())
	    // Fields are only legal if they're slots, or a type was expected
	    return new FieldNode(object, field, res.is_static() || field->is_import(), landmark());
	else {
	    error(*this, "type `%f' in value context", field);
	    return 0;
	}
    
    } else if (NodeRef *nref = f->cast_noderef()) {
	return nref->node();
    
    } else if (Namespace *namesp = f->cast_namespace()) {
    
	// It's OK to return a NamespaceNode only if `memberp' is true.
	if (res.is_member())
	    return new NamespaceNode(object, namesp, landmark());
    
	// Otherwise, it's an implicit call to the namespace's self-named
	// feature.
	Feature *subf = namesp->find(namesp->concrete_name());
	if (subf && subf->cast_ruleref()) {
	    // Recursive call to resolve_feature to handle all the
	    // zero-argument stuff.
	    return resolve_feature(original_m, m, object, subf, res);
	} else {
	    error(*this, "namespace `%F' cannot be called", f);
	    return 0;
	}
    
    } else if (Prototype *prototype = f->cast_prototype()) {
	return new PrototypeNode(prototype, false, landmark());
    
    } else {
	assert(0 && "unexpected kind of Feature in Expr::resolve_feature");
	return 0;
    }
}

Node *
IdentExpr::resolve(ModuleNames *m, Namespace *ns, ResolveArgs res) const
{
  Feature *f = ns->search(_ident);
  if (!f) {
    error(*this, "`%s' undeclared", _ident.c_str());
    return 0;
  }
  return resolve_feature(m, m, 0, f, res);
}

Node *
MemberExpr::resolve(ModuleNames *m, Namespace *ns, ResolveArgs res) const
{
  // Check for global name reference, ".X".
  if (_left == 0) {
    IdentExpr ident(_right, landmark());
    while (ns->parent()) ns = ns->parent();
    return ident.resolve(m, ns, res);
  }
  
  // Only return a namespace if _op is regular member (not pointer-to-member).
  Node *l = _left->resolve(m, ns, _op == '.' ? res.new_member() : res);
  if (!l) return 0;
  
  // Check for a PrototypeNode and resolve1 it
  if (PrototypeNode *proto_l = l->cast_prototype())
    proto_l->resolve1();
  
  // Turn (a->b) into ((*a).b)
  if (_op == opPtrMember)
    if (!(l = l->make_deref()))
      return 0;
  
  ModuleNames *left_module = l->type()->cast_modnames();
  Feature *f = l->feature(_right);
  if (!f) {
    if (NamespaceNode *namenode = l->cast_namespace())
      error(*this, "namespace `%n' has no feature `%s'",
	    namenode->namesp(), _right.c_str());
    else if (left_module)
      error(*this, "`%t' has no feature `%s'", l->type(), _right.c_str());
    else if (l->type() == any_type)
      // taking a member of type `any_type' is OK -- it's an exception or an
      // error we've already reported. Just return the thing we tried to take
      // a member of.
      return l;
    else
      error(*this, "can't take a member of type `%t'", l->type());
    return 0;
  }
  
  Node *obj = l->object();
  if (obj) {
    left_module = obj->type()->cast_modnames();
    res = res.make_static(obj->is_static());
    assert(left_module);
  } else if (!left_module)
    left_module = m;
  
  return resolve_feature(m, left_module, obj, f, res);
}

Node *
LetExpr::resolve(ModuleNames *m, Namespace *ns, ResolveArgs res) const
{
  Namespace letspace("letspace", ns, landmark());
  
  LetNode *let = new LetNode(*_params);
  
  Vector<Expr *> vec;
  _params->build_list(vec, true);
  for (int i = 0; i < vec.size(); i++) {
    Declaration decl3;
    vec[i]->declaration(&decl3);
    if (decl3.let_ok()) {
      
      Node *value = 0;
      if (decl3.value())
	value = decl3.value()->resolve(m, ns, res.clear());
      
      Type *type = 0;
      Node *type_node = 0;
      if (decl3.explicit_type()) {
	type_node = decl3.type()->resolve(m, ns, ResolveArgs::type_args());
	if (!type_node || !type_node->simple_type())
	  error(*decl3.type(), "bad type in let expression");
	else
	  type = type_node->type();
      }
      
      if (!type)
	if (!value) {
	  error(*vec[i], "can't find type in `let'");
	  type = any_type;
	} else
	  type = value->type();

      if (!type->allow_object()) {
	error(*vec[i], "can't declare an object of type `%t'", type);
	type = any_type;
      }
      
      VariableNode *var = new VariableNode(decl3.name(), type, *vec[i]);
      
      if (value) {
	Node *decl = new DeclarationNode(var, value, *vec[i]);
	let->add_param(decl);
      } else if (Node *node = make_construction(var, type_node))
	let->add_param(node);
      else
	return 0;
      
      letspace.def(decl3.name(), new NodeRef(decl3.name(), var));
      
    } else
      error(*vec[i], "bad let");
  }
  
  Node *body = _body->resolve(m, &letspace, res.clear());
  if (!body)
    return 0;
  
  let->set_body(body);
  return let;
}

Node *
BinaryExpr::resolve_mod_op(ModuleNames *m, Namespace *ns, ResolveArgs res) const
{
  Node *l = _left->resolve(m, ns, res);
  if (!l)
    return 0;
  
  // create a new protofrob only if necessary
  bool create_new_protofrob = true;
  if (BinaryExpr *binary_left = _left->cast_binary())
    if (binary_left->op().module_op())
      create_new_protofrob = false;
  
  // what prototype are we frobbing?
  Prototype *prototype = 0;
  if (PrototypeNode *protonode = l->cast_prototype()) {
    prototype = protonode->prototype();
    if (protonode->in_afterfrob())
      create_new_protofrob = false;
  } else if (FieldNode *fieldnode = l->cast_field())
    if (fieldnode->simple_type() && fieldnode->type()->cast_modnames())
      prototype = fieldnode->type()->cast_modnames()->prototype();
  
  if (!prototype) {
    error(*this, "bad module operator");
    return l;
  }

  // create new prototype if necessary
  if (create_new_protofrob)
    prototype = new Protofrob(prototype, ns, landmark());
  Protofrob *protofrob = prototype->cast_protofrob();
  assert(protofrob);

  // apply module operators to the protofrob
  switch ((int)_op) {

   case opNullFrob:
    // must add a ModuleFrob here! otherwise, a separate ModuleNames won't
    // get created. We use NullFrobs in module equations. Each equation is
    // expected to generate a typedef, so we must make sure that a separate
    // ModuleNames is generated, so something exists to output the typedef!
    protofrob->add_frob(ModuleFrob());
    break;
    
   case opHide:
   case opUsing:
   case opNotusing:
   case opNoinline2:
   case opDefaultinline2:
   case opInline2:
   case opPathinline2: {
     Vector<Expr *> v;
     _right->build_list(v, true);
     
     if (_op != opInline2)
       assert(!_optional);
     
     for (int i = 0; i < v.size(); i++) {
       ModuleFrob frob(_op, v[i], _optional, *v[i]);
       protofrob->add_frob(frob);
     }
     
     break;
   }
   
   case opShow:
   case opRename: {
     Vector<Expr *> v;
     _right->build_list(v, true);
     
     for (int i = 0; i < v.size(); i++) {
       Expr *old_name = v[i];
       Expr *new_name = 0;
       if (BinaryExpr *bin = v[i]->cast_binary())
	 if (bin->op() == opAssign) {
	   new_name = bin->left();
	   old_name = bin->right();
	 }
       
       ModuleFrob frob(_op, old_name, new_name, *v[i]);
       protofrob->add_frob(frob);
     }
     
     break;
   }
   
   default:
    assert(0);
    
  }

  // add to the import list if necessary
  if (create_new_protofrob) {
    if (FieldNode *fieldnode = l->cast_field()) {
      Protomodule *protom = m->module()->protomodule();
      Field *f = protom->add_import_prototype(protofrob, *this);
      return new FieldNode(fieldnode->field_of(), f, true, *this);
    } else
      return new PrototypeNode(protofrob, false, *this);
  } else
    return l;
}

Node *
BinaryExpr::resolve_call_args(ModuleNames *m, Namespace *ns, ResolveArgs res,
			      CallNode *call) const
{
  Rule *rule = call->rule();
  ListExpr *r = _right->cast_list();
  if (!r) {
    error(*_right, "syntax error (cast mistaken for call?)");
    return 0;
  }

  if (rule->undefined_implicit())
    // always OK
    return call;
  
  if (r->count() != rule->param_count()) {
    error(*this, "too %s arguments to method `%*r'",
	  r->count() < rule->param_count() ? "few" : "many", rule);
    return 0;
  }
  
  for (int i = 0; i < r->count(); i++) {
    Node *p = r->elt(i)->resolve(m, ns, res.clear());
    if (!p) return 0;
    
    p = p->type_convert(rule->param_type(i), *this);
    if (!p) {
      error(*r->elt(i), "argument %d to `%r' has wrong type (expected %t)",
	    i + 1, rule, rule->param_type(i));
      return 0;
    }
    
    call->add_param(p);
  }
  
  return call;
}

static Node *
finish_resolve_cast(Node *r, Type *l_type, const Landmark &lm)
{
  Node *cast = r ? r->type_cast(l_type, lm) : 0;
  if (cast && cast == r && l_type->cast_pointer())
    // XXX force cast so compiler will shut up
    return cast->make_cast(l_type, lm);
  else
    return cast;
}

static Node *
finish_resolve_arith_unary(Operator op, Node *r, Type *l_type,
			   const Landmark &lm)
{
  if (!r->type()->cast_arithmetic()) {
    error(lm, "can't use `%o' on type `%t'", (int)op, r->type());
    return 0;
  }
  r = new UnaryNode(r, op, r->type(), Node::cur_betweenliner, lm);
  return r->type_cast(l_type, lm);
}

Node *
BinaryExpr::resolve_construction(ModuleNames *m, Namespace *ns,
				 ResolveArgs res, Node *l, Node *object) const
{
  Type *ltype = l->type();
  Module *lmod = ltype ? ltype->cast_module() : 0;
  if (lmod == 0) {
    /* Not a module type; no construction allowed */
    error(*this, "can't construct objects of type `%t'", l->type());
    return 0;
  }
  
  Rule *ctor_rule = lmod->constructor_rule();
  if (!l->feature("constructor"))
    error(*l, "`%m's constructor is hidden", lmod);
  
  Node *ctor_object;
  if (l->simple_type())
    ctor_object = new VariableNode("_ctor_T", l->type(), *this);
  else
    ctor_object = l;
  
  ConstructorNode *ctor = new ConstructorNode
    (object, ctor_rule, Node::cur_betweenliner, landmark());
  
  return resolve_call_args(m, ns, res, ctor);
}

Node *
BinaryExpr::resolve_call(ModuleNames *m, Namespace *ns, ResolveArgs res) const
{
  Node *l = _left->resolve(m, ns, res.new_call());
  if (!l || l->type() == any_type)
    return l;
  
  if (CallNode *call = l->cast_call()) {
    //// Call of rule
    
    return resolve_call_args(m, ns, res, call);
    
  } else if (l->simple_type() && _op != opCastParen) {
    //// Constructor
    
    if (l->cast_constructor()) {
      error(*this, "already constructed");
      return 0;
    }
    Node *object = new VariableNode("_ctor_T", l->type(), *this);
    return resolve_construction(m, ns, res, l, object);
    
  } else if (l->simple_type()) {
    //// Cast expression
    
    Node *r = _right->resolve(m, ns, res.clear());
    return finish_resolve_cast(r, l->type(), *this);

  } else {
    error(*l, "bad parentheses (not call, constructor, or cast)");
    return 0;
  }
}

Node *
BinaryExpr::resolve_catch(ModuleNames *m, Namespace *ns, ResolveArgs res) const
{
  Node *l = _left->resolve(m, ns, res.clear());
  if (!l) return 0;
  
  Vector<Expr *> v;
  _right->build_list(v, true);
  
  ExceptionSet eset;
  int catch_all = 0;
  for (int i = 0; i < v.size(); i++) {
    ModuleNames *e_modnam = 0;
    Namespace *e_namesp = ns;
    PermString e_name = v[i]->parse_name(e_namesp, e_modnam, "catch");
    if (!e_name) return 0;

    Feature *feat = e_namesp->find(e_name);
    if (e_modnam == 0 && e_namesp == ns && !feat)
      feat = e_namesp->search(e_name);
    
    if (e_name == "all")
      catch_all++;
    else if (feat) {
      if (Exception *ex = feat->exception_value())
	eset += ex;
      else {
	error(*v[i], "can't catch %-F `%n.%s'", feat, e_namesp, e_name.c_str());
	return 0;
      }
    } else {
      error(*v[i], "no exception named `%n.%s'", e_namesp, e_name.c_str());
      return 0;
    }
  }
  
  if (catch_all > 1 || (catch_all && eset))
    warning(*this, "redundant exceptions after `catch all'");
  
  return new CatchNode(l, eset, catch_all, *this);
}

static Node *
resolve_address_of(Node *l, const Landmark &lm)
{
  if (!l->lvalue()) {
    error(lm, "can't take address of rvalue");
    return 0;
    
  } else
    // state_wrap to ensure that a parameter whose address is taken
    // is not inlined away
    // XXX throws away this expr's landmark & uses `l's
    return l->state_wrap()->make_address();
}

static Node *
resolve_inequality(Node *l, Operator compare_op, Node *r, Operator error_op,
		   const Landmark &lm)
{
    if (!l || !r)
	return 0;
  
    Type *common = Type::common_type(l->type(), r->type(), true);
    if (!common->cast_arithmetic() && !common->cast_pointer()) {
	if (common == any_type)
	    // OK to compare any_type
	    return (l->type() == any_type ? l : r);
	else {
	    error(lm, "can't use `%o' with types `%t'/`%t'", (int)error_op,
		  l->type(), r->type());
	    return 0;
	}
    }
  
    l = l->type_convert(common, lm);
    r = r->type_convert(common, lm);
    assert(l && r);
  
    if (common == seqint_type)
	compare_op = compare_op.seqint_version();
  
    return new BinaryNode(l, compare_op, r, bool_type, lm);
}

static Node *
simplify_value(Node *&comma, LetNode *&let, Node *value, const Landmark &lm)
{
    Node *simple = value->simple_value();
    if (simple == value)
	return simple;
  
    else if (simple) {
	if (comma)
	    comma = new SemistrictNode(comma, ',', value, value->type(), lm);
	else
	    comma = value;
	return simple;
    
    } else {
	if (!let)
	    let = new LetNode(lm);
	VariableNode *var = new VariableNode("_T_bc", value->type(), lm);
	let->add_param(new DeclarationNode(var, value, lm));
	return var;
    }
}

static Node *
finish_simplify(Node *comma, LetNode *let, Node *value)
{
    if (let) {
	let->set_body(value);
	value = let;
    }
    if (comma)
	value = new SemistrictNode(comma, ',', value, value->type(), *value);
    return value;
}

static Node *
special_assign(Node *l, Operator _op, Node *r, const Landmark &lm)
{
    Node *comma = 0;
    LetNode *let = 0;
  
    Node *l_rvalue = l;
    if (l->simple_value() == l)
	/* denada */;
    else {
	l = simplify_value(comma, let, resolve_address_of(l, lm), lm);
	l_rvalue = l->make_deref();
    }
    r = simplify_value(comma, let, r, lm);
  
    Node *body;
    switch ((int)_op) {
    
      case opMinAssign:
      case opMaxAssign: {
	  Node *node_cmp =
	      resolve_inequality(l_rvalue, _op == opMinAssign ? opGt : opLt, r,
				 _op, lm);
	  BinaryNode *cmp = (node_cmp ? node_cmp->cast_binary() : 0);
	  if (!cmp)
	      return node_cmp;
	  Node *r_cast = cmp->right()->type_convert(l->type(), lm);
	  EffectNode *assign = new EffectNode(l, '=', r_cast, l->type(), lm);
	  SemistrictNode *test = new SemistrictNode(cmp, opArrow, assign, bool_type, lm);
	  body = new SemistrictNode(test, ',', l_rvalue, l->type(), lm);
	  break;
      }
   
      default:
	assert(0 && "can't special assign this tingy");
	break;
	
    }
    return finish_simplify(comma, let, body);
}

Node *
BinaryExpr::resolve(ModuleNames *m, Namespace *ns, ResolveArgs res) const
{
    // Check for special kinds of operations.
    if (_op == '(' || _op == opCall || _op == opCastParen)
	return resolve_call(m, ns, res);
    if (_op.module_op())
	return resolve_mod_op(m, ns, res);
    if (_op == opCatch)
	return resolve_catch(m, ns, res);

    Node *l = _left->resolve(m, ns, res.add_allow_type(_op=='&' || _op=='*' || _op=='['));
    Node *r = _right->resolve(m, ns, res.add_allow_type(_op==opTypeDecl));
    if (!l || !r)
	return 0;
  
    if (_op.assignment())
	goto assignment;
  
    switch ((int)_op) {

      case '[': {
	  if (l->simple_type()) {
	      // This is the definition of a type `array-of X'.
	      // MEMLEAK on types
	      int size = -1;
	      if (r) {
		  if (!NodeOptimizer::to_integer_constant(r, m->module(), size))
		      error(*this, "array size must be static integer constant");
	      }
	      Type *arr_type = ArrayType::make(l->type(), size, *l);
	      if (!res.allow_type() && arr_type) {
		  error(*this, "type `%t' in value context", arr_type);
		  return 0;
	      } else if (arr_type) {
		  Literal arr_literal(arr_type, *l);
		  return new LiteralNode(arr_literal);
	      } else
		  return 0;
       
	  } else {
	      PointerType *ptr = l->type()->cast_pointer();
	      if (!ptr) {
		  error(*l, "left operand of `[]' must have array or pointer type");
		  return 0;
	      }
       
	      if (r->type()->cast_arithmetic()) {
		  Node *n = new BinaryNode(l, '+', r, ptr, landmark());
		  return ptr->make_deref(n);
	      } else {
		  error(*r, "right operand of `[]' must have integral type");
		  return 0;
	      }
	  }
      }
   
      case '+':
	// This might be a cast in the situation (TYPE)+x: we will have
	// misinterpreted the + as binary.
	if (l->simple_type())
	    return finish_resolve_arith_unary(opUnaryPlus, r, l->type(), *this);
	else {
	    PointerType *ptr;
	    Type *other = 0;
	    if ((ptr = l->type()->cast_pointer()))
		other = r->type()->cast_arithmetic();
	    else if ((ptr = r->type()->cast_pointer()))
		other = l->type()->cast_arithmetic();
	    if (other)
		return new BinaryNode(l, _op, r, ptr, landmark());
	    else
		goto binary_arithmetic;
	}
    
      case '-':
	// This might be a cast in the situation (TYPE)-x: we will have
	// misinterpreted the - as binary.
	if (l->simple_type())
	    return finish_resolve_arith_unary(opUnaryMinus, r, l->type(), *this);
	else {
	    PointerType *ptr = l->type()->cast_pointer();
	    Type *other = r->type();
	    
	    if (ptr && other->cast_arithmetic())
		return new BinaryNode(l, _op, r, ptr, landmark());
	    else if (ptr && other->cast_pointer()) {
		r = r->type_convert(ptr, *this);
		assert(r);
		return new BinaryNode(l, _op, r, int_type, landmark());
	    } else if (l->type() == seqint_type && r->type() == seqint_type) {
		// Special case: seqint - seqint == int.
		return new BinaryNode(l, _op, r, int_type, landmark());
	    } else
		goto binary_arithmetic;
	}
    
      case '&':
	// This might be a cast in the situation (TYPE)&x: we will have
	// misinterpreted the & as binary.
	if (l->simple_type()) {
	    r = resolve_address_of(r, landmark());
	    return finish_resolve_cast(r, l->type(), *this);
	} else
	    goto binary_arithmetic;
    
      case '*':
	// This might be a cast in the situation (TYPE)*x: we will have
	// misinterpreted the * as binary.
	if (l->simple_type()) {
	    r = r->make_deref();
	    return finish_resolve_cast(r, l->type(), *this);
	} else
	    goto binary_arithmetic;
      
      binary_arithmetic:
      case '/':
      case '%':
      case '^':
      case '|':
      case opLeftShift:
      case opRightShift: {
	  Type *resultt = ArithmeticType::binary_type(l->type(), r->type());
	  if (!resultt) {
	      error(*this, "can't use `%o' with types `%t'/`%t'", (int)_op, l->type(), r->type());
	      return 0;
	  }
	  
	  l = l->type_convert(resultt, *this);
	  r = r->type_convert(resultt, *this);
     
	  if (l && r)
	      return new BinaryNode(l, _op, r, resultt, landmark());
	  else
	      return 0;
      }
   
      case opLt:
      case opLeq:
      case opGt:
      case opGeq:
	return resolve_inequality(l, _op, r, _op, landmark());
   
      case opEq:
      case opNotEq: {
	  Type *common = Type::common_type(l->type(), r->type(), true);
	  l = l->type_convert(common, *this);
	  r = r->type_convert(common, *this);
     
	  if (common->cast_module()) {
	      error(*this, "can't compare module type `%m' for equality", common);
	      return 0;
	  }
     
	  if (l && r && common != void_type)
	      return new BinaryNode(l, _op, r, bool_type, landmark());
	  else
	      return 0;
      }
   
      assignment: {
	  if (l->type() == any_type)
	      // just return it
	      return l;
	  
	  if (!l->lvalue()) {
	      error(*l, "left operand of `%o' must be lvalue", (int)_op);
	      return 0;
	  }
     
	  Type *want = l->type();
	  if (_op == opAssign)
	      /* do nothing; cast below takes care of everything */;
	  else if (l->type()->cast_arithmetic())
	      /* any compound assignment OK on arithmetic type */;
	  else if (l->type()->cast_pointer() && r->type()->cast_arithmetic()
		   && (_op == opAddAssign || _op == opSubAssign))
	      /* pointer addition is OK */
	      want = r->type();
	  else {
	      error(*l, "can't use `%o' on type `%t'", (int)_op, l->type());
	      return 0;
	  }
     
	  l = l->state_wrap();
	  r = r->type_convert(want, *this);
	  if (!r)
	      return 0;
	  if (_op == opMinAssign || _op == opMaxAssign)
	      return special_assign(l, _op, r, landmark());
	  else
	      return new EffectNode(l, _op, r, l->type(), landmark());
      }
      
      case opLogAnd: {
	  l = l->type_convert(bool_type, *this);
	  r = r->type_convert(bool_type, *this);
	  if (l && r)
	      return new SemistrictNode(l, _op, r, bool_type, landmark());
	  else
	      return 0;
      }
   
      case opLogOr: {
	  l = l->type_convert(bool_type, *this);
	  if (l && r->type() == void_type) {
	      Node *f = new LiteralNode(void_type, 0, landmark());
	      return new ConditionalNode(l, f, r, void_type, landmark());
	  }
	  r = r->type_convert(bool_type, *this);
	  if (l && r)
	      return new SemistrictNode(l, _op, r, bool_type, landmark());
	  else
	      return 0;
      }
   
      case ',':
	return new SemistrictNode(l, _op, r, r->type(), landmark());
	
      case opArrow: {
	  l = l->type_convert(bool_type, *this);
	  if (l && r)
	      return new SemistrictNode(l, _op, r, bool_type, landmark());
	  else
	      return 0;
      }
   
      case opTypeDecl: {
	  if (!r->simple_type())
	      error(*r, "right operand of `:>' must be type");
	  return l->type_convert(r->type(), *this);
      }
   
    }
    
    assert(0 && "can't handle operator yet");
    return 0;
}

Node *
FunctionishExpr::resolve(ModuleNames *m, Namespace *ns, ResolveArgs res) const
{
    switch ((int)_op) {
    
      case opMin:
      case opMax: {
	  if (_args->count() != 2) {
	      error(*this, "`%o' takes 2 arguments", (int)_op);
	      return 0;
	  }
       
	  Node* l = _args->elt(0)->resolve(m, ns, res);
	  Node* r = _args->elt(1)->resolve(m, ns, res);
	  if (!l || !r)
	      return 0;
       
	  Node* comma = 0;
	  LetNode* let = 0;
	  l = simplify_value(comma, let, l, landmark());
	  r = simplify_value(comma, let, r, landmark());
	  Node* node_cmp =
	      resolve_inequality(l, _op == opMin ? opLt : opGt, r, _op, landmark());
	  BinaryNode* cmp = (node_cmp ? node_cmp->cast_binary() : 0);
	  if (!cmp)
	      return node_cmp;
	  
	  // We relied on resolve_inequality to cast l and r to their common
	  // type; get back the cast versions.
	  l = cmp->left();
	  r = cmp->right();
	  ConditionalNode *cond =
	      new ConditionalNode(cmp, l, r, l->type(), landmark());
	  return finish_simplify(comma, let, cond);
      }
	
    }
  
    assert(0 && "can't handle operator yet");
    return 0;
}

Node *
ConditionalExpr::resolve(ModuleNames *m, Namespace *ns, ResolveArgs res) const
{
    Node* t = _test->resolve(m, ns, res.clear());
    Node* y = _yes->resolve(m, ns, res.clear());
    Node* n = _no->resolve(m, ns, res.clear());
    if (!t || !y || !n)
	return 0;
  
    t = t->type_convert(bool_type, *this);
    Type *common = Type::common_type(y->type(), n->type(), true);
    y = y->type_convert(common, *this);
    n = n->type_convert(common, *this);
  
    if (t && y && n)
	return new ConditionalNode(t, y, n, common, landmark());
    else
	return 0;
}

Node *
ListExpr::resolve(ModuleNames *m, Namespace *ns, ResolveArgs res) const
{
  Node *result = 0;
  for (int i = 0; i < count(); i++) {
    Node *p = elt(i)->resolve(m, ns, res.clear());
    if (!p)
      return 0;
    if (result)
      result = new BinaryNode(result, ',', p, p->type(), p->landmark());
    else
      result = p;
  }
  return result;
}

Node *
UnaryExpr::resolve(ModuleNames *m, Namespace *ns, ResolveArgs res) const
{
  Node *l = 0;
  if (!_op.betweenliner()) {
    l = _left->resolve(m, ns, res.add_allow_type(_op == opDeref));
    if (!l) return 0;
  }
  
  Node *opt = _optional ? _optional->resolve(m, ns, res.clear()) : 0;
  if (!opt && _optional) return 0;

  int level;
  switch ((int)_op) {
    
   case opNoinline:		level = inlineNo; goto inlines;
   case opDefaultinline:	level = inlineDefault; goto inlines;
   case opInline:		level = inlineYes; goto inlines;
   case opPathinline:		level = inlinePath; goto inlines;
    
   inlines: {
     if (opt) {
       if (!NodeOptimizer::to_integer_constant(opt, m->module(), level))
	 error(*opt, "optional argument to inline must be const expression");
     }
     if (level < inlineNo || level > inlinePath) {
       level = 0;
       error(*opt, "inline level must be 0, 1, 2, or 3");
     }
     
     Betweenliner old = Node::cur_betweenliner;
     Node::cur_betweenliner = Betweenliner(true, level, old);
     
     l = _left->resolve(m, ns, res.clear());
     
     Node::cur_betweenliner = old;
     return l;
   }
   
   case opOutline: {
     int level = 5;
     
     if (opt) {
       if (!NodeOptimizer::to_integer_constant(opt, m->module(), level))
	 error(*opt,"optional argument to outline must be const expression");
     }
     if (level < 0 || level > 10) {
       level = 0;
       error(*opt, "outline level must be between 0 and 10");
     }
     
     Betweenliner old = Node::cur_betweenliner;
     Node::cur_betweenliner = Betweenliner(false, level, old);
     
     l = _left->resolve(m, ns, res.clear());
     
     Node::cur_betweenliner = old;
     return l;
   }
   
   case opDeref: {
     if (l->simple_type()) {
       // This is the definition of a type `ptr-to X'.
       // MEMLEAK on types
       Type *ptr_type = l->type()->make_pointer(*l);
       if (!res.allow_type() && ptr_type) {
	 error(*this, "type `%t' in value context", ptr_type);
	 return 0;
       } else if (ptr_type) {
	 Literal ptr_literal(ptr_type, *l);
	 return new LiteralNode(ptr_literal);
       } else
	 return 0;
       
     } else
       return l->make_deref();
   }
   
   case opAddress:
    return resolve_address_of(l, landmark());
    
   case '!': {
     l = l->type_convert(bool_type, *this);
     if (l)
       return new UnaryNode(l, _op, bool_type, Node::cur_betweenliner,
			    landmark());
     else
       return 0;
   }
   
   case opPreIncr:
   case opPostIncr:
   case opPreDecr:
   case opPostDecr:
     {
       if (!l->lvalue()) {
	 error(*l, "not lvalue");
	 return 0;
       }
       if (!l->type()->cast_arithmetic() && !l->type()->cast_pointer()) {
	 error(*l, "can't use `%o' on type `%t'", (int)_op, l->type());
	 return 0;
       }
       Node *n = new UnaryNode(l->state_wrap(), _op, l->type(),
			       Node::cur_betweenliner, landmark());
       n->make_temporary();
       return n;
     }
   
   default:
    return finish_resolve_arith_unary(_op, l, l->type(), landmark());
     
  }
  
  assert(0);
  return 0;
}

Node *
LiteralExpr::resolve(ModuleNames *, Namespace *, ResolveArgs res) const
{
  if (_literal.type() == node_type)
    return _literal.vnode();
  if (_literal.is_type() && !res.allow_type()) {
    error(*this, "type `%t' in value context", _literal.type());
    return 0;
  }
  return new LiteralNode(_literal);
}

Node *
CodeExpr::resolve(ModuleNames *m, Namespace *namesp, ResolveArgs res) const
{
  _code->parse(m->module(), namesp, res.is_static());
  return new CodeNode(_code);
}

Node *
SelfExpr::resolve(ModuleNames *m, Namespace *, ResolveArgs res) const
{
  if (res.is_static()) {
    error(*this, "can't refer to `%s' in static context",
	  _super ? "super" : "self");
    return 0;
  } else if (_super) {
    Module *parent = m->module()->parent_class();
    return new SelfNode(m->header_module(parent->base_modnames()),
			true, *this);
  } else
    return new SelfNode(m, false, *this);
}


/*****
 * resolve_ctor
 **/

Node *
Expr::make_construction(Node *object, Node *tnode) const
{
  if (!tnode)
    // This shouldn't happen unless the user has made a mistake and we've had
    // to define a fake type for `object'.
    return object;
  
  if (ConstructorNode *ctor_node = tnode->cast_constructor()) {
    ctor_node->set_call_of(object);
    return ctor_node;
  }
  
  if (Module *mod = tnode->type()->cast_module()) {
    if (!tnode->feature("constructor"))
      error(*tnode, "`%m's constructor is hidden", mod);
    
    return ConstructorNode::make_implicit
      (object, mod, Node::cur_betweenliner, landmark());
  }
  
  return object;
}

Node *
Expr::resolve_ctor(ModuleNames *m, Namespace *ns) const
{
  Node *self = new SelfNode(m, landmark());
  return resolve_ctor(m, ns, self);
}

Node *
Expr::resolve_ctor(ModuleNames *m, Namespace *ns, Node *&self) const
{
  // This is called when we're done with the initial constructor part.
  // Set self to 0. Other resolve_ctor methods check if self is 0 to tell if
  // we're done with the initial constructor part.
  self = 0;
  // FIXME duplicated below
  Node *vtblize = new VtblNode(m->module(), 0, landmark());
  Node *real = resolve(m, ns, false);
  if (!real) return 0;
  return new SemistrictNode(vtblize, ',', real, real->type(), *this);
}

Node *
BinaryExpr::resolve_ctor(ModuleNames *m, Namespace *ns, Node *&self) const
{
  switch ((int)_op) {
    
   case ',': {
     Node *left = _left->resolve_ctor(m, ns, self);
     if (!left)
	 return 0;
     
     Node *right;
     // We have finished the constructor section iff self == 0
     if (!self)
       right = _right->resolve(m, ns, false);
     else
       right = _right->resolve_ctor(m, ns, self);
     
     if (!right)
	 return 0;
     return new SemistrictNode(left, ',', right, right->type(), *this);
   }
   
   case opCall: {
     // MEMLEAK
     Node *l = _left->resolve(m, ns, ResolveArgs(false).new_call());
     if (!l) return 0;
     
     if (FieldNode *fn = l->cast_field()) {
       if (fn->field()->is_ancestor())
	 return resolve_construction(m, ns, false, l, self);
     } else if (SelfNode *sn = l->cast_self())
       if (sn->super())
	 return resolve_construction(m, ns, false, l, self);
     
     return Expr::resolve_ctor(m, ns, self);
   }
   
   default:
    return Expr::resolve_ctor(m, ns, self);
    
  }
}

Node *
IdentExpr::resolve_ctor(ModuleNames *m, Namespace *ns, Node *&self) const
{
  Node *real = resolve(m, ns, false);
  if (FieldNode *fn = real->cast_field())
    if (fn->field()->is_ancestor() || fn->field()->is_slot())
      return make_construction(self, real);
  
  // FIXME MEMLEAK
  return Expr::resolve_ctor(m, ns, self);
}


/*****
 * make_implicit_rules
 **/

void
LetExpr::make_implicit_rules(Namespace *ns, Protomodule *defs) const
{
  Namespace letspace("letspace", ns, landmark());
  Feature placeholder("<placeholder>", defs->module(), landmark());
  
  Vector<Expr *> vec;
  _params->build_list(vec, true);
  for (int i = 0; i < vec.size(); i++) {
    Declaration decl3;
    vec[i]->declaration(&decl3);
    if (decl3.let_ok()) {
      letspace.def(decl3.name(), &placeholder);
      if (decl3.value())
	decl3.value()->make_implicit_rules(ns, defs);
    }
  }
  
  _body->make_implicit_rules(&letspace, defs);
}

void
BinaryExpr::make_implicit_rules(Namespace *ns, Protomodule *defs) const
{
  _left->make_implicit_rules(ns, defs);
  _right->make_implicit_rules(ns, defs);
  if (_optional)
    _optional->make_implicit_rules(ns, defs);
}

void
FunctionishExpr::make_implicit_rules(Namespace *ns, Protomodule *defs) const
{
  _args->make_implicit_rules(ns, defs);
}

void
MemberExpr::make_implicit_rules(Namespace *ns, Protomodule *defs) const
{
  if (_left)
    _left->make_implicit_rules(ns, defs);
  else if (!ns || !ns->search(_right))
    defs->resolve1_add_implicit_rule(_right, landmark());
}

void
UnaryExpr::make_implicit_rules(Namespace *ns, Protomodule *defs) const
{
  _left->make_implicit_rules(ns, defs);
  if (_optional)
    _optional->make_implicit_rules(ns, defs);
}

void
ConditionalExpr::make_implicit_rules(Namespace *ns, Protomodule *defs) const
{
  _test->make_implicit_rules(ns, defs);
  _yes->make_implicit_rules(ns, defs);
  _no->make_implicit_rules(ns, defs);
}

void
ListExpr::make_implicit_rules(Namespace *ns, Protomodule *defs) const
{
  for (int i = 0; i < _l.size(); i++)
    _l[i]->make_implicit_rules(ns, defs);
}

void
IdentExpr::make_implicit_rules(Namespace *ns, Protomodule *defs) const
{
  if ((!ns || !ns->search(_ident))
      && _ident != all_string && _ident != allstatic_string)
    defs->resolve1_add_implicit_rule(_ident, landmark());
}


/*****
 * clone
 **/

Expr *
Expr::clone(const Landmark &) const
{
  assert(0);
  return 0;
}

LiteralExpr *
LiteralExpr::clone(const Landmark &lm) const
{
  return new LiteralExpr(Literal(_literal, lm));
}

SelfExpr *
SelfExpr::clone(const Landmark &lm) const
{
  return new SelfExpr(_super, lm);
}


/*****
 * write
 **/

void
Expr::write(Writer& w) const
{
  w << "???";
}

void
LetExpr::write(Writer& w) const
{
  w << "let " << _params << " in " << _body << " end";
}

void
BinaryExpr::write(Writer &w) const
{
  w << "(" << _left << " " << _op << " " << _right << ")";
}

void
FunctionishExpr::write(Writer &w) const
{
  w << _op << _args;
}

void
MemberExpr::write(Writer &w) const
{
  if (_left) w << _left;
  w << _op << _right;
}

void
UnaryExpr::write(Writer &w) const
{
  w << "(" << _op << _left << ")";
}

void
ConditionalExpr::write(Writer &w) const
{
  w << "(" << _test << " ? " << _yes << " : " << _no << ")";
}

void
ListExpr::write(Writer &w) const
{
  w << "(";
  for (int i = 0; i < _l.size(); i++) {
    if (i) w << ", ";
    w << _l[i];
  }
  w << ")";
}

void
IdentExpr::write(Writer &w) const
{
  w << _ident;
}

void
LiteralExpr::write(Writer &w) const
{
  w << _literal;
}

void
SelfExpr::write(Writer &w) const
{
  w << (_super ? "super" : "self");
}
