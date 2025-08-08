#include "syntax.h"
#include <string.h>

static void ansi(const char *s){ term_write((const unsigned char*)s, strlen(s)); }
static void out(const unsigned char*s,size_t n){ if(n) term_write(s,n); }
static int is_l(char c){return ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||c=='-');}
static int is_an(char c){return is_l(c)||(c>='0'&&c<='9');}

void syntax_write_html(const unsigned char *s, size_t len){
  const char *R="\x1b[0m", *TAG="\x1b[34m", *ATTR="\x1b[36m", *S="\x1b[32m", *C="\x1b[90m";
  ansi(R);
  // HTML comments
  if(len>=4 && s[0]=='<' && s[1]=='!' && s[2]=='-' && s[3]=='-'){ ansi(C); out(s,len); ansi(R); return; }
  size_t i=0; int str=0; int in_tag=0;
  while(i<len){ unsigned char c=s[i];
    if(!str && c=='<'){ in_tag=1; ansi(TAG); out(&s[i++],1); continue; }
    if(in_tag && !str && c=='>'){ out(&s[i++],1); ansi(R); in_tag=0; continue; }
    if(!str && (c=='"'||c=='\'')){ str=c; ansi(S); out(&s[i++],1); continue; }
    if(str){ if(c=='\\'&&i+1<len){ out(&s[i],2); i+=2; continue; } out(&s[i],1); if(c==str){ str=0; if(in_tag) ansi(TAG); else ansi(R);} i++; continue; }
    if(in_tag && is_l((char)c)){
      size_t st=i; i++; while(i<len&&is_an((char)s[i])) i++; size_t n=i-st; out(s+st,n); continue;
    }
    if(in_tag && c=='='){ ansi(ATTR); out(&s[i++],1); ansi(TAG); continue; }
    out(&s[i],1); i++;
  }
  ansi(R);
}