#ifndef LITERAL_HH
#define LITERAL_HH
#include <cassert>
#include "landmark.hh"
class Type;
class Node;

class Literal {
  
  Type *_type;
  
  union {
    long l;
    unsigned long ul;
    PermString::Capsule str;
    Node *node;
  } _v;

  enum {
    Isnt,
    Is_l,
    Is_ul,
    Is_str,
    Is_node,
    Is_type,
  } _is;
  
  Landmark _landmark;
  
 public:

  Literal();
  Literal(Type *, bool, const Landmark &);
  Literal(Type *, long, const Landmark &);
  Literal(Type *, unsigned long, const Landmark &);
  Literal(Type *, PermString, const Landmark &);
  Literal(Node *);
  Literal(Type *, const Landmark &);
  
  Literal(const Literal &, const Landmark &);
  
  operator const Landmark &() const	{ return _landmark; }
  const Landmark &landmark() const	{ return _landmark; }
  
  Type *type() const			{ return _type; }
  
  bool is_type() const			{ return _is == Is_type; }
  
  long vlong() const		{ assert(_is == Is_l); return _v.l; }
  PermString vstring() const	{ assert(_is == Is_str); return PermString::decapsule(_v.str); }
  Node *vnode() const		{ assert(_is == Is_node); return _v.node; }
  
  operator bool() const;
  
  void gen(Writer &) const;
  
  friend Writer &operator<<(Writer &, const Literal &);
  
};


inline
Literal::Literal()
  : _type(0), _is(Isnt)
{
}

inline
Literal::Literal(Type *t, long l, const Landmark &landmark)
  : _type(t), _is(Is_l), _landmark(landmark)
{
  _v.l = l;
}

inline
Literal::Literal(Type *t, unsigned long ul, const Landmark &landmark)
  : _type(t), _is(Is_ul), _landmark(landmark)
{
  _v.ul = ul;
}

inline
Literal::Literal(Type *t, PermString str, const Landmark &landmark)
  : _type(t), _is(Is_str), _landmark(landmark)
{
  _v.str = str.capsule();
}

inline
Literal::Literal(const Literal &l, const Landmark &landmark)
  : _type(l._type), _v(l._v), _is(l._is), _landmark(landmark)
{
}

#endif
