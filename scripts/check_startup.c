#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdint.h>
typedef int BOOL,HANDLE,HWND;typedef wchar_t WCHAR;typedef uint32_t DWORD;
typedef struct {HANDLE hProcess,hThread;} PROCESS_INFORMATION;
#undef NULL
#define NULL 0
#define TRUE 1
#define FALSE 0
#define INVALID_HANDLE_VALUE (-1)
#define GENERIC_READ 1
#define FILE_SHARE_READ 1
#define OPEN_EXISTING 1
#define FILE_ATTRIBUTE_NORMAL 0
#define CP_ACP 0
#define SW_SHOW 1
static int existing,missing,created,posted,shown,closed,released,sleeps,agentOn,initCalls,ready,initScenario;
static char configured[260]="\\SDMMC\\BT\\Downloads\\101907-mero-agent.exe\r\n";
static WCHAR started[260],arguments[64];
static HWND FindWindowW(const WCHAR *cls,void *title){(void)title;assert(!wcscmp(cls,L"MERO_FOSTON_AGENT"));return existing;}
static int RegisterWindowMessageW(const WCHAR *s){assert(!wcscmp(s,L"MERO_AGENT_CONNECT_V1"));return 123;}
static void PostMessageW(int h,int message,int show,int l){(void)show;(void)l;assert(h==1&&message==123);posted++;}
static void ShowWindow(int h,int show){(void)h;(void)show;shown++;}
static void SetForegroundWindow(int h){(void)h;}
static HANDLE CreateFileW(const WCHAR *p,int a,int b,void *c,int d,int e,void *f){(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;assert(!wcscmp(p,L"\\SDMMC\\MERO\\agent.cfg"));return missing?-1:2;}
static int ReadFile(HANDLE h,char *out,int cap,DWORD *n,void *o){(void)o;assert(h==2&&strlen(configured)<(unsigned)cap);*n=strlen(configured);memcpy(out,configured,*n);return 1;}
static void CloseHandle(int h){(void)h;closed++;}
static void ReleaseMutex(int h){(void)h;released++;}
static int MultiByteToWideChar(int cp,int flags,const char *s,int n,WCHAR *out,int cap){int i;(void)cp;(void)flags;(void)n;for(i=0;s[i]&&i<cap-1;i++)out[i]=(unsigned char)s[i];out[i]=0;return i+1;}
static int CreateProcessW(const WCHAR *path,const WCHAR *args,void *a,void *b,int inherit,int flags,void *env,void *dir,void *startup,PROCESS_INFORMATION *pi){(void)a;(void)b;(void)inherit;(void)flags;(void)env;(void)dir;(void)startup;wcscpy(started,path);wcscpy(arguments,args);created++;pi->hProcess=3;pi->hThread=4;return 1;}
#include "../wince/mero-agent-start.h"
static int AgentInit(HANDLE *mutex){initCalls++;if(initScenario==1){*mutex=0;return 0;}*mutex=1;return initCalls>=3;}
static void Note(const WCHAR *s){(void)s;}
static void PostMessageStub(void){}
#define WM_BT_RESULT 1
static int window;
static void Sleep(int ms){assert(ms==100);sleeps++;if(initScenario==2)agentOn=0;}
/* The real prefix calls PostMessageW with WM_BT_RESULT; adapt only that call. */
#undef PostMessageW
#define PostMessageW(h,m,w,l) PostMessageStub()
#include "agent-init-prefix.inc"
int main(void){
 existing=1;assert(MeroStartAgent(FALSE,FALSE)&&!created&&!posted&&!shown);
 assert(MeroStartAgent(TRUE,TRUE)&&posted==1&&shown==1);
 existing=0;assert(MeroStartAgent(FALSE,FALSE)&&created==1&&!wcscmp(arguments,L"--background"));
 assert(!wcscmp(started,L"\\SDMMC\\BT\\Downloads\\101907-mero-agent.exe"));
 assert(MeroStartAgent(TRUE,TRUE)&&created==2&&!wcscmp(arguments,L"--connect"));
 missing=1;assert(!MeroStartAgent(TRUE,TRUE)&&created==2);missing=0;
 strcpy(configured,"\\Windows\\explorer.exe");assert(!MeroStartAgent(TRUE,TRUE)&&created==2);
 strcpy(configured,"\\SDMMC\\MERO\\..\\bad.exe");assert(!MeroStartAgent(TRUE,TRUE)&&created==2);
 closed=released=sleeps=initCalls=ready=0;agentOn=1;initScenario=0;AgentLoop();
 assert(ready&&initCalls==3&&released==3&&closed==3&&sleeps==40);
 closed=released=sleeps=initCalls=ready=0;agentOn=1;initScenario=1;AgentLoop();assert(!ready&&initCalls==1&&!sleeps&&!closed);
 closed=released=sleeps=initCalls=ready=0;agentOn=1;initScenario=2;AgentLoop();assert(!ready&&initCalls==1&&sleeps==1&&released==1&&closed==1);
 puts("PASS: saved keyed-agent launch/reconnect, plain-client separation, delayed SD retry, mutex release and user cancellation");return 0;
}
