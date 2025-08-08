#include "syntax.h"
#include <string.h>

static void ansi(const char *s){ term_write((const unsigned char*)s, strlen(s)); }
static void out(const unsigned char*s,size_t n){ if(n) term_write(s,n); }
static int is_l(char c){return ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||c=='-');}
static int is_an(char c){return is_l(c)||(c>='0'&&c<='9');}

void syntax_write_ini(const unsigned char *s, size_t len){
  const char *R="\x1b[0m", *K="\x1b[36m", *C="\x1b[90m", *S="\x1b[32m";
  ansi(R);
  // Section headers [section]
  if(len>0 && s[0]=='['){ ansi(K); out(s,len); ansi(R); return; }
  // Comments begin with # or ;
  if(len>0 && (s[0]=='#' || s[0]==';')){ ansi(C); out(s,len); ansi(R); return; }
  // Key = value
  size_t i=0; if(i<len && is_l((char)s[i])){
    size_t st=i; i++; while(i<len&&is_an((char)s[i])) i++;
    if(i<len && (s[i]=='='||s[i]==':')){ ansi(K); out(s+st,i-st); ansi(R); out(&s[i],1); i++; }
  }
  // rest: highlight strings and comments trailing
  int str=0; while(i<len){ unsigned char c=s[i];
    if(!str&&(c=='"'||c=='\'')){ str=c; ansi(S); out(&s[i++],1); continue; }
    if(str){ if(c=='\\'&&i+1<len){ out(&s[i],2); i+=2; continue; } out(&s[i],1); if(c==str){ str=0; ansi(R);} i++; continue; }
    if(c=='#' || c==';'){ ansi(C); out(s+i,len-i); break; }
    out(&s[i],1); i++;
  }
  ansi(R);
}