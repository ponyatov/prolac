#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "literal.hh"
#include "type.hh"
#include "writer.hh"
#include "node.hh"
#include "codeblock.hh"


Literal::Literal(Node *node)
  : _type(node_type), _is(Is_node), _landmark(node->landmark())
{
  _v.node = node;
}

Literal::Literal(Type *type, const Landmark &landmark)
  : _type(type), _is(Is_type), _landmark(landmark)
{
  _v.ul = 0;
}


Literal::operator bool() const
{
  switch (_is) {
    
   case Is_l:
    return _v.l != 0;
    
   case Is_ul:
    return _v.ul != 0;

   case Is_str:
    return vstring();
    
   case Is_node:
    return _v.node;
    
   case Is_type:
    return _type;
    
   case Isnt:
    return false;
    
  }
  return false;
}


void
Literal::gen(Writer &w) const
{
  switch (_is) {
    
   case Is_l:
    w << _v.l;
    break;
    
   case Is_ul:
    w << _v.ul;
    break;

   case Is_str: {
     w << '\"';
     PermString str = vstring();
     const unsigned char *d = (const unsigned char *)str.c_str();
     for (int i = 0; i < str.length(); i++)
       if (d[i] == '\"' || d[i] == '\\')
	 w << '\\' << d[i];
       else if (d[i] > 31 && d[i] < 127)
	 w << d[i];
       else {
	 char q[5];
	 sprintf(q, "\\%03o", d[i]);
	 w << q;
       }
     break;
   }
   
   case Is_type:
    _type->gen(w);
    break;
    
   default:
    assert(0 && "ungenable literal");
    break;
    
  }
}


Writer &
operator<<(Writer &w, const Literal &lit)
{
  switch (lit._is) {
    
   case Literal::Is_l:
    w << lit._v.l << ":" << lit._type;
    break;
    
   case Literal::Is_ul:
    w << lit._v.ul << ":" << lit._type;
    break;

   case Literal::Is_str:
    w << '\"' << lit.vstring() << '\"';
    break;
    
   case Literal::Is_node:
    w << "<node literal>";
    break;
    
   case Literal::Is_type:
    w << lit.type() << ":type";
    break;

   case Literal::Isnt:
    w << "<NO>";
    break;
    
  }
  return w;
}
