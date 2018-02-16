#ifndef TOKEN_HH
#define TOKEN_HH
#include "landmark.hh"
#include "operator.hh"
#include <cstdio>

template <class K, class V> class HashMap;
class Expr;
class CodeBlock;

class Token {
  
  short _kind;
  short _cat;
  
  Landmark _landmark;
  PermString _print_string;
  
  union {
    void *p;
    PermString::Capsule string_capsule;
    Expr *expr;
  } _v;
  
  enum Categories {
    catBlank,
    catOperator,
    catString,
    catExpr,
  };
  
 public:
  
  Token(); // makes an uninitialized token
  Token(short, PermString, const Landmark &);
  Token(short, long, const Landmark &);
  Token(Operator, PermString);
  Token(Expr *);
  Token(const Token &, const Landmark &);
  
  operator bool() const			{ return _kind; }
  operator const Landmark &() const	{ return _landmark; }
  
  Operator kind() const			{ return _kind; }
  bool is(int k) const			{ return _kind == k; }
  bool is_operator() const		{ return _cat == catOperator; }
  
  bool starts_expr(bool no_infix) const;
  
  PermString vstring() const;
  operator PermString() const;
  Operator voperator() const		{ return Operator(_kind); }
  Expr *vexpr() const;
  
  void print() const;
  PermString print_string() const	{ return _print_string; }
  
};


class Tokenizer {
  
  static HashMap<PermString, Token> reserved_words;
  
  static int *char_class;
  static int char_class_storage[257];
  
  enum {
    Whitespace = 1,
    Textliteralstart = 2,
    Wordstart = 4,
    Word = 8,
    Numberstart = 16,
    Number = 32,
    Punct = 64,
  };
  
  static bool tis_whitespace(int);
  static bool tis_word_start(int);
  static bool tis_word(int);
  static bool tis_number_start(int);
  static bool tis_number(int);
  static bool tis_text_literal_start(int);
  static bool tis_punct(int);
  
  static void add_operator(Operator, PermString, int prec,
			   int flags, Operator terminator);
  static void static_initialize();
  
  //
  
  FILE *_f;
  
  static const int MaxUngot = 30;
  char _ungot[MaxUngot];
  int _ungot_pos;
  
  char *_s;
  int _slen;
  int _scap;
  
  PermString _file;
  unsigned _line;
  
  int _token_kind;
  
  unsigned _column;
  unsigned _line_non_ws;
  
  void increase_s();
  void append(int);
  void append0(int);
  void clear();
  
  int read();
  void unread(int);
  
  bool read_word(int);
  bool read_number(int);
  bool read_punct(int);
  bool check_text_literal(int);
  Token read_text_literal();
  
  bool handle_hash();
  void check_s_for_hash();
  Token handle_number();
  
 public:
  
  Tokenizer(FILE *, PermString, unsigned = 1);
  ~Tokenizer();
  
  Landmark landmark() const		{ return Landmark(_file, _line); }
  operator Landmark() const		{ return landmark(); }
  
  Token get_token();
  CodeBlock *get_code_block(bool);
  
};


inline PermString
Token::vstring() const
{
  assert(_cat == catString);
  return PermString::decapsule(_v.string_capsule);
}

inline
Token::operator PermString() const
{
  return vstring();
}

inline Expr *
Token::vexpr() const
{
  assert(_cat == catExpr);
  return _v.expr;
}

#endif
