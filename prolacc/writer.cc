#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "writer.hh"


Writer errwriter(stderr);
Writer outwriter(stdout);

WriterManip wmendl(WriterManip::wmEndl);
WriterManip wm_expand_nodes(WriterManip::wmExpandNodes, 1);
WriterManip wm_no_expand_nodes(WriterManip::wmExpandNodes, 0);
WriterManip wmnodesep(WriterManip::wmNodeSep);
WriterManip wmpushindent(WriterManip::wmPushIndent);
WriterManip wmpopindent(WriterManip::wmPopIndent);

Writer&
operator<<(Writer& w, bool b)
{
    return w << (b ? "true" : "false");
}

Writer&
operator<<(Writer& w, int i)
{
    return w << (long)i;
}

Writer&
operator<<(Writer& w, unsigned int u)
{
    return w << (unsigned long)u;
}

Writer&
operator<<(Writer& w, long l)
{
    char buf[BUFSIZ];
    int len;
    if (w.next_width())
	len = sprintf(buf, "%*ld", w.next_width(), l);
    else
	len = sprintf(buf, "%ld", l);
    w.write(buf, len);
    w.clear_next_width();
    return w;
}

Writer&
operator<<(Writer& w, unsigned long ul)
{
    char buf[BUFSIZ];
    int len;
    if (w.next_width())
	len = sprintf(buf, "%*lu", w.next_width(), ul);
    else
	len = sprintf(buf, "%lu", ul);
    w.write(buf, len);
    w.clear_next_width();
    return w;
}

Writer&
operator<<(Writer& w, void *v)
{
    char buf[BUFSIZ];
    int len = sprintf(buf, "%p", v);
    w.write(buf, len);
    return w;
}


Writer::Writer(FILE *f)
    : _f(f), _line(1),
      _buf(new char[256]), _buf_pos(0), _buf_cap(256),
      _level(0), _next_hang(0), _next_width(0), _pos(0),
      _extras((void *)0)
{
}

Writer::~Writer()
{
  if (_pos)
    output_buf_line();
  fflush(_f);
  delete[] _buf;
}


void
Writer::extend_buf(int by)
{
  if (_buf_cap >= _buf_pos + by)
    return;
  while (_buf_cap < _buf_pos + by)
    _buf_cap *= 2;
  char *new_buf = new char[_buf_cap];
  memcpy(new_buf, _buf, _buf_pos);
  delete[] _buf;
  _buf = new_buf;
}

inline void
Writer::bufc0(int c)
{
  _buf[_buf_pos++] = c;
}

inline void
Writer::bufs0(const char *s, int len)
{
  memcpy(_buf + _buf_pos, s, len);
  _buf_pos += len;
}

inline void
Writer::bufc(int c)
{
  if (_buf_pos >= _buf_cap) extend_buf(1);
  bufc0(c);
}

inline void
Writer::bufs(const char *s, int len)
{
  if (_buf_pos + len > _buf_cap) extend_buf(len);
  bufs0(s, len);
}

inline void
Writer::output_buf_line()
{
  bufc('\n');
  fwrite(_buf, 1, _buf_pos, _f);
  _buf_pos = _pos = 0;
  _line++;
}

char *
Writer::steal_buf()
{
  char *n = new char[_buf_pos + 1];
  memcpy(n, _buf, _buf_pos);
  n[_buf_pos] = 0;
  _buf_pos = _pos = 0;
  return n;
}

void
Writer::write(const char *s, int len)
{
  const char *start = s;
  extend_buf(len);
  
  for (int i = 0; i < len; s++, i++) {
    if (*s == '\n') {
      if (s > start)
	bufs0(start, s - start);
      output_buf_line();
      start = s + 1;
      
    } else if (_pos == 0 && s == start && _level + _next_hang) {
      _pos = _level + _next_hang;
      extend_buf(_pos);
      for (int p = 0; p < (_pos & ~7); p += 8)
	bufc0('\t');
      for (int p = _pos & ~7; p < _pos; p++)
	bufc0(' ');
    }
    
    _next_hang = 0;
  }
  
  if (s > start) {
    bufs0(start, s - start);
    _pos += s - start;
  }
}


void
Writer::manip(const WriterManip &wm)
{
    switch (wm.kind()) {
    
      case WriterManip::wmIndent:
	_level += wm.data();
	break;

      case WriterManip::wmPushIndent:
	_indent_stack.push_back(_level);
	_level = _pos;
	break;

      case WriterManip::wmPopIndent:
	if (_indent_stack.size()) {
	    _level = _indent_stack.back();
	    _indent_stack.pop_back();
	} else
	    _level = 0;
	break;
	
      case WriterManip::wmHang:
	_next_hang = -wm.data();
	break;
    
      case WriterManip::wmTab:
	if (_pos < wm.data()) {
	    int tab_to = wm.data() & ~7;
	    for (int p = _pos & ~7; p < tab_to; p += 8)
		bufc('\t');
	    for (int p = (tab_to > _pos ? tab_to : _pos); p < wm.data(); p++)
		bufc(' ');
	    _pos = wm.data();
	} else if (wm.data() == 0 && _pos == 0)
	    _next_hang = -_level;
	break;
    
      case WriterManip::wmEndl:
	if (_pos)
	    output_buf_line();
	break;
    
      case WriterManip::wmWidth:
	_next_width = wm.data();
	break;
    
      case WriterManip::wmSpace:
	if (_pos == 0)
	    _next_hang += wm.data();
	else {
	    for (int i = 0; i < wm.data(); i++) bufc(' ');
	    _pos += wm.data();
	}
	break;

      case WriterManip::wmExpandNodes:
	_extras.insert("ExpandNodes", (void *)wm.data());
	break;
    
      case WriterManip::wmNodeSep:
	if (_extras["ExpandNodes"])
	    *this << wmendl;
	else
	    bufc(' ');
	break;
    
    }
}
