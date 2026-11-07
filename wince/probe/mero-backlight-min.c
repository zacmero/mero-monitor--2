/* One-shot equivalent of the OEM minimum slider action. Leaves it at minimum. */
#include <windows.h>
#include <stdio.h>
#include "../mero-cinema-backlight.h"
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR args,int show){
    DWORD before=0,after=0,minimum=14,written;char report[256];int ok;
    (void)instance;(void)previous;(void)args;(void)show;
    ok=MeroBrightness(&before,0)&&MeroBrightness(&minimum,1)&&MeroBrightness(&after,0)&&after==minimum;
    int n=sprintf(report,"{\"pid\":%lu,\"tick\":%lu,\"ok\":%d,\"before\":%lu,\"requested\":14,\"after\":%lu}\n",
        GetCurrentProcessId(),GetTickCount(),ok,before,after);
    HANDLE file=CreateFileW(L"\\SDMMC\\MERO\\backlight-manual.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if(file==INVALID_HANDLE_VALUE)return 2;
    int saved=WriteFile(file,report,n,&written,NULL)&&written==(DWORD)n&&FlushFileBuffers(file);
    CloseHandle(file);return ok&&saved?0:1;
}
