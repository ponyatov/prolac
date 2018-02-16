#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "field.hh"
#include "module.hh"
#include "compiler.hh"
#include "writer.hh"
#include "node.hh"


inline
Field::Field(FieldKind fk, PermString bn, PermString gn, ModuleID origin,
	     Type *type, bool is_static, const Landmark &landmark)
  : Feature(bn, origin, landmark),
    _kind(fk), _gen_name(gn),
    _type(type), _is_static(is_static), _offset(-1)
{
}


Field *
Field::new_ancestor(ModuleID origin, PermString name,
		    ModuleNames *ancestor, const Landmark &lm)
{
  assert(ancestor->module());
  return new Field(fkAncestor, name, name, origin, ancestor, false, lm);
}

Field *
Field::new_import(ModuleID origin, PermString n, const Landmark &lm)
{
  return new Field(fkImport, n, "/* BAD */", origin, 0, true, lm);
}

Field *
Field::new_anonymous_import(ModuleID origin, Type *t, const Landmark &lm)
{
  return new Field(fkImport, "", "/* BAD */", origin, t, true, lm);
}

Field *
Field::new_slot(ModuleID origin, Namespace *namesp, PermString n,
		bool is_static, const Landmark &lm)
{
  PermString namesp_name = (namesp ? namesp->gen_subname() : PermString());
  PermString gen = n;
  if (namesp_name)
    gen = permprintf("%p__%p", namesp_name.capsule(), gen.capsule());
  if (is_static)
    gen = permprintf("%p__%p", origin->gen_module_name().capsule(), gen.capsule());
  return new Field(fkSlot, n, gen, origin, 0, is_static, lm);
}


Field *Field::the_no_field =
new Field(fkNothing, PermString(), PermString(), 0, 0, false, Landmark());


PermString
Field::description() const
{
  switch (_kind) {
   case fkAncestor: return "ancestor";
   case fkImport: return "import";
   case fkSlot: return "slot";
   default: assert(0); return PermString();
  }
}


void
Field::write(Writer &w) const
{
  w << description();
  w << " " << basename();
  if (_kind == fkSlot)
    w << " :> " << _type;
}


/*****
 * gen
 **/

void
Field::gen_value(Compiler *c, Node *object) const
{  
#ifdef ANCESTOR_CAST
  if (is_ancestor()) {
    c->out << "(*(";
    type()->gen(c);
    c->out << " *)(&";
  }
#endif
  
  if (!is_static()) {
    GenContext object_gctx = (is_ancestor() ? gctxNone : gctxMember);
    c->out << "(";
    
    if (object)
      object_gctx = object->gen_value_real(c, object_gctx);
    else
      c->out << "This";
    
    c->out << ")";
    
    if (!is_ancestor())
      c->out << (object_gctx == gctxMember ? "->" : ".");
  }
  
  if (!is_ancestor())
    c->out << compiled_name();
#ifdef ANCESTOR_CAST
  else
    c->out << "))";
#endif
}
