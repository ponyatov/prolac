#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "exception.hh"
#include <lcdf/vector.hh>
#include "writer.hh"


static Vector<Exception *> all_exceptions;

static int last_exception = -1;

Exception::Exception(PermString n, const Landmark &l)
  : _name(n), _eid(last_exception), _landmark(l)
{
  last_exception--;
  all_exceptions.push_back(this);
}


/* ExceptionSet */

inline void
ExceptionSetInstance::ensure_size(int size) const
{
  if (size > _v.size()) _v.resize(size, 0);
}

inline void
ExceptionSetInstance::set(const Exception *e)
{
  int i = -e->exception_id();
  ensure_size(i/8 + 1);
  _v[i/8] |= 1<<(i%8);
}

ExceptionSetInstance::ExceptionSetInstance(const Exception *e)
  : _v(1, 0)
{
  if (e) set(e);
}

bool
ExceptionSetInstance::test(const Exception *e) const
{
  int i = -e->exception_id();
  if (i/8 >= _v.size()) return false;
  return (_v[i/8] & (1<<(i%8))) != 0;
}

bool
ExceptionSetInstance::operator==(const ExceptionSetInstance &o) const
{
  o.ensure_size(_v.size());
  ensure_size(o._v.size());
  for (int i = 0; i < _v.size(); i++)
    if (_v[i] != o._v[i])
      return false;
  return true;
}

bool
ExceptionSetInstance::operator<=(const ExceptionSetInstance &o) const
{
  ensure_size(o._v.size());
  for (int i = 0; i < _v.size(); i++) {
    int x = _v[i] & o._v[i];
    if (x != _v[i])
      return false;
  }
  return true;
}

void
ExceptionSetInstance::operator+=(const ExceptionSetInstance &o)
{
  ensure_size(o._v.size());
  for (int i = 0; i < _v.size(); i++)
    _v[i] |= o._v[i];
}

void
ExceptionSetInstance::operator-=(const ExceptionSetInstance &o)
{
  ensure_size(o._v.size());
  for (int i = 0; i < _v.size(); i++)
    _v[i] &= ~o._v[i];
}

int
ExceptionSetInstance::element_after(int i)
{
  if (i >= 0) i = 0;
  int e = (-i)/8;
  i = ((-i)%8) + 1;
  
  for (; e < _v.size(); e++, i = 0) {
    int x = _v[e];
    for (; i < 8; i++)
      if (x&(1<<i))
	return -((e*8)+i);
  }
  
  return 0;
}


static Vector<ExceptionSetInstance> set_instances;

bool
ExceptionSet::subset(const ExceptionSet &o) const
{
  return set_instances[_eset] <= set_instances[o._eset];
}

static int
install_exception_set(const ExceptionSetInstance &si)
{
  for (int i = 0; i < set_instances.size(); i++)
    if (set_instances[i] == si)
      return i;
  int s = set_instances.size();
  set_instances.push_back(si);
  return s;
}

ExceptionSet &
ExceptionSet::operator+=(const Exception *e)
{
  ExceptionSetInstance si = set_instances[_eset];
  si.set(e);
  _eset = install_exception_set(si);
  return *this;
}

ExceptionSet &
ExceptionSet::operator+=(const ExceptionSet &o)
{
  ExceptionSetInstance si = set_instances[_eset];
  si += set_instances[o._eset];
  _eset = install_exception_set(si);
  return *this;
}

ExceptionSet &
ExceptionSet::operator-=(const ExceptionSet &o)
{
  ExceptionSetInstance si = set_instances[_eset];
  si -= set_instances[o._eset];
  _eset = install_exception_set(si);
  return *this;
}

Exception *
ExceptionSet::element(Exception *after) const
{
  int after_index = (after ? after->exception_id() : 0);
  int exception_index = set_instances[_eset].element_after(after_index);
  return all_exceptions[-exception_index];
}

void
ExceptionSet::write(Writer &w) const
{
  int count = 0;
  for (Exception *e = element(0); e; e = element(e)) {
    w << (count ? ", " : "") << e->name();
    count++;
  }
  if (!count) w << "<no exceptions>";
}


// make sure static initialization is done
ExceptionSet ExceptionSet::initializer(1, 2, 3);
ExceptionSet::ExceptionSet(int, int, int)
{
  ExceptionSetInstance null_instance;
  set_instances.push_back(null_instance);
  assert(!all_exceptions.size());
  all_exceptions.push_back((Exception *)0);
}


bool
ExceptionSet::contains(Exception *e) const
{
  return set_instances[_eset].test(e);
}
