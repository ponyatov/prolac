#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "declarat.hh"


Declaration::Declaration()
  : _ok(true), _name_count(0), _fixed_type(0), _explicit_name(0),
    _type(0), _value(0)
{
}


void
Declaration::set_name(PermString n)
{
  if (_name) {
    if (!_head)
      _head = _name;
  }
  _name = n;
  _name_count++;
}


void
Declaration::set_type(Expr *t)
{
  if (_fixed_type || _value)
    fail();
  _fixed_type = true;
  _type = t;
}


void
Declaration::implicit_type(Expr *t)
{
  if (_fixed_type || _value)
    fail();
  _type = t;
}


void
Declaration::set_value(Expr *v)
{
  if (_value)
    fail();
  _value = v;
}


void
Declaration::force_name()
{
  _explicit_name = true;
  assert(!_fixed_type);
  _type = 0;
}


bool
Declaration::let_ok() const
{
  return _ok && _type
    && (_fixed_type || _value)
    && _name_count == 1;
}


bool
Declaration::module_relation_ok() const
{
  return _ok && _type
    && !_value
    && (!_explicit_name || _name_count <= 2);
}


bool
Declaration::module_spec_ok() const
{
  return _ok && _type
    && !_value
    && !_explicit_name;
}
