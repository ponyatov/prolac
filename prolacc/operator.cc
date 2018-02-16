#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "operator.hh"
#include "compiler.hh"
#include "node.hh"

int Operator::all_flags[opLast];
int Operator::all_precedence[opLast];
int Operator::all_right_precedence[opLast];
Operator Operator::all_terminator[opLast];
Operator Operator::all_next[opLast];
PermString Operator::all_name[opLast - 256];

void
Operator::set_data(PermString name)
{
  assert(_op >= 0 && _op < opLast);
  if (_op >= 256)
    all_name[_op - 256] = name;
  all_precedence[_op] = -1;
  all_flags[_op] = 0;
  all_terminator[_op] = 0;
  all_next[_op] = 0;
}

void
Operator::set_data(PermString name, int precedence, int flags,
                   Operator terminator)
{
  assert(_op >= 0 && _op < opLast);
  
  if (flags & fSynonym)
    assert(all_precedence[_op]);
  else {
    assert(!all_precedence[_op] && !all_right_precedence[_op]);
    
    if (_op >= 256) all_name[_op - 256] = name;
    all_precedence[_op] = all_right_precedence[_op] = precedence;
    all_flags[_op] = flags;
    all_terminator[_op] = terminator;
    all_next[_op] = 0;
    
    // now test some flags for internal consistency
    if (terminator) assert(prefix() || !unary());
    if (takes_id()) assert(!terminator);
    if (functionish()) assert(prefix());
  }
}

void
Operator::set_right_precedence(int precedence)
{
  assert(_op >= 0 && _op < opLast);
  all_right_precedence[_op] = precedence;
}

void
Operator::insert_next_operator(Operator next)
{
  assert(_op >= 0 && _op < opLast && next._op >= 0 && next._op < opLast);
  all_next[next._op] = all_next[_op];
  all_next[_op] = next;
}

Operator
Operator::seqint_version() const
{
  if (_op >= opLt && _op <= opGeq)
    return _op + (opSeqLt - opLt);
  else
    return _op;
}

Operator
Operator::find(bool is_prefix) const
{
  Operator o = *this;
  while (o) {
    if (o.prefix() == is_prefix) return o;
    o = all_next[o._op];
  }
  return o;
}

PermString
Operator::name() const
{
  if (_op < 256) {
    char x = _op;
    return PermString(&x, 1);
  } else {
    assert(_op < opLast);
    return all_name[_op - 256];
  }
}

PermString
Operator::macro_name() const
{
  static char *macro_names[] = {
    "SEQ_LT", "SEQ_LEQ", "SEQ_GT", "SEQ_GEQ",
  };
  assert(_op >= opFirstMacro && _op < opLast);
  return macro_names[_op - opFirstMacro];
}

Writer &
operator<<(Writer &w, Operator op)
{
  w << op.name();
  return w;
}

void
Operator::gen_binary_value(Compiler *c, Node *left, Node *right) const
{
  if (is_macro()) {
    c->out << macro_name() << "((";
    left->gen_value(c);
    c->out << "), (";
    right->gen_value(c);
    c->out << "))";
  } else {
    c->out << "(";
    left->gen_value(c);
    c->out << ") " << name() << " (";
    right->gen_value(c);
    c->out << ")";
  }
}
