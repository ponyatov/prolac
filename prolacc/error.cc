#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "error.hh"
#include "token.hh"
#include "module.hh"
#include "operator.hh"
#include "rule.hh"
#include "field.hh"
#include "writer.hh"
#include "prototype.hh"
#include "expr.hh"
#include "node.hh"
#include "exception.hh"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cctype>


static bool need_error_context;
static char *error_context_msg;
int num_errors;
int num_warnings;


static void
print_error_context(const Landmark &l)
{
  if (need_error_context && error_context_msg) {
    fprintf(stderr, "%s: %s\n", l.file().c_str(), error_context_msg);
    need_error_context = false;
  }
}

#define L_WARNING 1
#define L_ERROR 2
#define L_SPECIAL -1

static void
verror(const Landmark &l, int level, const char *errfmt, va_list val)
{
  char crapbuf[256];
  
  if (level != L_SPECIAL)
    print_error_context(l);
  
  if (l)
    errwriter << l;
  if (level == L_WARNING)
    errwriter << "warning: ";
  
  while (*errfmt) {
    
    const char *nx = strchr(errfmt, '%');
    if (!nx) nx = strchr(errfmt, 0);
    errwriter.write(errfmt, nx - errfmt);
    errfmt = nx;
    if (!*errfmt) break;
    
    int dashes = 0;
    int stars = 0;
    
   reswitch:
    switch (*++errfmt) {
      
     case '-':
      dashes++;
      goto reswitch;

     case '*':
      stars++;
      goto reswitch;
      
     case 's': {
       const char *x = va_arg(val, const char *);
       errwriter << (x ? x : "(null)");
       break;
     }
     
     case 'd': {
       int x = va_arg(val, int);
       errwriter << x;
       break;
     }

     case 'E': {
       Expr *e = va_arg(val, Expr *);
       if (e) e->write(errwriter);
       else errwriter << "(null)";
       break;
     }

     case 'N': {
       Node *n = va_arg(val, Node *);
       if (n) n->write(errwriter);
       else errwriter << "(null)";
       break;
     }

     case 'S': {
       int x = va_arg(val, int);
       if (x != 1) errwriter << 's';
       break;
     }
     
     case 'c': {
       int x = va_arg(val, int) & 0xFF;
       if (x >= 32 && x <= 126)
	 errwriter << (char)x;
       else if (x < 32)
	 errwriter << '^' << (char)(x + 64);
       else {
	 sprintf(crapbuf, "\\%03o", x);
	 errwriter << crapbuf;
       }
       break;
     }
     
     case 'm': {
       Module *m = va_arg(val, Module *);
       if (m) m->write(errwriter);
       else errwriter << "(null)";
       break;
     }

     case 'n': {
       Namespace *ns = va_arg(val, Namespace *);
       if (dashes) {
	 if (ns) ns->write_sub(errwriter);
       } else {
	 if (ns) ns->write(errwriter);
	 else errwriter << "(null)";
       }
       break;
     }
     
     case 'e': {
       Exception *e = va_arg(val, Exception *);
       if (e) errwriter << e->name();
       else errwriter << "(null)";
       break;
     }

     case 'o': {
       int op = va_arg(val, int);
       errwriter << Operator(op).name();
       break;
     }
     
     case 't': {
       Type *t = va_arg(val, Type *);
       if (t) errwriter << t;
       else errwriter << "(null)";
       break;
     }
     
     case 'r': {
       Rule *r = va_arg(val, Rule *);
       if (r) {
	 if (dashes) r->write_name(errwriter);
	 else r->write(errwriter);
	 if (stars) {
	   errwriter << '(';
	   for (int i = 0; i < r->param_count(); i++) {
	     if (i) errwriter << ", ";
	     errwriter << r->param_type(i);
	   }
	   errwriter << ')';
	 }
       } else errwriter << "(null)";
       break;
     }
     
     case 'f': {
       Field *f = va_arg(val, Field *);
       if (f) {
	 if (dashes) errwriter << f->description();
	 else errwriter << f->basename();
       } else errwriter << "(null)";
       break;
     }
     
     case 'F': {
       Feature *f = va_arg(val, Feature *);
       if (f) {
	 if (dashes) errwriter << f->description();
	 else f->write(errwriter);
       } else errwriter << "(null)";
       break;
     }
     
     case 'p': {
       void *p = va_arg(val, void *);
       sprintf(crapbuf, "%p", p);
       errwriter << crapbuf;
       break;
     }
     
     case 'P': {
       Prototype *p = va_arg(val, Prototype *);
       if (p) p->write(errwriter);
       else errwriter << "(null)";
       break;
     }
     
     case '%':
      errwriter << '%';
      break;
      
     case 0:
     default:
      assert(0 && "BAD %");
      break;
      
    }
    
    errfmt++;
  }
  
  if (level != L_SPECIAL)
    errwriter << wmendl;
}


bool
error(const Landmark &l, const char *errfmt, ...)
{
  va_list val;
  va_start(val, errfmt);
  verror(l, L_ERROR, errfmt, val);
  va_end(val);
  num_errors++;
  return false;
}


bool
warning(const Landmark &l, const char *errfmt, ...)
{
  va_list val;
  va_start(val, errfmt);
  verror(l, L_WARNING, errfmt, val);
  va_end(val);
  num_warnings++;
  return false;
}


void
set_error_context(const char *errfmt, ...)
{
  errwriter << wmendl;
  va_list val;
  va_start(val, errfmt);
  verror(Landmark(), L_SPECIAL, errfmt, val);
  va_end(val);
  need_error_context = true;
  delete[] error_context_msg;
  error_context_msg = errwriter.steal_buf();
}

void
reset_error_context()
{
  need_error_context = false;
}
