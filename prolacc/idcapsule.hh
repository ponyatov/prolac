#ifndef IDCAPSULE_HH
#define IDCAPSULE_HH

template <class C>
class IDCapsule {
  
  C *_c;
  
 public:
  
  IDCapsule()				: _c(0) { }
  IDCapsule(const C *c)			: _c((C *)c) { }
  
  operator bool() const			{ return _c != 0; }
  int hashcode() const			{ return (int)_c; }
  
  C &operator*() const			{ return *_c; }
  C *operator->() const			{ return _c; }
  operator C *() const			{ return _c; }
  C *obj() const			{ return _c; }

  friend bool operator==<>(IDCapsule<C>, IDCapsule<C>);
  friend bool operator==<>(IDCapsule<C>, C*);
  friend bool operator==<>(IDCapsule<C>, const C*);
  friend bool operator!=<>(IDCapsule<C>, IDCapsule<C>);
  friend bool operator!=<>(IDCapsule<C>, C*);
  friend bool operator!=<>(IDCapsule<C>, const C*);
  
};


class Module;
typedef IDCapsule<Module> ModuleID;
class Field;
typedef IDCapsule<Field> FieldID;
class Rule;
typedef IDCapsule<Rule> RuleID;

template <class C>
inline bool
operator==(IDCapsule<C> c1, IDCapsule<C> c2)
{
  return c1._c == c2._c;
}

template <class C>
inline bool
operator==(IDCapsule<C> c1, C* c2)
{
  return c1._c == c2;
}

template <class C>
inline bool
operator==(IDCapsule<C> c1, const C* c2)
{
  return c1._c == c2;
}

template <class C>
inline bool
operator!=(IDCapsule<C> c1, IDCapsule<C> c2)
{
  return c1._c != c2._c;
}

template <class C>
inline bool
operator!=(IDCapsule<C> c1, C* c2)
{
  return c1._c != c2;
}

template <class C>
inline bool
operator!=(IDCapsule<C> c1, const C* c2)
{
  return c1._c != c2;
}

#endif
