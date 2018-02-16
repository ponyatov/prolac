#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "feature.hh"
#include "error.hh"
#include "module.hh"
#include "field.hh"
#include "writer.hh"
#include "node.hh"


RuleRef::RuleRef(PermString n, ModuleID origin, int ri, const Landmark &l)
  : Feature(n, origin, l), _ruleindex(ri), _contextsp(0)
{
}


void
Feature::write_full(Writer &w) const
{
  write(w);
}

void
Feature::write(Writer &w) const
{
  w << description() << " " << basename();
}


Rule *
RuleRef::rule_from(Module *m) const
{
  return m->find_rule(origin(), ruleindex());
}

void
RuleRef::write(Writer &w) const
{
  w << "method ";
  if (origin())
    w << origin();
  else
    w << "??";
  w << ":" << _ruleindex;
}

Exception *
RuleRef::exception_value() const
{
  return rule_from(origin())->exception_value();
}


NodeRef::NodeRef(PermString n, Node *node)
  : Feature(n, ModuleID(0), *node), _node(node)
{
}

void
NodeRef::write(Writer &w) const
{
  w << "node " << _node;
}
