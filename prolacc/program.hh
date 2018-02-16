#ifndef PROGRAM_HH
#define PROGRAM_HH
#include "namespace.hh"
class Module;
class Expr;
class Compiler;
class Rule;
class CodeBlock;


enum DebugTypes {
  dtNamespace = 1,
  dtRuleset = 2,
  dtTarget = 4,
  dtLocation = 8,
  dtNode = 16,
};


class Program: public Namespace {
  
  Vector<Prototype *> _protos;
  Vector<Prototype *> _protomodules;
  
  Vector<CodeBlock *> _pre_literal_code;
  Vector<CodeBlock *> _post_literal_code;
  
  Vector<Expr *> _exports;
  Vector<Namespace *> _export_namesp;
  Vector<Rule *> _export_rules;
  
  Vector<Rule *> _all_rules;

  Prototype *_error_prototype;	// to avoid spurious error reports
  
  Program(const Program &);
  
 public:
  
  Program();

  Prototype *error_prototype() const	{ return _error_prototype; }
  
  void define(Prototype *);
  void register_protomodule(Protomodule *);
  void add_export(Expr *, Namespace *);
  void add_literal_code(CodeBlock *);
  
  void resolve_names();
  void analyze_exports();
  void resolve_code();
  
  Protomodule *find_protomodule(ModuleID) const;
  //void resolve_rule(Rule *) const;
  
  void debug_print(HashMap<PermString, int> &, int);
  void compile_structs(Writer &);
  void compile_exports(Compiler *, HashMap<PermString, int> &, int);
  
};


#endif
