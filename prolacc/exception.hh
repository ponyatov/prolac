#ifndef EXCEPTION_HH
#define EXCEPTION_HH
#include "writer.hh"
#include "landmark.hh"
#include <lcdf/vector.hh>

class Exception {

  PermString _name;
  int _eid;
  Landmark _landmark;
  
 public:
  
  Exception(PermString, const Landmark &);
  
  PermString name() const		{ return _name; }
  int exception_id() const		{ return _eid; }
  const Landmark &landmark() const	{ return _landmark; }
  
};


class ExceptionSet {
  
  int _eset;
  
  static ExceptionSet initializer;
  explicit ExceptionSet(int fake, int initialization, int function);
  bool subset(const ExceptionSet &) const;
  
 public:
  
  ExceptionSet()			: _eset(0) { }
  
  operator bool() const			{ return _eset != 0; }
  bool operator!() const		{ return _eset == 0; }
  
  bool contains(Exception *e) const;
  Exception *element(Exception *after = 0) const;
  
  bool operator==(const ExceptionSet &o) const	{ return _eset == o._eset; }
  bool operator!=(const ExceptionSet &o) const	{ return _eset != o._eset; }
  bool operator<=(const ExceptionSet &o) const;
  bool operator>=(const ExceptionSet &o) const	{ return o <= *this; }
  
  ExceptionSet &operator+=(const Exception *);
  ExceptionSet &operator+=(const ExceptionSet &);
  ExceptionSet &operator-=(const ExceptionSet &);
  
  void write(Writer &) const;
  
};


class ExceptionSetInstance {
  // DO NOT USE -- internal to definition of ExceptionSet -- needed in header
  // file for vectori.cc
  
  mutable Vector<int> _v;
  inline void ensure_size(int) const;
  
 public:
  
  ExceptionSetInstance(const Exception * = 0);
  
  bool test(const Exception *) const;
  bool operator==(const ExceptionSetInstance &) const;
  bool operator<=(const ExceptionSetInstance &) const;
  
  inline void set(const Exception *);
  void operator+=(const ExceptionSetInstance &);
  void operator-=(const ExceptionSetInstance &);
  
  int element_after(int);
  
};


inline bool
ExceptionSet::operator<=(const ExceptionSet &o) const
{
  return _eset == o._eset || subset(o);
}

inline Writer &
operator<<(Writer &w, const Exception *e)
{
  return w << e->name();
}

inline Writer &
operator<<(Writer &w, const ExceptionSet &es)
{
  es.write(w);
  return w;
}

#endif
