#include "syntax.h"
#include <string.h>

static void ansi(const char *s){ term_write((const unsigned char*)s, strlen(s)); }
static void out(const unsigned char*s,size_t n){ if(n) term_write(s,n); }

void syntax_write_md(const unsigned char *s, size_t len){
  const char *R="\x1b[0m", *H="\x1b[36m", *C="\x1b[32m", *E="\x1b[33m", *L="\x1b[34m";
  ansi(R);
  if(len>0 && s[0]=='#'){ ansi(H); out(s,len); ansi(R); return; }
  if(len>=3 && s[0]=='`'&&s[1]=='`'&&s[2]=='`'){ ansi(C); out(s,len); ansi(R); return; }
  // highlight links [text](url)
  size_t i=0; while(i<len){ unsigned char c=s[i]; if(c=='['){ size_t st=i; while(i<len&&s[i]!=']') i++; if(i<len && i+1<len && s[i]==']' && s[i+1]=='('){ i+=2; while(i<len && s[i]!=')') i++; ansi(L); out(s+st,i-st+(i<len)); ansi(R); if(i<len) i++; continue; } i=st; }
    if(c=='*' || c=='_'){ ansi(E); out(&s[i],1); ansi(R); i++; continue; }
    out(&s[i],1); i++; }
  ansi(R);
}