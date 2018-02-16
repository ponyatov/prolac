#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "codeblock.hh"
#include "compiler.hh"
#include "namespace.hh"
#include "optimize.hh"
#include "field.hh"
#include "error.hh"
#include "module.hh"
#include <cstdlib>
#include <cctype>


CodeBlock::CodeBlock(const Landmark &landmark)
  : _l((Line *)malloc(sizeof(Line) * 4)), _n(0), _cap(4), _min_offset(9999),
    _empty(true), _landmark(landmark)
{
}

CodeBlock::~CodeBlock()
{
  for (unsigned i = 0; i < _n; i++)
    delete[] _l[i].s;
  free(_l);
}


void
CodeBlock::addl(const char *s, unsigned len, unsigned offset)
{
  // Incoming code block strings must end in newline.
  assert(s[len - 1] == '\n');
  
  while (*s == ' ' && len) {
    s++;
    len--;
    offset++;
  }
  if (len > 1)
    _empty = false;
  else if (_n == 0 && _empty) {
    // Don't bother to append opening empty lines.
    _landmark = _landmark.nextline();
    return;
  }
  
  if (_n >= _cap) {
    _cap *= 2;
    _l = (Line *)realloc(_l, sizeof(Line) * _cap);
  }
  
  _l[_n].s = new char[len + 1];
  memcpy(_l[_n].s, s, len);
  _l[_n].s[len] = 0;
  
  if (offset < _min_offset)
    _min_offset = offset;
  _l[_n].offset = offset - _min_offset;
  
  _n++;
}


void 
CodeBlock::gen(Compiler *c, NodeOptimizer *opt)
{
  if (!_empty)
    c->out << "{\n" << wmindent(2);
  
  // Which line in the Prolac specification did it come from?
  c->out << wmendl << wmtab(0) << "# " << _landmark.line()
	 << " \"" << _landmark.file() << "\"\n";
  
  if (_empty)
    c->out << "/* EMPTY */;\n";
  else {
    Node *self_node = opt->do_self();
    if (!self_node)
      self_node = new SelfNode(0, *this);
    for (int i = 0; i < _chunks.size(); i++)
      _chunks[i].gen(c, self_node, opt);
    c->out << wmindent(-2) << "}\n";
  }
  
  // Switch the line number back to the right line number in the C code.
  c->out << wmendl << wmtab(0) << "# " << c->out.output_line() + 1;
  if (c->out["filename"])
    c->out << " \"" << (const char *)c->out["filename"] << "\"";
  c->out << "\n";
}


void 
CodeBlock::gen_outer(Compiler *c)
{
  c->out << wmendl << wmtab(0) << "# " << _landmark.line()
	 << " \"" << _landmark.file() << "\"\n";
  for (unsigned i = 0; i < _n; i++)
    c->out << wmspace(_l[i].offset) << _l[i].s;
}


void
CodeChunk::gen(Compiler *c, Node *self, NodeOptimizer *opt) const
{
  switch (_which) {
    
   case chOffset:
    if (_v.offset >= 0)
      c->out << wmspace(_v.offset);
    else
      c->out << wmtab(0);
    break;
    
   case chString:
    c->out.write(_v.s.s, _v.s.l);
    break;
    
   case chNode: {
     Node *value = _v.node->optimize(opt);
     c->out << "(";
     value->gen_value(c);
     c->out << ")";
     break;
   }
   
   case chSlot:
    _v.slot->gen_value(c, self);
    break;
    
   case chSelf:
    c->out << "(";
    self->gen_value(c);
    c->out << ")";
    break;

   case chNone:
    break;
    
   default:
    assert(0);
    
  }
}


/*****
 * Parsing it into code chunks
 **/

class ChunkParser {
  
  enum State {
    s_NORMAL = 0,
    
    sc_C_COMMENT,
    sc_EOL_COMMENT,
    sc_BACKQUOTE,
    sc_CHAR,
    sc_STRING,
    sc_MEMBER,
    
    s_PRE_MEMBER,
    s_MEMBER,
  };
  
  CodeBlock *_block;
  Module *_module;
  Namespace *_namesp;
  bool _is_static;
  
  State _state;
  Namespace *_cur_namesp;
  Module *_cur_module;
  
  CodeChunk handle_feature(PermString);
  
 public:
  
  ChunkParser(CodeBlock *, Module *, Namespace *, bool);
  
  void parse(char *);
  bool indent_ok() const;
  
};

static PermString::Initializer initializer;
static PermString self_string = "self";

ChunkParser::ChunkParser(CodeBlock *b, Module *m, Namespace *ns,
			 bool is_static)
  : _block(b), _module(m), _namesp(ns), _is_static(is_static),
    _state(s_NORMAL), _cur_namesp(ns), _cur_module(m)
{
}

inline bool
ChunkParser::indent_ok() const
{
  return _state != sc_STRING && _state != sc_CHAR;
}

CodeChunk
ChunkParser::handle_feature(PermString s)
{
  CodeChunk chunk;
  
  if (Feature *f = _cur_namesp->search(s)) {
    if (NodeRef *noderef = f->cast_noderef())
      chunk = CodeChunk(noderef->node());
    
    else if (Field *field = f->cast_field()) {
      if (field->is_ancestor() || field->is_import()) {
	_cur_namesp = field->modnames();
	_cur_module = field->module();
	_state = s_PRE_MEMBER;
	return CodeChunk();
      } else if (_is_static && !field->is_static())
	warning(*_block, "referring to dynamic field `%f' in static code block", field);
      else
	chunk = CodeChunk(field);
      
    } else if (Namespace *new_namesp = f->cast_namespace()) {
      _cur_namesp = new_namesp;
      _state = s_PRE_MEMBER;
      return CodeChunk();
      
    } else if (RuleRef *ruleref = f->cast_ruleref()) {
      Rule *rule = _cur_module->find_rule(ruleref);
      Node *body = 0;

      if (rule && !rule->is_static() && _is_static)
	rule = 0;
      
      // Don't report circular dependency errors here; they're OK, and usually
      // symbolize that (e.g.) a Prolac method named `free' has a C block
      // which calls `free'.
      Rule::report_circular_dependency_error = false;
      if (rule && rule->param_count() == 0 && rule->body())
	body = NodeOptimizer::to_static_constant(rule->body(), _cur_module);
      Rule::report_circular_dependency_error = true;
      
      if (body) {
	_block->add_chunk(permprintf("/* %p */", s.capsule()));
	chunk = CodeChunk(body);
      } else if (_cur_namesp != _namesp)
	warning(*_block, "C blocks can only contain static constants");
      
    } else
      warning(*_block, "strange kind of feature in code chunk");
    
  } else if (_state == s_NORMAL && s == self_string)
    chunk = CodeChunk::make_self();
  
  _cur_namesp = _namesp;
  _cur_module = _module;
  _state = s_NORMAL;
  return chunk;
}


void
ChunkParser::parse(char *s)
{
  char *last = s;
  State old_state = s_NORMAL;
  
  while (*s)
    switch (_state) {
      
     case sc_C_COMMENT:
      for (; *s; s++)
	if (s[0] == '*' && s[1] == '/') {
	  s += 2;
	  _state = s_NORMAL;
	  break;
	}
      break;
      
     case sc_EOL_COMMENT:
      while (*s)
	s++;
      break;
      
     case sc_BACKQUOTE:
      for (; *s && *s != '`'; s++)
	;
      if (*s == '`') {
	*s = ' ';
	_state = s_NORMAL;
	s++;
      }
      break;
      
     case sc_CHAR:
     case sc_STRING:
       {
	 char terminator = _state == sc_CHAR ? '\'' : '\"';
	 for (; *s && *s != terminator; s++)
	   if (*s == '\\' && s[1])
	     s++;
	 if (*s == terminator) {
	   _state = s_NORMAL;
	   s++;
	 }
	 break;
       }
     
     case sc_MEMBER:
      while (isspace(*s))
	s++;
      if (*s) {
	if (isalpha(*s) || *s == '_')
	  while (isalnum(*s) || *s == '_')
	    s++;
	_state = s_NORMAL;
      }
      break;
      
      
     case s_PRE_MEMBER:
      while (isspace(*s))
	s++;
      if (s[0] == '.') {
	s++;
	_state = s_MEMBER;
      } else if (s[0]) {
	//warning(*_block, "unresolved namespace/element reference");
	_state = s_NORMAL;
	_cur_namesp = _namesp;
      }
      break;
      
     case s_MEMBER:
      while (isspace(*s))
	s++;
      if (isalpha(*s) || *s == '_') {
	char *start = s;
	while (isalnum(*s) || *s == '_')
	  s++;
	if (CodeChunk c = handle_feature(PermString(start, s - start))) {
	  _block->add_chunk(c);
	  last = s;
	}
	
      } else if (*s) {
	//warning(*_block, "unresolved namespace/element reference");
	_state = s_NORMAL;
	_cur_namesp = _namesp;
      }
      break;
      
     case s_NORMAL:
      if (isalpha(*s) || *s == '_') {
	// HANDLE AN IDENTIFIER
	char *start = s;
	while (isalnum(*s) || *s == '_')
	  s++;
	if (CodeChunk c = handle_feature(PermString(start, s - start))) {
	  _block->add_chunk(CodeChunk(last, start - last));
	  _block->add_chunk(c);
	  last = s;
	}
	
      } else if ((*s >= '0' && *s <= '9')
		 || (*s == '.' && s[1] >= '0' && s[1] <= '9')) {
	// SKIP A PREPROCESSING NUMBER
	while (isalnum(*s) || *s == '.') {
	  if ((*s == 'E' || *s == 'e') && (s[1] == '+' || s[1] == '-'))
	    s++;
	  s++;
	}
	
      } else if (*s == '.') {
	_state = sc_MEMBER;
	s++;
	
      } else if (*s == '-' && s[1] == '>') {
	_state = sc_MEMBER;
	s += 2;
	
      } else if (*s == '\'') {
	_state = sc_CHAR;
	s++;
	
      } else if (*s == '\"') {
	_state = sc_STRING;
	s++;
	
      } else if (*s == '`') {
	_state = sc_BACKQUOTE;
	*s = ' ';
	s++;
	
      } else if (*s == '/' && s[1] == '/') {
	old_state = _state;
	_state = sc_EOL_COMMENT;
	s += 2;
	
      } else if (*s == '/' && s[1] == '*') {
	_state = sc_C_COMMENT;
	s += 2;
	
      } else if (*s)
	// SKIP IT
	s++;
      
      break;
      
    }
  
  _block->add_chunk(CodeChunk(last, s - last));
  if (_state == sc_EOL_COMMENT)
    _state = old_state;
}


void
CodeBlock::parse(Module *m, Namespace *namesp, bool is_static)
{
  assert(!_chunks.size());
  ChunkParser parser(this, m, namesp, is_static);
  for (unsigned i = 0; i < _n; i++) {
    if (parser.indent_ok())
      add_chunk(CodeChunk::make_offset(_l[i].offset));
    else
      add_chunk(CodeChunk::make_offset(-1));
    parser.parse(_l[i].s);
  }
}
