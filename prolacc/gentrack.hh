#ifndef GENTRACK_HH
#define GENTRACK_HH


class GenTracker {
  
  int _track;
  
 public:
  
  GenTracker()		: _track(0) {}
  
  bool need() const	{ return _track & 1; }
  bool done() const	{ return _track == 4; }
  void mark()		{ if (!_track) _track = 1; }
  bool pregen_if();
  bool protogen_if();
  bool gen_if();
  
};


inline bool
GenTracker::pregen_if()
{
  if (_track == 1)
    return _track = 3, true;
  else
    return false;
}

inline bool
GenTracker::protogen_if()
{
  if (_track < 3)
    return _track = 3, true;
  else
    return false;
}

inline bool
GenTracker::gen_if()
{
  if (_track < 4)
    return _track = 4, true;
  else
    return false;
}

#endif
