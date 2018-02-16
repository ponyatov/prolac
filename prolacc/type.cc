#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "type.hh"
#include "error.hh"
#include "node.hh"
#include "module.hh"

int PointerType::pointer_size = 4;
PermString::Initializer permstr_initializer;


/*****
 * constructors
 **/

BuiltinType::BuiltinType(PermString n)
  : _name(n), _size(-1)
{
}

BuiltinType::BuiltinType(PermString n, int size)
  : _name(n), _size(size)
{
}

VtblType::VtblType(Module *m)
  : BuiltinType(permprintf("struct __vtbl_%p",
			   m->gen_module_name().capsule())),
    _vtbl_of(m)
{
}

AnyType::AnyType(PermString n, PermString gen_name)
  : _name(n), _gen_name(gen_name)
{
}

ArithmeticType::ArithmeticType(PermString n, int size, bool unsig, bool seqint)
  : _name(n), _unsigned(unsig), _seqint(seqint), _size(size)
{
}

ArrayType *
ArrayType::make(Type *t, int size, const Landmark &landmark)
{
  if (ArrayType *t_arr = t->cast_array()) {
    if (t_arr->_size < 0) {
      error(landmark, "nested array dimensions unknown");
      return 0;
    }
  }
  return new ArrayType(t, size);
}


Type *
Type::make_pointer(const Landmark &) const
{
  return new PointerType(this);
}

Type *
AnyType::make_pointer(const Landmark &) const
{
  // taking a pointer of any_type gives you any_type
  return (Type *)this;
}

Type *
ArrayType::make_pointer(const Landmark &landmark) const
{
  error(landmark, "pointer-to-array types do not exist");
  return 0;
}


/*****
 * conforms_to
 **/

bool
Type::conforms_to(const Type *t) const
{
  return same_type(t);
}

bool
PointerType::conforms_to(const Type *t2) const
{
  if (const PointerType *ptr2 = t2->cast_pointer()) {
    if (Module *m1 = _deref->cast_module())
      if (Module *m2 = ptr2->_deref->cast_module())
	return m1->has_ancestor(m2);
    return same_type(t2);
  } else
    return false;
}

bool
AnyType::conforms_to(const Type *t2) const
{
  return true;
}


/*****
 * same_type
 **/

bool
Type::same_type(const Type *t) const
{
  return t == this;
}

bool
PointerType::same_type(const Type *t) const
{
  if (const PointerType *ptr = t->cast_pointer())
    return _deref->same_type(ptr->_deref);
  else
    return false;
}


/*****
 * allow_*
 **/

bool
Type::allow_object() const
{
  return true;
}

bool
Type::allow_import() const
{
  return false;
}

bool
Type::allow_ancestor() const
{
  return false;
}

bool
BuiltinType::allow_object() const
{
  // bool is the only `real' builtin type.
  return (Type *)this == bool_type;
}

bool
ArrayType::allow_object() const
{
  // can't declare arrays with empty dimensions (unlike C)
  return _size >= 0;
}


/*****
 * convert_to
 **/

Node *
Type::convert_to(Node *n, const Landmark &lm, bool) const
{
  error(lm, "strange type to convert");
  return n;
}

Node *
Type::convert_error(Node *n, const Landmark &lm, bool cast) const
{
  if (cast)
    return 0;
  else {
    error(lm, "can't convert `%t' to `%t'", n->type(), this);
    return n;
  }
}

Node *
BuiltinType::convert_to(Node *n, const Landmark &lm, bool cast) const
{
  const Type *from = n->type();
  if (from == this || from == any_type)
    return n->make_cast(this, lm);
  
  // Allow all conversions to void
  if ((Type *)this == void_type)
    return n->make_cast(this, lm);
  
  if ((Type *)this == bool_type) {
    // Allow conversions from arithmetic -> bool and pointer -> bool
    if (from->cast_arithmetic())
      return n->make_cast(this, lm);
    else if (const PointerType *ptr_from = from->cast_pointer()) {
      Literal null_lit((Type *)ptr_from, 0L, lm);
      Node *null_node = new LiteralNode(null_lit);
      return new BinaryNode(n, opNotEq, null_node, bool_type, lm);
    }
  }
  
  return convert_error(n, lm, cast);
}

Node *
AnyType::convert_to(Node *n, const Landmark &lm, bool cast) const
{
  return n->make_cast(this, lm);
}

Node *
ArithmeticType::convert_to(Node *n, const Landmark &lm, bool cast) const
{
  const Type *from = n->type();
  if (from == this || from == any_type)
    return n->make_cast(this, lm);
  
  if (from->cast_arithmetic()) {
    // FIXME more complicated processing
    return n->make_cast(this, lm);
  }
  
  return convert_error(n, lm, cast);
}

Node *
PointerType::convert_to(Node *n, const Landmark &lm, bool cast) const
{
  const Type *from = n->type();
  if (from == this || from == any_type)
    return n->make_cast(this, lm);
  
  if (const PointerType *ptr_from = from->cast_pointer()) {
    // Handle pointers to modules.
    // XXX:
    // If casting (not converting), handle these BEFORE checking for same type.
    // That'll allow the user to club certain errors into submission
    // until we figure out how to fix the errors themselves.
    if (ModuleNames *mod = _deref->cast_modnames())
      if (ModuleNames *mod_from = ptr_from->_deref->cast_modnames())
	return mod->convert_pointer_to(n, mod_from, lm, cast);
    
    // Handle two pointer types to the same type. Maybe we should uniqueify
    // pointer types?
    if (_deref == ptr_from->_deref || _deref->same_type(ptr_from->_deref))
      return n;
    
    // Handle *void.
    if (_deref == void_type || ptr_from->_deref == void_type)
      return n->make_cast(this, lm);
  }
  
  // Handle null pointers.
  else if (from->cast_arithmetic())
    if (LiteralNode *lit = n->cast_literal())
      if (lit->vlong() == 0) {
	Literal null_lit((Type *)this, 0L, lm);
	return new LiteralNode(null_lit);
      }
  
  return convert_error(n, lm, cast);
}


/*****
 * cast_to
 **/

Node *
Type::cast_to(Node *n, const Landmark &lm) const
{
  return convert_to(n, lm);
}

Node *
Type::cast_error(Node *n, const Landmark &lm) const
{
  error(lm, "can't cast `%t' to `%t'", n->type(), this);
  return n;
}

Node *
ArithmeticType::cast_to(Node *n, const Landmark &lm) const
{
  if (Node *result = convert_to(n, lm, true))
    return result;
  
  // Allow bool -> arithmetic
  Type *from = n->type();
  if (from == bool_type)
    return n->make_cast(this, lm);
  
  return cast_error(n, lm);
}

Node *
PointerType::cast_to(Node *n, const Landmark &lm) const
{
  const Type *from = n->type();
  if (const PointerType *ptr_from = from->cast_pointer()) {
    // Handle pointers to modules.
    if (ModuleNames *mod = _deref->cast_modnames())
      if (ModuleNames *mod_from = ptr_from->_deref->cast_modnames())
	return mod->cast_pointer_to(n, mod_from, lm);
  }
  
  if (Node *result = convert_to(n, lm, true))
    return result;
  
  return cast_error(n, lm);
}

/*
Node *
PointerType::cast_to(Node *n) const
{
  if (Node *result = convert_to(n, true))
    return result;
  
  // Allow any pointer cast
  Type *from = n->type();
  if (PointerType *ptr_from = from->cast_pointer())
    return n->make_cast(this);
  
  return cast_error(n);
}
*/

/*****
 * make_deref, make_address
 **/

Node *
Type::make_deref(Node *n) const
{
  error(*n, "can't dereference object of type `%t'", this);
  return 0;
}

Node *
AnyType::make_deref(Node *n) const
{
  // always OK to deref a node of type any. Just return the node itself
  return n;
}

Node *
PointerType::make_deref(Node *n) const
{
  if (_deref == void_type) {
    error(*n, "can't dereference a `*void' pointer");
    return 0;
  } else
    return new UnaryNode(n, opDeref, _deref, n->betweenliner(), *n);
}


Node *
Type::make_address(Node *n, const Landmark &landmark) const
{
  if (!n->lvalue()) {
    error(*n, "not lvalue");
    return 0;
  }
  
  if (Type *ptr_type = make_pointer(landmark))
    return new UnaryNode(n, opAddress, ptr_type,
			 Node::cur_betweenliner, landmark);
  else
    return 0;
}

Node *
AnyType::make_address(Node *n, const Landmark &) const
{
  // Always OK to take the address of a node of type any. Just return the node.
  return n;
}

Node *
ArrayType::make_address(Node *n, const Landmark &) const
{
  // Can't take the address of the array.
  return n;
}


/*****
 * write
 **/

void
Type::write(Writer &w) const
{
  w << "<odd type>";
}

void
BuiltinType::write(Writer &w) const
{
  w << _name;
}

void
AnyType::write(Writer &w) const
{
  w << _name;
}

void
ArithmeticType::write(Writer &w) const
{
  w << _name;
}

void
PointerType::write(Writer &w) const
{
  w << "*" << _deref;
}

void
ArrayType::write(Writer &w) const
{
  w << deref() << "[";
  if (_size >= 0) w << _size;
  w << "]";
}


/*****
 * gen
 **/

int
Type::gen_parts(Writer &, PermString &) const
{
  assert(0 && "attempt to gen odd type");
  return -1;
}

int
BuiltinType::gen_parts(Writer &w, PermString &) const
{
  w << _name;
  return 0;
}

int
AnyType::gen_parts(Writer &w, PermString &) const
{
  w << _gen_name;
  return 0;
}

int
ArithmeticType::gen_parts(Writer &w, PermString &) const
{
  if (_unsigned && !_seqint)
    w << "unsigned " << (_name.c_str() + 1);
  else
    w << _name;
  if (_size == 8)
    w << " long";
  return 0;
}

int
PointerType::gen_parts(Writer &w, PermString &after) const
{
  int val = _deref->gen_parts(w, after);
  if (val == 0) w << " ";
  if (val < 2)
    w << "*";
  else {
    w << "(*";
    after = permprintf(")%p", after.capsule());
  }
  return 1;
}

int
ArrayType::gen_parts(Writer &w, PermString &after) const
{
  int val = deref()->gen_parts(w, after);
  if (val == 0) w << " ";
  if (_size >= 0)
    after = permprintf("[%d]%p", _size, after.capsule());
  else
    after = permprintf("[]%p", after.capsule());
  return 2;
}


void
Type::gen(Writer &w) const
{
  PermString after = "";
  gen_parts(w, after);
  w << after;
}

void
Type::gen_object(Writer &w, PermString obj) const
{
  PermString after = "";
  if (gen_parts(w, after) == 0) w << " ";
  w << obj << after;
}


/*****
 * gen_modules
 **/

void
Type::gen_prototype(Writer &) const
{
}

void
Type::gen_create(Writer &w) const
{
  gen_prototype(w);
}

void
PointerType::gen_prototype(Writer &w) const
{
  _deref->gen_prototype(w);
}


Type *namespace_type = new BuiltinType("<namespace>");
Type *node_type = new BuiltinType("<node>");
Type *code_type = new BuiltinType("<code>");
Type *any_type = new AnyType("<any>", "int");

Type *void_type = new BuiltinType("void");
Type *bool_type = new BuiltinType("bool", 4);

ArithmeticType *char_type = new ArithmeticType("char", 1, false, false);
ArithmeticType *short_type = new ArithmeticType("short", 2, false, false);
ArithmeticType *int_type = new ArithmeticType("int", 4, false, false);
ArithmeticType *long_type = new ArithmeticType("long", 8, false, false);

ArithmeticType *uchar_type = new ArithmeticType("uchar", 1, true, false);
ArithmeticType *ushort_type = new ArithmeticType("ushort", 2, true, false);
ArithmeticType *uint_type = new ArithmeticType("uint", 4, true, false);
ArithmeticType *ulong_type = new ArithmeticType("ulong", 8, true, false);

ArithmeticType *seqint_type = new ArithmeticType("seqint", 4, true, true);

PointerType *ptr_void_type = (PointerType *)void_type->make_pointer();
PointerType *ptr_char_type = (PointerType *)char_type->make_pointer();


Type *
Type::common_type(Type *t1, Type *t2, bool valued)
{
  // If they're the same type, return it.
  if (t1 == t2 || t1->same_type(t2))
    return t1;

  // If either is any, return the other.
  if (t1 == any_type)
    return t2;
  if (t2 == any_type)
    return t1;
  
  // If either is void, return void.
  if (t1 == void_type || t2 == void_type)
    return void_type;
  
  // If one is bool and the other is bool-convertible, return bool.
  if (t1 == bool_type && t2->bool_convertible()
      || t2 == bool_type && t1->bool_convertible())
    return bool_type;

  // If they're both arithmetic, return the binary type.
  if (Type *t = ArithmeticType::binary_type(t1, t2))
    return t;

  // Pointer conversions!
  if (PointerType *ptr1 = t1->cast_pointer())
    if (PointerType *ptr2 = t2->cast_pointer()) {
      
      // *void if either is *void.
      if (ptr1 == ptr_void_type || ptr2 == ptr_void_type)
	return ptr_void_type;
      
      // Return the ancestral pointer type if one exists.
      if (Module *m1 = ptr1->deref()->cast_module())
	if (Module *m2 = ptr2->deref()->cast_module()) {
	  if (m1->has_ancestor(m2))
	    return t2;
	  if (m2->has_ancestor(m1))
	    return t1;
	}
      
      // Otherwise, it's void. Worthwhile to warn on returning two
      // nonconvertible pointer types? Maybe; but not worthwhile returning
      // *void.
      return void_type;
      
    } else if (valued && t2->cast_arithmetic())
      // Situation `x :> *T != 0'
      return t1;
  
  // Situation `0 != x :> *T'
  if (valued && t2->cast_pointer() && t1->cast_arithmetic())
    return t2;
  
  return void_type;
}


ArithmeticType *
ArithmeticType::promote() const
{
  if (_size < 4)
    return int_type;
  else
    return (ArithmeticType *)this;
}


Type *
ArithmeticType::binary_type(Type *t1, Type *t2)
{
  // handle any_type (exceptions)
  if (t1 == any_type || t2 == any_type)
    return any_type;
  
  ArithmeticType *a1 = t1->cast_arithmetic();
  ArithmeticType *a2 = t2->cast_arithmetic();
  if (!a1 || !a2)
    return 0;
  
  a1 = a1->promote();
  a2 = a2->promote();
  
  if (a1 == ulong_type || a2 == ulong_type)
    return ulong_type;
  if (a1 == long_type || a2 == long_type)
    return long_type;
  if (a1 == seqint_type || a2 == seqint_type)
    return seqint_type;
  if (a1 == uint_type || a2 == uint_type)
    return uint_type;
  return int_type;
}
