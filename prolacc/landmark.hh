#ifndef LANDMARK_HH
#define LANDMARK_HH
#include <lcdf/permstr.hh>
class Writer;

class Landmark {
  
  PermString _file;
  unsigned _line;
  
 public:

  Landmark()				: _file("<none>"), _line(0) { }
  Landmark(PermString f, unsigned l)	: _file(f), _line(l) { }

  operator bool() const			{ return _line > 0; }
  
  PermString file() const		{ return _file; }
  unsigned line() const			{ return _line; }
  
  Landmark nextline() const		{ return Landmark(_file, _line + 1); }
  
  friend Writer &operator<<(Writer &, const Landmark &);
  
};

#endif
