#include <assert.h>
#include <string.h>
#include <wchar.h>
typedef unsigned int DWORD;typedef wchar_t WCHAR;typedef const WCHAR *LPCWSTR;typedef unsigned char BYTE;typedef int HANDLE;
#define GENERIC_READ 1
#define GENERIC_WRITE 2
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define OPEN_ALWAYS 4
#define OPEN_EXISTING 3
#define CREATE_ALWAYS 2
#define INVALID_HANDLE_VALUE -1
static DWORD raw=64,journal[2];static int failSave,sets;
static HANDLE CreateFileW(LPCWSTR p,int a,int s,void*b,int mode,int flags,void*t){(void)a;(void)s;(void)b;(void)flags;(void)t;if(!wcscmp(p,L"BKL1:"))return 1;if(failSave&&mode==CREATE_ALWAYS)return -1;return 2;}
static int ReadFile(HANDLE h,void *p,DWORD n,DWORD *got,void *unused){(void)unused;assert(h==2&&n==8);memcpy(p,journal,n);*got=n;return 1;}
static int WriteFile(HANDLE h,const void *p,DWORD n,DWORD *got,void *unused){(void)unused;assert(h==2&&n==8);memcpy(journal,p,n);*got=n;return 1;}
static int FlushFileBuffers(HANDLE h){(void)h;return 1;}
static void CloseHandle(HANDLE h){(void)h;}
static void *GetModuleHandleW(LPCWSTR p){(void)p;return 0;}
static void *GetProcAddress(void*h,LPCWSTR p){(void)h;(void)p;return 0;}
static int DeviceIoControl(HANDLE h,DWORD code,void*in,DWORD ins,void*out,DWORD outs,DWORD*got,void*ov){
 assert(h==1&&!ov);
 if(code==0x22000c){assert(!in&&!ins&&outs==4);*(DWORD*)out=raw;*got=0;return 1;}
 assert(code==0x220008&&ins==4&&!out&&!outs);raw=*(DWORD*)in;sets++;return 1;
}
#include "../wince/mero-cinema-backlight.h"
int main(int argc,char**argv){(void)argv;
 if(argc>1){journal[0]=0x324c4b42;journal[1]=19;raw=14;MeroCinemaBacklight(0);assert(raw==19&&!journal[0]);return 0;}
 MeroCinemaBacklight(1);assert(raw==14&&journal[1]==64);MeroCinemaBacklight(1);assert(sets==1&&journal[1]==64);
 MeroCinemaBacklight(0);assert(raw==64&&!journal[0]);
 failSave=1;MeroCinemaBacklight(1);assert(raw==64);failSave=0;
 raw=0;MeroCinemaBacklight(1);assert(raw==0);DWORD invalid=13;assert(!MeroBrightness(&invalid,1));return 0;
}
