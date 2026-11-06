/* Verified OEM Setting.exe ARM routines 0x15a50/0x15c9c:
 * UFNBUS GUID below; device info 0x630 bytes; default query 0x220014
 * (756 bytes), default set 0x220018 and runtime change 0x220010
 * (256 bytes). The old guessed 0x220008 is not a change request. */
#ifndef MERO_USB_H
#define MERO_USB_H
typedef struct {
    DWORD size;HANDLE device,parent;
    WCHAR legacy[6],key[260],deviceName[260],busName[260];
} MeroUsbDevice;
typedef struct {WCHAR name[128],description[250];} MeroUsbClient;
typedef char MeroUsbDeviceSizeCheck[sizeof(MeroUsbDevice)==0x630?1:-1];
typedef char MeroUsbClientSizeCheck[sizeof(MeroUsbClient)==756?1:-1];
typedef HANDLE (WINAPI *MeroFindDevice)(DWORD,const GUID*,MeroUsbDevice*);
static HANDLE MeroUsbOpen(void) {
    static const GUID guid={0xe2bdc372,0x598f,0x4619,{0xbc,0x50,0x54,0xb3,0xf7,0x84,0x8d,0x35}};
    MeroUsbDevice info;HANDLE search,file;HMODULE core=GetModuleHandleW(L"coredll.dll");
    MeroFindDevice find=(MeroFindDevice)GetProcAddress(core,L"FindFirstDevice");
    if(!find)return INVALID_HANDLE_VALUE;
    memset(&info,0,sizeof(info));info.size=sizeof(info);
    search=find(3,&guid,&info);
    if(search==INVALID_HANDLE_VALUE)return INVALID_HANDLE_VALUE;
    info.busName[259]=0;
    if(!info.busName[0]){FindClose(search);return INVALID_HANDLE_VALUE;}
    file=CreateFileW(info.busName,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,
                     OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
    FindClose(search);return file;
}
static int MeroUsbActive(const WCHAR *client) {
    HKEY root,key;DWORD index=0,size,type;WCHAR name[128],path[180],value[512];int active=0;
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"Drivers\\Active",0,KEY_READ,&root)!=ERROR_SUCCESS)return 0;
    while(size=128,RegEnumKeyExW(root,index++,name,&size,NULL,NULL,NULL,NULL)==ERROR_SUCCESS) {
        wsprintfW(path,L"Drivers\\Active\\%s",name);
        if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,path,0,KEY_READ,&key)!=ERROR_SUCCESS)continue;
        size=sizeof(value);type=0;memset(value,0,sizeof(value));
        if(RegQueryValueExW(key,L"Key",NULL,&type,(BYTE*)value,&size)==ERROR_SUCCESS&&type==REG_SZ) {
            WCHAR *last;value[511]=0;last=wcsrchr(value,L'\\');
            if(last&&!_wcsicmp(last+1,client))active=1;
        }
        RegCloseKey(key);if(active)break;
    }RegCloseKey(root);return active;
}
static BOOL MeroUsbQuery(WCHAR out[128]) {
    MeroUsbClient info;DWORD done=0;HANDLE file=MeroUsbOpen();BOOL ok;
    if(file==INVALID_HANDLE_VALUE)return FALSE;
    memset(&info,0,sizeof(info));
    ok=DeviceIoControl(file,0x00220014,NULL,0,&info,sizeof(info),&done,NULL);
    if(ok){info.name[127]=0;lstrcpynW(out,info.name,128);}
    CloseHandle(file);return ok;
}
static BOOL MeroUsbSet(const WCHAR *wanted) {
    MeroUsbClient previous;WCHAR name[128];DWORD done=0,error=0;HANDLE file;BOOL changed,ok;
    if(_wcsicmp(wanted,L"Serial_Class")&&_wcsicmp(wanted,L"Mass_Storage_Class"))return FALSE;
    file=MeroUsbOpen();if(file==INVALID_HANDLE_VALUE)return FALSE;
    memset(&previous,0,sizeof(previous));memset(name,0,sizeof(name));lstrcpynW(name,wanted,128);
    ok=DeviceIoControl(file,0x00220014,NULL,0,&previous,sizeof(previous),&done,NULL);
    previous.name[127]=0;
    if(!ok||!previous.name[0]){error=GetLastError();CloseHandle(file);SetLastError(error);return FALSE;}
    changed=_wcsicmp(previous.name,name)!=0;
    if(changed&&!DeviceIoControl(file,0x00220018,name,sizeof(name),NULL,0,NULL,NULL)) {
        error=GetLastError();CloseHandle(file);SetLastError(error);return FALSE;
    }
    if(MeroUsbActive(name)){CloseHandle(file);return TRUE;}
    ok=DeviceIoControl(file,0x00220010,name,sizeof(name),NULL,0,NULL,NULL);
    if(!ok) {
        error=GetLastError();
        if(changed)DeviceIoControl(file,0x00220018,previous.name,sizeof(previous.name),NULL,0,NULL,NULL);
    }
    CloseHandle(file);if(!ok)SetLastError(error);return ok;
}
#endif
