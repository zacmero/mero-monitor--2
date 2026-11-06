/* Narrow agent replacement helper. The plan hash is bound into this build. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>
#include "../mero-agent-protocol.h"
#include "mero-upgrade-plan.h"
static int Hash(const WCHAR *path,char hex[65]){BYTE b[4096],digest[32];DWORD n,total=0;AuthHash h;HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return 0;AuthInit(&h);while(1){if(!ReadFile(f,b,sizeof(b),&n,NULL)){CloseHandle(f);return 0;}if(!n)break;total+=n;if(total>2*1024*1024){CloseHandle(f);return 0;}AuthUpdate(&h,b,n);}CloseHandle(f);AuthFinal(&h,digest);AuthHex(digest,32,hex);return 1;}
static int Start(const WCHAR *path){STARTUPINFOW si;PROCESS_INFORMATION pi;memset(&si,0,sizeof(si));memset(&pi,0,sizeof(pi));si.cb=sizeof(si);if(!CreateProcessW(path,NULL,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi))return 0;CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return 1;}
static void Log(const char *message){HANDLE f=CreateFileW(L"\\SDMMC\\BT\\AgentUpdate\\upgrade-result.txt",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);DWORD done;if(f!=INVALID_HANDLE_VALUE){WriteFile(f,message,(DWORD)strlen(message),&done,NULL);FlushFileBuffers(f);CloseHandle(f);}}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR cmd,int show){
    char plan[2049],target[192],candidate[192],backup[192],oldSha[65],newSha[65],actual[65],normal[192];WCHAR t[192],c[192],b[192];HANDLE f,snap;DWORD n,found=0,pid=0;AuthHash hash;BYTE digest[32];PROCESSENTRY32 pe;
    (void)instance;(void)previous;(void)cmd;(void)show;
    /* Let the launching agent persist its receipt before it is stopped. */
    Sleep(2000);
    f=CreateFileW(L"\\SDMMC\\BT\\AgentUpdate\\plan.txt",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(f==INVALID_HANDLE_VALUE)return 1;
    if(!ReadFile(f,plan,sizeof(plan)-1,&n,NULL)||GetFileSize(f,NULL)!=n){CloseHandle(f);Log("error=plan_read\n");return 1;}CloseHandle(f);plan[n]=0;
    AuthInit(&hash);AuthUpdate(&hash,plan,n);AuthFinal(&hash,digest);AuthHex(digest,32,actual);
    if(strcmp(actual,MERO_UPGRADE_PLAN_SHA256)||!AgentGet(plan,"target",target,sizeof(target))||!AgentGet(plan,"candidate",candidate,sizeof(candidate))||!AgentGet(plan,"backup",backup,sizeof(backup))||!AgentGet(plan,"old_sha256",oldSha,sizeof(oldSha))||!AgentGet(plan,"new_sha256",newSha,sizeof(newSha))){Log("error=plan_validation\n");return 1;}
    if(!AgentPath(target,normal,1)||!AgentPath(candidate,normal,1)||!AgentPath(backup,normal,1)||!AgentPrefix(candidate,"\\SDMMC\\BT\\AgentUpdate\\")||!AgentPrefix(backup,"\\SDMMC\\BT\\AgentUpdate\\")){Log("error=path_policy\n");return 1;}
    const char *base=strrchr(target,'\\');base=base?base+1:target;size_t length=strlen(base);
    if(length<14||strcmp(base+length-14,"mero-agent.exe")){Log("error=not_agent_target\n");return 1;}
    MultiByteToWideChar(CP_ACP,0,target,-1,t,192);MultiByteToWideChar(CP_ACP,0,candidate,-1,c,192);MultiByteToWideChar(CP_ACP,0,backup,-1,b,192);
    if(!Hash(t,actual)){Log("error=target_read\n");return 1;}if(!strcmp(actual,newSha)){Log("ok=1\nmessage=already_updated\n");return 0;}
    if(strcmp(actual,oldSha)||!Hash(c,actual)||strcmp(actual,newSha)||GetFileAttributesW(b)!=INVALID_FILE_ATTRIBUTES){Log("error=hash_or_backup_validation\n");return 1;}
    const WCHAR *widebase=wcsrchr(t,L'\\');widebase=widebase?widebase+1:t;
    snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snap==INVALID_HANDLE_VALUE){Log("error=process_scan\n");return 1;}pe.dwSize=sizeof(pe);
    if(Process32First(snap,&pe))do{const WCHAR *name=wcsrchr(pe.szExeFile,L'\\');name=name?name+1:pe.szExeFile;if(!_wcsicmp(name,widebase)){pid=pe.th32ProcessID;found++;}}while(Process32Next(snap,&pe));CloseHandle(snap);
    if(found>1||pid==GetCurrentProcessId()){Log("error=ambiguous_agent_process\n");return 1;}
    if(found){f=OpenProcess(1,FALSE,pid);if(!f||!TerminateProcess(f,0)){if(f)CloseHandle(f);Log("error=stop_agent\n");return 1;}WaitForSingleObject(f,5000);CloseHandle(f);Sleep(300);}
    if(!MoveFileW(t,b)){Start(t);Log("error=backup_move\n");return 1;}
    if(!MoveFileW(c,t)){int restored=MoveFileW(b,t);int started=restored&&Start(t);Log(started?"error=install_move_rolled_back\n":"error=install_move_restore_failed_backup_preserved\n");return 1;}
    if(!Hash(t,actual)||strcmp(actual,newSha)||!Start(t)){
        WCHAR failed[260];wsprintfW(failed,L"%s.failed",b);int displaced=MoveFileW(t,failed);int restored=displaced&&MoveFileW(b,t);int started=restored&&Start(t);Log(started?"error=new_agent_launch_rolled_back\n":"error=new_agent_failed_restore_failed_backup_preserved\n");return 1;
    }
    Log("ok=1\nmessage=agent_replaced_and_started\n");return 0;
}
