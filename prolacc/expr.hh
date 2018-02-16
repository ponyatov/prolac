#ifndef PROLACC_EXPR_HH
#define PROLACC_EXPR_HH
#include <lcdf/vector.hh>
#include "literal.hh"
#include "operator.hh"
#include "resolvarg.hh"
class IdentExpr;
class BinaryExpr;
class ConditionalExpr;
class ListExpr;
class LiteralExpr;
class Feature;
class Namespace;
class ModuleNames;
class Module;
class Type;
class Node;
class CallNode;
class Declaration;
class CodeBlock;
class Betweenliner;
class Protomodule;

class Expr {
  
  Landmark _landmark;
  
 public:
  
  Expr(const Landmark &l)		: _landmark(l) { }
  virtual ~Expr()			{ }
  
  operator const Landmark &() const	{ return _landmark; }
  const Landmark &landmark() const	{ return _landmark; }
  void set_landmark(const Landmark &l)	{ _landmark = l; }
  
  virtual void build_list(Vector<Expr *> &l, bool) const;
  virtual void declaration(Declaration *dt) const;

  enum ParseNameFlags {
    parseUniqueify = 1, parseCreate = 2, parseInitial = 4
  };
  static bool name_ok(PermString, const Landmark &, const char *);
  virtual PermString parse_name
    (Namespace *&, ModuleNames *&, const char *, int flags = 0) const;
  virtual PermString create_name_text(const char *) const;
  
  virtual void make_implicit_rules(Namespace *, Protomodule *) const { }
  virtual Node *resolve(ModuleNames *, Namespace *, ResolveArgs) const;
  Node *resolve_feature(ModuleNames *, ModuleNames *, Node *, Feature *,
			ResolveArgs) const;

  Node *resolve_ctor(ModuleNames *, Namespace *) const;
  virtual Node *resolve_ctor(ModuleNames *, Namespace *, Node *&) const;
  Node *make_construction(Node *, Node *) const;
					     
  virtual Expr *clone(const Landmark &) const;
  virtual void write(Writer &) const;
  
  virtual IdentExpr *cast_ident()		{ return 0; }
  virtual BinaryExpr *cast_binary()		{ return 0; }
  virtual ConditionalExpr *cast_conditional()	{ return 0; }
  virtual ListExpr *cast_list()			{ return 0; }
  virtual LiteralExpr *cast_literal()		{ return 0; }
  
};


class LetExpr: public Expr {
  
  Expr *_params;
  Expr *_body;
  
 public:
  
  LetExpr(Expr *p, Expr *b);
  
  void make_implicit_rules(Namespace *, Protomodule *) const;
  Node *resolve(ModuleNames *, Namespace *, ResolveArgs) const;

  void write(Writer&) const;
  
};


class BinaryExpr: public Expr {
  
  Operator _op;
  Expr *_left;
  Expr *_right;
  Expr *_optional;
  
  Node *resolve_call(ModuleNames *, Namespace *, ResolveArgs) const;
  Node *resolve_call_args(ModuleNames *, Namespace *, ResolveArgs, CallNode *)
    const;
  Node *resolve_construction(ModuleNames *, Namespace *, ResolveArgs,
			     Node *l, Node *object) const;
  Node *resolve_mod_op(ModuleNames *, Namespace *, ResolveArgs) const;
  Node *resolve_catch(ModuleNames *, Namespace *, ResolveArgs) const;
  
 public:
  
  BinaryExpr(Expr *, Operator, Expr *, Expr * = 0);
  
  Operator op() const			{ return _op; }
  Expr *left() const			{ return _left; }
  Expr *right() const			{ return _right; }
  
  void write(Writer&) const;
  
  void build_list(Vector<Expr *> &, bool) const;
  void declaration(Declaration *) const;
  PermString parse_name(Namespace *&, ModuleNames *&, const char *, int = 0)
    const;
  PermString create_name_text(const char *) const;
  void make_implicit_rules(Namespace *, Protomodule *) const;
  
  Node *resolve(ModuleNames *, Namespace *, ResolveArgs) const;
  Node *resolve_ctor(ModuleNames *, Namespace *, Node *&) const;
  
  BinaryExpr *cast_binary()		{ return this; }
  
};


class FunctionishExpr: public Expr {
  
  Operator _op;
  ListExpr *_args;

 public:

  FunctionishExpr(Operator, ListExpr *);

  void make_implicit_rules(Namespace *, Protomodule *) const;
  Node *resolve(ModuleNames *, Namespace *, ResolveArgs) const;
  
  void write(Writer&) const;
  
};


class MemberExpr: public Expr {
  
  Operator _op;
  Expr *_left;
  PermString _right;
  
 public:
  
  MemberExpr(Expr *, Operator, PermString);
  MemberExpr(PermString, const Landmark &); // prefix `.'
  
  Operator op() const			{ return _op; }
  Expr *left() const			{ return _left; }
  PermString right() const		{ return _right; }
  
  void declaration(Declaration *) const;
  PermString parse_name(Namespace *&, ModuleNames *&, const char *, int = 0)
    const;
  PermString create_name_text(const char *) const;
  void make_implicit_rules(Namespace *, Protomodule *) const;
  
  Node *resolve(ModuleNames *, Namespace *, ResolveArgs) const;
  
  void write(Writer&) const;
  
};


class UnaryExpr: public Expr {
  
  Operator _op;
  Expr *_left;
  Expr *_optional;
  
 public:
  
  UnaryExpr(Operator, Expr *l, Expr *opt);
  
  Operator op() const			{ return _op; }
  Expr *left() const			{ return _left; }
  
  void build_list(Vector<Expr *> &, bool) const;
  
  void make_implicit_rules(Namespace *, Protomodule *) const;
  Node *resolve(ModuleNames *, Namespace *, ResolveArgs) const;
  
  void write(Writer&) const;
  
};


class ConditionalExpr: public Expr {
  
  Expr *_test;
  Expr *_yes;
  Expr *_no;
  
 public:
  
  ConditionalExpr(Expr *t, Expr *y, Expr *n);

  Expr *test() const			{ return _test; }
  Expr *yes() const			{ return _yes; }
  Expr *no() const			{ return _no; }
  
  void make_implicit_rules(Namespace *, Protomodule *) const;
  Node *resolve(ModuleNames *, Namespace *, ResolveArgs) const;
  
  void write(Writer&) const;
  
  ConditionalExpr *cast_conditional()	{ return this; }
  
};


class ListExpr: public Expr {
  
  Vector<Expr *> _l;
  
 public:
  
  ListExpr()					: Expr(Landmark()) { }
  
  int count() const				{ return _l.size(); }
  Expr *elt(int i) const			{ return _l[i]; }
  void append(Expr *e)				{ _l.push_back(e); }
  
  void build_list(Vector<Expr *> &, bool) const;
  void make_implicit_rules(Namespace *, Protomodule *) const;
  
  Node *resolve(ModuleNames *, Namespace *, ResolveArgs) const;
  
  void write(Writer&) const;
  
  ListExpr *cast_list()			{ return this; }
  
};


class IdentExpr: public Expr {
  
  PermString _ident;
  
 public:
  
  IdentExpr(PermString n, const Landmark &l)	: Expr(l), _ident(n) { }
  
  PermString ident() const		{ return _ident; }
  
  void declaration(Declaration *) const;
  PermString parse_name(Namespace *&, ModuleNames *&, const char *, int = 0)
    const;
  PermString create_name_text(const char *) const;
  void make_implicit_rules(Namespace *, Protomodule *) const;
  
  Node *resolve(ModuleNames *, Namespace *, ResolveArgs) const;
  Node *resolve_ctor(ModuleNames *, Namespace *, Node *&) const;
  
  IdentExpr *cast_ident()		{ return this; }
  void write(Writer&) const;
  
};


class LiteralExpr: public Expr {
  
  Literal _literal;
  
 public:
  
  LiteralExpr(const Literal &);
  
  Type *type() const			{ return _literal.type(); }
  long vlong() const			{ return _literal.vlong(); }
  PermString vstring() const		{ return _literal.vstring(); }
  
  Node *resolve(ModuleNames *, Namespace *, ResolveArgs) const;
  
  LiteralExpr *clone(const Landmark &) const;
  void write(Writer&) const;
  
  LiteralExpr *cast_literal()		{ return this; }
  
};


class CodeExpr: public Expr {
  
  CodeBlock *_code;
  
 public:
  
  CodeExpr(CodeBlock *);
  
  Node *resolve(ModuleNames *, Namespace *, ResolveArgs) const;
  
};


class SelfExpr: public Expr {
  
  bool _super;
  
 public:
  
  SelfExpr(bool, const Landmark &);
  
  Node *resolve(ModuleNames *, Namespace *, ResolveArgs) const;
  
  SelfExpr *clone(const Landmark &) const;
  void write(Writer&) const;
  
};


inline
LiteralExpr::LiteralExpr(const Literal &lit)
  : Expr(lit.landmark()), _literal(lit)
{
}

Writer& operator<<(Writer&, const char*);

inline Writer&
operator<<(Writer& w, const Expr* e)
{
    if (e)
	e->write(w);
    else
	w << "(null)";
    return w;
}

#endif
