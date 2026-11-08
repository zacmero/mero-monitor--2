#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
typedef wchar_t WCHAR;typedef unsigned int DWORD;
typedef int HWND;
#define CP_UTF8 65001
static DWORD now=1000;
static DWORD GetTickCount(void){return now;}
static int portrait=1,keyboard,physical,active=1,viewSent=1,requested=-1,shift,symbols,ctrl,scrolled;
static int modeRequest=-1;static DWORD modeRequestTick;
static int touchPressed,touchX,touchY,cols,rows,cx,cy;
static char screenText[2200],status[96];
static char token[17]="0123456789abcdef",input[256],notice[96];
static unsigned int inputLen,screenLength;static int screenUtf16;static DWORD received=1000,inputTick;
static int allow=1,commands;static char action[32],track[100];static unsigned int value;
static int MeroBusCommand(const char *a,const char *t,unsigned int v){if(!allow)return 0;commands++;strcpy(action,a);strcpy(track,t);value=v;return 1;}
static int WideCharToMultiByte(int cp,int flags,const WCHAR *in,int n,char *out,int cap,void *a,void *b){(void)cp;(void)flags;(void)a;(void)b;assert(n==1&&cap>=1);out[0]=(char)*in;return 1;}
static void MeroReturnToShell(void){}
static void DestroyWindow(HWND h){(void)h;}
#include "terminal_native.inc"
#include "mero-terminal-font.h"
static void frame(const char *s){char line[2048];strcpy(line,s);HostLine(line);}
int main(void){
    assert(MeroTerminalGlyph(0xe3)!=MeroTerminalGlyph('?'));
    assert(MeroTerminalGlyph(0xe0b0)!=MeroTerminalGlyph(0x25a1));
    assert(MeroTerminalGlyph(0xf07b)!=MeroTerminalGlyph(0x25a1));
    assert(MeroTerminalGlyph(0xa13a)!=MeroTerminalGlyph(0x25a1));
    assert(MeroTerminalGlyph(0x250c)!=MeroTerminalGlyph(0x25a1));
    assert(MeroTerminalGlyph(0x28ff)!=MeroTerminalGlyph(0x25a1));
    assert(MeroTerminalGlyph(0x1f642)==MeroTerminalGlyph(0x25a1));
    assert(!keyboard);
    frame("MERO:TERM:physical=1|input=");assert(physical&&!keyboard);
    frame("MERO:TERM:physical=0|input=");assert(!physical&&!keyboard);
    frame("MERO:TERM:physical=0|input=keyboard failed");assert(!keyboard);
    keyboard=1;frame("MERO:TERM:physical=1");assert(!keyboard);
    keyboard=1;modeRequest=0;frame("MERO:TERM:physical=0");assert(keyboard&&modeRequest==-1);
    keyboard=0;viewSent=1;notice[0]=0;
    Touch(80,15,1);assert(keyboard);Touch(80,15,1);assert(!keyboard);

    ctrl=1;Character(' ');assert(inputLen==1&&input[0]==0&&!ctrl);Flush();assert(strstr(track,".00"));commands=0;
    frame("MERO:TERM:encoding=u16|text=4QDnAAwlACUQJQ==");assert(screenUtf16&&screenLength==10);
    frame("MERO:TERM:encoding=u16|text=YQ==");assert(screenLength==0);
    char output[8];assert(Decode("YWJj",output,sizeof(output))&&!strcmp(output,"abc"));
    assert(!Decode("!!!!",output,sizeof(output)));assert(!Decode("YWJjZGVmZ2hp",output,sizeof(output)));
    assert(Enqueue("abc\r",4));allow=0;Flush();assert(inputLen==4&&commands==0);
    allow=1;Flush();assert(inputLen==0&&commands==1&&!strcmp(track,"0123456789abcdef.6162630d"));
    assert(TouchDown(10,20));assert(!TouchDown(10,20));TouchUp();assert(TouchDown(10,20));TouchUp();
    char data[34];memset(data,'x',31);data[31]=(char)0xc3;data[32]=(char)0xa1;data[33]=0;
    assert(Enqueue(data,33));Flush();assert(inputLen==2&&strlen(track)==79);Flush();assert(inputLen==0&&strstr(track,".c3a1"));
    keyboard=1;Flush();assert(!strcmp(action,"terminal-view")&&value==3&&viewSent==3);
    requested=3;Flush();assert(!strcmp(action,"terminal-keyboard")&&requested==-1);
    assert(Enqueue("old",3));now=17001;Flush();assert(inputLen==0&&strstr(notice,"cleared"));
    assert(!Enqueue("new",3));assert(!inputLen);
    now=1000;assert(!Enqueue(data,257));
    keyboard=1;viewSent=3;inputLen=0;
    Touch(258,318,1);Touch(40,318,1);Touch(68,350,1);Touch(244,414,1);
    assert(inputLen==4&&!memcmp(input,"pwd\r",4));Flush();assert(strstr(track,".7077640d"));
    physical=0;Touch(244,15,1);assert(!keyboard&&requested==3);Flush();assert(!strcmp(action,"terminal-keyboard"));
    physical=1;Touch(244,15,1);assert(!keyboard&&requested==4);Flush();assert(!strcmp(action,"terminal-touch"));
    physical=0;modeRequest=-1;Touch(244,15,1);Flush();assert(modeRequest==1);
    Touch(244,15,1);assert(modeRequest==0&&requested==4&&!keyboard);Flush();assert(!strcmp(action,"terminal-touch"));
    puts("PASS: actual native terminal decode bounds, input queue, UTF-8 packet boundaries, expiry and view controls");return 0;
}
