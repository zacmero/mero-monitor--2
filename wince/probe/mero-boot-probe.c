/* Read-only boot/USB diagnostic; writes its own report on SD. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>
#include "../mero-usb.h"
static HANDLE output;
static void Line(const char *s){DWORD done;WriteFile(output,s,strlen(s),&done,NULL);}
static void Value(const WCHAR *keyPath,const WCHAR *name,const char *label) {
    HKEY key;DWORD type,size=sizeof(WCHAR)*512;WCHAR value[512];char line[1200],text[1024];
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,keyPath,0,KEY_READ,&key)!=ERROR_SUCCESS)return;
    memset(value,0,sizeof(value));
    if(RegQueryValueExW(key,name,NULL,&type,(BYTE*)value,&size)==ERROR_SUCCESS){
        if(type==REG_DWORD){DWORD number;memcpy(&number,value,sizeof(number));snprintf(line,sizeof(line),"%s=%lu\r\n",label,number);}
        else {value[511]=0;WideCharToMultiByte(CP_UTF8,0,value,-1,text,sizeof(text),NULL,NULL);snprintf(line,sizeof(line),"%s=%s\r\n",label,text);}
        Line(line);
    }RegCloseKey(key);
}
int WINAPI WinMain(HINSTANCE a,HINSTANCE b,LPWSTR args,int show) {
    char line[600];HKEY active;DWORD index=0,size;WCHAR name[128],path[256];
    (void)a;(void)b;(void)args;(void)show;
    output=CreateFileW(L"\\SDMMC\\MERO\\boot-state.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(output==INVALID_HANDLE_VALUE)return 1;
    snprintf(line,sizeof(line),"uptime_ms=%lu\r\n",GetTickCount());Line(line);
    Value(L"Drivers\\USB\\FunctionDrivers",L"DefaultClientDriver","usb_default_client");
    Value(L"Software\\Mesada\\USB",L"UsbMode","mesada_usb_mode");
    Value(L"init",L"Launch50","boot_launch50");
    {WCHAR client[128];char text[300];
        if(MeroUsbQuery(client)){WideCharToMultiByte(CP_UTF8,0,client,-1,text,sizeof(text),NULL,NULL);snprintf(line,sizeof(line),"oem_usb_default=%s\r\n",text);Line(line);}
        else {snprintf(line,sizeof(line),"oem_usb_query_error=%lu\r\n",GetLastError());Line(line);}
    }
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"Drivers\\Active",0,KEY_READ,&active)==ERROR_SUCCESS){
        while(size=128,RegEnumKeyExW(active,index++,name,&size,NULL,NULL,NULL,NULL)==ERROR_SUCCESS){
            wsprintfW(path,L"Drivers\\Active\\%s",name);Value(path,L"Name","active_name");Value(path,L"Key","active_key");
        }RegCloseKey(active);
    }
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32 pe;pe.dwSize=sizeof(pe);
    if(snap!=INVALID_HANDLE_VALUE){if(Process32First(snap,&pe))do{
        char process[512];WideCharToMultiByte(CP_UTF8,0,pe.szExeFile,-1,process,sizeof(process),NULL,NULL);
        snprintf(line,sizeof(line),"process=%lu %s\r\n",pe.th32ProcessID,process);Line(line);
    }while(Process32Next(snap,&pe));CloseHandle(snap);}
    FlushFileBuffers(output);CloseHandle(output);return 0;
}
