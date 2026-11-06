#ifndef MERO_AGENT_PROTOCOL_H
#define MERO_AGENT_PROTOCOL_H
#include "mero-auth.h"
#include <stdlib.h>
#include <stdio.h>
#define AGENT_MAX_TEXT 2048
#define AGENT_MAX_FILE (16u*1024u*1024u)
static int AgentGet(const char *body,const char *name,char *out,unsigned int cap){
    const char *p=body,*end;unsigned int len=(unsigned int)strlen(name),found=0;
    if(!cap)return 0;
    out[0]=0;
    while(*p){end=strchr(p,'\n');if(!end)return 0;
        if((unsigned int)(end-p)>len&&p[len]=='='&&!memcmp(p,name,len)){unsigned int n=(unsigned int)(end-p)-len-1;if(found++||n>=cap)return 0;memcpy(out,p+len+1,n);out[n]=0;}
        p=end+1;}
    return found==1;
}
static int AgentVerifyLimit(char *text,unsigned int n,const unsigned char key[32],unsigned int limit){
    unsigned char expected[32],actual[32];unsigned int i,pos;
    if(n<69||n>limit||text[n-1]!='\n')return 0;
    for(i=0;i<n;i++)if(!text[i]||text[i]=='\r'||(unsigned char)text[i]>126)return 0;
    pos=n-69;if(pos&&text[pos-1]!='\n')return 0;
    if(memcmp(text+pos,"mac=",4))return 0;
    text[n-1]=0;if(!AuthUnhex(text+pos+4,actual,32)){text[n-1]='\n';return 0;}text[n-1]='\n';
    AuthMac(key,text,pos,expected);if(!AuthEqual(actual,expected,32))return 0;
    text[pos]=0;return 1;
}
static int AgentVerify(char *text,unsigned int n,const unsigned char key[32]) { return AgentVerifyLimit(text,n,key,AGENT_MAX_TEXT); }
static int AgentSign(char *text,unsigned int cap,const unsigned char key[32]){unsigned char mac[32];char hex[65];unsigned int n=(unsigned int)strlen(text);if(n+70>cap)return 0;AuthMac(key,text,n,mac);AuthHex(mac,32,hex);snprintf(text+n,cap-n,"mac=%s\n",hex);return 1;}
static int AgentNumber(const char *p,unsigned int *value){unsigned int n=0,i;if(!*p)return 0;for(i=0;p[i];i++){unsigned int c=(unsigned char)p[i];if(c<'0'||c>'9'||n>429496729u||(n==429496729u&&c>'5'))return 0;n=n*10+c-'0';}*value=n;return 1;}
static int AgentPrefix(const char *p,const char *prefix){while(*prefix){char a=*p++,b=*prefix++;if(a>='A'&&a<='Z')a+=32;if(b>='A'&&b<='Z')b+=32;if(a!=b)return 0;}return 1;}
static int AgentPath(const char *input,char out[192],int write){
    unsigned int i,n=(unsigned int)strlen(input),start=1;if(!n||n>180||(input[0]!='/'&&input[0]!='\\'))return 0;
    for(i=0;i<n;i++){unsigned char c=input[i];if(c<32||c>126||strchr(":*?\"<>|",c))return 0;out[i]=c=='/'?'\\':(char)c;}out[n]=0;
    for(i=1;i<=n;i++)if(!out[i]||out[i]=='\\'){unsigned int len=i-start;if(!len||(len==1&&out[start]=='.')||(len==2&&out[start]=='.'&&out[start+1]=='.'))return 0;start=i+1;}
    if(!write)return AgentPrefix(out,"\\SDMMC\\")||AgentPrefix(out,"\\Windows\\")||AgentPrefix(out,"\\ResidentFlash\\")||AgentPrefix(out,"\\ResidentFlash2\\")||(n==6&&AgentPrefix(out,"\\SDMMC"))||(n==8&&AgentPrefix(out,"\\Windows"))||(n==14&&AgentPrefix(out,"\\ResidentFlash"))||(n==15&&AgentPrefix(out,"\\ResidentFlash2"));
    if(AgentPrefix(out,"\\SDMMC\\MERO\\agent-")||AgentPrefix(out,"\\SDMMC\\MERO\\deploy-backups\\")||AgentPrefix(out,"\\SDMMC\\MERO\\deploy-staging\\")||(n==26&&AgentPrefix(out,"\\SDMMC\\MERO\\deploy-backups"))||(n==26&&AgentPrefix(out,"\\SDMMC\\MERO\\deploy-staging")))return 0;
    return AgentPrefix(out,"\\SDMMC\\MERO\\")||AgentPrefix(out,"\\SDMMC\\BT\\")||AgentPrefix(out,"\\SDMMC\\Photos\\")||AgentPrefix(out,"\\SDMMC\\Stream\\");
}
static int AgentAppName(const char *p){unsigned int n=(unsigned int)strlen(p),i;if(n<10||n>63||!AgentPrefix(p,"mero-")||strcmp(p+n-4,".exe"))return 0;for(i=0;i<n;i++)if(!((p[i]>='a'&&p[i]<='z')||(p[i]>='0'&&p[i]<='9')||p[i]=='-'||p[i]=='.'))return 0;return 1;}
#endif
