#ifndef CODEBLOCK_HH
#define CODEBLOCK_HH
#include "landmark.hh"
#include <lcdf/vector.hh>
#include "idcapsule.hh"
class Compiler;
class Namespace;
class NodeOptimizer;
class Field;
class Node;
class Module;


enum CParseState {
  cparseNormal = 0,
  cparseString,
  cparseChar,
  cparseCComment,
  cparseEOLComment,
};


class CodeChunk {
  
  enum ChunkType {
    chNone, chSelf, chOffset, chString, chNode, chSlot
  } _which;
  union {
    int offset;
    struct { const char *s; unsigned l; } s;
    Node *node;
    Field *slot;
  } _v;
  
  CodeChunk(ChunkType which)		: _which(which) { }
  
 public:
  
  CodeChunk()				: _which(chNone) { }
  CodeChunk(const char *s, unsigned l)	: _which(chString)
  					  { _v.s.s = s; _v.s.l = l; }
  CodeChunk(PermString s)		: _which(chString)
  					  { _v.s.s = s.c_str(); _v.s.l = s.length(); }
  CodeChunk(Node *n)			: _which(chNode) { _v.node = n; }
  CodeChunk(Field *f)			: _which(chSlot) { _v.slot = f; }
  CodeChunk(IDCapsule<Field> f)		: _which(chSlot) { _v.slot = f; }

  static CodeChunk make_offset(int off);
  static CodeChunk make_self()		{ return CodeChunk(chSelf); }
  
  operator bool() const			{ return _which != chNone; }
  
  void gen(Compiler *, Node *, NodeOptimizer *) const;
  
};


class CodeBlock {
  
  struct Line {
    unsigned offset;
    char *s;
    unsigned slen;
  };
  
  Line *_l;
  unsigned _n;
  unsigned _cap;
  unsigned _min_offset;
  bool _empty;
  
  Vector<CodeChunk> _chunks;
  
  Landmark _landmark;
  
 public:
  
  CodeBlock(const Landmark &);
  ~CodeBlock();
  
  operator const Landmark &() const	{ return _landmark; }
  const Landmark &landmark() const	{ return _landmark; }
  
  void addl(const char *, unsigned, unsigned);
  
  void gen(Compiler *, NodeOptimizer *);
  void gen_outer(Compiler *);

  void add_chunk(const CodeChunk &cc)	{ _chunks.push_back(cc); }
  void parse(Module *, Namespace *, bool is_static);
  
};


inline CodeChunk
CodeChunk::make_offset(int off)
{
  CodeChunk c(chOffset);
  c._v.offset = off;
  return c;
}

#endif
