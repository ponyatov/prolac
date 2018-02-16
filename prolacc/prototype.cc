#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "prototype.hh"
#include "error.hh"
#include "expr.hh"
#include "module.hh"
#include "node.hh"
#include "target.hh"
#include "field.hh"
#include "rule.hh"
#include "ruleset.hh"
#include "operator.hh"
#include "optimize.hh"
#include "writer.hh"
#include "location.hh"
#include "compiler.hh"
#include "program.hh"

Prototype::Prototype(PermString bn, Namespace *parentsp, const Landmark &lm)
  : Feature(bn, ModuleID(0), lm),
    _modnames(0), _module(0), _parent_space(parentsp),
    _resolve_level(0)
{
}

inline void
Prototype::set_modnames(ModuleNames *mn)
{
  _modnames = mn;
  _module = mn->module();
}

Prototype *
Prototype::install_fake_modnames()
{
  Protomodule *protom =
    new Protomodule(basename(), parent_space(), 0, landmark());
  set_modnames(protom->modnames());
  return protom->frobbed();
}

bool
Prototype::begin_resolve(int lev, bool *ok)
{
  if (_resolve_level >= lev) {
    *ok = true;
    return true;
  } else if (_resolve_level < 0) {
    // Only report circular dependency bugs on the first resolve level.
    if (_resolve_level == -1) {
      error(*this, "circular module dependency with `%P'", this);
      // install fake module to prevent segmentation faults
      install_fake_modnames();
    }
    *ok = false;
    return true;
  } else {
    _resolve_level = -lev;
    set_error_context("In module `%P':", this);
    return false;
  }
}

void
Prototype::end_resolve(int lev)
{
  _resolve_level = lev;
  reset_error_context();
}

void
Prototype::write(Writer &w) const
{
  if (_parent_space->parent())
    w << _parent_space << '.' << basename();
  else
    w << basename();
}


/*****
 * Protomodule
 **/


Protomodule::Protomodule(PermString n, Namespace *ns, Program *program,
			 const Landmark &lm)
  : Prototype(n, ns, lm),
    _program(program), _real_import_count(0),
    _implicit_ctor_parent(0), _ctor_rulei(-1),
    _after_frob(0)
{
  _pre_frobbed = new Protofrob(this, ns, lm);
  _frobbed = new Protofrob(_pre_frobbed, ns, lm);
  set_modnames(new ModuleNames(n, ns, this, lm));
  // OK to pass a null program if we're making a fake Protomodule
  if (program) program->register_protomodule(this);
}


void
Protomodule::def1(PermString n, Namespace *ns, Feature *f, const char *context)
{
  if (Feature *old = ns->find(n))
    if (old != f && old->origin() == module()) {
      if (Namespace *oldns = old->cast_namespace())
	if (Field *field = f->cast_field())
	  if (field->is_import()) {
	    // special error for import overriding namespace
	    error(*oldns, "can't override methods in import `%s'", n.c_str());
	    goto ok;
	  }
      if (n != inaccessible_string) {
	error(*f, "`%-n%s' redefined%s", ns, n.c_str(), context);
	error(*old, "previous definition was here");
      }
    }
  
 ok:
  ns->def(n, f);
}

void
Protomodule::def1(PermString n, Namespace *ns, Feature *f)
{
  def1(n, ns, f, "");
}

void
Protomodule::add_parent(PermString n, Expr *p)
{
  _parent_exprs.push_back(p);
  _parent_names.push_back(n);
  def1(n, modnames(), new Feature(n, module(), *p), " in module header");
}

void
Protomodule::add_import(PermString n, Expr *p)
{
  _import_exprs.push_back(p);
  _import_names.push_back(n);
  _real_import_count++;
  def1(n, modnames(), new Feature(n, module(), *p), " in module header");
}

Field *
Protomodule::add_import_prototype(Prototype *prototype, const Landmark &lm)
{
  prototype->resolve1();
  _imports.push_back(prototype);
  Field *f = Field::new_anonymous_import(module(), prototype->modnames(), lm);
  _import_fields.push_back(f);
  return f;
}


Rule *
Protomodule::add_rule(PermString name, Namespace *where,
		      const Landmark &landmark,
		      Expr *param, Expr *return_type, Expr *body)
{
  int ri = _rules.size();
  RuleRef *rref = new RuleRef(name, module(), ri, landmark);
  def1(where, rref);
  _rulerefs.push_back(rref);
  
  Rule *r = new Rule(name, module(), where, landmark);
  _rules.push_back(r);
  
  _rule_parameters.push_back(param);
  _rule_return_types.push_back(return_type);
  _rule_bodies.push_back(body);
  
  return r;
}

int
Protomodule::add_slot(PermString name, Namespace *where,
		      const Landmark &landmark, bool is_static,
		      Expr *type_expr)
{
  Module *mod = module();
  Field *f = Field::new_slot(mod, where, name, is_static, landmark);
  _slots.push_back(f);
  def1(where, f);
  
  int slot_num = _slot_types.size();
  _slot_types.push_back(type_expr);
  _slot_offsets.push_back((Expr *)0);
  return slot_num;
}

void
Protomodule::add_constructor(const Landmark &landmark,
			     Expr *param, Expr *body)
{
  if (_ctor_rulei >= 0) {
    error(landmark, "can only define 1 constructor");
    return;
  }
  
  Module *mod = module();
  int ri = _rules.size();
  RuleRef *rref = new RuleRef("constructor", mod, ri, landmark);
  def1(modnames(), rref);
  _rulerefs.push_back(rref);
  
  Rule *r = new Rule("constructor", mod, modnames(), landmark);
  _rules.push_back(r);
  
  _rule_parameters.push_back(param);
  Expr *e = new LiteralExpr(Literal(void_type, landmark));
  _rule_return_types.push_back(e);
  _rule_bodies.push_back(body);
  
  _ctor_rulei = ri;
}

void
Protomodule::add_exception(PermString name, Namespace *where,
			   const Landmark &landmark)
{
  Module *mod = module();
  Exception *ex = new Exception(name, landmark);
  //def1(where, ex);
  
  int ri = _rules.size();
  RuleRef *rref = new RuleRef(name, mod, ri, landmark);
  def1(where, rref);
  _rulerefs.push_back(rref);
  
  Rule *r = new Rule(name, mod, modnames(), landmark);
  r->make_exception(ex);
  _rules.push_back(r);
  
  _rule_parameters.push_back(0);
  _rule_return_types.push_back(0);
  _rule_bodies.push_back(0);
}


void
Protomodule::set_after_frob(Expr *e)
{
  assert(!_after_frob);
  _after_frob = e;
}


/*****
 * resolve
 **/

void
Protomodule::resolve0()
{
  ResolveArgs res_args = ResolveArgs::type_args();
  Namespace *psp = parent_space();
  
  // PARENTS
  for (int i = 0; i < _parent_exprs.size(); i++) {
    Node *node = _parent_exprs[i]->resolve(modnames(), psp, res_args);
    PrototypeNode *proto_node = (node ? node->cast_prototype() : 0);
    
    if (proto_node) {
      _parents.push_back(proto_node->prototype());
      // Don't need to define anything; we'll inherit its fields, including
      // its ancestor field with itself.
      // Then below, we def1() it.
    } else {
      if (node)	error(*_parent_exprs[i], "parent not module");
      def1(modnames(), new NodeRef(_parent_names[i], new LiteralNode(any_type, 0, *_parent_exprs[i])));
    }
  }
  
  // IMPORTS
  for (int i = 0; i < _import_exprs.size(); i++) {
    Node *node = _import_exprs[i]->resolve(modnames(), psp, res_args);
    PrototypeNode *proto_node = (node ? node->cast_prototype() : 0);
    
    if (proto_node) {
      _imports.push_back(proto_node->prototype());
      // Set the Field's import module later, after resolve1()ing the import.
      // Otherwise, the module would be 0 if we imported a Protofrob.
      Field *f = Field::new_import
	(module(), _import_names[i], *_import_exprs[i]);
      _import_fields.push_back(f);
    } else {
      if (node) error(*_import_exprs[i], "import not module");
      def1(modnames(), new NodeRef(_import_names[i], new LiteralNode(any_type, 0, *_import_exprs[i])));
    }
  }
}


void
Protomodule::def1_header(PermString n, Namespace *into, Field *field) const
{
  modnames()->def(n, field);
  module()->add_field(field);
}


void
Protomodule::resolve1_implicit_constructor()
{
  assert(_ctor_rulei < 0);
  Landmark lm = landmark();
  
  add_constructor(lm, 0, 0);
  
  Rule *my_ctor = _rules[_ctor_rulei];
  ModuleFrob inline_frob(opInline2, my_ctor, inlineSimpleConstructor, lm);
  _pre_frobbed->add_frob(inline_frob);
  
  Expr *ctor_expr = 0;
  for (int i = 0; i < _parents.size(); i++) {
    // FIXME: What to do if parent[i] is an ancestor of parent[j]?
    Rule *pctor = _parents[i]->module()->constructor_rule();
    
    if (pctor->param_count() > 0 && _implicit_ctor_parent) {
      error(*this, "can't generate implicit constructor");
      error(*this, "(more than one parent constructor takes parameters)");
      
    } else if (pctor->param_count() > 0) {
      _implicit_ctor_parent = pctor;
      
      ListExpr *paramlist = new ListExpr;
      for (int pnum = 0; pnum < pctor->param_count(); pnum++) {
	PermString pname = pctor->param_name(pnum);
	my_ctor->add_param(pname, lm);
	paramlist->append(new IdentExpr(pname, lm));
      }
      
      Expr *parent_expr = new IdentExpr(_parents[i]->basename(), landmark());
      Expr *pcexpr = new BinaryExpr(parent_expr, opCall, paramlist); 
      if (ctor_expr)
	ctor_expr = new BinaryExpr(ctor_expr, ',', pcexpr);
      else
	ctor_expr = pcexpr;
    }
  }
  
  if (!ctor_expr)
    ctor_expr = new LiteralExpr(Literal(bool_type, 0L, landmark()));
  _rule_bodies[_ctor_rulei] = ctor_expr;
}


void
Protomodule::resolve1_make_implicit_rules(int ri)
{
  Rule *rule = _rules[ri];
  if (_rule_parameters[ri]) {
    ListExpr *paramlist = _rule_parameters[ri]->cast_list();
    assert(paramlist);
    do_rule_parameters(rule, paramlist, false);
  }
  if (_rule_bodies[ri])
    _rule_bodies[ri]->make_implicit_rules(rule->namesp(), this);
}


void
Protomodule::resolve1_add_implicit_rule(PermString name,
					const Landmark &landmark)
{
  int ri = _rules.size();
  RuleRef *rref = new RuleRef(name, module(), ri, landmark);
  def1(modnames(), rref);
  _rulerefs.push_back(rref);
  
  Rule *rule = new Rule(name, module(), modnames(), landmark);
  rule->make_implicit();
  _rules.push_back(rule);
  
  // Hide & inline implicit rules by default.
  ModuleFrob inline_frob(opInline2, rule, inlineImplicitRule, landmark);
  _pre_frobbed->add_frob(inline_frob);
  ModuleFrob hide_frob(opHide, name, landmark);
  _pre_frobbed->add_frob(hide_frob);
}


bool
Protomodule::resolve1()
{
  bool ok = true;
  if (begin_resolve(1, &ok))
    return ok;
  
  resolve0();
  Module *mod = module();
  ModuleNames *modnam = modnames();
  
  for (int i = 0; i < _parents.size(); i++) {
    if (!_parents[i]->resolve1())
      ok = false;
    ModuleNames *p = _parents[i]->modnames();
    if (!p || !mod->resolve1_parent(p->module()))
      ok = false;
    assert(p->allow_ancestor());
    modnam->resolve1_parent(p);
  }
  mod->resolve1_self();
  
  // Set up our constructor now so its definition will be in the internal
  // namespace.
  if (_ctor_rulei < 0)
    resolve1_implicit_constructor();
  
  // Set up our namespace.
  // 1. Combine parents.
  Namespace *result_namesp = new Namespace(basename(), landmark());
  for (int i = 0; i < _parents.size(); i++)
    if (ModuleNames *p = _parents[i]->modnames())
      p->name_resolve_parent(result_namesp, this, p);
    else
      ok = false;
  
  // 2. Define parents and imports.
  for (int i = 0; i < _parents.size(); i++) {
    ModuleNames *mn = _parents[i]->modnames();
    Field *parent_field = Field::new_ancestor
      (mod, mn->basename(), mn, *_parents[i]);
    def1_header(_parent_names[i], modnam, parent_field);
  }
  for (int i = 0; i < _imports.size(); i++) {
    Field *import_field = _import_fields[i];
    def1_header(_import_names[i], modnam, import_field);
  }
  
  // 3. Combine our internal definitions.
  modnam->name_resolve_internal(result_namesp, this);
  modnam->change_concrete(result_namesp->concrete());
  
  // 4. Force the self-reference, but hide it to others.
  def1_header(mod->basename(), modnam, mod->self());
  _pre_frobbed->add_frob(ModuleFrob(opRedoSelfRef, "", *mod));
  
  // 5. Set rule namespaces to the new versions.
  for (int i = 0; i < _rules.size(); i++)
    _rules[i]->set_contextsp(_rulerefs[i]->contextsp());
  
  // 6. Make implicit rules.
  _real_rule_count = _rules.size();
  for (int i = 0; i < _real_rule_count; i++)
    resolve1_make_implicit_rules(i);
  
  // Handle the afterfrob.
  if (_after_frob)
    _after_frob->resolve(modnam, parent_space(), ResolveArgs::type_args());
  
  // for each rule R: decide whether it's an override or not, and put
  // it into the correct Ruleset.
  for (int rulei = 0; rulei < _rules.size(); rulei++)
    if (rulei != _ctor_rulei)
      if (!mod->resolve1_rule(_rules[rulei], _rulerefs[rulei]))
	ok = false;
  
  // Resolve the constructor.
  mod->resolve1_constructor(_rules[_ctor_rulei], _rulerefs[_ctor_rulei]);
  
  // Put each slot into the module.
  for (int sloti = 0; sloti < _slots.size(); sloti++)
    if (!mod->resolve1_field(_slots[sloti]))
      ok = false;
  
  end_resolve(1);
  return ok;
}


// Return true to say `don't replace old rule with new rule'.

bool
Protomodule::resolve1_rule_override(RuleRef *myrr, RuleRef *ancrr)
{
  if (myrr == ancrr)
    return false;
  
  Rule *ancrule = ancrr->rule_from(ancrr->origin());
  if (ancrule->constructor())
    return false;
  
  if (myrr->origin() == module()) {
    Rule *myrule = _rules[ myrr->ruleindex() ];
    if (ancrule->is_static()) {
      if (myrule->is_static())
	/* XXX warning? */;
      else
	error(*myrr, "dynamic method `%r' overrides static method", myrule);
      return false;
    } else {
      if (myrule->is_static())
	error(*myrule, "static method `%r' overrides dynamic method", myrule);
      else
	myrule->set_origin( ancrr->origin(), ancrr->ruleindex() );
      return true;
    }
    
  } else {
    error(*myrr, "inheriting `%s' from multiple places",
	  myrr->basename().c_str());
    return false;
  }
}


void
Protomodule::do_rule_parameters(Rule *rule, ListExpr *param, bool second_pass)
{
  ModuleNames *me = modnames();
  for (int paramnum = 0; paramnum < param->count(); paramnum++) {
    Expr *e = param->elt(paramnum);
    IdentExpr *name_expr = 0;
    Expr *type_expr = 0;
    
    if (BinaryExpr *bin = e->cast_binary())
      if (bin->op() != opTypeDecl) {
	if (!second_pass)
	  error(*bin, "syntax error (parameter not defined with `:>')");
      } else {
	name_expr = bin->left()->cast_ident();
	if (!name_expr && !second_pass)
	  error(*bin->left(), "syntax error (bad parameter name)");
	type_expr = bin->right();
      }
    
    if (!second_pass) {
      PermString name;
      if (name_expr)
	name = name_expr->ident();
      else
	name = permprintf("<param %d>", paramnum);
      
      rule->add_param(name, *e);
      
    } else {
      if (!type_expr)
	type_expr = e;
      
      Node *type_node =
	type_expr->resolve(me, rule->contextsp(), ResolveArgs::type_args());
      
      Type *param_type;
      if (type_node && type_node->simple_type())
	param_type = type_node->type();
      else {
	if (type_node) error(*e, "bad parameter type");
	param_type = any_type;
      }
      
      if (!param_type->allow_object()) {
	error(*e, "can't declare an object of type `%t'", param_type);
	param_type = any_type;
      }
      
      rule->set_param_type(paramnum, param_type);
    }
  }
}


bool
Protomodule::resolve2_rule(int ri)
{
  assert(0 <= ri && ri < _real_rule_count);
  Rule *rule = _rules[ri];
  if (rule->return_type()) return true;

  // Deal with the parameters.
  if (_rule_parameters[ri]) {
    ListExpr *param = _rule_parameters[ri]->cast_list();
    do_rule_parameters(rule, param, true);
  } else if (ri == _ctor_rulei && _implicit_ctor_parent)
    // Special case: implicit constructor parent has parameters.
    for (int j = 0; j < _implicit_ctor_parent->param_count(); j++)
      rule->set_param_type(j, _implicit_ctor_parent->param_type(j));
  
  // Deal with the return type.
  Type *return_type = void_type;
  if (_rule_return_types[ri]) {
    Node *n = _rule_return_types[ri]->resolve
      (modnames(), rule->contextsp(), ResolveArgs::type_args());
    if (n && n->simple_type())
      return_type = n->type();
    else
      error(*_rule_return_types[ri], "bad return type (assuming `void')");
  }
  rule->set_return_type(return_type);
  
  return true;
}


bool
Protomodule::resolve2_slot(int fi)
{
  Field *slot = _slots[fi];
  Namespace *namesp = modnames(); // FIXME bad namespace
  
  Node *n =
    _slot_types[fi]->resolve(modnames(), namesp, ResolveArgs::type_args());
  
  Type *slot_type;
  if (n && n->simple_type())
    slot_type = n->type();
  else {
    if (n)
      error(*_slot_types[fi], "bad %s type", slot->description().c_str());
    slot_type = any_type;
  }
  
  if (!slot_type->allow_object()) {
    error(*_slot_types[fi], "can't declare an object of type `%t'", slot_type);
    slot_type = any_type;
  }
  
  slot->set_type(slot_type);
  return true;
}


bool
Protomodule::resolve2()
{
  bool ok = true;
  Module *mod = module();
  if (begin_resolve(2, &ok))
    return ok;
  
  for (int i = 0; i < _parents.size(); i++) {
    if (!_parents[i]->resolve2())
      ok = false;
    Module *mom = _parents[i]->module();
    if (!mom || !mod->resolve2_parent(mom))
      ok = false;
  }
  
  // for each field F: resolve 1 the field; add F to the table.
  for (int i = 0; i < _imports.size(); i++) {
    if (!_imports[i]->resolve1())
      ok = false;
    ModuleNames *im = _imports[i]->modnames();
    if (im) {
      assert(im->allow_import());
      _import_fields[i]->set_type(im);
    } else
      ok = false;
  }
  
  // resolve the module's module map
  // mod->resolve2_module_map();
  _frobbed->resolve1();
  for (int i = 0; i < _parents.size(); i++)
    mod->insert_module_map(_parents[i]->modnames());
  for (int i = 0; i < _real_import_count; i++)
    mod->insert_module_map(_imports[i]->modnames());
  
  for (int sloti = 0; sloti < _slots.size(); sloti++)
    if (!resolve2_slot(sloti))
      ok = false;
  
  // for each rule R: resolve the type of the rule.
  for (int ri = 0; ri < _real_rule_count; ri++)
    if (!resolve2_rule(ri))
      ok = false;

  // resolve the frobbed modnames so that we can set our module correctly
  module()->set_default_modnames(_frobbed->modnames());
  
  end_resolve(2);
  return ok;
}


void
Protomodule::resolve3_make_implicit_path(Module *m, Vector<Module *> &parents)
{
  Vector<Field *> fields;
  m->grep_my_fields(fields);
  
  for (int i = 0; i < fields.size(); i++)
    if (fields[i]->origin() == m->actual()) {
      ModuleNames *modnam = fields[i]->modnames();
      if (!modnam)
	if (Type *t = fields[i]->type()->deref())
	  modnam = t->cast_modnames();
      
      if (modnam)
	if (fields[i]->is_ancestor()) {
	  if (!modnam->is_using_empty(true))
	    parents.push_back(modnam->cast_module());
	  
	} else if (!modnam->is_using_empty(false))
	  _implicit_path.push_back(fields[i]);
    }
}

bool
Protomodule::resolve3()
{
  bool ok = true;
  if (begin_resolve(3, &ok))
    return ok;
  
  for (int i = 0; i < _imports.size(); i++)
    _imports[i]->resolve2();
  
  // Make the breadth-first-search resolver for implicit rules. This way of
  // doing it is unfortunately wasteful in most cases; now that we're saving
  // the graph in an object, we can use the parents' graphs to create our own
  // -- but only if the parents haven't had any module operators applied in
  // the meantime. Since this is at least nontrivial to check, do it the slow
  // way.
  if (_real_rule_count < _rules.size()) {
    Vector<Module *> parents;
    parents.push_back(module());
    while (parents.size()) {
      Vector<Module *> nparents;
      for (int i = 0; i < parents.size(); i++)
	resolve3_make_implicit_path(parents[i], nparents);
      _implicit_epochs.push_back(_implicit_path.size());
      parents = nparents;
    }
  }
  
  end_resolve(3);
  return ok;
}


Node *
Protomodule::resolve4_constructor_body(Rule *rule, Expr *body_expr)
{
  Expr *false_expr = new LiteralExpr(Literal(bool_type, 0L, Landmark()));
  body_expr = new BinaryExpr(body_expr, ',', false_expr);
  Node *body = body_expr->resolve_ctor(modnames(), rule->namesp());
  
  if (body) {
    ParentFixer fixer(modnames(), landmark());
    for (int i = 0; i < _parents.size(); i++)
      fixer.add_parent(_parents[i]->module());
    body->optimize(&fixer);
    body = fixer.add_implicits(body);
  }
  
  return body;
}

bool
Protomodule::resolve4_rule(int ri)
{
  assert(0 <= ri && ri < _real_rule_count);
  ModuleNames *modnam = modnames();
  Rule *rule = _rules[ri];
  if (rule->compiled()) return true;
  
  rule->set_inlining(true);
  
  Node *body = 0;
  if (Expr *be = _rule_bodies[ri]) {
    if (rule->constructor())
      body = resolve4_constructor_body(rule, be);
    else
      body = be->resolve(modnam, rule->namesp(), rule->is_static());
  }
  if (body)
    body = body->type_convert(rule->return_type(), *rule);
  
  if (body) {
    // Now fix constructors so they are correct.
    ConstructorFixer fixer(rule);
    body = body->optimize(&fixer);
    
    // If there were any recursive calls, mark the tail recursions.
    if (fixer.recursion() && body->mark_tail_recursions(rule)) {
      TailRecursionFixer tail_fixer(rule);
      body = body->optimize(&tail_fixer);
      body = new LabelNode(body);
    }
  }
  
  rule->set_body(body);
  rule->set_inlining(false);
  return true;
}


bool
Protomodule::resolve4_implicit_rule(int ri)
{
  assert(_real_rule_count <= ri && ri < _rules.size());
  Rule *rule = _rules[ri];
  
  PermString name = rule->basename();
  Field *found = 0;
  ModuleNames *found_mod = 0;
  int found_count = 0;
  int epoch = 0;
  for (int i = 0; i < _implicit_path.size(); i++) {
    
    if (i == _implicit_epochs[epoch]) {
      if (found_count) break;
      else epoch++;
    }
    
    ModuleNames *modnam = _implicit_path[i]->modnames();
    // If it's not a ModuleNames itself, it's a pointer to a ModuleNames.
    if (!modnam)
      modnam = _implicit_path[i]->type()->deref()->cast_modnames();
    
    if (modnam->has_implicit_feature(name, _implicit_path[i]->is_static())) {
      if (found_count == 0) {
	found = _implicit_path[i];
	found_mod = modnam;
      } else {
	if (found_count == 1) {
	  error(*rule, "implicit method `%s' is ambiguous:", name.c_str());
	  error(*found, "  found it in `%f'", found);
	  found = 0;
	}
	error(*_implicit_path[i], "  found it in `%f'", _implicit_path[i]);
      }
      found_count++;
    }
  }
  
  if (!found) {
    static int context = 0;
    error(*rule, "`%s' undefined (first use this module)", name.c_str());
    if (!context) {
      error(*rule, "(The identifier was not declared in this module,");
      error(*rule, "and no matching implicit method could be found.)");
      context = 1;
    }
    rule->make_undefined_implicit();
    return true;
  }
  
  Feature *feat = found_mod->find(name);
  if (Exception *ex = feat->exception_value()) {
    // special handling for implicit exceptions
    rule->make_exception(ex);
    modnames()->set_inline_level(rule, inlineImplicitRule);
    return true;
  }
  
  RuleRef *rref = feat->find_ruleref();
  Rule *found_rule = found_mod->module()->find_rule(rref);
  found_rule->make_type();		// make the body if necessary
  assert(found_rule);
  
  if (!found_rule->is_static() && found->is_static()) {
    error(*rule, "found implicit dynamic `%s' method", name.c_str());
    error(*rule, "(must be static; found through %-f `%f')", found, found);
    return true;
  }
  
  int nparam = found_rule->param_count();
  
  Node *field;
  if (found->is_slot()) {
    field = new FieldNode(0, found, false, *rule);
    if (found->type()->cast_pointer())
      field = found->type()->make_deref(field);
  } else
    field = new FieldNode(0, found, true, *rule);
  
  Type *ret_type = modnames()->header_type(found_rule->return_type());
  CallNode *call =
    new CallNode(field, found_rule, ret_type, false, Betweenliner(), *rule);
  
  for (int i = 0; i < nparam; i++)
    call->add_param(new ParamNode(permprintf("P%d", i), i,
				  found_rule->param_type(i), *rule));
  
  for (int i = 0; i < nparam; i++) {
    rule->add_param(permprintf("P%d", i), *rule);
    rule->set_param_type(i, found_rule->param_type(i));
  }
  rule->set_return_type(found_rule->return_type());
  rule->set_body(call);
  rule->set_static(found_rule->is_static());
  modnames()->set_inline_level(rule, inlineImplicitRule);
  return true;
}

bool
Protomodule::resolve4_slot(int sloti)
{
  ModuleNames *modnam = modnames();
  Expr *offset_expr = _slot_offsets[sloti];
  if (!offset_expr) return true;
  
  // XXX should be in the slot's namespace, but we don't store that currently
  Node *offset_node = offset_expr->resolve(modnam, modnam, true);
  if (!offset_node) return false;
  
  int offset;
  if (NodeOptimizer::to_integer_constant(offset_node, module(), offset)) {
    _slots[sloti]->set_offset(offset);
    return true;
  } else
    return false;
}


bool
Protomodule::resolve4()
{
  bool ok = true;
  if (begin_resolve(4, &ok))
    return ok;

  // OK to skip resolve3() on parents and imports; only chance this hasn't
  // been done is if they're affected by module operators.
  for (int i = 0; i < _parents.size(); i++)
    _parents[i]->resolve4();
  
  // for each implicit rule: create its return type, parameter types, & body.
  // Must do before resolving real rules' bodies below, or we won't know the
  // right types for the implicit rules.
  for (int ri = _real_rule_count; ri < _rules.size(); ri++)
    if (!resolve4_implicit_rule(ri))
      ok = false;
  
  // now that we know implicit rules' types, check overriding types.
  module()->check_override_types();
  
  // for each rule R: resolve the body of the rule and its inline level,
  // and count its own personal exceptions.
  for (int ri = 0; ri < _real_rule_count; ri++)
    if (!resolve4_rule(ri))
      ok = false;
  
  // for each rule: if its return type is this module, then (now that we've
  // resolved all of the internal rules), reset it to the afterfrob type.
  if (modnames() != frobbed()->modnames())
    for (int i = 0; i < _real_rule_count; i++)
      if (_rules[i]->return_type() == modnames())
	_rules[i]->set_return_type(frobbed()->modnames());
  
  // for each slot: determine its offset, if the offset was given
  for (int i = 0; i < _slots.size(); i++)
    if (_slot_offsets[i] && !resolve4_slot(i))
      ok = false;
  
  end_resolve(4);
  return ok;
}


bool
Protomodule::resolve5_rule(int ri)
{
  Rule *rule = _rules[ri];
  if (rule->body()) {
    // First, try to turn the body into a static constant. MEMLEAK!
    Node *body = NodeOptimizer::to_static_constant(rule->body(), module());

    // Check to see that if rule is trivial (has few operations) and inline
    // it if it is.
    if (body->count_operations() <= inlineTrivialNodeCount) {
      ModuleFrob inline_frob(opInline2, rule, inlineTrivial, *rule);
      _pre_frobbed->add_frob(inline_frob);
      rule->set_body(body);
    } else if (rule->body()->count_operations() <= inlineTrivialNodeCount) {
      ModuleFrob inline_frob(opInline2, rule, inlineTrivial, *rule);
      _pre_frobbed->add_frob(inline_frob);
    }
  }
  return true;
}

bool
Protomodule::resolve5()
{
  // The penultimate step: resolve4 all parents and imports, and make rule
  // bodies a bit smaller. This step is needed to ensure that all Protofrobs
  // are resolved (even ones that were created in the module header).
  bool ok = true;
  if (begin_resolve(5, &ok))
    return ok;
  
  for (int i = 0; i < _parents.size(); i++)
    _parents[i]->resolve5();
  for (int i = 0; i < _imports.size(); i++)
    _imports[i]->resolve4();

  // copy the parent's inliners, since we are subtyping them.
  if (_parents.size())
    modnames()->copy_inliners(*_parents[0]->modnames());
  
  // for each rule R: make the rule a bit smaller if you can, and count its
  // own personal exceptions.
  for (int ri = 0; ri < _real_rule_count; ri++)
    if (!resolve5_rule(ri))
      ok = false;
  
  end_resolve(5);
  return ok;
}


void
Protomodule::resolve6(bool &changed)
{
  Module *mod = module();

  // Resolve5 all imports to get inlining information.
  for (int i = 0; i < _imports.size(); i++)
    _imports[i]->resolve5();
  
  // Diddle around with exceptions. Goal: find out which exceptions can reach
  // where.
  for (int ri = 0; ri < _real_rule_count; ri++) {
    Rule *rule = _rules[ri];
    if (!rule->body()) continue;
    
    ExceptionSet new_eset;
    ExceptionCounter comb(new_eset);
    rule->body()->optimize(&comb);
    
    Ruleset *rset = mod->find_ruleset(rule->origin());
    if (Rule *parent = rset->parent_rule(rule->ruleindex())) {
      new_eset += parent->all_exceptions();
      // also must propagate child's exceptions to parent
      if (parent->merge_exceptions(new_eset))
        changed = true;
    }
    
    if (rule->merge_exceptions(new_eset))
      changed = true;
  }
}


void
Protomodule::resolve7()
{
  // Report errors when exceptions can be thrown from non-void or -bool
  // returning methods.
  for (int ri = 0; ri < _real_rule_count; ri++)
    if (_rules[ri]->can_throw()) {
      Rule *r = _rules[ri];
      if (r->return_type() != void_type && r->return_type() != bool_type
	  && r->return_type() != any_type) {
	static int context = 0;
	error(*r, "method `%r :> %t' can throw exception `%e'",
	      r, r->return_type(), r->all_exceptions().element());
	ExceptionLocator loc(r->all_exceptions().element());
	r->body()->optimize(&loc);
	if (loc.context())
	  error(*r, "(exception thrown by %r)", loc.context());
	if (!context) {
	  error(*r, "(Only methods returning `void' or `bool' may throw exceptions.)");
	  context = 1;
	}
      }
    }
  
  // Resolve the frobbed version and copy our inliners from it. That way,
  // inside our module, we will pay attention to what the user said to inline
  // in the afterfrob.
  _frobbed->resolve5();
  modnames()->copy_inliners(*_frobbed->modnames());
}


void
Protomodule::resolve_rule(Rule *rule)
{
  //warning(*rule, "<doing %r>", rule);
  resolve1();
  resolve2();
  resolve3();
  for (int ri = 0; ri < _real_rule_count; ri++)
    if (_rules[ri] == rule) {
      resolve4_rule(ri);
      resolve5_rule(ri);
      return;
    }
  for (int ri = _real_rule_count; ri < _rules.size(); ri++)
    if (_rules[ri] == rule) {
      resolve4_implicit_rule(ri);
      return;
    }
}


void
Protomodule::resolve_rule_type(Rule *rule)
{
  int ri;
  for (ri = 0; ri < _rules.size(); ri++)
    if (_rules[ri] == rule)
      goto found;
  return;

 found:
  resolve1();
  if (ri < _real_rule_count)
    resolve2_rule(ri);
  else {
    //assert(0 && "can't resolve implicit rule types yet");
    resolve2();
    resolve3();
    resolve4_implicit_rule(ri);
  }
}


/*****
 * Protofrob
 **/

Protofrob::Protofrob(Prototype *p, Namespace *frobspace, const Landmark &lm)
  : Prototype(p->basename(), frobspace, lm), _actual(p)
{
}

bool
Protofrob::any_frobs() const
{
  for (int i = 0; i < _frobs.size(); i++)
    if (_frobs[i].kind() != opNullFrob)
      return true;
  return false;
}

bool
Protofrob::resolve1()
{
  bool ok = true;
  if (begin_resolve(1, &ok))
    return ok;
  
  ok = _actual->resolve1();
  
  if (_frobs.size()) {
    ModuleNames *modnam = new ModuleNames(*_actual->modnames(), this);
    set_modnames(modnam);
    
    for (int i = 0; i < _frobs.size(); i++)
      if (!_frobs[i].resolve1(modnam))
	ok = false;
  } else
    set_modnames(_actual->modnames());
  
  end_resolve(1);
  return ok;
}

bool
Protofrob::resolve2()
{
  return _actual->resolve2();
}

bool
Protofrob::resolve3()
{
  return _actual->resolve3();
}

bool
Protofrob::resolve4()
{
  return _actual->resolve4();
}

bool
Protofrob::resolve5()
{
  bool ok = true;
  if (begin_resolve(5, &ok))
    return ok;
  
  ok = _actual->resolve5();
  // Wait to do inlining until now. Why? We want to inline small rules by
  // default -- but we don't know whether a rule is small or not until after
  // its body has been static-constantized -- which happens in resolve5()!
  
  // Copy base inliners from the frobbed version.
  modnames()->copy_inliners(*_actual->modnames());

  // resolve inline frobs
  for (int i = 0; i < _frobs.size(); i++)
    if (!_frobs[i].resolve5(modnames(), parent_space()))
      ok = false;

  end_resolve(5);
  return ok;
}

void
Protofrob::resolve6(bool &changed)
{
  _actual->resolve6(changed);
}

void
Protofrob::resolve7()
{
  _actual->resolve7();
}


/*****
 * Protoequate
 **/

Protoequate::Protoequate(PermString n, Namespace *namesp, Expr *e,
			 const Landmark &landmark)
  : Prototype(n, namesp, landmark),
    // see expr.cc:resolve_mod_frob
    // for discussion of why NullFrob is necessary
    _expr(new BinaryExpr(e, opNullFrob, 0)), _proto(0)
{
}

bool
Protoequate::resolve1()
{
  bool ok = false;
  if (begin_resolve(1, &ok))
    return ok;
  
  Node *node = _expr->resolve(0, parent_space(), ResolveArgs::type_args());
  if (node)
    if (PrototypeNode *protonode = node->cast_prototype()) {
      // Force it to be a Protofrob
      Protofrob *pfrob = protonode->prototype()->cast_protofrob();
      _proto = pfrob;
      assert(pfrob);
      
      ok = pfrob->resolve1();
      
      set_modnames(pfrob->modnames());
      if (pfrob->any_frobs()) {
	modnames()->force_equation(basename(), parent_space());
	ModuleFrob frob(opRedoSelfRef, "", *this);
	frob.resolve1(modnames());
      } else if (pfrob->base_modnames() != modnames()) {
	// report error messages using the base modnames
	ModuleNames *omn = pfrob->base_modnames();
	modnames()->force_trivial_equation(basename(), parent_space(), omn);
      }
    }
  
  if (!_proto) {
    error(*this, "bad class equation `%E'", _expr);
    // add a fake module to prevent segmentation faults
    _proto = install_fake_modnames();
    ok = _proto->resolve1();
  }
  
  end_resolve(1);
  return ok;
}

bool
Protoequate::resolve2()
{
  return _proto->resolve2();
}

bool
Protoequate::resolve3()
{
  return _proto->resolve3();
}

bool
Protoequate::resolve4()
{
  return _proto->resolve4();
}

bool
Protoequate::resolve5()
{
  return _proto->resolve5();
}

void
Protoequate::resolve6(bool &changed)
{
  _proto->resolve6(changed);
}

void
Protoequate::resolve7()
{
  _proto->resolve7();
}


/*****
 * outer_modnames
 **/

ModuleNames *
Prototype::outer_modnames() const
{
  return modnames();
}

ModuleNames *
Protomodule::outer_modnames() const
{
  return _frobbed->outer_modnames();
}

/*****
 * grep_rules
 **/

void
Protomodule::grep_rules(Vector<Rule *> &rv) const
{
  for (int i = 0; i < _rules.size(); i++)
    rv.push_back(_rules[i]);
  if (_ctor_rulei < 0)
    rv.push_back(module()->constructor_rule());
}

void
Protofrob::grep_rules(Vector<Rule *> &rv) const
{
  _actual->grep_rules(rv);
}

void
Protoequate::grep_rules(Vector<Rule *> &rv) const
{
  if (_proto) _proto->grep_rules(rv);
}
