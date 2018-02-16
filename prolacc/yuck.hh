#ifndef YUCK_HH
#define YUCK_HH
#include "token.hh"
#include "modfrob.hh"
#include "operator.hh"
class Protomodule;
class Module;
class Expr;
class ListExpr;
class Name;
class Program;
class Namespace;

class Yuck {
  
  static const int TstackSize = 8;
  
  Tokenizer *_tokenizer;
  
  Token tcircle[TstackSize];
  int tpos;
  int tfull;
  
  Protomodule *_cmodule;
  Namespace *_cnamesp;
  
  Program *_prog;
  
  bool _brace_is_block;
  
  void save(const Token &);
  void savem(const Token *, ...);
  
  void skip(int, int = 0);
  bool expect(int, bool reporterror = true);
  
  Expr *yif_expr(Expr *test);
  Expr *triple_or_expr(Expr *left, Expr *right) const;
  Expr *yfunctionish_expr(Operator op);
  
  bool name_ok(PermString, bool, const Landmark &) const;
  PermString parse_name(Expr *, Namespace *&) const;
  
 public:
  
  Yuck(Tokenizer *, Program *);
  
  Token lex();
  
  Expr *yexpr(int precedence = -1, int terminator = opNone, int old_precedence = -1, Operator = 0);
  Expr *yno_comma_expr();
  ListExpr *yexpr_list(int terminator);
  Expr *yname_expr();
  Expr *ytype_expr();
  
  void ymodulerelation();
  void yparent();
  
  bool ynamespace(Namespace *, PermString, const Landmark &);
  bool yrule(Namespace *, PermString, const Landmark &, bool staticp);
  bool yfield(Namespace *, PermString, bool staticp, const Landmark &);
  bool yexception(Namespace *, PermString, const Landmark &);
  bool ydefinition();
  
  bool ymodule_equation(Namespace *, PermString, const Landmark &);
  bool ymodule_def(Namespace *, PermString, const Landmark &);
  void yexport();
  
};


inline Token
Yuck::lex()
{
  if (tpos == tfull)
    return _tokenizer->get_token();
  else {
    int p = tpos;
    tpos = (tpos + 1) % TstackSize;
    return tcircle[p];
  }
}

#endif
