#ifndef DECLARAT_HH
#define DECLARAT_HH
#include <lcdf/permstr.hh>
class Expr;

struct Declaration {
  
  bool _ok;
  int _name_count;
  bool _fixed_type;
  bool _explicit_name;
  
  PermString _head;
  PermString _name;
  Expr *_type;
  Expr *_value;
  
 public:
  
  Declaration();
  
  bool explicit_name() const	{ return _explicit_name; }
  bool explicit_type() const	{ return _fixed_type; }
  PermString head() const	{ return _head; }
  PermString name() const	{ return _name; }
  Expr *type() const		{ return _type; }
  Expr *value() const		{ return _value; }
  
  bool let_ok() const;
  bool module_relation_ok() const;
  bool module_spec_ok() const;
  
  void fail()			{ _ok = false; }
  
  void set_name(PermString);
  void set_type(Expr *);
  void implicit_type(Expr *);
  void force_name();
  void set_value(Expr *);
  
};

#endif
