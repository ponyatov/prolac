#ifndef OPERATOR_HH
#define OPERATOR_HH
#include <lcdf/permstr.hh>
class Compiler;
class Node;
class Writer;

enum OperatorValue {
  // Some groups of OperatorValues must be contiguous. They are marked with
  // a comment.
  
  // Nonexistent operator & nonexistent token.
  // The tokenizer will never return opNone
  opNone = 0,
  
  // characters = 0..255
  
  // Operator values internal to Tokenizer
  opPunctuation = 256,
  opNumber,
  opSlashStarComment,
  opEOLComment,

  opLastInternal,
  
  // Generic tokens
  opIdentifier = opLastInternal,
  opExpr,
  
  // Keywords
  opHas,
  opModule,
  opField,
  
  opStatic,
  opExport,
  
  opThen,
  opElse,
  opElseif,
  opEnd,
  opIn,
  
  opException,
  
  opRuleDef,
  opLiteralCode,
  
  // Actual operators
  opFirst,

  // must be contiguous
  opNullFrob = opFirst,
  opHide,
  opShow,
  opRename,
  opUsing,
  opNotusing,
  opNoinline2,
  opDefaultinline2,
  opInline2,
  opPathinline2,
  opRedoSelfRef,

  // must be contiguous
  opNoinline,
  opDefaultinline,
  opInline,
  opPathinline,
  opOutline,
  
  opIf,
  opLet,
  opCatch,
  
  opTypeDecl,

  opIncrement,
  opDecrement,
  opPtrMember,
  opLeftShift,
  opRightShift,

  opCall,
  opCastParen,
  opPrefixBracket,
  opPrefixDot,
  opUnaryPlus,
  opUnaryMinus,
  opAddress,
  opDeref,
  
  opMin,
  opMax,
  
  opLt, //contiguous
  opLeq,
  opGt,
  opGeq,
  
  opEq,
  opNotEq,
  
  opLogAnd, //contiguous
  opLogOr,
  opArrow,
  opTripleOr,
  
  opAssign, //contiguous; note, opAssign != '='
  opAddAssign,
  opSubAssign,
  opMulAssign,
  opDivAssign,
  opModAssign,
  opXorAssign,
  opAndAssign,
  opOrAssign,
  opLeftShiftAssign,
  opRightShiftAssign,
  opMinAssign,
  opMaxAssign,
  
  opPreIncr, //contiguous w/assignments
  opPostIncr,
  opPreDecr,
  opPostDecr,
  
  opCopy,
  
  opFirstMacro,
  
  opSeqLt = opFirstMacro, //contiguous, same order as w/o Seq
  opSeqLeq,
  opSeqGt,
  opSeqGeq,
  
  opCodeBlock,
  
  opLast,
  
  opprecComma = 5,
  opprecCodeBlock = 5,
  opprecType = 25,
  opprecName = 30,
  
};


class Operator {
  
  int _op;
  
  static PermString all_name[opLast - 256];
  static int all_precedence[opLast];
  static int all_right_precedence[opLast];
  static int all_flags[opLast];
  static Operator all_terminator[opLast];
  static Operator all_next[opLast];
  
 public:
  
  enum Flags {
    fPrefix = 1,
    fUnary = 2,
    fRightAssoc = 4,
    fTakesID = 8,
    fTakesSpecial = 16,
    fOptionalArg = 32,
    fFunctionish = 64,
    fAssignVariant = 128,
    fTakesList = 256,
    fSynonym = 512,
  };
  
  Operator()			: _op(0) { }
  Operator(int op);
  
  void set_data(PermString name);
  void set_data(PermString name, int prec, int flags, Operator terminator);
  void set_right_precedence(int);
  void insert_next_operator(Operator);
  
  operator bool() const		{ return _op != 0; }
  operator int() const		{ return _op; }
  
  PermString name() const;
  
  int precedence() const		{ return all_precedence[_op]; }
  int right_precedence() const		{ return all_right_precedence[_op]; }
  Operator terminator() const		{ return all_terminator[_op]; }
  Operator find(bool prefix) const;
  
  bool test(int flag) const		{ return all_flags[_op] & flag; }
  bool right_assoc() const		{ return test(fRightAssoc); }
  bool prefix() const			{ return test(fPrefix); }
  bool unary() const			{ return test(fUnary); }
  bool takes_id() const			{ return test(fTakesID); }
  bool takes_list() const		{ return test(fTakesList); }
  bool takes_special() const		{ return test(fTakesSpecial); }
  bool optional_arg() const		{ return test(fOptionalArg); }
  bool functionish() const		{ return test(fFunctionish); }
  bool assign_variant() const		{ return test(fAssignVariant); }
  
  bool tokenizer_internal() const;
  
  bool is_macro() const;
  PermString macro_name() const;
  
  bool module_op() const;
  bool betweenliner() const;
  bool logical() const;
  bool assignment() const;
  bool side_effects() const;
  
  Operator seqint_version() const;
  
  void gen_binary_value(Compiler *, Node *, Node *) const;
  
};

Writer &operator<<(Writer &, Operator);


inline
Operator::Operator(int op)
  : _op(op)
{
  assert(_op >= 0 && _op < opLast);
}

inline bool
Operator::tokenizer_internal() const
{
  return _op >= 256 && _op < opLastInternal;
}

inline bool
Operator::is_macro() const
{
  return _op >= opFirstMacro;
}

inline bool
Operator::module_op() const
{
  return _op >= opNullFrob && _op <= opRedoSelfRef;
}

inline bool
Operator::betweenliner() const
{
  return _op >= opNoinline && _op <= opOutline;
}

inline bool
Operator::logical() const
{
  return _op >= opLogAnd && _op <= opArrow;
}

inline bool
Operator::assignment() const
{
  return _op >= opAssign && _op <= opMaxAssign;
}

inline bool
Operator::side_effects() const
{
  return _op >= opAssign && _op <= opPostDecr;
}

#endif
