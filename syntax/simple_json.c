#include "syntax.h"
#include <string.h>

static void ansi(const char *seq){ term_write((const unsigned char*)seq, strlen(seq)); }
static void out(const unsigned char *s,size_t n){ if(n) term_write(s,n); }
static int is_l(char c){return ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_');}
static int is_an(char c){return is_l(c)||(c>='0'&&c<='9');}
static int eq_ci(const unsigned char*s,size_t n,const char*w){ size_t m=strlen(w); if(m!=n) return 0; for(size_t i=0;i<n;++i){ char a=(char)s[i], b=w[i]; if(a>='A'&&a<='Z') a=(char)(a-'A'+'a'); if(b>='A'&&b<='Z') b=(char)(b-'A'+'a'); if(a!=b) return 0;} return 1; }

void syntax_write_json(const unsigned char *s, size_t len){
  const char *R="\x1b[0m", *STR="\x1b[32m", *NUM="\x1b[35m", *CST="\x1b[33m", *PUNC="\x1b[34m";
  ansi(R);
  size_t i=0; int str=0; while(i<len){ unsigned char c=s[i];
    if(!str&&(c=='"')){ str=c; ansi(STR); out(&s[i++],1); continue; }
    if(str){ if(c=='\\'&&i+1<len){ out(&s[i],2); i+=2; continue; } out(&s[i],1); if(c=='"'){ str=0; ansi(R);} i++; continue; }
    if((c>='0'&&c<='9')||(c=='-'&&i+1<len&&s[i+1]>='0'&&s[i+1]<='9')){ size_t st=i; i++; while(i<len&&((s[i]>='0'&&s[i]<='9')||s[i]=='.'||s[i]=='e'||s[i]=='E'||s[i]=='+'||s[i]=='-')) i++; ansi(NUM); out(s+st,i-st); ansi(R); continue; }
    if(is_l((char)c)){
      size_t st=i; i++; while(i<len&&is_an((char)s[i])) i++; size_t n=i-st;
      if(eq_ci(s+st,n,"true")||eq_ci(s+st,n,"false")||eq_ci(s+st,n,"null")){ ansi(CST); out(s+st,n); ansi(R);} else { out(s+st,n);} continue;
    }
    if(c=='{'||c=='}'||c=='['||c==']'||c==':'||c==','){ ansi(PUNC); out(&s[i],1); ansi(R); i++; continue; }
    out(&s[i],1); i++;
  }
  ansi(R);
}