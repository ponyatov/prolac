#ifndef FORK_HH
#define FORK_HH
#include "location.hh"
#include "target.hh"

class Fork: public Location {

  Node *_node;
  GenCode _gen_code;
  bool _value_used;
  
  bool _state_coded;
  
 public:
  
  Fork(Node *, GenCode, bool);
  
  bool must_gen_state() const;
  Node *node() const			{ return _node; }
  bool value_used() const		{ return _value_used; }
  GenCode gen_code() const		{ return _gen_code; }
  
  void grep_forks(Vector<Fork *> &) const;
  
  void gen_state(Compiler *);
  void gen_value(Compiler *);
  void gen_effect(Compiler *, bool);
  
  void print();

  Fork *cast_fork()			{ return this; }
  
};


class Jumper {

  BlockLocation *_takeoff;
  BlockLocation *_landing;
  bool _direct;
  int _outline_edge;
  
  void redirect(BlockLocation *);
  
 public:
  
  Jumper(BlockLocation *, BlockLocation *, int);
  void destroy();
  
  BlockLocation *landing() const	{ return _landing; }
  bool direct() const			{ return _direct; }
  bool forced_goto() const		{ return !_direct; }
  
  void make_direct()			{ _direct = 1; }

  int can_direct(int = 0) const;
  int can_direct_copy(int = 0) const;
  BlockLocation *make_direct_copy();
  
  void gen(Compiler *);
  void print();
  
};


inline
Fork::Fork(Node *n, GenCode gc, bool vu)
  : _node(n), _gen_code(gc), _value_used(vu), _state_coded(0)
{
}

inline bool
Fork::must_gen_state() const
{
  return _node->must_gen_state() && !_state_coded;
}

inline
Jumper::Jumper(BlockLocation *from, BlockLocation *to, int outline_edge)
  : _takeoff(from), _landing(to), _direct(0), _outline_edge(outline_edge)
{
  to->add_enter(this);
}

#endif
