#include "syntax.h"
#include <string.h>

static void ansi(const char *seq) { term_write((const unsigned char*)seq, strlen(seq)); }
static void out(const unsigned char *s, size_t n) { if (n) term_write(s, n); }

static int is_l(char c){return ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_');}
static int is_an(char c){return is_l(c)||(c>='0'&&c<='9');}
static int in_list_ci(const unsigned char*s,size_t n,const char*const*L,size_t LN){
  for(size_t i=0;i<LN;++i){const char*w=L[i];size_t m=strlen(w);if(m==n){size_t j=0;int ok=1;for(;j<n;++j){char a=(char)s[j],b=w[j];if(a>='A'&&a<='Z')a=(char)(a- 'A'+'a');if(b>='A'&&b<='Z')b=(char)(b- 'A'+'a');if(a!=b){ok=0;break;}}if(ok)return 1;}}return 0;
}

void syntax_write_clike(const unsigned char *s, size_t len) {
  static const char *K[]={"if","else","for","while","switch","case","break","continue","return","struct","union","enum","typedef","static","const","volatile","extern","inline","sizeof","void","char","short","int","long","float","double","signed","unsigned","class","public","private","protected","template","typename","using","namespace","virtual","override","final","try","catch","throw","new","delete","this","operator","import","export","from","package","interface","extends","implements","function","const","let","var","async","await","yield"};
  const char *R="\x1b[0m", *C="\x1b[90m", *S="\x1b[32m", *N="\x1b[35m", *KW="\x1b[36m";
  ansi(R);
  size_t i=0; int str=0; while(i<len){unsigned char c=s[i];
    if(!str&&(i+1<len&&s[i]=='/'&&s[i+1]=='/')){ansi(C); out(s+i,len-i); break;}
    if(!str&&(c=='"'||c=='\'')) {str=c; ansi(S); out(&s[i++],1); continue;}
    if(str){ if(c=='\\'&&i+1<len){ out(&s[i],2); i+=2; continue;} out(&s[i],1); if(c==str){str=0; ansi(R);} i++; continue;}
    if((c>='0'&&c<='9')||(c=='.'&&i+1<len&&s[i+1]>='0'&&s[i+1]<='9')){
      size_t st=i; if(c=='0'&&i+1<len&&(s[i+1]=='x'||s[i+1]=='X')){ i+=2; while(i<len){unsigned char d=s[i]; if((d>='0'&&d<='9')||(d>='a'&&d<='f')||(d>='A'&&d<='F')) i++; else break;} }
      else { i++; while(i<len&&((s[i]>='0'&&s[i]<='9')||s[i]=='_')) i++; if(i<len&&(s[i]=='.'||s[i]=='e'||s[i]=='E')){ i++; while(i<len&&((s[i]>='0'&&s[i]<='9')||s[i]=='+'||s[i]=='-')) i++; } }
      ansi(N); out(s+st,i-st); ansi(R); continue; }
    if(is_l((char)c)){
      size_t st=i; i++; while(i<len&&is_an((char)s[i])) i++; size_t n=i-st;
      if(in_list_ci(s+st,n,K,sizeof(K)/sizeof(K[0]))){ ansi(KW); out(s+st,n); ansi(R);} else { out(s+st,n);} continue;
    }
    out(&s[i],1); i++;
  }
  ansi(R);
}