#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "fork.hh"
#include "compiler.hh"
#include "writer.hh"


void
Fork::grep_forks(Vector<Fork *> &fl) const
{
  fl.push_back((Fork *)this);
}

void
Jumper::destroy()
{
  _landing->remove_enter(this);
  delete this;
}


int
Jumper::can_direct(int) const
{
  return (_outline_edge >= 5 ? -1 : 11) - _takeoff->outline_level();
}

int
Jumper::can_direct_copy(int) const
{
  return can_direct() > 0 && _landing->enter_count() <= 1;
}


void
Jumper::redirect(BlockLocation *block)
{
  block->add_enter(this);
  _landing->remove_enter(this);
  _landing = block;
}


BlockLocation *
Jumper::make_direct_copy()
{
  assert(can_direct_copy() > 0);
  redirect( _landing->copy() );
  return _landing;
}


void
Fork::print()
{
  errwriter << wmtab(24) << (_value_used ? 'V' : '-') <<
	     (_node->must_gen_state() ? ' ' : 'c');
  
  _node->write_short(errwriter);
  
  switch (_gen_code) {
   case gcTest: errwriter << " <test> "; break;
   case gcYes: errwriter << " <yes> "; break;
   case gcNo: errwriter << " <no> "; break;
   default: errwriter << " "; break;
  }
  
  errwriter << wmendl;
}


void
Jumper::print()
{
  errwriter << _landing->id() << " of " <<
    _landing->enter_count();
  if (_outline_edge >= 0)
    errwriter << "  out[" << _outline_edge << "]";
  errwriter << wmendl;
}


/* gen_state must be called on every fork before gen_value */

void
Fork::gen_state(Compiler *c)
{
  _node->set_gen_code(_gen_code);
  _node->gen_state(c);
}

/* exactly one of gen_effect() and gen_value() should be called on a given
   fork */

void
Fork::gen_value(Compiler *c)
{
  _node->gen_value(c);
}

void
Fork::gen_effect(Compiler *c, bool in_expr)
{
  // XXX do we need to rework the logic of must_gen_value?
  if (!_value_used && _node->must_gen_value()) {
    _node->gen_value(c);
    c->out << (in_expr ? ",\n" : ";\n");
  }
}


void
Jumper::gen(Compiler *c)
{
  if (_direct)
    _landing->gen(c);
  else
    c->out << "goto " << landing()->label() << ";\n";
}
