#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../wince/mero-agent-protocol.h"
int main(int argc,char **argv){
    unsigned char key[32],digest[32];char input[8193],hex[65],path[192];size_t n;
    if(argc<3||!AuthUnhex(argv[1],key,32))return 2;
    n=fread(input,1,sizeof(input)-1,stdin);input[n]=0;
    if(!strcmp(argv[2],"verify")){printf("%d\n",AgentVerify(input,(unsigned int)n,key));return 0;}
    if(!strcmp(argv[2],"sign")){if(!AgentSign(input,sizeof(input),key))return 2;fputs(input,stdout);return 0;}
    if(!strcmp(argv[2],"path")){int ok=AgentPath(input,path,argc>3?atoi(argv[3]):0);printf("%d\n",ok);return 0;}
    if(!strcmp(argv[2],"number")){unsigned int value;printf("%d\n",AgentNumber(input,&value));return 0;}
    /* Exercise streaming boundaries, not just a one-shot digest. */
    AuthHash hash;AuthInit(&hash);size_t first=n/3;AuthUpdate(&hash,input,(unsigned int)first);AuthUpdate(&hash,input+first,(unsigned int)(n-first));AuthFinal(&hash,digest);AuthHex(digest,32,hex);puts(hex);
    AuthMac(key,input,(unsigned int)n,digest);AuthHex(digest,32,hex);puts(hex);
    {char out[16];assert(AgentGet("a=1\n","a",out,sizeof(out)));assert(!AgentGet("a=1\na=2\n","a",out,sizeof(out)));assert(AgentAppName("mero-gallery.exe"));assert(!AgentAppName("nk.exe"));}
    return 0;
}
