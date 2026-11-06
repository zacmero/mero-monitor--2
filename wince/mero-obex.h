/* OBEX wire codec, shared with host-side protocol checks. No platform ABI here. */
#ifndef MERO_OBEX_H
#define MERO_OBEX_H
#include <string.h>
static unsigned int Obex16(const unsigned char *p) { return ((unsigned int)p[0]<<8)|p[1]; }
static unsigned int Obex32(const unsigned char *p) { return ((unsigned int)p[0]<<24)|((unsigned int)p[1]<<16)|((unsigned int)p[2]<<8)|p[3]; }
static void ObexWrite16(unsigned char *p,unsigned int n) { p[0]=(unsigned char)(n>>8);p[1]=(unsigned char)n; }
static void ObexWrite32(unsigned char *p,unsigned int n) { p[0]=(unsigned char)(n>>24);p[1]=(unsigned char)(n>>16);p[2]=(unsigned char)(n>>8);p[3]=(unsigned char)n; }
static unsigned int ObexConnect(unsigned char *p) {
    static const unsigned char target[16]={0xf9,0xec,0x7b,0xc4,0x95,0x3c,0x11,0xd2,0x98,0x4e,0x52,0x54,0,0xdc,0x9e,9};
    p[0]=0x80;ObexWrite16(p+1,26);p[3]=0x10;p[4]=0;ObexWrite16(p+5,4096);
    p[7]=0x46;ObexWrite16(p+8,19);memcpy(p+10,target,16);return 26;
}
static unsigned int ObexRequest(unsigned char *p,unsigned char opcode,int haveId,unsigned int id,const char *name,const char *type) {
    unsigned int n=3,i,len;p[0]=opcode;
    if(haveId){p[n++]=0xcb;ObexWrite32(p+n,id);n+=4;}
    if(name){len=(unsigned int)strlen(name);if(len>95)return 0;p[n++]=1;ObexWrite16(p+n,3+2*(len+1));n+=2;
        for(i=0;i<len;i++){p[n++]=0;p[n++]=(unsigned char)name[i];}p[n++]=0;p[n++]=0;}
    if(type){len=(unsigned int)strlen(type)+1;if(len>64)return 0;p[n++]=0x42;ObexWrite16(p+n,len+3);n+=2;memcpy(p+n,type,len);n+=len;}
    ObexWrite16(p+1,n);return n;
}
/* Return 0 on malformed header. Variable-length headers must include 3 bytes. */
static unsigned int ObexHeader(const unsigned char *p,unsigned int n,unsigned int pos,unsigned int *payload,unsigned int *length) {
    unsigned int total;if(pos>=n)return 0;
    switch(p[pos]>>6){
    case 0:case 1:if(n-pos<3)return 0;total=Obex16(p+pos+1);if(total<3||total>n-pos)return 0;*payload=pos+3;*length=total-3;break;
    case 2:total=2;if(n-pos<total)return 0;*payload=pos+1;*length=1;break;
    default:total=5;if(n-pos<total)return 0;*payload=pos+1;*length=4;break;}
    return pos+total;
}
#endif
