#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "target.hh"
#include "compiler.hh"
#include "writer.hh"
#include "fork.hh"


int Target::last_id = 0;


Target::Target(Node *n, bool vu, Target *s, Target *f, GenCode gc)
  : _id(++last_id), _gen_code(gc),
    _node(n), _betweenliner(n->betweenliner()), _value_used(vu), _alias(false),
    _succeed(s), _fail(f), _enter(0),
    _primary_fork(0), _primary_block(0),
    _printed(0)
{
  if (last_id < 0) last_id = 0;
}


void
Target::make_alias(Target *s)
{
  _alias = true;
  _succeed = s;
}


void
Target::count_enter()
{
  if (_alias)
    _succeed->count_enter();
  else {
    _enter++;
    if (_enter == 1) {
      if (_succeed) _succeed->count_enter();
      if (_fail) _fail->count_enter();
    }
  }
}


BlockLocation *
Target::connect(BlockLocation *block, Compiler *comp)
{
  if (_alias)
    return _succeed->connect(block, comp);
  
  if (_enter > 1 && block) {
    if (!_primary_block)
      connect(0, comp);
    block->make_jump(_primary_block);
    return 0;
  }
  
  if (!block && _primary_block)
    return _primary_block;
  
  if (!block) {
    _primary_block = block =
      new BlockLocation(_betweenliner, _id, _label_name);
    comp->add_block(block);
  }
  
  _primary_fork = new Fork(_node, _gen_code, _value_used);
  block->append(_primary_fork);
  
  if (_fail == 0 || _fail == _succeed) {
    if (_succeed == 0)
      /* denada */;
    else if (_succeed->outline_epoch() != outline_epoch())
      goto end_this_block;
    else
      _succeed->connect(block, comp);
    
  } else {
   end_this_block:
    BlockLocation *succeed_block =
      _succeed ? _succeed->connect(0, comp) : 0;
    BlockLocation *fail_block =
      _fail ? _fail->connect(0, comp) : 0;
    
    if (succeed_block == 0 && fail_block == 0)
      block->make_no_exit();
    else if (succeed_block == 0)
      block->make_jump(fail_block);
    else if (fail_block == 0 || fail_block == succeed_block)
      block->make_jump(succeed_block);
    else
      block->make_branch(succeed_block, fail_block);
  }
  
  return block;
}


static int printfloor;

void
Target::print()
{
  printfloor++;
  print_recurse();
}


void
Target::print0()
{
  errwriter << wmwidth(2) << _id << ": "
	    << (_value_used ? 'V' : '-')
	    << (_node->must_gen_state() ? ' ' : 'c');

  if (_alias) {
    errwriter << " ALIAS " << _succeed->_id << "\n";
    return;
  }
  
  _node->write_short(errwriter);
  
  switch (gen_code()) {
   case gcTest: errwriter << " <test> "; break;
   case gcYes: errwriter << " <yes> "; break;
   case gcNo: errwriter << " <no> "; break;
   default: errwriter << " "; break;
  }
  
  errwriter <<  "S->" << (_succeed ? _succeed->_id : 0);
  errwriter << " F->" << (_fail ? _fail->_id : 0);

  if (_betweenliner.outline_level() > 0)
    errwriter << " out[" << _betweenliner.outline_level() << "]";
  
  errwriter << wmendl;
}


void
Target::print_recurse()
{
  if (_printed >= printfloor) return;
  _printed = printfloor;
  
  print0();
  
  if (_succeed) _succeed->print_recurse();
  if (_fail) _fail->print_recurse();
}
