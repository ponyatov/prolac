#ifndef TYPE_HH
#define TYPE_HH
#include "landmark.hh"
class ArithmeticType;
class PointerType;
class ArrayType;
class Namespace;
class Feature;
class Module;
class ModuleNames;
class Node;
class Writer;

class Type {
  
 public:
  
  Type()					{}
  
  Type *make_pointer() const		{ return make_pointer(Landmark()); }
  virtual Type *make_pointer(const Landmark &) const;
  
  virtual Type *deref() const			{ return 0; }
  
  virtual Feature *feature(PermString) const	{ return 0; }
  
  virtual bool conforms_to(const Type *) const;
  virtual bool bool_convertible() const		{ return false; }
  virtual bool same_type(const Type *) const;
  
  virtual bool allow_object() const;
  virtual bool allow_import() const;
  virtual bool allow_ancestor() const;
  virtual Module *vtbl_of() const		{ return 0; }
  
  virtual Node *convert_to(Node *, const Landmark &, bool = false) const;
  virtual Node *cast_to(Node *, const Landmark &) const;
  virtual Node *make_address(Node *, const Landmark &) const;
  virtual Node *make_deref(Node *) const;
  Node *convert_error(Node *, const Landmark &, bool) const;
  Node *cast_error(Node *, const Landmark &) const;
  
  virtual int size() const				{ return -1; }
  virtual int align() const				{ return -1; }
  
  virtual void write(Writer &) const;
  void gen(Writer &) const;
  void gen_object(Writer &, PermString) const;
  virtual int gen_parts(Writer &, PermString &) const;
  virtual void gen_prototype(Writer &) const;
  virtual void gen_create(Writer &) const;
  
  static Type *common_type(Type *, Type *, bool valued = false);
  
  virtual ArithmeticType *cast_arithmetic()	       	{ return 0; }
  virtual const ArithmeticType *cast_arithmetic() const	{ return 0; }
  virtual PointerType *cast_pointer()			{ return 0; }
  virtual const PointerType *cast_pointer() const	{ return 0; }
  virtual ArrayType *cast_array()			{ return 0; }
  virtual const ArrayType *cast_array() const		{ return 0; }
  virtual ModuleNames *cast_modnames()			{ return 0; }
  virtual const ModuleNames *cast_modnames() const	{ return 0; }
  virtual Module *cast_module() const			{ return 0; }
  
};


class BuiltinType: public Type {
  
  PermString _name;
  int _size;
  
 public:
  
  BuiltinType(PermString);
  BuiltinType(PermString, int);
  
  bool allow_object() const;
  bool bool_convertible() const;
  
  Node *convert_to(Node *, const Landmark &, bool = false) const;
  
  int size() const					{ return _size; }
  int align() const					{ return _size; }
  
  void write(Writer &) const;
  int gen_parts(Writer &, PermString &) const;
  
};

class VtblType: public BuiltinType {
  
  Module *_vtbl_of;
  
 public:
  
  VtblType(Module *);
  
  Module *vtbl_of() const			{ return _vtbl_of; }
  
};


class AnyType: public Type {
  
  PermString _name;
  PermString _gen_name;
  
 public:
  
  AnyType(PermString, PermString);
  
  bool bool_convertible() const			{ return true; }

  bool conforms_to(const Type *) const;
  Node *convert_to(Node *, const Landmark &, bool = false) const;
  
  Type *make_pointer(const Landmark &) const;
  Node *make_address(Node *, const Landmark &) const;
  Node *make_deref(Node *) const;
  
  int size() const				{ return 1; }
  int align() const				{ return 1; }
  
  void write(Writer &) const;
  int gen_parts(Writer &, PermString &) const;
  
};


class ArithmeticType: public Type {
  
  PermString _name;
  bool _unsigned;
  bool _seqint;
  int _size;
  
 public:
  
  ArithmeticType(PermString, int size, bool unsign, bool seqint);
  
  bool bool_convertible() const			{ return true; }
  
  Node *convert_to(Node *, const Landmark &, bool = false) const;
  Node *cast_to(Node *, const Landmark &) const;

  int size() const				{ return _size; }
  int align() const				{ return _size; }
  
  ArithmeticType *promote() const;
  static Type *binary_type(Type *, Type *);
  
  void write(Writer &) const;
  int gen_parts(Writer &, PermString &) const;
  
  ArithmeticType *cast_arithmetic()		{ return this; }
  const ArithmeticType *cast_arithmetic() const	{ return this; } 
  
};


class PointerType: public Type {
  
  Type *_deref;

  static int pointer_size;
  
 public:
  
  PointerType(const Type *t)			: _deref((Type *)t) {}
  
  Type *deref() const				{ return _deref; }
  
  bool conforms_to(const Type *) const;
  bool bool_convertible() const			{ return true; }
  bool same_type(const Type *) const;
  Module *vtbl_of() const			{ return _deref->vtbl_of(); }
  
  int size() const				{ return pointer_size; }
  int align() const				{ return pointer_size; }
  
  Node *convert_to(Node *, const Landmark &, bool = false) const;
  Node *cast_to(Node *, const Landmark &) const;
  Node *make_deref(Node *) const;
  
  void write(Writer &) const;
  int gen_parts(Writer &, PermString &) const;
  void gen_prototype(Writer &) const;
  
  PointerType *cast_pointer()			{ return this; }
  const PointerType *cast_pointer() const	{ return this; }
  
};


class ArrayType: public PointerType {
  
  int _size;
  
  ArrayType(Type *t, int b)			: PointerType(t), _size(b) {}
  
 public:
  
  static ArrayType *make(Type *, int, const Landmark &);

  Type *make_pointer(const Landmark &) const;
  
  bool allow_object() const;
  
  //bool conforms_to(const Type *) const;
  bool bool_convertible() const			{ return _size < 0; }
  //bool same_type(const Type *) const;
  
  //Node *convert_to(Node *, bool = false) const;
  //Node *cast_to(Node *) const;
  Node *make_address(Node *, const Landmark &) const;

  int nelem() const			{ return _size; }
  int size() const			{ return _size * deref()->size(); }
  int align() const			{ return deref()->align(); }
  
  void write(Writer &) const;
  int gen_parts(Writer &, PermString &) const;
  
  ArrayType *cast_array()			{ return this; }
  const ArrayType *cast_array() const		{ return this; }
  
};


// readonly
extern Type *namespace_type;
extern Type *node_type;
extern Type *code_type;
extern Type *any_type;

extern Type *void_type;
extern Type *bool_type;

extern PointerType *ptr_void_type;
extern PointerType *ptr_char_type;

extern ArithmeticType *char_type;
extern ArithmeticType *short_type;
extern ArithmeticType *int_type;
extern ArithmeticType *long_type;

extern ArithmeticType *uchar_type;
extern ArithmeticType *ushort_type;
extern ArithmeticType *uint_type;
extern ArithmeticType *ulong_type;

extern ArithmeticType *seqint_type;


inline bool
BuiltinType::bool_convertible() const
{
  return (Type *)this == bool_type;
}

inline Writer &
operator<<(Writer &w, Type *t)
{
  if (t) t->write(w);
  return w;
}

#endif
