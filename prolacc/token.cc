#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "token.hh"
#include <lcdf/hashmap.hh>
#include "error.hh"
#include "type.hh"
#include "expr.hh"
#include "codeblock.hh"
#include "writer.hh"
#include <cctype>
#include <cerrno>
#include <cstdlib>

Token::Token()
  : _kind(0), _cat(catBlank)
{
}

Token::Token(short kind, PermString string, const Landmark &landmark)
  : _kind(kind), _cat(catString), _landmark(landmark),
    _print_string(string)
{
  _v.string_capsule = string.capsule();
}

Token::Token(Operator o, PermString name)
  : _kind((int)o), _cat(catOperator), _print_string(name)
{
}

Token::Token(Expr *expr)
  : _kind(opExpr), _cat(catExpr), _print_string("<expr>")
{
  _v.expr = expr;
}

Token::Token(const Token &token, const Landmark &landmark)
  : _kind(token._kind), _cat(token._cat), _landmark(landmark),
    _print_string(token._print_string)
{
  _v.p = token._v.p;
}

void
Token::print() const
{
  errwriter << _landmark << _kind << " `" << _print_string << "'\n";
}

bool
Token::starts_expr(bool no_infix) const
{
  if (_kind == opIdentifier || _kind == opExpr)
    return true;
  else if (_cat == catOperator) {
    Operator op = Operator(_kind);
    // No infix operator? Then it starts an expression.
    // OR: Has prefix operator? Then it starts an expression.
    if (no_infix ? !op.find(false) : (bool)op.find(true))
      return true;
  }
  return false;
}


///////////////////////////////////////////////////////////////////////////////

// FIXME Gcc sucks. Should be (TIdentifier)
HashMap<PermString, Token> Tokenizer::reserved_words;

int *Tokenizer::char_class;
int Tokenizer::char_class_storage[257];


void
Tokenizer::add_operator(Operator op, PermString name, int precedence,
                        int flags, Operator terminator)
{
  op.set_data(name, precedence, flags, terminator);
  Token *tp = reserved_words.findp(name);
  if (tp)
    tp->voperator().insert_next_operator(op);
  else
    reserved_words.insert(name, Token(op, name));
}

// Depends on EOF == -1.

void
Tokenizer::static_initialize()
{
  if (!reserved_words.empty())
    return;
  
#define ADDKW(name, kind) { \
	Token t(kind, name, Landmark()); \
	reserved_words.insert(name, t); \
      }
#define ADDEXPR(name, expr) { \
	Token t(expr); \
	reserved_words.insert(name, t); \
      }
#define ADDOPTERM(name, op, prec, flags, term) \
        add_operator(op, name, prec, flags, term)
#define ADDOP1(name, op, prec, flags) \
	ADDOPTERM(name, op, prec, flags, opNone)
#define ADDOP(name, op, prec) \
	ADDOPTERM(name, op, prec, 0, opNone)
  
  unsigned prefix = Operator::fPrefix;
  unsigned unary = Operator::fUnary;
  unsigned right = Operator::fRightAssoc;
  unsigned takes_id = Operator::fTakesID;
  unsigned takes_list = Operator::fTakesList;
  unsigned optional_arg = Operator::fOptionalArg;
  unsigned functionish = Operator::fFunctionish;
  unsigned assign_variant = Operator::fAssignVariant;
  unsigned synonym = Operator::fSynonym;

  // must come first, although it's got a lower precedence, so that the token
  // has kind '(', rather than opCall
  ADDOPTERM("(", '(',		29, prefix|unary, ')');

  ADDOPTERM("if", opIf,		32, prefix|unary, opThen);
  ADDOPTERM("let", opLet,	32, prefix|unary, opIn);
  ADDOP1("min",	opMin,		32, prefix|functionish|assign_variant);
  ADDOP1("max",	opMax,		32, prefix|functionish|assign_variant);
  
  ADDOP1(".",	'.',		31, takes_id);
  ADDOP1(".",	opPrefixDot,	31, prefix|unary|takes_id);
  ADDOP1("->",	opPtrMember,	31, takes_id);
  assert(opprecName + 1 ==	31);
  
  ADDOPTERM("[", '[',		30, 0, ']');
  ADDOPTERM("[", opPrefixBracket, 30, prefix|unary|optional_arg, ']');
  ADDOPTERM("(", opCall,	30, takes_list, ')');
  ADDOP1("++",	opPostIncr,	30, unary|right);
  ADDOP1("--",	opPostDecr,	30, unary|right);

  // see above
  //ADDOPTERM("(", '(',		29, prefix|unary, ')');
  
  ADDOP("hide",	opHide,		28);
  ADDOP("show",	opShow,		28);
  ADDOP("rename", opRename,	28);
  ADDOP("using", opUsing,	28);
  ADDOP("notusing", opNotusing,	28);
  ADDOP1("inline", opInline2,	28, optional_arg);
  ADDOP1("__inline", opInline2,	28, synonym);
  ADDOP("noinline", opNoinline2, 28);
  ADDOP("defaultinline", opDefaultinline2, 28);
  ADDOP("pathinline", opPathinline2, 28);
  
  ADDOP(":>",	opTypeDecl,	27);
  
  ADDOP1("&",	opAddress,	26, prefix|unary|right);
  ADDOP1("*",	opDeref,	26, prefix|unary|right);
  ADDOP1("+",	opUnaryPlus,	26, prefix|unary|right);
  ADDOP1("-",	opUnaryMinus,	26, prefix|unary|right);
  ADDOP1("~",	'~',		26, prefix|unary|right);
  ADDOP1("!",   '!',		26, prefix|unary|right);
  ADDOP1("++",	opPreIncr,	26, prefix|unary|right);
  ADDOP1("--",	opPreDecr,	26, prefix|unary|right);
  ADDOP1("inline", opInline,	26, prefix|unary|right|optional_arg);
  ADDOP1("__inline", opInline,	26, synonym);
  ADDOP1("noinline", opNoinline, 26, prefix|unary|right);
  ADDOP1("defaultinline", opDefaultinline, 26, prefix|unary|right);
  ADDOP1("pathinline", opPathinline, 26, prefix|unary|right);
  assert(opprecType + 1 ==	26);
  
  ADDOP("*",	'*',		25);
  ADDOP("/",	'/',		25);
  ADDOP("%",	'%',		25);
  
  ADDOP("+",	'+',		24);
  ADDOP("-",	'-',		24);
  
  ADDOP("<<",	opLeftShift,	23);
  ADDOP(">>",	opRightShift,	23);
  
  ADDOP("<",	opLt,		22);
  ADDOP("<=",	opLeq,		22);
  ADDOP(">",	opGt,		22);
  ADDOP(">=",	opGeq,		22);
  
  ADDOP("==",	opEq,		20);
  ADDOP("!=",	opNotEq,	20);
  
  ADDOP("&",	'&',		19);
  ADDOP("^",	'^',		18);
  ADDOP("|",	'|',		17);
  
  ADDOP("&&",	opLogAnd,	16);
  ADDOP("||",	opLogOr,	15);
  
  // XXX There is an inconsistency here! GCC and G++ put ?: here (precedence
  // level 11), but the C++ standard puts it at 8! We follow GCC for now...
  ADDOPTERM("?", '?',		11, right, ':');
  
  ADDOP1("=",	opAssign,	10, right);
  ADDOP1("+=",	opAddAssign,	10, right);
  ADDOP1("-=",	opSubAssign,	10, right);
  ADDOP1("*=",	opMulAssign,	10, right);
  ADDOP1("/=",	opDivAssign,	10, right);
  ADDOP1("%=",	opModAssign,	10, right);
  ADDOP1("^=",	opXorAssign,	10, right);
  ADDOP1("&=",	opAndAssign,	10, right);
  ADDOP1("|=",	opOrAssign,	10, right);
  ADDOP1("<<=",	opLeftShiftAssign, 10, right);
  ADDOP1(">>=",	opRightShiftAssign, 10, right);
  ADDOP1("min=", opMinAssign,	10, right);
  ADDOP1("max=", opMaxAssign,	10, right);
  
  ADDOP1("outline", opOutline,	6, prefix|unary|right|optional_arg);
  
  ADDOP(",",	',',		5);
  assert(opprecComma ==		5);
  assert(opprecCodeBlock ==	5);
  
  ADDOP1("==>",	opArrow,	3, right);
  
  ADDOP("|||",	opTripleOr,	2);
  // Prefix case bars equal grouping. (Damn you, John Jannotti!)
  //ADDOP1("|||", '(',		2, prefix|unary);
  
  ADDOP("catch", opCatch,	1);
  Operator(opCatch).set_right_precedence(opprecName);
  
  Operator(opCopy).set_data("<copy>");
  Operator(opCastParen).set_data("<cast>", Operator(opCall).precedence(), 0, ')');
  Operator(opNullFrob).set_data("<null-module-op>");
  
  ADDKW("//",	opEOLComment);
  ADDKW("/*",	opSlashStarComment);
  
  ADDKW("has", opHas);
  ADDKW("module", opModule);
  ADDKW("class", opModule);
  ADDKW("field", opField);
  ADDKW("static", opStatic);
  ADDKW("export", opExport);
  ADDKW("then", opThen);
  ADDKW("else", opElse);
  ADDKW("elseif", opElseif);
  ADDKW("end", opEnd);
  ADDKW("in", opIn);
  ADDKW("exception", opException);
  ADDKW("::=", opRuleDef);
  ADDKW("%{", opLiteralCode);
  
#define ADD_LIT_EXPR(name, ctor_args) { \
	Literal literal ctor_args; \
	ADDEXPR(name, new LiteralExpr(literal)); \
      }
  
  Landmark lm;
  
  ADD_LIT_EXPR("void", (void_type, lm));
  ADD_LIT_EXPR("bool", (bool_type, lm));
  
  ADD_LIT_EXPR("char", (char_type, lm));
  ADD_LIT_EXPR("short", (short_type, lm));
  ADD_LIT_EXPR("int", (int_type, lm));
  ADD_LIT_EXPR("long", (long_type, lm));
  
  ADD_LIT_EXPR("uchar", (uchar_type, lm));
  ADD_LIT_EXPR("ushort", (ushort_type, lm));
  ADD_LIT_EXPR("uint", (uint_type, lm));
  ADD_LIT_EXPR("ulong", (ulong_type, lm));
  
  ADD_LIT_EXPR("seqint", (seqint_type, lm));
  
  ADD_LIT_EXPR("true", (bool_type, 1L, lm));
  ADD_LIT_EXPR("false", (bool_type, 0L, lm));
  
  ADDEXPR("self", new SelfExpr(false, lm));
  ADDEXPR("super", new SelfExpr(true, lm));
  
  // 3 other keywords are treated as normal identifiers until a later stage
  // of processing:
  // all
  // allstatic
  // constructor
  
  char_class = char_class_storage + 1;
  int i;
  
  for (i = -1; i < 256; i++)
    char_class[i] = 0;
  
  char *s;
  // Don't need to make s `unsigned char *' because it will only have
  // lower-ASCII characters in it!
  for (s = " \t\r\n\f\v"; *s; s++)
    char_class[*s] |= Whitespace;
  for (s = "L\'\""; *s; s++)
    char_class[*s] |= Textliteralstart;
  for (s = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_"; *s; s++)
    char_class[*s] |= Wordstart | Word;
  for (s = "0123456789-"; *s; s++)
    char_class[*s] |= Word;
  for (s = "0123456789."; *s; s++)
    char_class[*s] |= Numberstart | Number;
  for (s = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"; *s; s++)
    char_class[*s] |= Number;
  for (s = "!#$%&()*+,-./:;<=>?@[\\]^`{|}~"; *s; s++)
    char_class[*s] |= Punct;
  
  // Make sure opNone works out OK.
  assert(opNone == 0 && char_class[opNone] == 0);
}


inline bool
Tokenizer::tis_whitespace(int c)
{
  return (char_class[c] & Whitespace) != 0;
}

inline bool
Tokenizer::tis_word_start(int c)
{
  return (char_class[c] & Wordstart) != 0;
}

inline bool
Tokenizer::tis_word(int c)
{
  return (char_class[c] & Word) != 0;
}

inline bool
Tokenizer::tis_number_start(int c)
{
  return (char_class[c] & Numberstart) != 0;
}

inline bool
Tokenizer::tis_number(int c)
{
  return (char_class[c] & Number) != 0;
}

inline bool
Tokenizer::tis_text_literal_start(int c)
{
  return (char_class[c] & Textliteralstart) != 0;
}

inline bool
Tokenizer::tis_punct(int c)
{
  return (char_class[c] & Punct) != 0;
}


/////////////////////


Tokenizer::Tokenizer(FILE *f, PermString file, unsigned line)
  : _f(f), _ungot_pos(0),
    _s(new char[256]), _slen(0), _scap(256),
    _file(file), _line(line),
    _column(0), _line_non_ws(0)
{
  static_initialize();
}


Tokenizer::~Tokenizer()
{
  delete[] _s;
}


void
Tokenizer::increase_s()
{
  char *new_s = new char[_scap * 2];
  memcpy(new_s, _s, _slen);
  delete[] _s;
  _s = new_s;
  _scap *= 2;
}


int
Tokenizer::read()
{
  int c;
  if (_ungot_pos)
    c = _ungot[--_ungot_pos];
  else
    c = fgetc(_f);
  if (c == '\n') {
    _line++;
    _column = _line_non_ws = 0;
  } else
    _column++;
  return c;
}


void
Tokenizer::unread(int c)
{
  if (c != EOF) {
    assert(_ungot_pos < MaxUngot);
    _ungot[_ungot_pos++] = c;
    //_column--;
    if (c == '\n') _line--;
  }
}


inline void
Tokenizer::append0(int c)
{
  _s[_slen++] = c;
}


inline void
Tokenizer::append(int c)
{
  if (_slen >= _scap)
    increase_s();
  _s[_slen++] = c;
}


inline void
Tokenizer::clear()
{
  _slen = 0;
}


bool
Tokenizer::read_word(int c)
{
  append(c);
  while (1) {
    c = read();
    
    if (tis_word(c)) {
      if (tis_punct(c)) {
	// Identifiers can't contain two adjacent puncts & can't end with a
	// punct. Check that here.
	int d = read();
	unread(d);
	if (!tis_word(d) || tis_punct(d))
	  goto word_end;
	// Transmute all punctuation characters to `_' here so we don't have
	// to worry about them later, when generating C code. Maybe a better
	// solution would be to keep them around for error messages...
	c = '_';
      }
      append(c);
      
    } else {
     word_end:
      unread(c);
      _token_kind = opIdentifier;
      return 1;
    }
  }
}


bool
Tokenizer::read_number(int c)
{
  // Returns a preprocessing number.
  
  if (c == '.') {
    c = read();
    if (!isdigit(c)) {
      unread(c);
      return 0;
    } else
      append('.');
  }
  
  append(c);
  for (c = read(); tis_number(c); c = read()) {
    append(c);
    if (c == 'e' || c == 'E') {
      c = read();
      if (c == '+' || c == '-')
	append(c);
      else
	unread(c);
    }
  }
  unread(c);
  
  _token_kind = opNumber;
  return 1;
}


bool
Tokenizer::read_punct(int c)
{
  int d;
  assert(_slen + 3 < _scap);
  
  append0(c);
  // Below, `$' stands for the input character.
  
  switch (c) {
    
   default:
    // illegal character
    return 0;
    
   case '~':
   case '(':
   case ')':
   case ',':
   case ';':
   case '?':
   case '{':
   case '}':
   case '[':
   case ']':
   case '@':
   case '\n': // \n is made into a Punct when processing #-directives.
    // Always single-character tokens (`$')
    break;
    
   case '!':
   case '*':
   case '^':
    // Either `$' or `$='
    goto maybe_equal;
    
   case '%':
    // Either `%', `%=', or `%{'
    c = read();
    if (c == '{' || c == '=')
      append0(c);
    else
      unread(c);
    break;
    
   case '<':
   case '>':
    // Either `$', `$$', `$=', or `$$='
    // (the relational and shift operators)
    d = read();
    if (d == c)
      append0(d);
    else
      unread(d);
    goto maybe_equal;
    
   maybe_equal:
    // Check for an equal sign
    c = read();
    if (c == '=')
      append0(c);
    else
      unread(c);
    break;
    
   case '|':
    // Either `$', `$=', `$$', or `$$$'
    d = read();
    if (d == c) {
      append0(d);
      d = read();
      if (d == c)
	append0(d);
      else
	unread(d);
    } else if (d == '=')
      append0(d);
    else
      unread(d);
    break;
    
   case '&':
   case '+':
    // Either `$', `$=', or `$$'
    d = read();
    if (d == c || d == '=')
      append0(d);
    else
      unread(d);
    break;
    
   case '#':
    // Either `$' or `$$'
    d = read();
    if (d == c)
      append0(d);
    else
      unread(d);
    break;
    
   case '-':
    // Special case: Either `-', `--', `-=', or `->'
    c = read();
    if (c == '-' || c == '=' || c == '>')
      append0(c);
    else
      unread(c);
    break;
    
   case ':':
    // Special case: Either `:', `::', `::=', or `:>'
    c = read();
    if (c == ':') {
      append0(c);
      goto maybe_equal;
    } else if (c == '>')
      append0(c);
    else
      unread(c);
    break;
    
   case '=':
    // Special case: Either `=', `==', or `==>'
    c = read();
    if (c == '=') {
      append0(c);
      c = read();
      if (c == '>') {
	append0(c);
	break;
      }
    }
    unread(c);
    break;
    
   case '/':
    // Special case: / may introduce either of two forms of comment, or /=
    c = read();
    if (c == '/' || c == '*' || c == '=')
      append0(c);
    else
      unread(c);
    break;
    
   case '.':
    // Only two forms allowed: `.' and `...'. Two dots are expanded to three.
    c = read();
    if (c == '.') {
      append0(c);
      append0(c);
      c = read();
      if (c != '.')
	// FIXME report error?
	unread(c);
    } else
      unread(c);
    break;
    
  }
  
  // The punctuation token is in s.
  _token_kind = opPunctuation;
  return 1;
}


bool
Tokenizer::check_text_literal(int c)
{
  // Accept, but ignore, an "L" denoting a long string constant.
  // Leave the single or double quote as the first character to be read.
  if (c == 'L') {
    c = read();
    unread(c);
    return (c == '\'' || c == '\"');
  } else {
    assert(c == '\'' || c == '\"');
    unread(c);
    return 1;
  }
}

Token
Tokenizer::read_text_literal()
{
  int terminator = read();
  assert(terminator == '\'' || terminator == '\"');
  int ischar = (terminator == '\'');
  
  while (1) {
    int c = read();
    
    if (c == '\n') {
      
      warning(*this, "ANSI C forbids newline in %s constant",
		ischar ? "character" : "string");
      // But fall through to the end, where we append the \n to the string.
      
    } else if (c == EOF) {
      
      error(*this, "unexpected end of file");
      goto done;
      
    } else if (c == terminator) {

      goto done;
      
    } else if (c == '\\') {
      
      c = read();
      switch (c) {
	
       case '\n':
	// \(newline) contracts to nothing.
	continue;
	
       case '\'': case '\"': case '\\': case '?':
	break;
	
       case 'a': c = '\a'; break;
       case 'b': c = '\b'; break;
       case 'f': c = '\f'; break;
       case 'n': c = '\n'; break;
       case 'r': c = '\r'; break;
       case 't': c = '\t'; break;
       case 'v': c = '\v'; break;
	
       case 'x': {
	 int i = read();
	 c = 0;
	 while (1) {
	   if (i >= '0' && i <= '9')
	     c = c * 16 + i - '0';
	   else if (i >= 'a' && i <= 'f')
	     c = c * 16 + i - 'a' + 10;
	   else if (i >= 'A' && i <= 'F')
	     c = c * 16 + i - 'A' + 10;
	   else
	     break;
	 }
	 break;
       }
       
       case '0': case '1': case '2': case '3':
       case '4': case '5': case '6': case '7':
	 {
	   int i = c;
	   c = 0;
	   for (int j = 0; j < 3 && i >= '0' && i <= '7'; j++, i = read())
	     c = c * 8 + i - '0';
	   unread(i);
	   break;
	 }
       
       default:
	warning(*this, "unknown escape sequence");
	break;
	
      }
      
    }
    
    append(c);
  }

 done:
  if (ischar) {
    if (_slen != 1)
      error(*this, "long character constant (only using first character)");
    Literal lit(char_type, (long)((unsigned char)_s[0]), *this);
    return Token(new LiteralExpr(lit));
  } else {
    Literal lit(ptr_char_type, PermString(_s, _slen), *this);
    return Token(new LiteralExpr(lit));
  }
}


bool
Tokenizer::handle_hash()
{
  if (_line_non_ws != 1)
    return 0;
  
  // deal now with `#'.
  // Make \n a special token for end-of-line; it won't be eaten as space.
  char_class['\n'] = Punct;
  
  // Now read either `# NUMBER ["string"]' or `# line NUMBER ["string"]'.
  
  Token t = get_token();
  LiteralExpr *lit;
  
  if (t.is(opIdentifier)) {
    if (t.vstring() != "line")
      goto error;
    t = get_token();
  }

  // XXX this is not particularly robust
  
  // parse line number
  if (!t.is(opExpr))
    goto error;
  
  lit = t.vexpr()->cast_literal();
  if (!lit || lit->type() != int_type)
    goto error;
  
  _line = lit->vlong() - 1;

  // parse optional filename
  t = get_token();
  if (!t.is(opExpr))
    goto await_nl;

  lit = t.vexpr()->cast_literal();
  if (!lit || lit->type() != ptr_char_type)
    goto await_nl;

  _file = lit->vstring();
  
 await_nl:
  while (t.kind() != '\n' && t)
    t = get_token();
  
  // return the binding of '\n' to normal.
  char_class['\n'] = Whitespace;
  // reset _line_non_ws to 0, since we just finished a line. If we don't do
  // this, we can't parse two adjacent #line directives.
  _line_non_ws = 0;
  return 1;
  
 error:
  error(*this, "unknown # directive");
  goto await_nl;
}


Token
Tokenizer::handle_number()
{
  bool force_int = false;
  bool force_real = false;
  bool force_hex = _s[0] == '0' && (_s[1] == 'x' || _s[1] == 'X');
  bool is_unsigned = false;
  bool is_long = false;
  bool is_float = false;
  bool is_seqint = false;
  
  // handle suffixes
  for (; _slen >= 0; _slen--)
    switch (_s[_slen - 1]) {
      
     case 'L': case 'l':
      if (is_long || is_seqint || force_real)
	warning(*this, "garbage at end of number");
      is_long = true;
      break;
      
     case 'U': case 'u':
      if (is_unsigned || is_seqint || force_real)
	warning(*this, "garbage at end of number");
      warning(*this, "unsigned not yet supported");
      force_int = is_unsigned = true;
      break;

     case 'Q': case 'q':
      if (is_seqint || is_unsigned || is_long || force_real)
	warning(*this, "garbage at end of number");
      force_int = is_seqint = true;
      break;
      
     case 'F': case 'f':
      if (force_hex)
	goto main;
      if (force_int)
	warning(*this, "garbage at end of number");
      force_real = is_float = true;
      break;
      
     default:
      goto main;
      
    }
  
 main:
  char *rest;
  _s[_slen] = 0;
  errno = 0;
  
  long val = strtol(_s, &rest, 0);
  if (errno == ERANGE)
    warning(*this, "integer constant out of range");
  
  if (force_real || *rest == '.' || *rest == 'e' || *rest == 'E')
    error(*this, "floating constants not currently supported");
  else if (*rest)
    error(*this, "floating constant or garbage at end of number");
  
  Literal value_literal;
  if (is_seqint) 
    value_literal = Literal(seqint_type, val, landmark());
  else if (is_long)
    value_literal = Literal(long_type, val, landmark());
  else
    value_literal = Literal(int_type, val, landmark());
  
  LiteralExpr *expr = new LiteralExpr(value_literal);
  return Token(expr);
}


void
Tokenizer::check_s_for_hash()
{
  char *s = _s;
  
  while (isspace(*s)) s++;
  if (*s++ != '#') return;
  
  while (isspace(*s)) s++;
  if (s[0] == 'l' && s[1] == 'i' && s[2] == 'n' && s[3] == 'e') s += 4;
  while (isspace(*s)) s++;
  
  if (!isdigit(*s)) return;
  // Don't subtract 1 because we've already read the `\n' which terminates
  // this line; we're effectively, therefore, on the next line.
  _line = strtol(s, &s, 10);
  
  while (isspace(*s)) s++;
  if (*s++ != '\"') return;
  
  char *start = s;
  // FIXME backslashes in filenames
  while (*s && *s != '\"') s++;
  _file = PermString(start, s - start);
}


CodeBlock *
Tokenizer::get_code_block(bool is_percent)
{
  assert(_column);
  unsigned offset = is_percent ? 0 : _column - 1;
  CodeBlock *cb = new CodeBlock(landmark());
  
  unsigned block_nest = 1;
  int c;
  CParseState c_state = cparseNormal;
  
  do {
    clear();
    CParseState initial_state = c_state;
    
    for (c = read(); c != EOF && c != '\n'; c = read())
      switch (c_state) {
	
       case cparseChar:
       case cparseString:
	append(c);
	if ((c == '\'' && c_state == cparseChar)
	    || (c == '\"' && c_state == cparseString))
	  c_state = cparseNormal;
	else if (c == '\\') {
	  c = read();
	  if (c != '\n' && c != EOF)
	    append(c);
	  else
	    unread(c);
	}
	break;
	
       case cparseEOLComment:
	append(c);
	break;
	
       case cparseCComment:
	 if (c != '*')
	   append(c);
	 else {
	   while (c == '*') {
	     append(c);
	     c = read();
	   }
	   if (c == '/') {
	     append(c);
	     c_state = cparseNormal;
	   } else
	     unread(c);
	 }
	 break;
	 
       case cparseNormal:
	if (!is_percent && c == '{')
	  block_nest++;
	else if (c == '}') {
	  if (!is_percent && --block_nest == 0)
	    goto loop_done;
	  else if (is_percent && _slen && _s[_slen-1] == '%') {
	    _slen--;
	    block_nest = 0;
	    goto loop_done;
	  }
	} else if (c == '\'')
	  c_state = cparseChar;
	else if (c == '\"')
	  c_state = cparseString;
	
	append(c);
	
	if (c == '/') {
	  c = read();
	  if (c == '/') {
	    c_state = cparseEOLComment;
	    append(c);
	  } else if (c == '*') {
	    c_state = cparseCComment;
	    append(c);
	  } else
	    unread(c);
	}	
	break;
	
       default:
	assert(0);
	
      }
    
   loop_done:
    append('\n');
    if (c_state == cparseEOLComment)
      c_state = cparseNormal;
    
    if (initial_state == cparseNormal)
      check_s_for_hash();
    cb->addl(_s, _slen, offset);
    offset = 0;
    
  } while (c != EOF && block_nest);
  
  return cb;
}


Token
Tokenizer::get_token()
{
  while (1) {
    
    clear();
    int c = read();
    while (tis_whitespace(c))
      c = read();
    _line_non_ws++;
    
    _token_kind = 0;
    do {
      if (tis_text_literal_start(c) && check_text_literal(c))
	return read_text_literal();
      if (tis_word_start(c) && read_word(c))
	break;
      if (tis_number_start(c) && read_number(c))
	break;
      if (tis_punct(c) && read_punct(c))
	break;
    } while (0);
    
    if (_token_kind == 0) {
      if (c == EOF)
	return Token(0, "", landmark());
      else
	error(*this, "illegal character `%c'", c);
      continue;
    }
    
    PermString string(_s, _slen);
    
    if (_token_kind == opIdentifier || _token_kind == opPunctuation) {
      
      // Look up identifiers or punctuations in our reserved words hash table.
      Token *tokp = reserved_words.findp(string);
      
      if (tokp) {
	
	// Check for assign variants: because `min=' won't actually be parsed
	// as a single token.
	if (tokp->is_operator() && tokp->voperator().assign_variant()) {
	  c = read();
	  if (c == '=') {
	    append(c);
	    string = PermString(_s, _slen);
	    tokp = reserved_words.findp(string);
	    assert(tokp);
	  } else
	    unread(c);
	}
	
	if (tokp->kind() == opExpr)
	  return Token(tokp->vexpr()->clone(landmark()));
	
	else if (!tokp->kind().tokenizer_internal())
	  return Token(*tokp, landmark());
	
	else
	  _token_kind = tokp->kind();
	
      } else if (_token_kind == opPunctuation) {
	// If we don't find a punctuation in the table,
	// it was a one-character punctuation, whose value is just that
	// character.
	assert(_slen == 1);
	_token_kind = _s[0];
      }
      
    }
    
    switch (_token_kind) {
      
     case opEOLComment:
      do {
	c = read();
      } while (c != '\n' && c != EOF);
      break;
      
     case opSlashStarComment:
      do {
	while (c != '*' && c != EOF)
	  c = read();
	while (c == '*')
	  c = read();
      } while (c != EOF && c != '/');
      break;
      
     case opNumber:
      return handle_number();
      
     case '#':
      if (!handle_hash())
	return Token(_token_kind, string, landmark());
      break;
      
     case opIdentifier:
      return Token(opIdentifier, string, landmark());
      
     default:
      return Token(_token_kind, string, landmark());
      
    }
    
  }
}
