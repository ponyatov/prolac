#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "modfrob.hh"
#include "module.hh"
#include "prototype.hh"
#include "expr.hh"
#include "node.hh"
#include "field.hh"
#include "error.hh"
#include "writer.hh"
#include "optimize.hh"
#include "globmatch.hh"
#include <cstring>


ModuleFrob::ModuleFrob(Operator op, Expr *e1, Expr *e2, const Landmark &lm)
  : _kind(op), _landmark(lm), _is1(isExpr), _is2(e2 ? isExpr : isBlank)
{
  assert(e1);
  _v1.e = e1;
  _v2.e = e2;
}

ModuleFrob::ModuleFrob(Operator op, PermString n, const Landmark &lm)
  : _kind(op), _landmark(lm), _is1(isPermString), _is2(isBlank)
{
  _v1.psc = n.capsule();
}

ModuleFrob::ModuleFrob(Operator op, PermString n, int level,
		       const Landmark &lm)
  : _kind(op), _landmark(lm), _is1(isPermString), _is2(isInt)
{
  _v1.psc = n.capsule();
  _v2.i = level;
}

ModuleFrob::ModuleFrob(Operator op, Rule *rule, int level, const Landmark &lm)
  : _kind(op), _landmark(lm), _is1(isRule), _is2(isInt)
{
  _v1.rule = rule;
  _v2.i = level;
}


PermString
ModuleFrob::resolve_identifier(Expr *e, const char *context)
{
  if (IdentExpr *ident = e->cast_ident())
    return ident->ident();
  error(*e, "argument to %s must be an identifier", context);
  return "<inaccessible>";
}


int
ModuleFrob::resolve_integer_constant(Expr *e, Namespace *namesp,
				     const char *context)
{
  Node *node = e->resolve(0, namesp, true);
  int val;
  if (node && NodeOptimizer::to_integer_constant(node, 0, val))
    return val;
  error(*e, "argument to %s must be an integer constant", context);
  return 0;
}


bool
ModuleFrob::hide_handle_match(Namespace *namesp, int feati,
			      Module *origin, ModuleNames *for_using,
			      const char *after) const
{
  Feature *feat = namesp->feat(feati);
  
  // if hiding an ancestor's features, check that this belongs to them
  if (origin && feat->origin() != origin)
    return false;
  
  // if hiding an ancestor, switch to another hide_find_matches
  if (Field *field = feat->cast_field())
    if (field->is_ancestor()) {
      Module *new_origin = field->module();
      if (new_origin != origin && after) // don't get stuck!
	return hide_find_matches(namesp, after, new_origin, for_using);
      else
	after = 0;
    }
  
  // endgame: hiding a feature
  if (!after) {
    PermString name = namesp->featname(feati);
    namesp->undef(name);
    if (namesp == for_using)
      for_using->set_using(name, false);
    return true;
  }
  
  // better be hiding features belonging to a namespace
  namesp = feat->cast_namespace();
  if (!namesp)
    return false;
  namesp->make_unique();
  return hide_find_matches(namesp, after, origin, for_using);
}

bool
ModuleFrob::hide_whole_namespace(Namespace *namesp, ModuleNames *for_using) const
{
  if (namesp == for_using) {
    // hide everything
    for_using->clear();
    for_using->clear_using(false);
  } else {
    // hide a particular namespace
    Namespace *parent = namesp->parent();
    PermString name = parent->find_name(namesp);
    parent->undef(name);
  }
  return true;
}

bool
ModuleFrob::hide_find_matches(Namespace *namesp, const char *pattern,
			      Module *origin, ModuleNames *for_using) const
{
  bool any = false;
  
  // divide `pattern' into before & after sections
  char *before_free = 0;
  const char *before = pattern, *after = 0;
  if (char *s = strchr(pattern, '.')) {
    before = before_free = new char[s - pattern + 1];
    memcpy(before_free, pattern, s - pattern);
    before_free[s - pattern] = 0;
    after = (s[1] ? s + 1 : 0);
  }
  
  // check for `*/all': hide all a namespace's features
  if (strcmp(before, "all") == 0)
    before = "*";
  if (!origin && !after && strcmp(before, "*") == 0) {
    any = hide_whole_namespace(namesp, for_using);
    goto done;
  }
  
  // check for not-a-pattern-at-all
  if (strpbrk(before, "*?[\\") == 0) {
    if (int feati = namesp->find_index(before))
      any |= hide_handle_match(namesp, feati, origin, for_using, after);
  } else
    for (int i = 1; i < namesp->featcount(); i++) {
      PermString name = namesp->featname(i);
      if (glob_match(before, name.c_str()))
	any |= hide_handle_match(namesp, i, origin, for_using, after);
    }
  
 done:
  if (before_free) delete[] before_free;
  return any;
}

bool
ModuleFrob::do_hide(ModuleNames *mn) const
{
  assert((_is1 == isExpr || _is1 == isPermString) && _is2 == isBlank);
  mn->make_unique();
  
  // check for `hide "pattern"'
  const char *pattern;
  if (_is1 == isExpr && _v1.e->cast_literal()
      && _v1.e->cast_literal()->type() == ptr_char_type)
    pattern = _v1.e->cast_literal()->vstring().c_str();
  else if (_is1 == isExpr)
    pattern = _v1.e->create_name_text("hide").c_str();
  else /* _is1 == isPermString */
    pattern = PermString::decapsule(_v1.psc).c_str();
  if (!pattern)
    return false;

  bool any = hide_find_matches(mn, pattern, 0, mn);
  if (!any)
    warning(*this, "no matches for hide pattern `%s'", pattern);
  return any;
}


Namespace *
ModuleFrob::copy_namespace_parents(Namespace *from, Namespace *into) const
{
  if (from->parent()) {
    into = copy_namespace_parents(from->parent(), into);
    if (!into) return 0;
    
    PermString from_name = from->parent()->find_name(from);
    if (Feature *f = into->find(from_name)) {
      if (Namespace *namesp = f->cast_namespace()) {
	namesp->make_unique();
	return namesp;
      } else {
	error(*this, "bad to namespace in frob");
	return 0;
      }
    } else {
      Namespace *namesp = new Namespace(from_name, into, *this);
      into->def(from_name, namesp);
      return namesp;
    }
    
  } else
    return into;
}


bool
ModuleFrob::do_show(ModuleNames *mn, bool is_rename) const
{
  assert(_is1 == isExpr && (_is2 == isExpr || _is2 == isBlank));
  mn->make_unique();
  const char *message = is_rename ? "rename" : "show";
  
  ModuleNames *e_modnam = 0;
  Namespace *e_namesp = is_rename ? mn : mn->base_modnames();
  PermString e_name = _v1.e->parse_name(e_namesp, e_modnam, message);
  Feature *e_feature;
  
  if (!e_name)
    return false;
  else if (is_rename && e_modnam)
    return error(*this, "can't rename ancestors' features");
  else if (e_name == all_string)
    return error(*this, "unimplemented: %s all", message);
  else if (!(e_feature = e_namesp->find(e_name)))
    return error(*this, "`%s' undefined in %s", e_name.c_str(), message);
  else if (Field *e_field = e_feature->cast_field()) {
    if (e_field->is_ancestor())
      return error(*this, "can't %s ancestor `%s'", message, e_name.c_str());
  }
  
  PermString new_name;
  Namespace *new_namesp = mn;
  if (_is2 == isExpr) {
    e_modnam = 0;
    new_name = _v2.e->parse_name(new_namesp, e_modnam, message,
				 Expr::parseUniqueify | Expr::parseCreate);
    if (e_modnam)
      return error(*this, "can't %s into an ancestor", message);
  } else if (is_rename)
    return error(*this, "syntax error in rename");
  else {
    new_namesp = copy_namespace_parents(e_namesp, mn);
    new_name = e_name;
  }

  // Look for the new name.
  if (!new_namesp || !new_name)
    return false;
  
  else if (e_name == constructor_string || new_name == constructor_string) {
    if (e_name != constructor_string
	|| new_name != constructor_string
	|| e_namesp != mn->base_modnames()
	|| new_namesp != mn)
      return error(*this, "%sing constructor improperly", message);
    
  } else if (!Expr::name_ok(new_name, *this, message))
    return false;
  
  else if (Feature *f = new_namesp->find(new_name))
    if (f != e_feature)
      return error(*this, "`%s' already defined in %s",
		   new_name.c_str(), message);
  
  // Actually do the renaming.
  if (is_rename)
    e_namesp->undef(e_name);
  new_namesp->def(new_name, e_feature);
  
  return true;
}


bool
ModuleFrob::do_redo_self_ref(ModuleNames *mn) const
{
  // make new field
  Field *self =
    Field::new_ancestor(mn->module(), mn->basename(), mn, mn->landmark());
  
  // replace old self-references.

  // XXX what if it's squirreled away in a nested namespace? -- currently we
  // don't allow anyone to create that situation.
  for (int i = 1; i < mn->featcount(); i++) {
    Feature *f = mn->feat(i);
    if (Field *field = f->cast_field()) {
      if (field->is_ancestor() && field->module() == mn->module())
	mn->def(mn->featname(i), self);
    }
  }
  
  // add new self-reference
  if (Feature *feat = mn->find(mn->basename())) {
    if (Field *field = feat->cast_field())
      if (field->is_ancestor() && field->module() == mn->module())
	return true;
    error(*this, "`%n.%s' already defined", mn, mn->basename().c_str());
  }
  mn->def(mn->basename(), self);
  return true;
}


bool
ModuleFrob::do_using(ModuleNames *mn, bool isusing) const
{
  assert(_is1 == isExpr && _is2 == isBlank);
  PermString name = resolve_identifier(_v1.e, "using");
  
  if (name == all_string)
    mn->clear_using(isusing);
  
  else if (name == allstatic_string) {
    if (!isusing) {
      warning(*this, "`notusing allstatic' changed to `notusing all'");
      mn->clear_using(false);
    } else
      mn->set_using_all_static();
    
  } else {
    Feature *f = mn->find(name);
    if (!f)
      warning(*this, "no feature `%s' to %suse", name.c_str(), isusing?"":"un");
    else
      mn->set_using(name, isusing);
  }
  
  return true;
}


void
ModuleFrob::inline_namespace(ModuleNames *mn, Namespace *namesp,
			     Vector<Rule *> &rule_vec) const
{
  for (int i = 1; i < namesp->featcount(); i++) {
    Feature *f = namesp->feat(i);
    if (RuleRef *rref = f->cast_ruleref()) {
      Rule *rule = mn->find_rule(rref);
      assert(rule);
      rule_vec.push_back(rule);
      
    } else if (Namespace *subnamesp = f->cast_namespace())
      inline_namespace(mn, subnamesp, rule_vec);
  }
}


bool
ModuleFrob::do_inline_1(ModuleNames *mn)
{
  if (_is1 == isRule) return true;
  assert(_is1 == isExpr || _is1 == isPermString);
  
  ModuleNames *e_modnam = 0;
  Namespace *e_namesp = mn;
  PermString e_name;
  if (_is1 == isExpr)
    e_name = _v1.e->parse_name(e_namesp, e_modnam, "inline");
  else
    e_name = PermString::decapsule(_v1.psc);
  
  if (!e_name) {
    _is1 = isBlank;
    return false;
  }
  
  else if (e_name == all_string) {
    if (!e_modnam && e_namesp == mn) {
      // inline everything
      _is1 = isModule;
      _v1.module = 0;
      
    } else if (e_namesp == e_modnam) {
      // inline a particular module's rules
      _is1 = isModule;
      _v1.module = e_modnam->module();
      
    } else {
      // inline a particular namespace's rules (and its subnamespace's rules)
      Vector<Rule *> *rule_vec = new Vector<Rule *>;
      inline_namespace(mn, e_namesp, *rule_vec);
      if (rule_vec->size() == 0)
	error(*this, "nothing in that namespace to inline");
      _is1 = isRuleVec;
      _v1.rule_vec = rule_vec;
    }
    
    return true;
    
  } else if (Feature *feat = e_namesp->find(e_name))
    if (RuleRef *rref = feat->find_ruleref()) {
      Rule *rule = mn->find_rule(rref);
      assert(rule);
      _is1 = isRule;
      _v1.rule = rule;
      return true;
    }
  
  error(*this, "no `%s' method to inline", e_name.c_str());
  _is1 = isBlank;
  return false;
}


bool
ModuleFrob::resolve1(ModuleNames *mn)
{
  switch ((int)_kind) {

   case opNullFrob:
    return true;
    
   case opHide:
    return do_hide(mn);
    
   case opShow:
    return do_show(mn, false);

   case opRedoSelfRef:
    return do_redo_self_ref(mn);
    
   case opRename:
    return do_show(mn, true);
    
   case opUsing:
    return do_using(mn, true);
    
   case opNotusing:
    return do_using(mn, false);
    
   case opNoinline2:
   case opDefaultinline2:
   case opInline2:
   case opPathinline2:
    return do_inline_1(mn);
    
   default:
    assert(0 && "compiler bug: bad module frob");
    break;
    
  }
  return true;
}


bool
ModuleFrob::do_inline_5(ModuleNames *mn, Namespace *namesp) const
{
  if (_is1 == isBlank) return false;
  assert((_is1 == isModule || _is1 == isRule || _is1 == isRuleVec)
	 && (_is2 == isExpr || _is2 == isInt || _is2 == isBlank));
  
  int level;
  if (_is2 == isExpr)
    level = resolve_integer_constant(_v2.e, namesp, "inline");
  else if (_is2 == isInt)
    level = _v2.i;
  else
    switch ((int)_kind) {
     case opNoinline2: level = inlineNo; break;
     case opDefaultinline2: level = inlineDefault; break;
     case opInline2: level = inlineYes; break;
     case opPathinline2: level = inlinePath; break;
     default: assert(0); level = 0; break;
    }
  
  if (_is1 == isRule)
    mn->set_inline_level(_v1.rule, level);
  
  else if (_is1 == isRuleVec) {
    // inline a list of rules
    Vector<Rule *> &rule_vec = *_v1.rule_vec;
    for (int i = 0; i < rule_vec.size(); i++)
      mn->set_inline_level(rule_vec[i], level);
    
  } else if (_v1.module)
    // inline a particular module's rules
    mn->clear_inline(_v1.module, level);
  
  else {
    // inline everything
    int eacher = 0;
    Module *m = mn->module();
    ModuleID ancestor;
    while (m->each_ancestor(eacher, ancestor))
      mn->clear_inline(ancestor, level);
  }
  
  return true;
}


bool
ModuleFrob::resolve5(ModuleNames *mn, Namespace *namesp) const
{
  switch ((int)_kind) {
    
   case opNoinline2:
   case opDefaultinline2:
   case opInline2:
   case opPathinline2:
    return do_inline_5(mn, namesp);

   default:
    return true;

  }
}


bool
ModuleFrob::full_resolve(ModuleNames *mn, Namespace *namesp)
{
  return resolve1(mn) && resolve5(mn, namesp);
}
