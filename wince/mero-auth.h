/* SHA-256 / HMAC-SHA256 for signed Bluetooth jobs and file verification. */
#ifndef MERO_AUTH_H
#define MERO_AUTH_H
#include <string.h>
typedef unsigned int AuthWord;
typedef char AuthWordMustBe32Bits[(sizeof(AuthWord)==4)?1:-1];
typedef struct { AuthWord h[8]; unsigned long long bytes; unsigned char block[64]; unsigned int used; } AuthHash;
static AuthWord AuthRotate(AuthWord x,unsigned int n){return (x>>n)|(x<<(32-n));}
static void AuthBlock(AuthHash *s,const unsigned char *p){
    static const AuthWord k[64]={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    AuthWord w[64],a,b,c,d,e,f,g,h,t,u;unsigned int i;
    for(i=0;i<16;i++)w[i]=((AuthWord)p[i*4]<<24)|((AuthWord)p[i*4+1]<<16)|((AuthWord)p[i*4+2]<<8)|p[i*4+3];
    for(i=16;i<64;i++)w[i]=w[i-16]+(AuthRotate(w[i-15],7)^AuthRotate(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(AuthRotate(w[i-2],17)^AuthRotate(w[i-2],19)^(w[i-2]>>10));
    a=s->h[0];b=s->h[1];c=s->h[2];d=s->h[3];e=s->h[4];f=s->h[5];g=s->h[6];h=s->h[7];
    for(i=0;i<64;i++){t=h+(AuthRotate(e,6)^AuthRotate(e,11)^AuthRotate(e,25))+((e&f)^((~e)&g))+k[i]+w[i];u=(AuthRotate(a,2)^AuthRotate(a,13)^AuthRotate(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;}
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void AuthInit(AuthHash *s){static const AuthWord iv[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};memcpy(s->h,iv,sizeof(iv));s->bytes=0;s->used=0;}
static void AuthUpdate(AuthHash *s,const void *value,unsigned int n){const unsigned char *p=value;s->bytes+=n;while(n){unsigned int take=64-s->used;if(take>n)take=n;memcpy(s->block+s->used,p,take);s->used+=take;p+=take;n-=take;if(s->used==64){AuthBlock(s,s->block);s->used=0;}}}
static void AuthFinal(AuthHash *s,unsigned char out[32]){unsigned long long bits=s->bytes*8;unsigned int i;s->block[s->used++]=0x80;if(s->used>56){memset(s->block+s->used,0,64-s->used);AuthBlock(s,s->block);s->used=0;}memset(s->block+s->used,0,56-s->used);for(i=0;i<8;i++)s->block[63-i]=(unsigned char)(bits>>(8*i));AuthBlock(s,s->block);for(i=0;i<32;i++)out[i]=(unsigned char)(s->h[i/4]>>(24-8*(i%4)));}
static void AuthMac(const unsigned char key[32],const void *data,unsigned int n,unsigned char out[32]){unsigned char pad[64],inner[32];AuthHash h;unsigned int i;for(i=0;i<64;i++)pad[i]=(i<32?key[i]:0)^0x36;AuthInit(&h);AuthUpdate(&h,pad,64);AuthUpdate(&h,data,n);AuthFinal(&h,inner);for(i=0;i<64;i++)pad[i]=(i<32?key[i]:0)^0x5c;AuthInit(&h);AuthUpdate(&h,pad,64);AuthUpdate(&h,inner,32);AuthFinal(&h,out);}
static void AuthHex(const unsigned char *p,unsigned int n,char *out){static const char hex[]="0123456789abcdef";unsigned int i;for(i=0;i<n;i++){out[i*2]=hex[p[i]>>4];out[i*2+1]=hex[p[i]&15];}out[n*2]=0;}
static int AuthUnhex(const char *p,unsigned char *out,unsigned int n){unsigned int i;for(i=0;i<n*2;i++){unsigned int c=(unsigned char)p[i],v;if(c>='0'&&c<='9')v=c-'0';else if(c>='a'&&c<='f')v=c-'a'+10;else return 0;if(!(i&1))out[i/2]=(unsigned char)(v<<4);else out[i/2]|=(unsigned char)v;}return p[n*2]==0;}
static int AuthEqual(const unsigned char *a,const unsigned char *b,unsigned int n){unsigned int i,d=0;for(i=0;i<n;i++)d|=a[i]^b[i];return d==0;}
#endif
