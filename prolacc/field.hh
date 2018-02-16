#ifndef FIELD_HH
#define FIELD_HH
#include "feature.hh"
#include "type.hh"
class Compiler;

class Field : public Feature {
  
  enum FieldKind {
    fkNothing,
    fkAncestor,
    fkImport,
    fkSlot,
  };
  
  FieldKind _kind;
  
  PermString _gen_name;
  
  Type *_type;
  bool _is_static;
  int _offset;
  
  static Field *the_no_field;
  
  Field(FieldKind, PermString, PermString, ModuleID,
	Type *, bool, const Landmark &);
  
 public:
  
  static Field *new_ancestor
    (ModuleID me, PermString, ModuleNames *ancestor, const Landmark &);
  static Field *new_import
    (ModuleID me, PermString, const Landmark &);
  static Field *new_anonymous_import
    (ModuleID me, Type *, const Landmark &);
  static Field *new_slot
    (ModuleID me, Namespace *, PermString, bool is_static, const Landmark &);
  
  static Field *no_field() { return the_no_field; }
  
  Type *type() const			{ return _type; }
  ModuleNames *modnames() const;
  ModuleID module() const;
  
  bool is_ancestor() const		{ return _kind == fkAncestor; }
  bool is_import() const		{ return _kind == fkImport; }
  bool is_slot() const			{ return _kind == fkSlot; }
  PermString description() const;
  
  bool lvalue() const			{ return is_slot(); }
  bool is_type() const			{ return is_ancestor()||is_import(); }
  bool is_static() const		{ return _is_static; }
  bool allow_static() const		{ return _is_static || is_ancestor(); }
  bool is_object() const		{ return !is_import(); }
  bool is_in_object() const		{ return !is_import() && !_is_static; }
  
  int size() const			{ return _type->size(); }
  int align() const			{ return _type->align(); }
  int offset() const			{ return _offset; }
  
  void set_type(Type *t)		{ _type = t; }
  void set_offset(int o)		{ _offset = o; }
  
  void write(Writer &) const;
  
  PermString compiled_name() const	{ return _gen_name; }
  void gen_value(Compiler *, Node *) const;

  Field *cast_field()			{ return this; }
  
};


inline ModuleNames *
Field::modnames() const
{
  return _type ? _type->cast_modnames() : 0;
}

inline ModuleID
Field::module() const
{
  return _type ? _type->cast_module() : 0;
}

inline Writer &
operator<<(Writer &w, const Field *f)
{
  f->write(w);
  return w;
}

#endif
