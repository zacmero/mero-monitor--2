/* Actual native delivery code with in-memory journal and window shims. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "../wince/mero-agent-protocol.h"
typedef wchar_t WCHAR;typedef int HWND;
#define lstrcpyW wcscpy
enum {VK_UP=1,VK_DOWN,VK_LEFT,VK_RIGHT,VK_HOME,VK_END,VK_DELETE,VK_PRIOR,VK_NEXT,VK_RETURN,VK_TAB,VK_ESCAPE};
enum {WM_KEYDOWN=100,WM_KEYUP,WM_CHAR,CP_ACP,CP_UTF8};
#define MERO_AGENT_DEVICE "00:18:E4:1F:03:AF"
static unsigned char agentKey[32];
static char disk[512];static WCHAR foregroundName[64]=L"MERO_TERMINAL";
static int posted,sent,failWrite,lastMessage,lastValue;
static int AgentRead(const WCHAR *p,char *out,unsigned int cap){(void)p;assert(strlen(disk)<cap);strcpy(out,disk);return strlen(disk);}
static int AgentWrite(const WCHAR *p,const char *in,unsigned int n){(void)p;assert(n<sizeof(disk));if(failWrite)return 0;memcpy(disk,in,n);disk[n]=0;return 1;}
static int AgentSend(const char *name,const WCHAR *path){(void)path;assert(!strcmp(name,"keyboard-result.txt"));sent++;return 1;}
static int GetForegroundWindow(void){return 1;}
static void GetClassNameW(HWND h,WCHAR *out,int cap){(void)h;assert(wcslen(foregroundName)<(unsigned)cap);wcscpy(out,foregroundName);}
static int MultiByteToWideChar(int cp,int flags,const char *in,int n,WCHAR *out,int cap){int i;(void)cp;(void)flags;for(i=0;i<(n<0?(int)strlen(in)+1:n)&&i<cap;i++)out[i]=(unsigned char)in[i];return i;}
static int PostMessageW(HWND h,int message,unsigned int value,int extra){(void)extra;assert(h==1);posted++;lastMessage=message;lastValue=value;return 1;}
#include "../wince/bluetooth/mero-keyboard.h"
static void result(const char *expected){char copy[512],value[96];strcpy(copy,disk);assert(AgentVerify(copy,strlen(copy),agentKey));assert(AgentGet(copy,"result",value,sizeof(value)));assert(!strcmp(value,expected));}
int main(void){
    AgentKeyboard("0123456789abcdef:MERO_TERMINAL:61620d");assert(posted==3&&lastMessage==WM_CHAR&&lastValue==13);result("queued");
    AgentKeyboard("0123456789abcdef:MERO_TERMINAL:61620d");assert(posted==3&&sent==2);
    wcscpy(foregroundName,L"Launch");AgentKeyboard("1123456789abcdef:MERO_TERMINAL:61");assert(posted==3);result("wrong_foreground_app");
    wcscpy(foregroundName,L"MERO_TERMINAL");failWrite=1;AgentKeyboard("2123456789abcdef:MERO_TERMINAL:61");assert(posted==3);failWrite=0;
    AgentKeyboard("3123456789abcdef:MERO_FOSTON_AGENT:61");AgentKeyboard("bad:MERO_TERMINAL:61");AgentKeyboard("4123456789abcdef:MERO_TERMINAL:zz");assert(posted==3);
    AgentKeyboard("7123456789abcdef:MERO_TERMINAL:61c080");AgentKeyboard("8123456789abcdef:MERO_TERMINAL:610062");assert(posted==3);
    AgentKeyboard("5123456789abcdef:MERO_TERMINAL:1b5b41");assert(posted==5&&lastMessage==WM_KEYUP&&lastValue==VK_UP);
    wcscpy(foregroundName,L"MeroShellWndClass");AgentKeyboard("6123456789abcdef:MeroShellWndClass:0d");assert(posted==7&&lastValue==VK_RETURN);
    wcscpy(foregroundName,L"MERO_AMBIENT");AgentKeyboard("9123456789abcdef:MERO_AMBIENT:1b");assert(posted==9&&lastValue==VK_ESCAPE);
    AgentKeyboard("a123456789abcdef:MERO_FOREGROUND:20");assert(posted==10&&lastValue==32);
    wcscpy(foregroundName,L"Launch");AgentKeyboard("b123456789abcdef:MERO_FOREGROUND:61");assert(posted==10);result("wrong_foreground_app");
    puts("PASS: actual signed keyboard journal, duplicate suppression, target/focus bounds, write failures and special keys");return 0;
}
