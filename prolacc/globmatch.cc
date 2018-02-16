#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "globmatch.hh"
#include <lcdf/vector.hh>
#include <cstring>

#define OP_STAR		256
#define OP_QUESTION	257
#define OP_CHARCLASS	258

// compile a pattern into a list of integers.
static void
compile_pattern(const char *pattern, Vector<int> &compiled)
{
  const unsigned char *ucpat = (const unsigned char *)pattern;
  
  for (; *ucpat; ucpat++)
    switch (*ucpat) {

     case '*':
      compiled.push_back(OP_STAR);
      break;

     case '?':
      compiled.push_back(OP_QUESTION);
      break;

     case '[': {
       const unsigned char *old_ucpat = ucpat;
       int old_size = compiled.size();
       int cc_index = compiled.size() + 1;
       
       compiled.push_back(OP_CHARCLASS);
       compiled.resize(cc_index + 8, 0);
       ucpat++;

       bool negated;
       if (*ucpat == '^') {
	 negated = true;
	 ucpat++;
       } else
	 negated = false;
       
       for (; *ucpat && *ucpat != ']'; ucpat++) {
	 int c1, c2;
	 if (*ucpat == '\\' && ucpat[1]) ucpat++;
	 c1 = *ucpat;
	 
	 if (ucpat[1] == '-' && ucpat[2] && ucpat[2] != ']') {
	   ucpat += 2;
	   if (*ucpat == '\\' && ucpat[1]) ucpat++;
	   c2 = *ucpat;
	 } else
	   c2 = c1;
	 
	 for (int i = c1; i <= c2; i++)
	   compiled[cc_index + (c1/32)] |= (1 << (c1%32));
       }

       if (*ucpat == 0) {
	 compiled.resize(old_size);
	 ucpat = old_ucpat;
	 goto normal_case;
       }

       if (negated)
	 for (int i = 0; i < 8; i++)
	   compiled[cc_index + i] ^= 0xFFFFFFFF;
       break;
     }
     
     case '\\':
      if (ucpat[1]) {
	compiled.push_back(ucpat[1]);
	ucpat++;
      } else
	compiled.push_back('\\');
      break;

     default:
     normal_case:
      compiled.push_back(*ucpat);
      break;
      
    }
}


#define NO_AUX 0x7FFFFFFF

bool
glob_match(const char *pattern, const char *string)
{
  Vector<int> compiled;
  compile_pattern(pattern, compiled);
  
  Vector<int> star_length(compiled.size(), 0);
  Vector<int> star_pos(compiled.size(), -1);
  
  for (int p = 0; p < compiled.size(); p++) {
    if (compiled[p] == OP_STAR) star_pos[p] = 0;
    else if (compiled[p] == OP_CHARCLASS) p += 8;
  }
  
#if 0
  for (int p = 0; p < compiled.size(); p++)
    switch (compiled[p]) {
     case OP_STAR: printf("*"); break;
     case OP_QUESTION: printf("?"); break;
     case OP_CHARCLASS: {
       printf("[");
       for (int i = 0; i < 256; i++)
	 if (compiled[p + 1 + (i/32)] & (1 << (i%32))) {
	   if (i == ']' || i == '\\')
	     printf("\\%c", i);
	   else if (i > 31 && i < 127)
	     printf("%c", i);
	   else
	     printf("\\%03o", i);
	 }
       printf("]");
       p += 8;
       break;
     }
     case '[': case '*': case '?': case '\\': printf("\\%c", compiled[p]); break;
     default:
      if (compiled[p] > 31 && compiled[p] < 127)
	printf("%c", compiled[p]);
      else
	printf("\\%03o", compiled[p]);
      break;
    }
  printf("\n");
#endif
  
  int p = 0;
  int s = 0;
  int slen = strlen(string);
  const unsigned char *ucstr = (const unsigned char *)string;
  
  while (true) {
    
    // succeed for as long as possible
    for (; p < compiled.size(); p++)
      switch (compiled[p]) {
	
       case OP_STAR:
	star_pos[p] = s;
	star_length[p] = slen - s;
	s = slen;
	break;
	
       case OP_QUESTION:
	if (s < slen)
	  s++;
	else
	  goto fail;
	break;
	
       case OP_CHARCLASS: {
	 int c = ucstr[s];
	 if (s < slen && (compiled[p + 1 + (c/32)] & (1 << (c%32)))) {
	   s++;
	   p += 8;
	 } else
	   goto fail;
	 break;
       }
       
       default:
	if (s < slen && ucstr[s] == compiled[p])
	  s++;
	else
	  goto fail;
	break;
	
      }

    // if we got here, we succeeded
    if (s == slen)
      return true;
    else
      goto fail;
    
   fail:
    // if fail, then backtrack
    for (; p > 0; p--)
      // can't check that compiled[p-1] == OP_STAR because it could be a
      // OP_CHARCLASS data word!
      if (star_pos[p-1] >= 0 && star_length[p-1] > 0) {
	star_length[p-1]--;
	s = star_pos[p-1] + star_length[p-1];
	break;
      }
    
    if (p == 0)
      return false;
  }
}
