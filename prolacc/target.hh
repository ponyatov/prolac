#ifndef TARGET_HH
#define TARGET_HH
#include "node.hh"
class Fork;
class Jumper;
class BlockLocation;

class Target {
  
  int _id;
  GenCode _gen_code;
  
  Node *_node;
  Betweenliner _betweenliner;
  bool _value_used;
  bool _alias;
  
  Target *_succeed;
  Target *_fail;
  int _enter;
  
  Fork *_primary_fork;
  BlockLocation *_primary_block;
  PermString _label_name;
  
  int _printed;
  
  static int last_id;
  
  void print_recurse();
  
 public:
  
  Target(Node *, bool, Target *, Target * = 0, GenCode = gcNormal);
  
  int id() const				{ return _id; }
  GenCode gen_code() const			{ return _gen_code; }
  bool value_used() const			{ return _value_used; }
  Target *succeed() const			{ return _succeed; }
  Target *fail() const				{ return _fail; }

  void make_alias(Target *);
  void set_label_name(PermString ln)		{ _label_name = ln; }
  
  int outline_epoch() const	{ return _betweenliner.outline_epoch(); }
  void set_betweenliner(Betweenliner b)		{ _betweenliner = b; }
  
  void count_enter();
  BlockLocation *connect(BlockLocation *, Compiler *);
  
  void print();
  void print0();
  
};

#endif
