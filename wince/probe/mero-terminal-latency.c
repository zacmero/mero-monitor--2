/* Harmless printf round trip: app input -> PC PTY -> authenticated RAM frame. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../mero-bus.h"
#include "../mero-live-codec.h"
int WINAPI WinMain(HINSTANCE a,HINSTANCE b,LPWSTR args,int show){
    HWND h;DWORD start,elapsed=0,done;int ok=0,count=0,i;
    char command[64],hex[129],marker[32],report[384],snapshot[4096],decoded[1100],eventId[80]="";HANDLE file;
    long queued=-1,acked=-1;
    (void)a;(void)b;(void)args;(void)show;
    /* The launching RPC persists its result to SD after CreateProcess.
       Never measure that durable journal work as ordinary typing latency. */
    Sleep(10000);h=FindWindowW(L"MERO_TERMINAL",NULL);start=GetTickCount();
    if(!h||GetForegroundWindow()!=h)goto report;
    snprintf(command,sizeof(command),"printf 'L%%s\\n' %08lx\r",start);snprintf(marker,sizeof(marker),"\nL%08lx\n",start);
    for(i=0;command[i];i++)sprintf(hex+i*2,"%02x",(unsigned char)command[i]);
    for(i=0;command[i];i++)PostMessageW(h,WM_CHAR,(unsigned char)command[i],0);
    while((DWORD)(GetTickCount()-start)<20000){
        snapshot[0]=0;if(MeroBusLock()){strcpy(snapshot,mero_bus->lines);
            if(queued<0&&strstr(mero_bus->event,hex)){queued=GetTickCount()-start;strcpy(eventId,mero_bus->eventId);}
            if(acked<0&&eventId[0]&&!strcmp(eventId,mero_bus->ack))acked=GetTickCount()-start;
            ReleaseMutex(mero_bus_mutex);}
        char *p=strstr(snapshot,"MERO:TERM:"),*end;p=p?strstr(p,"|text="):NULL;
        if(p){p+=6;end=strpbrk(p,"|\r\n");if(end)*end=0;
            if(MeroBase64(p,decoded,sizeof(decoded))>=0){char *found=decoded;count=0;while((found=strstr(found,marker))!=NULL){count++;found+=strlen(marker);}
                if(count){elapsed=GetTickCount()-start;ok=1;break;}}
        }Sleep(10);
    }
report:
    snprintf(report,sizeof(report),"ok=%d\nid=%08lx\ndefer_ms=10000\ninput_to_received_frame_ms=%lu\noutput_occurrences=%d\nqueued_ms=%ld\nack_ms=%ld\n",ok,start,elapsed,count,queued,acked);
    file=CreateFileW(L"\\SDMMC\\MERO\\terminal-latency.txt",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file!=INVALID_HANDLE_VALUE){WriteFile(file,report,strlen(report),&done,NULL);CloseHandle(file);}
    MeroBusClose();return ok?0:1;
}
