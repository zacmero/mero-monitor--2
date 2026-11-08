/* READ ONLY: registry/configuration/file attributes. No device handles or IOCTLs.
 * Writes only its own bounded SD report, preserving an earlier report. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <pm.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>
static HANDLE output;
static DWORD total,keys;
static int healthy=1,truncated;
#define REPORT_LIMIT (256*1024)
static void Line(const char *line){
    DWORD n=(DWORD)strlen(line),done;
    if(total+n>REPORT_LIMIT){truncated=1;return;}
    if(!WriteFile(output,line,n,&done,NULL)||done!=n)healthy=0;
    total+=n;
}
static void Utf8(const WCHAR *value,char *out,int cap){
    if(!WideCharToMultiByte(CP_UTF8,0,value,-1,out,cap,NULL,NULL))snprintf(out,cap,"[value too long]");
}
static void Registry(HKEY base,const WCHAR *path,const char *root,int depth){
    HKEY key;LONG error;DWORD i=0,n,type,size;WCHAR name[128],child[512];
    union{DWORD word;WCHAR wide[512];BYTE bytes[1024];}data;
    char label[1536],value[2048],line[4096];
    if(keys>=400||total>REPORT_LIMIT-4096){truncated=1;return;}
    Utf8(path,label,sizeof(label));snprintf(line,sizeof(line),"\r\n[%s\\%s]\r\n",root,label);Line(line);
    error=RegOpenKeyExW(base,path,0,KEY_READ,&key);
    if(error!=ERROR_SUCCESS){snprintf(line,sizeof(line),"open_error=%ld\r\n",error);Line(line);return;}
    keys++;
    for(i=0;i<128;i++){
        n=128;size=sizeof(data.bytes);memset(&data,0,sizeof(data));name[0]=0;
        error=RegEnumValueW(key,i,name,&n,NULL,&type,data.bytes,&size);
        if(error==ERROR_NO_MORE_ITEMS)break;
        if(error!=ERROR_SUCCESS){snprintf(line,sizeof(line),"value_index=%lu error=%ld bytes=%lu\r\n",i,error,size);Line(line);continue;}
        name[127]=0;Utf8(name,label,sizeof(label));
        if(type==REG_DWORD&&size==4)snprintf(value,sizeof(value),"%lu (0x%08lx)",data.word,data.word);
        else if(type==REG_SZ||type==REG_EXPAND_SZ){data.wide[511]=0;Utf8(data.wide,value,sizeof(value));}
        else if(type==REG_MULTI_SZ){
            WCHAR joined[512];DWORD j,limit=size/sizeof(WCHAR);if(limit>511)limit=511;
            for(j=0;j<limit;j++)joined[j]=data.wide[j]?data.wide[j]:L'|';joined[limit]=0;Utf8(joined,value,sizeof(value));
        }else snprintf(value,sizeof(value),"[type=%lu bytes=%lu; binary omitted]",type,size);
        snprintf(line,sizeof(line),"%s=%s\r\n",label,value);Line(line);
    }
    if(depth>0)for(i=0;i<128;i++){
        n=128;error=RegEnumKeyExW(key,i,name,&n,NULL,NULL,NULL,NULL);
        if(error==ERROR_NO_MORE_ITEMS)break;
        if(error!=ERROR_SUCCESS)continue;
        name[127]=0;
        if(lstrlenW(path)+lstrlenW(name)+2>=512){truncated=1;continue;}
        wsprintfW(child,L"%s\\%s",path,name);Registry(base,child,root,depth-1);
        if(keys>=400||total>REPORT_LIMIT-4096)break;
    }
    RegCloseKey(key);
}
static void Directory(const WCHAR *path){
    WIN32_FIND_DATAW fd;WCHAR pattern[260];HANDLE find;DWORD count=0;
    char text[1024],line[1300];ULARGE_INTEGER freeBytes,totalBytes;
    Utf8(path,text,sizeof(text));snprintf(line,sizeof(line),"\r\n[FILES %s]\r\n",text);Line(line);
    freeBytes.QuadPart=totalBytes.QuadPart=0;
    if(GetDiskFreeSpaceExW(path,&freeBytes,&totalBytes,NULL)){snprintf(line,sizeof(line),"volume_total_hi=%lu volume_total_lo=%lu volume_free_hi=%lu volume_free_lo=%lu\r\n",totalBytes.HighPart,totalBytes.LowPart,freeBytes.HighPart,freeBytes.LowPart);Line(line);}
    wsprintfW(pattern,L"%s\\*",path);find=FindFirstFileW(pattern,&fd);
    if(find==INVALID_HANDLE_VALUE){snprintf(line,sizeof(line),"find_error=%lu\r\n",GetLastError());Line(line);return;}
    do{
        Utf8(fd.cFileName,text,sizeof(text));snprintf(line,sizeof(line),"%s\tsize=%lu\tattributes=0x%08lx\r\n",text,fd.nFileSizeLow,fd.dwFileAttributes);Line(line);
    }while(++count<256&&FindNextFileW(find,&fd));
    if(count>=256)truncated=1;
    FindClose(find);
}
static void FileHeader(const WCHAR *path){
    BYTE bytes[32];DWORD n,i,attributes=GetFileAttributesW(path);char text[512],line[700],hex[65];
    Utf8(path,text,sizeof(text));snprintf(line,sizeof(line),"\r\n[FILE HEADER %s] attributes=0x%08lx\r\n",text,attributes);Line(line);
    if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&0x40)){Line("not_read=absent_or_ROM\r\n");return;}
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    if(file==INVALID_HANDLE_VALUE){snprintf(line,sizeof(line),"open_error=%lu\r\n",GetLastError());Line(line);return;}
    DWORD size=GetFileSize(file,NULL);int ok=ReadFile(file,bytes,sizeof(bytes),&n,NULL);CloseHandle(file);
    if(!ok){Line("read_failed\r\n");return;}
    for(i=0;i<n;i++)sprintf(hex+i*2,"%02x",bytes[i]);hex[n*2]=0;
    snprintf(line,sizeof(line),"size=%lu first_bytes=%s\r\n",size,hex);Line(line);
}
/* Export addresses are metadata, not permission or successful hardware access.
   Never load an OEM DLL or invoke the functions listed by this inventory. */
static void ApiInventory(void){
    const WCHAR *modules[]={L"coredll.dll",L"storeapi.dll",L"buses.dll",L"spc.dll",L"FMC.dll",L"block.dll",L"usbfn.dll",L"gps.dll",L"dmadrv.dll",NULL};
    const WCHAR *core[]={L"KernelIoControl",L"VirtualCopy",L"SetKMode",L"SetProcPermissions",L"GetCurrentPermissions",L"GetSystemMemoryDivision",L"ReadProcessMemory",L"MapCallerPtr",L"RegSaveKey",L"RegCopyFile",L"FindFirstStore",L"FindFirstPartition",L"OpenStore",L"GetStoreInfo",L"SetSystemPowerState",L"GetSystemPowerState",L"GetDevicePower",L"SetDevicePower",NULL};
    const WCHAR *drivers[]={L"GIO_Init",L"GIO_IOControl",L"BKL_Init",L"BKL_IOControl",L"PWM_Init",L"PWM_IOControl",L"SPC_Init",L"SPC_IOControl",L"FMC_Init",L"FMC_IOControl",L"DSK_Init",L"DSK_IOControl",L"UFN_Init",L"UFN_IOControl",L"COM_Init",L"COM_IOControl",NULL};
    unsigned int i,j;char name[256],symbol[128],line[700];
    Line("\r\n[LOADED EXPORT PRESENCE ONLY - NOT EXECUTED]\r\n");
    for(i=0;modules[i];i++){
        HMODULE module=GetModuleHandleW(modules[i]);const WCHAR **names=i<2?core:drivers;
        Utf8(modules[i],name,sizeof(name));snprintf(line,sizeof(line),"module=%s current_process_loaded=%d handle=0x%08lx\r\n",name,module!=NULL,(DWORD)module);Line(line);
        if(!module)continue;
        for(j=0;names[j];j++){
            FARPROC address=GetProcAddress(module,names[j]);Utf8(names[j],symbol,sizeof(symbol));
            snprintf(line,sizeof(line),"export=%s!%s present=%d address=0x%08lx\r\n",name,symbol,address!=NULL,(DWORD)address);Line(line);
        }
    }
}
static void ModuleInventory(void){
    MODULEENTRY32 entry;HANDLE snapshot;DWORD count=0;char name[1024],path[1024],line[2300];
    Line("\r\n[LOADED MODULES - VIRTUAL ADDRESSES]\r\n");
    snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_GETALLMODS,0);
    if(snapshot==INVALID_HANDLE_VALUE){snprintf(line,sizeof(line),"snapshot_error=%lu\r\n",GetLastError());Line(line);return;}
    memset(&entry,0,sizeof(entry));entry.dwSize=sizeof(entry);
    if(Module32First(snapshot,&entry))do{
        entry.szModule[MAX_MODULE_NAME32]=0;entry.szExePath[MAX_PATH-1]=0;
        Utf8(entry.szModule,name,sizeof(name));Utf8(entry.szExePath,path,sizeof(path));
        snprintf(line,sizeof(line),"module=%s base=0x%08lx bytes=%lu path=%s\r\n",name,(DWORD)entry.modBaseAddr,entry.modBaseSize,path);Line(line);
        if(++count>=256){Line("module_list_limit=256\r\n");truncated=1;break;}
        entry.dwSize=sizeof(entry);
    }while(Module32Next(snapshot,&entry));
    else {snprintf(line,sizeof(line),"first_module_error=%lu\r\n",GetLastError());Line(line);}
    CloseToolhelp32Snapshot(snapshot);
}
static void IdentityQuery(void){
    WCHAR value[256];char text[1024],line[1300];SYSTEM_INFO info;STORE_INFORMATION store;
    typedef BOOL (*StoreQuery)(LPSTORE_INFORMATION);
    StoreQuery query=(StoreQuery)GetProcAddress(GetModuleHandleW(L"coredll.dll"),L"GetStoreInformation");
    Line("\r\n[READ-ONLY IDENTITY AND OBJECT STORE]\r\n");
    memset(value,0,sizeof(value));
    if(SystemParametersInfoW(SPI_GETOEMINFO,256,value,0)){value[255]=0;Utf8(value,text,sizeof(text));snprintf(line,sizeof(line),"oem=%s\r\n",text);Line(line);}
    else {snprintf(line,sizeof(line),"oem_query_error=%lu\r\n",GetLastError());Line(line);}
    memset(value,0,sizeof(value));
    if(SystemParametersInfoW(SPI_GETPLATFORMTYPE,256,value,0)){value[255]=0;Utf8(value,text,sizeof(text));snprintf(line,sizeof(line),"platform=%s\r\n",text);Line(line);}
    else {snprintf(line,sizeof(line),"platform_query_error=%lu\r\n",GetLastError());Line(line);}
    memset(&info,0,sizeof(info));GetSystemInfo(&info);snprintf(line,sizeof(line),"processor_arch=%u level=%u revision=%u page_size=%lu processors=%lu\r\n",info.wProcessorArchitecture,info.wProcessorLevel,info.wProcessorRevision,info.dwPageSize,info.dwNumberOfProcessors);Line(line);
    memset(&store,0,sizeof(store));
    if(query&&query(&store)){snprintf(line,sizeof(line),"object_store_bytes=%lu object_store_free=%lu\r\n",store.dwStoreSize,store.dwFreeSize);Line(line);}
    else {snprintf(line,sizeof(line),"object_store_query_available=%d error=%lu\r\n",query!=NULL,GetLastError());Line(line);}
}
static void PowerQuery(void){
    typedef DWORD (*StateQuery)(LPWSTR,DWORD,PDWORD);
    typedef DWORD (*DeviceQuery)(PVOID,DWORD,PCEDEVICE_POWER_STATE);
    StateQuery state=(StateQuery)GetProcAddress(GetModuleHandleW(L"coredll.dll"),L"GetSystemPowerState");
    DeviceQuery device=(DeviceQuery)GetProcAddress(GetModuleHandleW(L"coredll.dll"),L"GetDevicePower");
    WCHAR name[128]={0};DWORD flags=0,error;char text[512],line[800];CEDEVICE_POWER_STATE power;
    Line("\r\n[READ-ONLY POWER QUERIES]\r\n");
    if(state){error=state(name,sizeof(name),&flags);name[127]=0;Utf8(name,text,sizeof(text));snprintf(line,sizeof(line),"system_query_error=%lu name=%s flags=0x%08lx\r\n",error,text,flags);Line(line);}
    if(device){
        power=PwrDeviceUnspecified;error=device((PVOID)L"BKL1:",POWER_NAME,&power);
        snprintf(line,sizeof(line),"bkl_cached_error=%lu state=%d\r\n",error,(int)power);Line(line);
        power=PwrDeviceUnspecified;error=device((PVOID)L"BKL1:",POWER_NAME|POWER_FORCE,&power);
        snprintf(line,sizeof(line),"bkl_driver_query_error=%lu state=%d\r\n",error,(int)power);Line(line);
    }
}
int WINAPI WinMain(HINSTANCE a,HINSTANCE b,LPWSTR args,int show){
    const WCHAR *report=L"\\SDMMC\\MERO\\firmware-probe.txt";WCHAR backup[128];
    DWORD tick=GetTickCount();char line[512];OSVERSIONINFOW version;SYSTEM_INFO system;MEMORYSTATUS memory;
    (void)a;(void)b;(void)args;(void)show;
    if(GetFileAttributesW(report)!=INVALID_FILE_ATTRIBUTES){
        wsprintfW(backup,L"\\SDMMC\\MERO\\firmware-probe-prev-%lu.txt",tick);
        if(!MoveFileW(report,backup))return 2;
    }
    output=CreateFileW(report,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(output==INVALID_HANDLE_VALUE)return 1;
    snprintf(line,sizeof(line),"MERO READ-ONLY FIRMWARE CONFIGURATION PROBE\r\npid=%lu uptime_ms=%lu\r\n",GetCurrentProcessId(),tick);Line(line);
    memset(&version,0,sizeof(version));version.dwOSVersionInfoSize=sizeof(version);
    if(GetVersionExW(&version)){snprintf(line,sizeof(line),"os=%lu.%lu build=%lu platform=%lu\r\n",version.dwMajorVersion,version.dwMinorVersion,version.dwBuildNumber,version.dwPlatformId);Line(line);}
    GetSystemInfo(&system);memset(&memory,0,sizeof(memory));memory.dwLength=sizeof(memory);GlobalMemoryStatus(&memory);
    snprintf(line,sizeof(line),"processor_type=%lu physical_total=%lu physical_free=%lu\r\n",system.dwProcessorType,memory.dwTotalPhys,memory.dwAvailPhys);Line(line);
    Registry(HKEY_LOCAL_MACHINE,L"init","HKLM",0);
    Registry(HKEY_LOCAL_MACHINE,L"Ident","HKLM",0);
    Registry(HKEY_LOCAL_MACHINE,L"Platform","HKLM",2);
    Registry(HKEY_LOCAL_MACHINE,L"HARDWARE","HKLM",3);
    Registry(HKEY_LOCAL_MACHINE,L"System\\GDI","HKLM",2);
    Registry(HKEY_LOCAL_MACHINE,L"System\\StorageManager","HKLM",3);
    Registry(HKEY_LOCAL_MACHINE,L"System\\CurrentControlSet\\Control\\Power","HKLM",3);
    Registry(HKEY_LOCAL_MACHINE,L"System\\State","HKLM",1);
    Registry(HKEY_LOCAL_MACHINE,L"System\\BootVars","HKLM",1);
    Registry(HKEY_LOCAL_MACHINE,L"Drivers\\BuiltIn","HKLM",2);
    Registry(HKEY_LOCAL_MACHINE,L"Drivers\\Active","HKLM",1);
    Registry(HKEY_LOCAL_MACHINE,L"Software\\Mesada","HKLM",2);
    Registry(HKEY_LOCAL_MACHINE,L"Software\\Microsoft\\Shell","HKLM",1);
    Registry(HKEY_CURRENT_USER,L"ControlPanel\\Power","HKCU",2);
    Registry(HKEY_CURRENT_USER,L"ControlPanel\\BackLight","HKCU",1);
    IdentityQuery();ApiInventory();ModuleInventory();PowerQuery();
    Directory(L"\\ResidentFlash");Directory(L"\\ResidentFlash2");Directory(L"\\SDMMC");
    Directory(L"\\SDMMC\\BACKUP");
    Directory(L"\\");Directory(L"\\Temp");
    FileHeader(L"\\ResidentFlash2\\ARC.BIN");FileHeader(L"\\ResidentFlash2\\BEE.BIN");FileHeader(L"\\SDMMC\\logo.nb0");
    {const WCHAR *files[]={L"\\Windows\\nk.exe",L"\\Windows\\buses.dll",L"\\Windows\\default.hv",L"\\system.hv",L"\\user.hv",NULL};int i;
     Line("\r\n[ROM AND HIVE FILE ATTRIBUTES]\r\n");
     for(i=0;files[i];i++){char path[300];Utf8(files[i],path,sizeof(path));snprintf(line,sizeof(line),"%s attributes=0x%08lx\r\n",path,GetFileAttributesW(files[i]));Line(line);}}
    snprintf(line,sizeof(line),"\r\nCOMPLETE keys=%lu bytes_before_footer=%lu truncated=%d healthy=%d\r\n",keys,total,truncated,healthy);Line(line);
    int ok=FlushFileBuffers(output)&&healthy;CloseHandle(output);return ok?0:3;
}
