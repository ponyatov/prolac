#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "idcapsule.hh"
// No C necessary, so we make this the instantiation file.

#include "module.hh"
template class IDCapsule<Module>;
template bool operator==(ModuleID, ModuleID);
template bool operator!=(ModuleID, ModuleID);

#include "field.hh"
template class IDCapsule<Field>;
template bool operator==(FieldID, FieldID);
template bool operator!=(FieldID, FieldID);

#include "rule.hh"
template class IDCapsule<Rule>;
template bool operator==(RuleID, RuleID);
template bool operator!=(RuleID, RuleID);
