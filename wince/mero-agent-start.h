#ifndef MERO_AGENT_START_H
#define MERO_AGENT_START_H
/* Restore the already provisioned agent when the existing boot chain starts
 * Mero Shell. Its path is stored on SD; no boot/USB/OEM registry writes. */
#define MERO_AGENT_CLASS L"MERO_FOSTON_AGENT"
#define MERO_AGENT_CONNECT_MESSAGE L"MERO_AGENT_CONNECT_V1"
static BOOL MeroStartAgent(BOOL show,BOOL reconnect) {
    HANDLE f;DWORD n;char path[260];WCHAR wide[260];PROCESS_INFORMATION pi;
    HWND agent=FindWindowW(MERO_AGENT_CLASS,NULL);
    if(agent){
        if(reconnect)PostMessageW(agent,RegisterWindowMessageW(MERO_AGENT_CONNECT_MESSAGE),show,0);
        if(show){ShowWindow(agent,SW_SHOW);SetForegroundWindow(agent);}
        return TRUE;
    }
    f=CreateFileW(L"\\SDMMC\\MERO\\agent.cfg",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(f==INVALID_HANDLE_VALUE)return FALSE;
    memset(path,0,sizeof(path));
    if(!ReadFile(f,path,sizeof(path)-1,&n,NULL)){CloseHandle(f);return FALSE;}CloseHandle(f);
    path[strcspn(path,"\r\n")]=0;
    if(strncmp(path,"\\SDMMC\\BT\\Downloads\\",20) && strncmp(path,"\\SDMMC\\MERO\\",12))return FALSE;
    if(strstr(path,"..") || strlen(path)<4 || strcmp(path+strlen(path)-4,".exe"))return FALSE;
    if(!MultiByteToWideChar(CP_ACP,0,path,-1,wide,260))return FALSE;
    memset(&pi,0,sizeof(pi));
    if(CreateProcessW(wide,show?L"--connect":L"--background",NULL,NULL,FALSE,0,NULL,NULL,NULL,&pi)) {
        CloseHandle(pi.hProcess);CloseHandle(pi.hThread);
        return TRUE;
    }
    return FALSE;
}
#endif
