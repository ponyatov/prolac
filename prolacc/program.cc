#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "program.hh"
#include "prototype.hh"
#include "optimize.hh"
#include "compiler.hh"
#include "expr.hh"
#include "error.hh"
#include "codeblock.hh"


Program::Program()
  : Namespace(PermString(), Landmark())
{
  _error_prototype = new Protomodule("<any>", this, this, Landmark());
}

void
Program::define(Prototype *m)
{
  _protos.push_back(m);
}

void
Program::register_protomodule(Protomodule *pm)
{
  _protomodules.push_back(pm);
}

void
Program::resolve_names()
{
  for (int i = 0; i < _protos.size(); i++)
    _protos[i]->resolve1();
  
  for (int i = 0; i < _protos.size(); i++)
    _protos[i]->resolve2();
  
  for (int i = 0; i < _protos.size(); i++)
    _protos[i]->resolve3();
  
  for (int i = 0; i < _protos.size(); i++)
    _protos[i]->resolve4();

  _all_rules.clear();
  for (int i = 0; i < _protos.size(); i++)
    _protos[i]->grep_rules(_all_rules);

  // don't do the rest of resolve yet: receiver class analysis needs
  // unoptimized rules
}

Protomodule *
Program::find_protomodule(ModuleID module) const
{
  for (int i = 0; i < _protomodules.size(); i++)
    if (module == _protomodules[i]->module())
      return _protomodules[i]->cast_protomodule();
  return 0;
}

#if 0
void
Program::resolve_rule(Rule *r) const
{
  Protomodule *pm = find_protomodule(r->actual());
  assert(pm);
  pm->resolve_rule(r);
}
#endif

void
Program::add_literal_code(CodeBlock *cb)
{
  if (_protos.size())
    _post_literal_code.push_back(cb);
  else
    _pre_literal_code.push_back(cb);
}

void
Program::debug_print(HashMap<PermString, int> &debug_map, int all_debug)
{
  for (int i = 0; i < _protos.size(); i++) {
    Prototype *p = _protos[i];
    int dv = debug_map[p->basename()] | all_debug;
    if (dv & (dtNamespace | dtRuleset))
      p->modnames()->write_full(errwriter, dv & dtNamespace, dv & dtRuleset);
  }
}

void
Program::add_export(Expr *e, Namespace *cns)
{
  _exports.push_back(e);
  _export_namesp.push_back(cns);
}

void
Program::analyze_exports()
{
  // Mark exported rules
  for (int i = 0; i < _exports.size(); i++) {
    Vector<Expr *> list;
    _exports[i]->build_list(list, true);
    
    for (int j = 0; j < list.size(); j++) {
      ModuleNames *modnam = 0;
      Namespace *ns = _export_namesp[i];
      PermString name =
	list[j]->parse_name(ns, modnam, "export");
      ModuleID module = modnam ? modnam->module() : 0;
      
      if (!modnam)
	/* report error below */;
      
      else if (name == all_string) {
	// By default, use the module operators from this particular
	// ModuleNames.
	module->set_default_modnames(modnam);
	module->grep_my_rules(_export_rules);
	continue;		// skip error message
	
      } else if (Feature *f = ns->find(name)) {
	if (RuleRef *rref = f->find_ruleref()) {
	  Rule *rule = modnam->find_rule(rref);
	  _export_rules.push_back(rule);
	  continue;		// skip error message
	}

      } else {
	error(*list[j], "`%n.%s' undefined in export", ns, name.c_str());
	continue;		// skip error message
      }
      
      error(*list[j], "can't export `%E'", list[j]);
    }
  }
  
  // Receiver class analysis
  // We must do this here, before rules are likely to be extensively inlined.
  // The reason: receiver class analysis analyzes constructor calls. So it
  // depends on those constructor calls not having been inlined away!!
  for (int r = 0; r < _export_rules.size(); r++)
    _export_rules[r]->receiver_class_analysis();
  
  // Mark exported constructors as generating receiver classes
  for (int r = 0; r < _export_rules.size(); r++)
    if (_export_rules[r]->constructor()) {
      Module *m = _export_rules[r]->origin();
      m->mark_receiver_class(m);
    }
}

void
Program::resolve_code()
{
  for (int i = 0; i < _protos.size(); i++)
    _protos[i]->resolve5();
  
  while (1) {
    bool changed = false;
    for (int i = 0; i < _protos.size(); i++)
      _protos[i]->resolve6(changed);
    if (!changed) break;
  }
  
  for (int i = 0; i < _protos.size(); i++)
    _protos[i]->resolve7();

  // Find dynamic dispatches and mark rules for generation
  // Only do this after inlining, to avoid dead-code definitions of methods
  // that are only inlined
  for (int r = 0; r < _export_rules.size(); r++) {
    // Create a fake Node calling the rule and analyze that. That way we'll
    // mark this rule as a dynamic dispatch if necessary, even if no one in
    // the rest of the Prolac program calls it.
    CallNode call(0, _export_rules[r], void_type, false,
		  Betweenliner(), Landmark());
    ((Node &)call).callable_analysis();
    _export_rules[r]->mark_gen();
  }
  
  // Now that we know whether VTBLs contain any dynamic dispatches, we can do
  // class layout
  for (int i = 0; i < _protos.size(); i++)
    _protos[i]->module()->layout1();
  for (int i = 0; i < _protos.size(); i++)
    _protos[i]->module()->layout2();
}

void
Program::compile_exports(Compiler *c,
			 HashMap<PermString, int> &debug_map, int all_debug)
{
  // Generate VTBL 
  for (int i = 0; i < _protos.size(); i++)
    _protos[i]->module()->gen_vtbl_proto(c->out);
  
  // Output pre-literal code (in %{ %})
  for (int i = 0; i < _pre_literal_code.size(); i++)
    _pre_literal_code[i]->gen_outer(c);
  
  // All necessary code
  bool done = false;
  while (!done) {
    done = true;
    for (int i = 0; i < _all_rules.size(); i++) {
      Rule *rule = _all_rules[i];
      if (rule->need_gen()) {
	int dv = debug_map[rule->basename()] | all_debug;
	if (dv && !(dv & (dtNamespace | dtRuleset | dtNode)))
	  warning(*rule, "compiling `%r'", rule);
	c->compile(rule, dv & dtNode, dv & dtTarget, dv & dtLocation);
	done = false;
      }
    }
  }
  
  // %{ Post-literal code %}
  for (int i = 0; i < _post_literal_code.size(); i++)
    _post_literal_code[i]->gen_outer(c);
  
  // Generate VTBLs
  for (int i = 0; i < _protos.size(); i++)
    _protos[i]->module()->gen_vtbl(c->out, _protos[i]->outer_modnames());
}

void
Program::compile_structs(Writer &w)
{
  w << "/* structure definitions */\n";
  for (int i = 0; i < _protos.size(); i++)
    _protos[i]->modnames()->gen_create(w);

  // Prototypes for all rules
  w << "\n/* prototypes for exported methods */\n";
  for (int i = 0; i < _all_rules.size(); i++) {
    Rule *rule = _all_rules[i];
    rule->gen_prototype(w, true);
  }

  w << "\n";
}
