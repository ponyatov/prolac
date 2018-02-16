#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "yuck.hh"
#include "module.hh"
#include "expr.hh"
#include "declarat.hh"
#include "error.hh"
#include "program.hh"
#include "prototype.hh"
#include "codeblock.hh"
#include "node.hh"
#include "writer.hh"
#include <cstdarg>


Yuck::Yuck(Tokenizer *tokenizer, Program *prog)
  : _tokenizer(tokenizer), tpos(0), tfull(0),
    _cmodule(0), _cnamesp(prog), _prog(prog),
    _brace_is_block(0)
{
}


inline void
Yuck::save(const Token &t)
{
  tcircle[tfull] = t;
  tfull = (tfull + 1) % TstackSize;
  assert(tfull != tpos);
}

void
Yuck::savem(const Token *t, ...)
{
  va_list val;
  va_start(val, t);
  while (t) {
    save(*t);
    t = va_arg(val, const Token *);
  }
  va_end(val);
}


// TODO: test save, savem

void
Yuck::skip(int t1, int t2)
{
  Token t;
  do {
    t = lex();
  } while (!t.is(t1) && !t.is(t2) && !t.is('}') && t);
}


Expr *
Yuck::yif_expr(Expr *test)
{
  Expr *then_case = yexpr();
  Expr *else_case = 0;
  
  Token t = lex();
  if (t.kind() == opElseif) {
    // parse elseif statements
    Expr *else_test = yexpr(-1, opThen);
    if (else_test) {
      else_case = yif_expr(else_test);
      goto done;
    }
  }
  if (t.kind() == opElse) {
    else_case = yexpr();
    t = lex();
  }
  if (t.kind() != opEnd) {
    save(t);
    error(t, "bad if-then-else-end");
  }
  
 done:
  return new ConditionalExpr(test, then_case, else_case);
}


Expr *
Yuck::triple_or_expr(Expr *left, Expr *right) const
{
  if (BinaryExpr *lb = left->cast_binary()) {
    // Massage (x, y) ||| c
    // into     x, (y ||| c). This is safe evaluation-order wise and makes
    // the relative precedence of , and ||| livable.
    if (lb->op() == ',')
      // MEMLEAK lb
      return new BinaryExpr(lb->left(), ',',
			    triple_or_expr(lb->right(), right));
    if (lb->op() == opArrow)
      return new ConditionalExpr(lb->left(), lb->right(), right);
    
  } else if (ConditionalExpr *lcond = left->cast_conditional()) {
    Expr *no = triple_or_expr(lcond->no(), right);
    return new ConditionalExpr(lcond->test(), lcond->yes(), no);
  }

  return new BinaryExpr(left, opLogOr, right);
}


Expr *
Yuck::yfunctionish_expr(Operator op)
{
  Token t = lex();
  if (t.kind() != '(') {
    error(t, "operator `%o()' is called like a function", (int)op);
    return 0;
  }
  ListExpr *right = yexpr_list(')');
  return new FunctionishExpr(op, right);
}


// Warning: can consume 0 tokens.
#define ABORT do { save(t); goto done; } while (0)
#define DONE goto done

#define MOVABLE_OP(op) (*op == opArrow || *op == opTripleOr)
#define PRECCALC(p,precedence) (p < precedence || (p == precedence && !op->right_assoc()))

Expr *
Yuck::yexpr(int precedence, int terminator, int old_precedence, Operator)
{
  Expr *left = 0;
  if (old_precedence < 0) old_precedence = precedence;
  
  while (1) {
    
    Token t = lex();
    
    if (t.kind() == terminator)
      DONE;
    
    if (t.is_operator()) {
      
      // Find a prefix operator only if left is nonexistent right now.
      Operator op = t.voperator().find(left == 0);
      if (!op) {
	// Report an error if operator used in wrong context.
	error(t, "`%o' %s be used as a prefix operator", (int)t.voperator(),
	      left ? "can only" : "can't");
	ABORT;
      }
      
      int op_old_prec = op.precedence();
#ifdef PRECEDENCE_WARN
      if (op == opArrow) op_old_prec = 6;
      else if (op == opTripleOr) op_old_prec = 5;
      else if (op == '?') op_old_prec = 8;
      if (!op.prefix()) {
	bool is_prec = PRECCALC(op.precedence(), precedence);
	if (old_precedence != precedence && op.precedence() != op_old_prec) {
	  if (is_prec != PRECCALC(op_old_prec, old_precedence))
	    warning(t, "precedence change! on %o", (int)op);
	} else {
	  if (is_prec != PRECCALC(op_old_prec, precedence))
	    warning(t, "precedence change on %o", (int)op);
	  if (is_prec != PRECCALC(op->precedence(), old_precedence))
	    warning(t, "precedence change after %o", (int)old_prec_op);
	}
      }
#endif

      if (op.precedence() < precedence
	  || (op.precedence() == precedence && !op.right_assoc()))
	if (!op.prefix())
	  ABORT;
      
      if (op.functionish()) {
	left = yfunctionish_expr(op);
	continue;
      }
      
      Expr *optional = 0;
      if (op.optional_arg()) {
	// special check for prefix '[': we model prefix '[]' as being a unary
	// prefix operator with a (always-there) optional argument
	if (!t.is('[')) t = lex();
	if (t.is('['))
	  optional = yexpr(-1, ']');
	else
	  save(t);
      }
      
      Expr *right = 0;
      if (op.terminator()) {
	if (op.prefix())
	  left = yexpr(-1, op.terminator());
	else if (op.takes_list())
	  right = yexpr_list(op.terminator());
	else
	  right = yexpr(-1, op.terminator());
	
      } else {
	if (op.prefix())
	  left = yexpr(op.precedence(), opNone, op_old_prec, op);
	else if (!op.unary())
	  right = yexpr(op.right_precedence(), opNone, op_old_prec, op);
      }
      
      // can't deal with a prefix operator followed by no expression
      if (!left && op.prefix())
	continue;
      
      // handle expr.id
      if (op.takes_id()) {
	if (op.prefix())
	  right = left;
	if (right) {
	  IdentExpr *rightid = (right ? right->cast_ident() : 0);
	  if (!rightid)
	    error(*right, "`%o' followed by `%E', which is not an identifier", (int)op, right);
	  else if (op.prefix())
	    left = new MemberExpr(rightid->ident(), *right);
	  else
	    left = new MemberExpr(left, op, rightid->ident());
	}
	continue;
      }
      
      // handle normal unary & binary expressions
      switch ((int)op) {
	
       case '?': { // Ternary ?:
	 Expr *falsecase = yexpr(op.precedence(), opNone, op_old_prec, op);
	 if (right && falsecase)
	   left = new ConditionalExpr(left, right, falsecase);
	 break;
       }
       
       case opIf: // if-then-else-end
	left = yif_expr(left);
	break;
	
       case opLet:
	right = yexpr(-1, opEnd);
	if (right)
	  left = new LetExpr(left, right);
	break;
	
       case '(': {
	 // Unary parens can either be grouping parens, or casting parens.
	 // Check the next token -- the token that follows (expression). If
	 // it's an object token (identifier, literal, or expression), they
	 // are casting parens. Otherwise they are grouping parens. Casting
	 // parens are represented as function call parens. They are
	 // distinguished at the compilation stage. This is necessary because
	 // casting parens & function call parens are actually ambiguous.
	 
	 // ...BUT (8/2, 8/10) this causes ambiguity with constructor parens!
	 // Solution: store unary `casting' parens as opCastParen, not just
	 // plain '(', and slightly altering the grammar relative to C. We
	 // distinguish `(X)(Y)' from `X(Y)': if X is a module, `X(Y)' is
	 // construction, while `(X)(Y)' is a cast. Still, there are issues.

	 // ...BUT (4.Sep.1999) don't ever read a binary cast expr if the
	 // current precedence is too high to allow it.
	 Token u = lex();
	 save(u);
	 if ((u.starts_expr(true) || u.is('('))
	     && precedence < Operator(opCastParen).precedence()) {
	   right = yexpr(op.precedence());
	   if (right)
	     left = new BinaryExpr(left, opCastParen, right);
	 }
	 break;
       }
       
       case ',':
	if (right)
	  left = new BinaryExpr(left, ',', right, optional);
	// otherwise, just return `left'; it's fine -- assume that the user
	// was confused and meant a terminating comma rather than a
	// separating one (like `;' in C).
	break;
	
       case opTripleOr:
	// Triple-or is syntactic sugar, which we decode now.
	if (right)
	  left = triple_or_expr(left, right);
	break;
	
       case opPrefixBracket:
	// magically turn prefix [] into postfix []!
	left = new BinaryExpr(left, '[', optional);
	break;
	
       default:
	if (op.unary())
	  left = new UnaryExpr(op, left, optional);
	else if (right)
	  left = new BinaryExpr(left, op, right, optional);
	else
	  error(t, "`%o' requires a right operand", (int)op);
	break;
	
      }
      
    } else if (t.is(opIdentifier)) {
      
      if (left) ABORT;
      left = new IdentExpr(t, t);
      
    } else if (t.is(opExpr)) {
      
      if (left) ABORT;
      left = t.vexpr();
      
    } else if (t.is('{') && _brace_is_block) {
      // Code block.
      
      // Code blocks are essentially like nullary operators, but some special
      // processing is involved to add implicit `true's and comma operators.
      
      // {C} has precedence, but it only comes into play when it's an infix
      // operator (i.e. not a prefix operator). An example of infix code
      // block: `xxx {C}' An example of prefix code block: `xxx ==> {C}'
#ifdef PRECEDENCE_WARN
      if (left) {
	bool is_prec = (opprecCodeBlock < precedence);
	if (old_precedence != precedence && opprecCodeBlock != 7) {
	  if (is_prec != (7 < old_precedence))
	    warning(t, "precedence change! on {}");
	} else {
	  if (is_prec != (7 < precedence))
	    warning(t, "precedence change on {}");
	  if (is_prec != (opprecCodeBlock < old_precedence))
	    warning(t, "precedence change after {}");
	}
      }
#endif
      
      if (left && opprecCodeBlock < precedence)
	ABORT;
      
      CodeBlock *code = _tokenizer->get_code_block(false);
      CodeExpr *code_expr = new CodeExpr(code);
      if (left)
	left = new BinaryExpr(left, ',', code_expr);
      else
	left = code_expr;
      
      Token u = lex();
      save(u);
      // Allow anything that could possibly start a prefix expression.
      if (u.starts_expr(false)) {
	Expr *right = yexpr(opprecCodeBlock, terminator, 7, opCodeBlock);
	if (right)
	  left = new BinaryExpr(left, ',', right);
      }
      
    } else ABORT;
    
  }
  
 done:
  return left;
}

#undef ABORT
#undef DONE


inline Expr *
Yuck::yno_comma_expr()
{
  return yexpr(opprecComma);
}


ListExpr *
Yuck::yexpr_list(int terminator)
{
  ListExpr *exprlist = new ListExpr;
  int count = 0;
  while (1) {
    
    Expr *e = yno_comma_expr();
    if (e) {
      exprlist->append(e);
      if (!count++) exprlist->set_landmark(*e);
    }
    
    Token t = lex();
    if (t.is(terminator))
      return exprlist;
    else if (t.is(','))
      /*nada*/;
    else if (t.is(opIdentifier) || t.is(opExpr) || t.is_operator()) {
      // oops: the next token was part of an expression. natural assumption is
      // that a comma was missing
      error(t, "syntax error (missing comma?)");
      save(t);
      // assume a missing comma & continue processing
    } else {
      error(t, "syntax error (unclosed expression list)");
      save(t);
      return exprlist;
    }
  }
}


Expr *
Yuck::yname_expr()
{
  return yexpr(opprecName);
}


Expr *
Yuck::ytype_expr()
{
  Expr *e = yexpr(opprecType);
  return e;
}


bool
Yuck::yrule(Namespace *ns, PermString name, const Landmark &landmark,
	    bool staticp)
{
  Token t;
  
  Expr *returnexpr = 0;
  ListExpr *parameters = 0;
  
  while (1) {
    
    t = lex();
    if (t.is('(')) {
      parameters = yexpr_list(')');
      continue; // read return type if necessary
      
    } else if (t.is(opTypeDecl)) {
      returnexpr = ytype_expr();
      t = lex();
    }
    
    break;
  }
  
  if (!t.is(opRuleDef)) {
    error(landmark, "syntax error (can't find definition)");
    error(landmark, "(Did you use `;' instead of `,' inside a method body?)");
    return false;
  }
  
  _brace_is_block = true;
  Expr *bodyexpr = yexpr();
  _brace_is_block = false;
  
  if (name == "constructor") {
    if (returnexpr)
      error(*returnexpr, "constructor with return type");
    if (ns != _cmodule->modnames())
      error(landmark, "constructor in a nested namespace");
    if (staticp)
      error(landmark, "`static' constructor");
    _cmodule->add_constructor
      (landmark, parameters, bodyexpr);
    
  } else {
    Rule *rule = _cmodule->add_rule
      (name, ns, landmark, parameters, returnexpr, bodyexpr);
    rule->set_static(staticp);
  }
  
  expect(';');
  return true;
}


bool
Yuck::expect(int kind, bool reporterror)
{
  Token t = lex();
  if (!t.is(kind)) {
    if (reporterror) {
      if (kind > 32 && kind < 127)
	error(t, "expected `%c'", kind);
      else
	error(t, "expected `<internal %d>'", kind);
    }
    save(t);
    //*((int *)0) = 1;
    return false;
  } else
    return true;
}


bool
Yuck::ynamespace(Namespace *namesp, PermString name, const Landmark &landmark)
{
  Namespace *old_cnamesp = _cnamesp;
  
  if (Feature *oldf = namesp->find(name)) {
    if (Namespace *old = oldf->cast_namespace())
      _cnamesp = old;
    else {
      error(landmark, "overriding `%F' with namespace", oldf);
      _cnamesp = new Namespace(name, _cnamesp, landmark);
    }
  } else {
    // Define a new namespace.
    Namespace *newnamesp = new Namespace(name, namesp, landmark);
    namesp->def(name, newnamesp);
    _cnamesp = newnamesp;
  }
  
  while (ydefinition()) ;
  
  expect('}');
  
  _cnamesp = old_cnamesp;
  return 1;
}


void
Yuck::ymodulerelation()
{
  Expr *e = yno_comma_expr();
  if (!e) return;
  
  Declaration decl;
  e->declaration(&decl);
  if (decl.module_relation_ok()) {
    
    if (!decl.explicit_name() || !decl.head()) {
      // [N :>] T
      //if (decl.explicit_name())
      //error(*e, "old-style import (use module equation instead)");
      _cmodule->add_import(decl.name(), decl.type());
      
    } else {
      // A.B :> T
      // add redefinition.
      error(*e, "redefinitions are not supported");
    }
    
  } else
    error(*e, "syntax error (bad module relation)");
}


void
Yuck::yparent()
{
  Expr *e = yno_comma_expr();
  Declaration decl;
  e->declaration(&decl);
  if (decl.module_spec_ok())
    _cmodule->add_parent(decl.name(), decl.type());
  else
    error(*e, "syntax error (bad parent)");
}


bool
Yuck::ymodule_equation(Namespace *namesp, PermString module_name,
		       const Landmark &landmark)
{
  Expr *e = yno_comma_expr();
  Declaration decl;
  e->declaration(&decl);
  
  if (decl.module_spec_ok() && !decl.explicit_name()) {
    Prototype *proto = new Protoequate(module_name, namesp, e, landmark);
    namesp->def(module_name, proto);
    _prog->define(proto);
    return true;
  }
  
  error(*e, "syntax error (bad module equation)");
  return false;
}


bool
Yuck::ymodule_def(Namespace *namesp, PermString module_name,
		  const Landmark &landmark)
{
  Token t = lex();
  if (t.is(opRuleDef))
    return ymodule_equation(namesp, module_name, landmark);
  else
    save(t);
  
  Protomodule *module =
    new Protomodule(module_name, namesp, _prog, landmark);
  // `modulename' is defined as `module->frobbed()', module with the default
  // namespace operations (hiding implicit rules) and any user-specified ones.
  namesp->def(module_name, module->frobbed());
  _prog->define(module->frobbed());
  set_error_context("In module `%P':", module->frobbed());
  
  Protomodule *prevmodule = _cmodule;
  Namespace *prevnamesp = _cnamesp;
  _cmodule = module;
  
  bool modulerelations = false;
  bool supertypes = false;
  while (1) {
    Token tdivider = lex();
    switch ((int)tdivider.kind()) {
      
     case opHas:
      do {
	ymodulerelation();
	tdivider = lex();
      } while (tdivider.is(','));
      save(tdivider);
      modulerelations = true;
      break;
      
     case opTypeDecl:
      if (modulerelations)
	error(tdivider, "supertypes must precede module relations");
      do {
	yparent();
	tdivider = lex();
      } while (tdivider.is(','));
      save(tdivider);
      supertypes = true;
      break;
      
     case '{':
      goto modulebeginning;
      
     case 0:
      _cmodule = 0;
      goto done;
      
     default:
      error(tdivider, "what the fuckk is it?");
      skip(opHas);
      break;
      
    }
  }
  
 modulebeginning: {
    
    _cnamesp = _cmodule->modnames();
    
    while (ydefinition()) ;
    
    expect('}');
    
    PrototypeNode *proto =
      new PrototypeNode(_cmodule->frobbed(), true, *_cmodule);
    Literal literal(proto, *_cmodule);
    LiteralExpr *literal_expr = new LiteralExpr(literal);
    save(Token(literal_expr));
    Expr *e = yexpr();
    _cmodule->set_after_frob(e);
  }
  
 done:
  reset_error_context();
  _cmodule = prevmodule;
  _cnamesp = prevnamesp;
  return true;
}


void
Yuck::yexport()
{
  Expr *exporter = yexpr();
  if (_cmodule)
    error(*exporter, "syntax error (export in module)");
  else
    _prog->add_export(exporter, _cnamesp);
  expect(';');
}


bool
Yuck::yfield(Namespace *ns, PermString name, bool is_stat, const Landmark &lm)
{
  Expr *type_expr = ytype_expr();
  
  int slot_num = _cmodule->add_slot(name, ns, lm, is_stat, type_expr);
  
  Token t = lex();
  if (t.is('@')) {
    Expr *offset_expr = yexpr();
    _cmodule->set_slot_offset(slot_num, offset_expr);
  } else
    save(t);
  
  expect(';');
  return true;
}


bool
Yuck::yexception(Namespace *ns, PermString name, const Landmark &lm)
{
  _cmodule->add_exception(name, ns, lm);
  expect(';');
  return true;
}


bool
Yuck::name_ok(PermString name, bool rule, const Landmark &lm) const
{
  if (name == all_string || name == allstatic_string) {
    error(lm, "invalid name");
    return false;
  } else if (!rule && name == constructor_string) {
    error(lm, "invalid name");
    return false;
  } else
    return true;
}


bool
Yuck::ydefinition()
{
  bool is_static = false;
  bool is_field = false;
  bool is_module = false;
  bool is_exception = false;
  Token t;
  
  while (1) {
    t = lex();
    bool *flag;
    switch ((int)t.kind()) {
      
     case opModule:
      flag = &is_module;
      goto set_flag;
      
     case opField:
      flag = &is_field;
      goto set_flag;
      
     case opStatic:
      flag = &is_static;
      goto set_flag;

     case opException:
      flag = &is_exception;
      goto set_flag;
      
     set_flag:
      if (*flag)
	error(t, "syntax error (duplicate keyword)");
      *flag = true;
      break;
      
     case opIdentifier:
     case '(':
      goto done;
      
     case opExport:
      yexport();
      return true;
      
     case ';':
      break;
      
     case opLiteralCode:
       {
	 // FIXME `inline static %{'
	 CodeBlock *code = _tokenizer->get_code_block(true);
	 _prog->add_literal_code(code);
	 return true;
       }
     
     default:
      save(t);
      return false;

    }
  }
  
 done:
  
  Landmark landmark = t;
  save(t);
  Expr *name_expr = yname_expr();
  Namespace *namesp = _cnamesp;
  ModuleNames *placeholder;
  PermString name =
    name_expr->parse_name(namesp, placeholder, "feature definition",
			  Expr::parseInitial);
  if (!name)
    error(*name_expr, "%E", name_expr);
  
  // Eat a token to see if we're dealing with a namespace. Below, cases which
  // will need this token will save() it back onto the stack.
  t = lex();
  bool is_namespace = t.is('{');
  
  if (!name || !name_ok(name, !is_namespace && !is_field, landmark))
    name = inaccessible_string;
  
  if (is_module) {
    if (is_static || is_field || is_exception)
      error(t, "confusion before module");
    save(t);
    return ymodule_def(namesp, name, landmark);
    
  } else if (is_namespace) {
    if (is_static || is_field || is_exception)
      error(t, "confusion before namespace");
    return ynamespace(namesp, name, landmark);
    
  } else if (is_field) {
    if (!_cmodule)
      error(t, "field must be within some module");
    else if (!t.is(opTypeDecl) || is_exception)
      error(t, "confusion before field");
    else
      return yfield(namesp, name, is_static, landmark);
    
  } else if (is_exception) {
    if (!_cmodule)
      error(t, "exception must be within some module");
    else if (is_static)
      error(t, "exceptions cannot be static");
    else if (is_field)
      error(t, "confusion before field");
    else {
      save(t);
      return yexception(namesp, name, landmark);
    }
    
  } else {
    if (!_cmodule)
      error(t, "every method must be defined inside a module");
    else if (!t.is(opTypeDecl) && !t.is(opRuleDef) && !t.is('('))
      error(t, "confusion before method `%s'", t.print_string().c_str());
    else {
      save(t);
      return yrule(namesp, name, landmark, is_static);
    }
  }
  
  skip(';');
  return false;
}
