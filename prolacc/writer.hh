#ifndef WRITER_HH
#define WRITER_HH
#include <cstdio>
#include <cstring>
#include <lcdf/permstr.hh>
#include <lcdf/hashmap.hh>
#include <lcdf/vector.hh>

class WriterManip { public:

    enum Kind {
	wmIndent,
	wmHang,
	wmTab,
	wmWidth,
	wmEndl,
	wmSpace,
	wmExpandNodes,
	wmNodeSep,
	wmPushIndent,
	wmPopIndent,
    };

    explicit WriterManip(Kind k, int d = 0) : _kind(k), _data(d) { }
  
    Kind kind() const			{ return _kind; }
    int data() const			{ return _data; }
  
  private:
  
    Kind _kind;
    int _data;

};


WriterManip wmindent(int);
WriterManip wmhang(int);
WriterManip wmtab(int);
WriterManip wmwidth(int);
WriterManip wmspace(int);
WriterManip wmexpandnodes(bool);
extern WriterManip wmendl;
extern WriterManip wmnodesep;
extern WriterManip wmpushindent;
extern WriterManip wmpopindent;


class Writer { public:
  
    Writer(FILE *f);
    ~Writer();

    void *&operator[](PermString x)	{ return _extras.find_force(x); }
    unsigned output_line() const	{ return _line; }

    char *steal_buf();
  
    int next_width() const		{ return _next_width; }
    void clear_next_width()		{ _next_width = 0; }
  
    void write(const char *, int);
    void manip(const WriterManip &);

  private:

    FILE *_f;
    unsigned _line;
  
    char *_buf;
    int _buf_pos;
    int _buf_cap;

    int _level;
    int _next_hang;
    int _next_width;
    int _pos;

    HashMap<PermString, void *> _extras;
    Vector<int> _indent_stack;
    
    Writer(const Writer &);
    Writer &operator=(const Writer &);
  
    void extend_buf(int);
    void bufc0(int c);
    void bufs0(const char *s, int);
    void bufc(int c);
    void bufs(const char *s, int);
    void output_buf_line();
      
};


extern Writer errwriter;
extern Writer outwriter;

inline Writer &
operator<<(Writer &w, const char *s)
{
    w.write(s, strlen(s));
    return w;
}

inline Writer &
operator<<(Writer &w, PermString s)
{
    if (s)
	w.write(s.c_str(), s.length());
    return w;
}

inline Writer &
operator<<(Writer &w, const WriterManip &wm)
{
    w.manip(wm);
    return w;
}

inline Writer &
operator<<(Writer &w, char c)
{
    w.write(&c, 1);
    return w;
}

Writer& operator<<(Writer&, bool);
Writer& operator<<(Writer&, int);
Writer& operator<<(Writer&, unsigned int);
Writer& operator<<(Writer&, long);
Writer& operator<<(Writer&, unsigned long);
Writer& operator<<(Writer&, void*);
Writer& operator<<(Writer&, const char*);


inline WriterManip
wmindent(int amt)
{
    return WriterManip(WriterManip::wmIndent, amt);
}

inline WriterManip
wmhang(int amt)
{
    return WriterManip(WriterManip::wmHang, amt);
}

inline WriterManip
wmtab(int to)
{
    return WriterManip(WriterManip::wmTab, to);
}

inline WriterManip
wmwidth(int w)
{
    return WriterManip(WriterManip::wmWidth, w);
}

inline WriterManip
wmspace(int s)
{
    return WriterManip(WriterManip::wmSpace, s);
}

inline WriterManip
wmexpandnodes(bool y)
{
    return WriterManip(WriterManip::wmExpandNodes, y);
}

#endif
