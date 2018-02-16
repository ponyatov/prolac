#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "location.hh"
#include "fork.hh"
#include "compiler.hh"


int BlockLocation::last_id;


BlockLocation::BlockLocation(Betweenliner b, int target_id, PermString label)
  : _id(++last_id), _label(label),
    _target_id(target_id),
    _reached(0), _did_better_logical(false), _have_label(false),
    _exit_yes(0), _exit_no(0),
    _betweenliner(b)
{
  if (!_label) _label = "B";
}


void
BlockLocation::destroy()
{
  for (int i = 0; i < _locs.size(); i++)
    _locs[i]->destroy();
  if (_exit_yes) _exit_yes->destroy();
  if (_exit_no) _exit_no->destroy();
  delete this;
}


LogicalLocation::LogicalLocation(bool isand, Location *left, Location *right)
  : _isand(isand), _left(left), _right(right)
{
  assert(!_left->must_gen_state());
  assert(!_right->must_gen_state());
}


void
BlockLocation::append(Location *l)
{
  l->mark_collected();
  _locs.push_back(l);
}


bool
BlockLocation::must_gen_state() const
{
  for (int i = 0; i < _locs.size(); i++)
    if (_locs[i]->must_gen_state())
      return true;
  return false;
}


int
BlockLocation::outline_epoch() const
{
  return _betweenliner.outline_epoch();
}

int
BlockLocation::outline_level() const
{
  return _betweenliner.outline_level();
}


void
BlockLocation::remove_enter(Jumper *deleter)
{
  for (int i = 0; i < _enters.size(); i++)
    if (_enters[i] == deleter) {
      Jumper *last = _enters.back();
      _enters.pop_back();
      if (last != deleter)
	_enters[i] = last;
      return;
    }
  assert(0);
}


BlockLocation *
BlockLocation::copy()
{
  assert(_enters.size() == 1);
  return this;
}


int
calc_outline_edge(BlockLocation *a, BlockLocation *b)
{
  if (a->outline_epoch() != b->outline_epoch())
    return b->outline_level();
  else
    return -1;
}

void
BlockLocation::make_jump(BlockLocation *l)
{
  assert(!_exit_yes);
  _exit_yes = new Jumper(this, l, calc_outline_edge(this, l));
  _exit_no = 0;
}

void
BlockLocation::make_branch(BlockLocation *yes, BlockLocation *no)
{
  assert(!_exit_yes);
  _exit_yes = new Jumper(this, yes, calc_outline_edge(this, yes));
  _exit_no = new Jumper(this, no, calc_outline_edge(this, no));
}

void
BlockLocation::make_no_exit()
{
  _exit_yes = 0;
  _exit_no = 0;
}

/*****
 * better_logical and related
 **/

bool
BlockLocation::try_better_logical(bool right_yes)
{
  Jumper *l_j = maybe(!right_yes);
  Jumper *r_j = maybe(right_yes);
  if (!l_j || !r_j) return false;
  
  // Look at right branch `r'.
  BlockLocation *r = r_j->landing();
  
  // Left branch of right branch must be an empty jumper...
  Jumper *r_l_j = r->maybe(!right_yes);
  if (!r_l_j) return false;
  // ...to the same place!
  if (r_l_j->landing()->target_id() != l_j->landing()->target_id())
    return false;
  
  // Check that no intermediate statements need state code.
  for (int i = 0; i < r->count(); i++)
    if (r->at(i)->must_gen_state())
      return false;
  //fprintf(stderr, "needstate\n");
  
  // Can we duplicate the code here?
  if (r_j->can_direct_copy() <= 0) return false;
  
  // Oooo! We can make a Logical.
  Jumper *r_r_j = r->maybe(right_yes);
  
  r = r_j->make_direct_copy();
  r->make_no_exit();
  
  Location *old_test = _locs.back();
  _locs.pop_back();
  append(new LogicalLocation(right_yes == true, old_test, r));
  
  r->reach();
  r_j->destroy();
  r_l_j->destroy();
  if (right_yes)
    _exit_yes = r_r_j;
  else
    _exit_no = r_r_j;
  
  return true;
}


void
BlockLocation::better_logical()
{
  if (!_exit_yes || !_exit_no || _did_better_logical)
    return;
  
  _did_better_logical = true;
  _exit_yes->landing()->better_logical();
  _exit_no->landing()->better_logical();
  while (try_better_logical(true) || try_better_logical(false))
    ;
}


/*****
 * placement
 **/

void
BlockLocation::decide_direct()
{
  if (reached()) return;
  
  int min_direct = 1;
  Jumper *j_direct = 0;
  for (int i = 0; i < _enters.size(); i++) {
    int direct = _enters[i]->can_direct();
    if (direct >= min_direct) {
      min_direct = direct;
      j_direct = _enters[i];
    }
  }
  if (j_direct) {
    j_direct->make_direct();
    reach();
  }
}


bool
BlockLocation::need_label() const
{
  for (int i = 0; i < _enters.size(); i++)
    if (_enters[i]->forced_goto())
      return true;
  return false;
}


/*****
 * grep_forks, find_temporaries
 **/

void
BlockLocation::grep_forks(Vector<Fork *> &fl) const
{
  for (int i = 0; i < _locs.size(); i++)
    _locs[i]->grep_forks(fl);
}


void
LogicalLocation::grep_forks(Vector<Fork *> &fl) const
{
  _left->grep_forks(fl);
  _right->grep_forks(fl);
}


void
BlockLocation::make_temporaries()
{
  Vector<Fork *> forks;
  grep_forks(forks);
  
  int i;
  for (i = 0; i < forks.size(); i++)
    forks[i]->node()->reset_usage();
  for (i = 0; i < forks.size(); i++) {
    Node *n = forks[i]->node();
    n->mark_usage();
    if (forks[i]->value_used())
      n->incr_usage();
  }
  
  for (i = 0; i < forks.size(); i++)
    if (forks[i]->node()->unequal_usage()
	&& !forks[i]->node()->simple_value())
      forks[i]->node()->make_temporary();
}


/*****
 * gen
 **/

void
BlockLocation::gen(Compiler *c)
{
  if (_have_label)
    c->out << wmhang(1) << _label << ":\n";
  
  int last_val = _locs.size() - 1;
  for (int i = 0; i < last_val; i++) {
    _locs[i]->gen_state(c);
    _locs[i]->gen_effect(c, false);
  }
  
  back()->gen_state(c);
  
  if (_exit_no) {
    assert(_exit_yes);
    
    Jumper *yes;
    Jumper *no;
    c->out << "if (" << wmindent(4);
    if (_exit_yes->direct() && !_exit_no->direct()) {
      yes = _exit_no;
      no = _exit_yes;
      
      c->out << "!(";
      back()->gen_value(c);
      c->out << ")";
      
    } else {
      yes = _exit_yes;
      no = _exit_no;
      
      back()->gen_value(c);
    }
    
    c->out << ") {\n" << wmindent(-2);
    yes->gen(c);
    c->out << wmindent(-2) << "}\n";
    no->gen(c);
    
  } else if (_exit_yes) {
    back()->gen_effect(c, false);
    _exit_yes->gen(c);
    
  } else {
    /* if exit_yes == 0, we are returning from the routine. generate the
       appropriate `return' statement in C */
    Fork *back_fork = back()->cast_fork();
    assert(back_fork);
    if (back_fork->node()->type() == void_type) {
      back_fork->gen_effect(c, false);
      c->out << "return;\n";
    } else {
      c->out << "return ";
      back_fork->gen_value(c);
      c->out << ";\n";
    }
  }
}


void
BlockLocation::gen_value(Compiler *c)
{
  assert(_exit_yes == 0 && _exit_no == 0);
  for (int i = 0; i < _locs.size() - 1; i++)
    _locs[i]->gen_effect(c, true);
  back()->gen_value(c);
}


void
LogicalLocation::gen_state(Compiler *c)
{
  _left->gen_state(c);
}

void
LogicalLocation::gen_value(Compiler *c)
{
  c->out << "(" << wmindent(2);
  _left->gen_value(c);
  c->out << ")\n" << (_isand ? "&&" : "||") << " (" << wmindent(4);
  _right->gen_value(c);
  c->out << ")" << wmindent(-6);
}



/*****
 * print
 **/

void
BlockLocation::print()
{
  errwriter << wmwidth(4) << _id << wmtab(8);
  if (_have_label)
    errwriter << _label << " ";
  if (_enters.size() > 1)
    errwriter << "ent[" << _enters.size() << "] ";
  if (outline_level() != -1)
    errwriter << "out[" << outline_level() << "] ";

  errwriter << wmtab(24);
  for (int i = 0; i < _locs.size(); i++)
    _locs[i]->print();
  if (_exit_yes) {
    if (_exit_no)
      errwriter << "     Y> ";
    else
      errwriter << "     >> ";
    _exit_yes->print();
  }
  if (_exit_no) {
    errwriter << "     N> ";
    _exit_no->print();
  }
}


void
LogicalLocation::print()
{
  errwriter << (_isand ? "&&< " : "||< ") << wmindent(4);
  _left->print();
  errwriter << wmhang(2) << "> ";
  _right->print();
  errwriter << wmindent(-4);
}
