/* Run the actual OEM USB control code with 16-bit WCHAR and fake CE APIs. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
typedef uint32_t DWORD;typedef uint16_t WORD;typedef unsigned char BYTE;
typedef int HANDLE,HMODULE,HKEY,BOOL;typedef wchar_t WCHAR;
typedef struct {DWORD Data1;WORD Data2,Data3;BYTE Data4[8];} GUID;
#define WINAPI
#define TRUE 1
#define FALSE 0
#define INVALID_HANDLE_VALUE (-1)
#define GENERIC_READ 1
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define OPEN_EXISTING 3
#define FILE_FLAG_OVERLAPPED 0x40000000
#define HKEY_LOCAL_MACHINE 1
#define KEY_READ 1
#define ERROR_SUCCESS 0
#define REG_SZ 1
static int cmp(const WCHAR *a,const WCHAR *b){while(*a&&*a==*b){a++;b++;}return *a-*b;}
static WCHAR *copy(WCHAR *d,const WCHAR *s,int cap){int i;for(i=0;i<cap-1&&s[i];i++)d[i]=s[i];d[i]=0;return d;}
static WCHAR *usb_last(WCHAR *s,WCHAR ch){WCHAR *p=NULL;for(;*s;s++)if(*s==ch)p=s;return p;}
#define _wcsicmp cmp
#define lstrcpynW copy
#define wcsrchr usb_last
static WCHAR defaultName[128],activeName[128];
static unsigned int calls[8],callCount,findClosed,opened,failSet,failChange,missing;
static DWORD lastError;
static DWORD GetLastError(void){return lastError;}
static void SetLastError(DWORD n){lastError=n;}
static HMODULE GetModuleHandleW(const WCHAR *s){(void)s;return 1;}
static int FindClose(HANDLE h){assert(h==77);findClosed++;return 1;}
static void CloseHandle(HANDLE h){assert(h==3);lastError=777;}
static HANDLE CreateFileW(const WCHAR *name,DWORD access,DWORD share,void *security,int mode,DWORD flags,void *template){
    (void)security;(void)template;assert(!cmp(name,L"BUS\\UFN1"));assert(access==GENERIC_READ&&share==3&&mode==OPEN_EXISTING&&flags==FILE_FLAG_OVERLAPPED);opened++;return 3;
}
static int RegOpenKeyExW(int root,const WCHAR *path,int zero,int access,HKEY *key){(void)root;(void)zero;(void)access;*key=!cmp(path,L"Drivers\\Active")?1:2;return 0;}
static int RegEnumKeyExW(HKEY key,DWORD index,WCHAR *name,DWORD *size,void *a,void *b,void *c,void *d){(void)key;(void)size;(void)a;(void)b;(void)c;(void)d;if(index)return 1;copy(name,L"01",128);return 0;}
static void wsprintfW(WCHAR *out,const WCHAR *format,...){(void)format;copy(out,L"Drivers\\Active\\01",180);}
static int RegQueryValueExW(HKEY key,const WCHAR *name,void *reserved,DWORD *type,BYTE *bytes,DWORD *size){
    WCHAR value[512];int n=0;(void)key;(void)name;(void)reserved;copy(value,L"Drivers\\USB\\FunctionDrivers\\",512);
    while(value[n])n++;copy(value+n,activeName,512-n);while(value[n])n++;n++;assert(*size>=(DWORD)n*2);memcpy(bytes,value,n*2);*size=n*2;*type=REG_SZ;return 0;
}
static void RegCloseKey(HKEY h){(void)h;}
struct MeroTestDevice;
static HANDLE FindDevice(DWORD kind,const GUID *guid,void *pointer){
    /* bus name offset is the OEM DEVMGR_DEVICE_INFORMATION layout. */
    BYTE *bytes=pointer;assert(kind==3&&guid->Data1==0xe2bdc372);assert(*(DWORD*)bytes==1584);
    copy((WCHAR*)(bytes+1064),L"BUS\\UFN1",260);return 77;
}
static void *GetProcAddress(HMODULE h,const WCHAR *name){(void)h;assert(!cmp(name,L"FindFirstDevice"));return missing?NULL:(void*)FindDevice;}
static int DeviceIoControl(HANDLE h,DWORD code,void *in,DWORD inSize,void *out,DWORD outSize,DWORD *done,void *overlap){
    (void)done;(void)overlap;assert(h==3&&callCount<8);calls[callCount++]=code;
    if(code==0x220014){assert(!in&&!inSize&&outSize==756);memset(out,0,outSize);copy(out,defaultName,128);return 1;}
    assert((code==0x220018||code==0x220010)&&in&&inSize==256&&!out&&!outSize);
    if(code==0x220018){if(failSet){lastError=55;return 0;}copy(defaultName,in,128);return 1;}
    if(failChange){lastError=99;return 0;}copy(activeName,in,128);return 1;
}
#include "../wince/mero-usb.h"
static void reset(const WCHAR *def,const WCHAR *active){copy(defaultName,def,128);copy(activeName,active,128);callCount=opened=findClosed=failSet=failChange=missing=0;}
int main(void){
    WCHAR value[128];assert(sizeof(WCHAR)==2);
    reset(L"Serial_Class",L"Serial_Class");assert(MeroUsbQuery(value)&&!cmp(value,L"Serial_Class")&&callCount==1);
    reset(L"Serial_Class",L"Serial_Class");assert(MeroUsbSet(L"Serial_Class")&&callCount==1); /* No link reset. */
    reset(L"Mass_Storage_Class",L"Mass_Storage_Class");assert(MeroUsbSet(L"Serial_Class")&&callCount==3&&calls[1]==0x220018&&calls[2]==0x220010);
    assert(!cmp(defaultName,L"Serial_Class")&&!cmp(activeName,L"Serial_Class"));
    reset(L"Serial_Class",L"Mass_Storage_Class");assert(MeroUsbSet(L"Serial_Class")&&callCount==2&&calls[1]==0x220010);
    reset(L"Mass_Storage_Class",L"Serial_Class");assert(MeroUsbSet(L"Serial_Class")&&callCount==2&&calls[1]==0x220018);
    reset(L"Mass_Storage_Class",L"Mass_Storage_Class");failChange=1;
    assert(!MeroUsbSet(L"Serial_Class")&&callCount==4&&!cmp(defaultName,L"Mass_Storage_Class")&&GetLastError()==99);
    reset(L"Mass_Storage_Class",L"Mass_Storage_Class");failSet=1;assert(!MeroUsbSet(L"Serial_Class")&&callCount==2&&GetLastError()==55);
    reset(L"Serial_Class",L"Serial_Class");assert(!MeroUsbSet(L"Unknown_Class")&&!callCount&&!opened);
    missing=1;assert(!MeroUsbQuery(value)&&!callCount);
    puts("PASS: OEM USB sizes/codes, controller bus lookup, active-link preservation, default/current mismatch and rollback");return 0;
}
