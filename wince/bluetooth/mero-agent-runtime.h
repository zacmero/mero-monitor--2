/* Included by mero-bt.c only in the keyed agent build. */
#include "../mero-agent-protocol.h"
#include "../mero-live-codec.h"
#include "../mero-art.h"
#include <tlhelp32.h>
static unsigned char agentKey[32];
static WCHAR agentRoot[160];
static unsigned int agentLast;
static void AgentStatusExtras(char *result,unsigned int cap) {
    HKEY key;WCHAR value[256];char boot[512]="unavailable";DWORD size=sizeof(value),type;
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"init",0,KEY_READ,&key)==ERROR_SUCCESS) {
        if(RegQueryValueExW(key,L"Launch50",NULL,&type,(BYTE*)value,&size)==ERROR_SUCCESS&&type==REG_SZ) {
            value[255]=0;WideCharToMultiByte(CP_ACP,0,value,-1,boot,sizeof(boot),NULL,NULL);
        }
        RegCloseKey(key);
    }
    unsigned int len=strlen(result);
    snprintf(result+len,cap-len,"boot_launch50=%s\n",boot);
    if(MeroBusLock()) {
        len=strlen(result);
        snprintf(result+len,cap-len,"live_generation=%lu\nlive_age_sec=%lu\nlive_weather=%d\nlive_music=%d\nlive_ack=%s\nlive_result=%s\n",
            mero_bus->generation,mero_bus->generation?(DWORD)(GetTickCount()-mero_bus->tick)/1000:99999,
            strstr(mero_bus->lines,"MERO:WX:")!=NULL,strstr(mero_bus->lines,"MERO:NOW:")!=NULL,mero_bus->ack,mero_bus->result);
        ReleaseMutex(mero_bus_mutex);
    }
}

static void AgentWide(const char *p,WCHAR *out,int cap){MultiByteToWideChar(CP_ACP,0,p,-1,out,cap);}
static int AgentWrite(const WCHAR *path,const void *data,DWORD size){HANDLE f=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);DWORD done=0;int ok;if(f==INVALID_HANDLE_VALUE)return 0;ok=WriteFile(f,data,size,&done,NULL)&&done==size&&FlushFileBuffers(f);CloseHandle(f);return ok;}
static int AgentRead(const WCHAR *path,char *data,unsigned int cap){HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);DWORD n=0,size;data[0]=0;if(f==INVALID_HANDLE_VALUE)return 0;size=GetFileSize(f,NULL);if(size>=cap||!ReadFile(f,data,size,&n,NULL)||n!=size){CloseHandle(f);return 0;}CloseHandle(f);data[n]=0;return (int)n;}
static int AgentHash(const WCHAR *path,char hex[65],unsigned int *size){HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);BYTE buffer[4096],digest[32];DWORD n;AuthHash hash;int ok=1;unsigned int total=0;if(f==INVALID_HANDLE_VALUE)return 0;AuthInit(&hash);while(1){if(!ReadFile(f,buffer,sizeof(buffer),&n,NULL)){ok=0;break;}if(!n)break;if(total>AGENT_MAX_FILE-n){ok=0;break;}AuthUpdate(&hash,buffer,n);total+=n;}CloseHandle(f);if(!ok)return 0;AuthFinal(&hash,digest);AuthHex(digest,32,hex);*size=total;return 1;}
static int AgentCopy(const WCHAR *source,const WCHAR *dest){HANDLE a,b;BYTE buffer[4096];DWORD n,done;unsigned int total=0;int ok=1;a=CreateFileW(source,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(a==INVALID_HANDLE_VALUE)return 0;b=CreateFileW(dest,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(b==INVALID_HANDLE_VALUE){CloseHandle(a);return 0;}while(1){if(!ReadFile(a,buffer,sizeof(buffer),&n,NULL)){ok=0;break;}if(!n)break;if(total>AGENT_MAX_FILE-n||!WriteFile(b,buffer,n,&done,NULL)||done!=n){ok=0;break;}total+=n;}if(!FlushFileBuffers(b))ok=0;CloseHandle(a);CloseHandle(b);return ok;}
static void AgentParents(WCHAR *path){unsigned int i;for(i=1;path[i];i++)if(path[i]=='\\'){WCHAR c=path[i];path[i]=0;CreateDirectoryW(path,NULL);path[i]=c;}}
static void AgentJobPath(unsigned int seq,const WCHAR *name,WCHAR out[260]){wsprintfW(out,L"%s\\job-%u\\%s",agentRoot,seq,name);}
static int AgentSend(const char *name,const WCHAR *path){
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);DWORD size,got;unsigned int total=0,n,take,response;int first=1,ok=1;
    if(f==INVALID_HANDLE_VALUE)return 0;size=GetFileSize(f,NULL);if(size>AGENT_MAX_FILE){CloseHandle(f);return 0;}
    do{n=ObexRequest(packet,2,haveId,connectionId,first?name:NULL,NULL);if(first){packet[n++]=0xc3;ObexWrite32(packet+n,size);n+=4;}
        unsigned int limit=peerLimit;if(limit>512)limit=512;
        take=limit-n-3;if(take>size-total)take=size-total;
        packet[0]=(total+take==size)?0x82:2;packet[n++]=(total+take==size)?0x49:0x48;ObexWrite16(packet+n,take+3);n+=2;
        if(!ReadFile(f,packet+n,take,&got,NULL)||got!=take){ok=0;break;}n+=take;ObexWrite16(packet+1,n);response=Exchange(n);
        if(!response||reply[0]!=(total+take==size?0xa0:0x90)){ok=0;break;}total+=take;first=0;
    }while(total<size);CloseHandle(f);return ok;
}
static int AgentOwnFile(const WCHAR *path){WCHAR self[260];GetModuleFileNameW(NULL,self,260);return !_wcsicmp(self,path);}
static int AgentControl(const char *name,int launch,const char *args,unsigned int *value){
    WCHAR wide[96],path[260],wideArgs[32];PROCESS_INFORMATION pi;STARTUPINFOW si;
    if(args[0] && (!launch ||
        !((!strcmp(name,"mero-shell.exe")&&!strcmp(args,"--home")) ||
          (!strcmp(name,"mero-media-ctrl.exe")&&(!strcmp(args,"--weather")||!strcmp(args,"--fm")||!strcmp(args,"--gps")||!strcmp(args,"--clock")))))) return 0;
    AgentWide(args,wideArgs,32);
    if(!AgentAppName(name))return 0;AgentWide(name,wide,96);wsprintfW(path,L"\\SDMMC\\MERO\\%s",wide);if(AgentOwnFile(path))return 0;
    *value=0;if(launch){memset(&si,0,sizeof(si));memset(&pi,0,sizeof(pi));si.cb=sizeof(si);if(!CreateProcessW(path,args[0]?wideArgs:NULL,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi))return 0;*value=pi.dwProcessId;CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return 1;}
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32 pe;if(snap==INVALID_HANDLE_VALUE)return 0;pe.dwSize=sizeof(pe);
    if(Process32First(snap,&pe))do{const WCHAR *base=wcsrchr(pe.szExeFile,L'\\');base=base?base+1:pe.szExeFile;if(_wcsicmp(base,wide))continue;if(pe.th32ProcessID==GetCurrentProcessId()){CloseHandle(snap);return 0;}HANDLE p=OpenProcess(1,FALSE,pe.th32ProcessID);if(!p||!TerminateProcess(p,0)){if(p)CloseHandle(p);CloseHandle(snap);return 0;}CloseHandle(p);(*value)++;}while(Process32Next(snap,&pe));CloseHandle(snap);return 1;
}
static int AgentPE(const WCHAR *path){
    BYTE h[96];HANDLE f;DWORD got,size,offset;int ok=0;f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return 0;
    size=GetFileSize(f,NULL);if(size<256||size>2*1024*1024)goto done;if(!ReadFile(f,h,64,&got,NULL)||got!=64||h[0]!='M'||h[1]!='Z')goto done;
    memcpy(&offset,h+60,4);if(offset>size-96||SetFilePointer(f,offset,NULL,FILE_BEGIN)==INVALID_SET_FILE_POINTER)goto done;
    if(!ReadFile(f,h,96,&got,NULL)||got!=96||memcmp(h,"PE\0\0",4))goto done;
    unsigned int machine=h[4]|((unsigned int)h[5]<<8),magic=h[24]|((unsigned int)h[25]<<8),sub=h[92]|((unsigned int)h[93]<<8);
    ok=(machine==0x1c0||machine==0x1c2)&&magic==0x10b&&(sub==2||sub==9);
done:CloseHandle(f);return ok;
}
static int AgentRestoreMissing(unsigned int seq){
    WCHAR meta[260],old[260],target[192];char text[2049],path[192],normal[192];int n;
    AgentJobPath(seq,L"meta.txt",meta);n=AgentRead(meta,text,sizeof(text));if(!n||!AgentVerify(text,n,agentKey)||!AgentGet(text,"path",path,sizeof(path))||!AgentPath(path,normal,1))return 0;
    AgentWide(normal,target,192);AgentJobPath(seq,L"old-live.bin",old);
    if(GetFileAttributesW(target)==INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(old)!=INVALID_FILE_ATTRIBUTES)return MoveFileW(old,target);
    return 0;
}
static int AgentInstall(unsigned int seq,const char *path,const WCHAR *incoming,char hash[65],unsigned int *size){
    char normal[192],oldHash[65]="",verify[65],meta[2049];WCHAR target[192],backup[260],old[260],metapath[260],failed[260];unsigned int oldSize=0,checkSize;int existed;
    if(!AgentPath(path,normal,1))return 0;AgentWide(normal,target,192);if(AgentOwnFile(target))return 0;
    unsigned int len=(unsigned int)strlen(normal);if(len>=4&&(AgentPrefix(normal+len-4,".exe")||AgentPrefix(normal+len-4,".dll"))&&!AgentPE(incoming))return 0;
    existed=GetFileAttributesW(target)!=INVALID_FILE_ATTRIBUTES;
    AgentJobPath(seq,L"backup.bin",backup);AgentJobPath(seq,L"old-live.bin",old);AgentJobPath(seq,L"meta.txt",metapath);AgentJobPath(seq,L"failed.bin",failed);
    if(existed&&(!AgentHash(target,oldHash,&oldSize)||!AgentCopy(target,backup)||!AgentHash(backup,verify,&checkSize)||strcmp(oldHash,verify)||checkSize!=oldSize))return 0;
    snprintf(meta,sizeof(meta),"v=1\nseq=%u\npath=%s\nold_sha256=%s\nnew_sha256=%s\n",seq,normal,oldHash,hash);if(!AgentSign(meta,sizeof(meta),agentKey)||!AgentWrite(metapath,meta,(DWORD)strlen(meta)))return 0;
    AgentParents(target);if(existed&&!MoveFileW(target,old))return 0;
    if(!MoveFileW(incoming,target)){if(existed)MoveFileW(old,target);return 0;}
    if(!AgentHash(target,verify,&checkSize)||strcmp(hash,verify)||*size!=checkSize){MoveFileW(target,failed);if(existed)MoveFileW(old,target);return 0;}
    return 1;
}
static int AgentList(const WCHAR *path,const WCHAR *dest){
    WCHAR pattern[260];WIN32_FIND_DATAW fd;HANDLE find,f;char line[1024],name[768];DWORD done;int ok=1;
    wsprintfW(pattern,L"%s\\*",path);find=FindFirstFileW(pattern,&fd);if(find==INVALID_HANDLE_VALUE)return 0;
    f=CreateFileW(dest,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE){FindClose(find);return 0;}
    do{if(!WideCharToMultiByte(CP_UTF8,0,fd.cFileName,-1,name,sizeof(name),NULL,NULL)){ok=0;break;}
        int n=snprintf(line,sizeof(line),"%s\t%lu\t%s\n",fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY?"dir":"file",fd.nFileSizeLow,name);
        if(n<0||n>=(int)sizeof(line)||!WriteFile(f,line,n,&done,NULL)||done!=(DWORD)n){ok=0;break;}
    }while(FindNextFileW(find,&fd));if(!FlushFileBuffers(f))ok=0;CloseHandle(f);FindClose(find);return ok;
}
static int AgentInit(HANDLE *mutex){
    unsigned char digest[32];char tag[65];WCHAR wide[65],name[96],pattern[200];WIN32_FIND_DATAW fd;HANDLE find;AuthHash hash;
    if(!AuthUnhex(MERO_AGENT_KEY_HEX,agentKey,32))return 0;AuthInit(&hash);AuthUpdate(&hash,agentKey,32);AuthFinal(&hash,digest);AuthHex(digest,32,tag);tag[16]=0;AgentWide(tag,wide,65);
    wsprintfW(name,L"MERO_FOSTON_AGENT_%s",wide);*mutex=CreateMutexW(NULL,TRUE,name);if(!*mutex||GetLastError()==ERROR_ALREADY_EXISTS){if(*mutex)CloseHandle(*mutex);*mutex=NULL;Note(L"Another device agent is already running");return 0;}
    wsprintfW(agentRoot,L"\\SDMMC\\MERO\\agent-%s",wide);CreateDirectoryW(L"\\SDMMC\\MERO",NULL);
    if(!CreateDirectoryW(agentRoot,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS){Note(L"Agent needs writable SD: use ActiveSync, not Mass Storage");return 0;}
    agentLast=0;wsprintfW(pattern,L"%s\\job-*",agentRoot);find=FindFirstFileW(pattern,&fd);
    if(find!=INVALID_HANDLE_VALUE){do{char value[32]={0};unsigned int seq;if(!WideCharToMultiByte(CP_ACP,0,fd.cFileName+4,-1,value,sizeof(value),NULL,NULL))continue;if((fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&AgentNumber(value,&seq)&&seq>agentLast)agentLast=seq;}while(FindNextFileW(find,&fd));FindClose(find);}
    if(agentLast)AgentRestoreMissing(agentLast);return 1;
}
static void AgentExecute(unsigned int seq,const char *body,const char *op,char result[2049]){
    char path[192]="",normal[192],wanted[65]="",sha[65]="",number[32],message[96]="operation_failed";WCHAR file[260],incoming[260],dest[192];unsigned int size=0,expected=0,value=0;int ok=0;
    if(!AgentGet(body,"path",path,sizeof(path))&&strcmp(op,"status")){snprintf(result,2049,"v=1\ndevice=%s\nseq=%u\nop=%s\nok=0\nmessage=invalid_path_field\n",MERO_AGENT_DEVICE,seq,op);return;}
    AgentJobPath(seq,L"reply.bin",file);AgentJobPath(seq,L"incoming.bin",incoming);
    if(!strcmp(op,"status")){MEMORYSTATUS memory;ULARGE_INTEGER free;memset(&memory,0,sizeof(memory));memory.dwLength=sizeof(memory);GlobalMemoryStatus(&memory);free.QuadPart=0;GetDiskFreeSpaceExW(L"\\SDMMC",&free,NULL,NULL);
        WCHAR module[260];char moduleName[260],moduleHash[65]="";unsigned int moduleSize=0;GetModuleFileNameW(NULL,module,260);WideCharToMultiByte(CP_ACP,0,module,-1,moduleName,sizeof(moduleName),NULL,NULL);AgentHash(module,moduleHash,&moduleSize);
        snprintf(result,2049,"v=1\ndevice=%s\nseq=%u\nop=status\nok=1\nmessage=agent_ready\nversion=1\nagent_path=%s\nagent_sha256=%s\nfree_mem_mb=%lu\nfree_sd_mb=%lu\nsize=0\nsha256=\nvalue=0\n",MERO_AGENT_DEVICE,seq,moduleName,moduleHash,memory.dwAvailPhys/(1024*1024),(DWORD)(free.QuadPart/(1024*1024)));AgentStatusExtras(result,2049);return;}
    if(!strcmp(op,"get")||!strcmp(op,"ls")){
        if(!AgentPath(path,normal,0)){strcpy(message,"read_path_forbidden");goto done;}AgentWide(normal,dest,192);
        ok=!strcmp(op,"ls")?AgentList(dest,file):AgentCopy(dest,file);if(ok)ok=AgentHash(file,sha,&size);
    }else if(!strcmp(op,"put")){
        if(!AgentPath(path,normal,1)){strcpy(message,"write_path_forbidden");goto done;}
        if(!AgentGet(body,"size",number,sizeof(number))||!AgentNumber(number,&expected)||expected>AGENT_MAX_FILE||!AgentGet(body,"sha256",wanted,sizeof(wanted))||strlen(wanted)!=64){strcpy(message,"invalid_payload_metadata");goto done;}
        ULARGE_INTEGER free;free.QuadPart=0;if(!GetDiskFreeSpaceExW(L"\\SDMMC",&free,NULL,NULL)||free.QuadPart<(unsigned long long)expected*3+65536){strcpy(message,"insufficient_SD_space");goto done;}
        HANDLE out=CreateFileW(incoming,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(out==INVALID_HANDLE_VALUE)goto done;
        unsigned int unused=0;ok=GetBody("agent-payload.bin",NULL,out,&unused);if(ok)ok=FlushFileBuffers(out);CloseHandle(out);
        if(!ok||!AgentHash(incoming,sha,&size)||size!=expected||strcmp(sha,wanted)){ok=0;strcpy(message,"payload_hash_or_size_mismatch");goto done;}
        ok=AgentInstall(seq,normal,incoming,sha,&size);value=seq;
    }else if(!strcmp(op,"rollback")){
        unsigned int ref;WCHAR metaFile[260],backup[260];char meta[2049];
        if(!AgentGet(body,"ref",number,sizeof(number))||!AgentNumber(number,&ref)||!ref||ref>=seq)goto done;
        AgentJobPath(ref,L"meta.txt",metaFile);int n=AgentRead(metaFile,meta,sizeof(meta));
        if(!n||!AgentVerify(meta,n,agentKey)||!AgentGet(meta,"path",path,sizeof(path))||!AgentGet(meta,"old_sha256",wanted,sizeof(wanted))||strlen(wanted)!=64){strcpy(message,"no_verified_previous_file");goto done;}
        AgentJobPath(ref,L"backup.bin",backup);if(!AgentCopy(backup,incoming)||!AgentHash(incoming,sha,&size)||strcmp(sha,wanted)){strcpy(message,"backup_hash_mismatch");goto done;}
        ok=AgentInstall(seq,path,incoming,sha,&size);value=seq;
    }else if(!strcmp(op,"launch")||!strcmp(op,"stop")){
        char args[32]="";AgentGet(body,"args",args,sizeof(args));
        ok=AgentControl(path,!strcmp(op,"launch"),args,&value);
    }
    else strcpy(message,"unsupported_operation");
done:
    if(ok)strcpy(message,"complete");
    snprintf(result,2049,"v=1\ndevice=%s\nseq=%u\nop=%s\nok=%d\nmessage=%s\nerror=%lu\nsize=%u\nsha256=%s\nvalue=%u\n",MERO_AGENT_DEVICE,seq,op,ok,message,ok?0:GetLastError(),size,sha,value);
}
static void AgentArtwork(const char *sha,const char *track) {
    char file[96],actual[65];BYTE digest[32];unsigned int n,i;AuthHash hash;
    if(!FindWindowW(L"MERO_MUSIC",NULL)) {
        if(mero_art&&MeroBusLock()){memset(mero_art,0,sizeof(*mero_art));ReleaseMutex(mero_bus_mutex);MeroArtClose();}
        return;
    }
    if(strlen(sha)!=64||strlen(track)!=11||!AuthUnhex(sha,digest,32))return;
    for(i=0;track[i];i++)if(!((track[i]>='a'&&track[i]<='z')||(track[i]>='A'&&track[i]<='Z')||(track[i]>='0'&&track[i]<='9')||track[i]=='_'||track[i]=='-'))return;
    if(!MeroArtOpen())return;
    if(MeroBusLock()) {
        if(!strcmp(mero_art->sha,sha)){strcpy(mero_art->track,track);ReleaseMutex(mero_bus_mutex);return;}
        ReleaseMutex(mero_bus_mutex);
    }
    snprintf(file,sizeof(file),"art-%s.565",sha);
    if(!GetBody(file,NULL,INVALID_HANDLE_VALUE,&n)){ClosePeer();return;}
    if(n!=MERO_ART_BYTES)return;
    AuthInit(&hash);AuthUpdate(&hash,xml,n);AuthFinal(&hash,digest);AuthHex(digest,32,actual);
    if(strcmp(actual,sha)||!FindWindowW(L"MERO_MUSIC",NULL)||!MeroBusLock())return;
    memcpy(mero_art->pixels,xml,n);strcpy(mero_art->sha,sha);strcpy(mero_art->track,track);
    ReleaseMutex(mero_bus_mutex);
}
static void AgentLive(void) {
    static char serial[40] = "";
    static int sawFirst;
    char body[8193], version[8], device[32], next[40], epoch[24], data[5600];
    char lines[MERO_BUS_BYTES], ack[80], result[96], event[2049],art[65]="",artTrack[32]="";
    unsigned int n, stamp;
    if (channel == INVALID_SOCKET) return;
    if (!GetBody("live-state.txt", NULL, INVALID_HANDLE_VALUE, &n)) { ClosePeer(); return; }
    if (n > 8192) return;
    memcpy(body, xml, n); body[n] = 0;
    if (!AgentVerifyLimit(body, n, agentKey, 8192) ||
        !AgentGet(body,"v",version,sizeof(version)) || strcmp(version,"1") ||
        !AgentGet(body,"device",device,sizeof(device)) || strcmp(device,MERO_AGENT_DEVICE) ||
        !AgentGet(body,"serial",next,sizeof(next)) || !next[0] ||
        !AgentGet(body,"epoch",epoch,sizeof(epoch)) || !AgentNumber(epoch,&stamp) ||
        !AgentGet(body,"data",data,sizeof(data)) ||
        !AgentGet(body,"ack",ack,sizeof(ack)) || !AgentGet(body,"result",result,sizeof(result)) ||
        MeroBase64(data,lines,sizeof(lines)) < 0) return;
    {unsigned int i;for(i=0;next[i];i++)if(next[i]<'0'||next[i]>'9')return;}
    if(serial[0] && (strlen(next)<strlen(serial) ||
       (strlen(next)==strlen(serial) && strcmp(next,serial)<0)))return;
    /* A file left on the PC while the bridge is stopped is not a live link.
       Require two increasing snapshots before publishing after agent startup. */
    if(!sawFirst){strcpy(serial,next);sawFirst=1;return;}
    AgentGet(body,"art",art,sizeof(art));AgentGet(body,"art_track",artTrack,sizeof(artTrack));
    event[0] = 0;
    if (!MeroBusLock()) return;
    if (strcmp(serial,next)) {
        strcpy(serial,next); strcpy(mero_bus->lines,lines);
        mero_bus->tick=GetTickCount(); mero_bus->epoch=stamp;
        if (!++mero_bus->generation) ++mero_bus->generation;
        strcpy(mero_bus->ack,ack); strcpy(mero_bus->result,result);
    }
    if (mero_bus->event[0] && strcmp(mero_bus->eventId,mero_bus->ack))
        snprintf(event,sizeof(event),"v=1\ndevice=%s\n%s",MERO_AGENT_DEVICE,mero_bus->event);
    ReleaseMutex(mero_bus_mutex);
    if (event[0] && AgentSign(event,sizeof(event),agentKey)) {
        CreateDirectoryW(L"\\Temp",NULL);
        if (AgentWrite(L"\\Temp\\MeroLiveEvent.txt",event,strlen(event)) &&
            !AgentSend("live-event.txt",L"\\Temp\\MeroLiveEvent.txt")) ClosePeer();
    }
    if(channel!=INVALID_SOCKET)AgentArtwork(art,artTrack);
}
static void AgentLoop(void){
    HANDLE mutex=NULL;char body[2049],op[32],number[32],device[32],version[8],result[2049];unsigned int n,seq,sent=0;
    /* SD and the USB function driver may become ready after the shell. */
    while(agentOn&&!AgentInit(&mutex)) {
        if(!mutex)goto finish; /* Another keyed agent already owns the link. */
        ReleaseMutex(mutex);CloseHandle(mutex);mutex=NULL;
        int i;Note(L"Waiting for writable SD / ActiveSync; retrying...");
        PostMessageW(window,WM_BT_RESULT,0,0);
        for(i=0;i<20&&agentOn;i++)Sleep(100);
    }
    if(!agentOn)goto finish;
    Note(L"Agent ON: waiting for signed PC commands");PostMessageW(window,WM_BT_RESULT,0,0);
    while(agentOn){
        if(InterlockedExchange(&agentReconnect,0))ClosePeer();
        if(channel==INVALID_SOCKET&&!OpenPeer())goto pause;
        if(!GetBody("agent-job.txt",NULL,INVALID_HANDLE_VALUE,&n)){ClosePeer();goto pause;}
        if(n>AGENT_MAX_TEXT)goto pause;memcpy(body,xml,n);body[n]=0;
        if(!AgentVerify(body,n,agentKey)||!AgentGet(body,"v",version,sizeof(version))||strcmp(version,"1")||!AgentGet(body,"device",device,sizeof(device))||strcmp(device,MERO_AGENT_DEVICE)||!AgentGet(body,"seq",number,sizeof(number))||!AgentNumber(number,&seq)||!seq||seq<agentLast||!AgentGet(body,"op",op,sizeof(op)))goto pause;
        if(seq==sent)goto pause;
        WCHAR dir[260],receipt[260],data[260];wsprintfW(dir,L"%s\\job-%u",agentRoot,seq);AgentJobPath(seq,L"result.txt",receipt);AgentJobPath(seq,L"reply.bin",data);
        if(seq>agentLast){
            if(!CreateDirectoryW(dir,NULL)){Note(L"Cannot persist job; check SD write access");goto pause;}
            agentLast=seq;AgentExecute(seq,body,op,result);
            if(!AgentSign(result,sizeof(result),agentKey)||!AgentWrite(receipt,result,(DWORD)strlen(result))){Note(L"Job result persistence failed; inspect recovery backup");goto pause;}
        }
        int length=AgentRead(receipt,result,sizeof(result));char verified[2049],ok[8];memcpy(verified,result,length+1);
        if(!length||!AgentVerify(verified,length,agentKey)){
            snprintf(result,sizeof(result),"v=1\ndevice=%s\nseq=%u\nop=%s\nok=0\nmessage=interrupted_job_inspect_backup_before_retry\nsize=0\nsha256=\nvalue=0\n",MERO_AGENT_DEVICE,seq,op);
            if(!AgentSign(result,sizeof(result),agentKey)||!AgentWrite(receipt,result,(DWORD)strlen(result)))goto pause;
            length=AgentRead(receipt,result,sizeof(result));memcpy(verified,result,length+1);if(!AgentVerify(verified,length,agentKey))goto pause;
        }
        if(AgentGet(verified,"ok",ok,sizeof(ok))&&!strcmp(ok,"1")&&(!strcmp(op,"get")||!strcmp(op,"ls"))&&!AgentSend("agent-output.bin",data)){ClosePeer();goto pause;}
        if(!AgentSend("agent-result.txt",receipt)){ClosePeer();goto pause;}
        sent=seq;
        WCHAR msg[180];wsprintfW(msg,L"Agent job %u complete; waiting for PC",seq);Note(msg);PostMessageW(window,WM_BT_RESULT,0,0);
pause:
        AgentLive();
        {int i;for(i=0;i<20&&agentOn;i++)Sleep(100);}
    }
finish:
    MeroArtClose();
    if(mutex){ReleaseMutex(mutex);CloseHandle(mutex);}ClosePeer();InterlockedExchange(&agentOn,0);
}
