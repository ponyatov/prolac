#ifndef LOCATION_HH
#define LOCATION_HH
#include "node.hh"
class BlockLocation;
class Fork;
class Jumper;


class Location {
  
  bool _collected;
  
 public:
  
  Location()					: _collected(0) { }
  virtual ~Location()				{ }
  virtual void destroy()			{ assert(0); }
  
  virtual bool must_gen_state() const		{ return true; }
  
  virtual void better_logical()			{ }
  
  void mark_collected()			{ assert(!_collected); _collected =1; }
  
  virtual void grep_forks(Vector<Fork *> &) const { }
  
  virtual void gen_state(Compiler *)		{ assert(0); }
  virtual void gen_value(Compiler *)		{ assert(0); }
  virtual void gen_effect(Compiler *, bool)	{ assert(0); }
  
  virtual void print()				{ }

  virtual Fork *cast_fork()			{ return 0; }
  
};


class BlockLocation: public Location {
  
  int _id;
  PermString _label;
  int _target_id;
  
  bool _reached;
  bool _did_better_logical: 1;
  bool _have_label: 1;
  
  Vector<Location *> _locs;
  Jumper *_exit_yes;
  Jumper *_exit_no;
  
  Betweenliner _betweenliner;
  
  Vector<Jumper *> _enters;
  
  static int last_id;
  
  Jumper *maybe(bool isyes) const	{ return isyes?_exit_yes:_exit_no; }
  
  bool try_better_logical(bool);
  
 public:
  
  BlockLocation(Betweenliner, int target_id, PermString label_name);
  void destroy();
  
  bool reached() const			{ return _reached; }
  void reach()				{ _reached = true; }
  void unreach()			{ _reached = false; }
  
  int id() const			{ return _id; }
  PermString label() const		{ return _label; }
  int target_id() const			{ return _target_id; }
  
  int enter_count() const		{ return _enters.size(); }
  void add_enter(Jumper *e)		{ _enters.push_back(e); }
  void remove_enter(Jumper *);
  
  int count() const			{ return _locs.size(); }
  bool empty() const			{ return _locs.size() == 0; }
  
  bool brancher() const			{ return _exit_yes && _exit_no; }
  
  Location *at(int e) const		{ return _locs[e]; }
  Location *back() const;
  
  void append(Location *);
  
  Jumper *exit_yes() const		{ return _exit_yes; }
  Jumper *exit_no() const		{ return _exit_no; }
  
  void make_jump(BlockLocation *l);
  void make_branch(BlockLocation *y, BlockLocation *n);
  void make_no_exit();
  
  bool must_gen_state() const;
  int outline_epoch() const;
  int outline_level() const;
  
  BlockLocation *copy();
  
  void better_logical();
  
  void decide_direct();
  bool need_label() const;
  void set_label(int);
  
  void grep_forks(Vector<Fork *> &) const;
  void make_temporaries();
  
  void gen(Compiler *);
  void gen_value(Compiler *);
  
  void print();
  
};


class LogicalLocation: public Location {

  bool _isand;
  Location *_left;
  Location *_right;
  
 public:
  
  LogicalLocation(bool, Location *, Location *);

  bool must_gen_state() const			{ return false; }
  
  void grep_forks(Vector<Fork *> &) const;
  void gen_state(Compiler *);
  void gen_value(Compiler *);
  
  void print();
  
};


inline Location *
BlockLocation::back() const
{
  int c = _locs.size() - 1;
  return c < 0 ? 0 : _locs[c];
}

inline void
BlockLocation::set_label(int labelno)
{
  _label = permprintf("%p_%d", _label.capsule(), labelno);
  _have_label = 1;
}

#endif
