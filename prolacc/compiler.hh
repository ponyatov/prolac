#ifndef COMPILER_HH
#define COMPILER_HH
#include "writer.hh"
#include <lcdf/vector.hh>
#include "rule.hh"
class Target;
class BlockLocation;
class ModuleNames;
class Node;
class Rule;


class Compiler {

  int _max_inline_level;
  
  Node *_body_root;
  ModuleNames *_gen_modnames;
  Vector<BlockLocation *> _blocks;
  Vector<Node *> _temporaries;
  Vector<Rule *> _marked_rules;
  Target *_exception_handler;
  Target *_rethrow_handler;

  void make_rethrow_handler();
  
 public:
  
  Writer &out;
  Writer &proto_out;
  
  Compiler(Writer &, Writer &, int max_inline_level = inlinePath);
  
  void compile(Rule *, bool debug_node = 0, bool debug_target = 0,
	       bool debug_loc = 0);
  
  void add_block(BlockLocation *b)		{ _blocks.push_back(b); }

  void set_exception_handler(Target *);
  Target *exception_handler();
  
  void gen();
  
};

#endif
