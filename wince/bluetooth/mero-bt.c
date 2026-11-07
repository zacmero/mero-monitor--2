/* Native OBEX FTP client. Downloads stay in SD staging; never changes boot/ROM. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../mero-obex.h"
#include "../mero-live.h"
#include "../mero-shell-return.h"
#ifdef MERO_AGENT_BUILD
#include "mero-agent-key.h"
static volatile LONG agentOn;
static volatile LONG agentReconnect;
static volatile LONG agentManualAction = -1, agentManualBusy;
#define BT_WINDOW_CLASS L"MERO_FOSTON_AGENT"
#else
#define BT_WINDOW_CLASS L"MERO_BT_FILES"
#endif
#define WM_BT_RESULT (WM_APP+7)
#define MAX_FILES 64
static HWND window;
static volatile LONG busy;
static char names[MAX_FILES][96];
static int count,selected;
static WCHAR status[180]=L"Connect to mero-machine; files stay in SDMMC/BT";
static SOCKET channel=INVALID_SOCKET;
static unsigned int connectionId,peerLimit=4096;
static int haveId;
static unsigned char packet[4096],reply[4096];
static char xml[65536];
static DWORD lastRequest;

static void Note(const WCHAR *s) {
    HANDLE f;DWORD done;char utf8[720];
    lstrcpynW(status,s,180);
    f=CreateFileW(L"\\SDMMC\\MERO\\mero-bt.log",GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(f!=INVALID_HANDLE_VALUE){SetFilePointer(f,0,NULL,FILE_END);
        int n=WideCharToMultiByte(CP_UTF8,0,s,-1,utf8,sizeof(utf8)-3,NULL,NULL);
        if(n>0){utf8[n-1]='\r';utf8[n]='\n';WriteFile(f,utf8,n+1,&done,NULL);}CloseHandle(f);}
}
static void ClosePeer(void){if(channel!=INVALID_SOCKET){closesocket(channel);channel=INVALID_SOCKET;}haveId=0;}
static int WaitSocket(int reading) {
    fd_set f;struct timeval t;t.tv_sec=12;t.tv_usec=0;FD_ZERO(&f);FD_SET(channel,&f);
    return select(0,reading?&f:NULL,reading?NULL:&f,NULL,&t)>0;
}
static int TransferBytes(unsigned char *p,unsigned int n,int reading) {
    unsigned int done=0;int got;
    while(done<n){if(!WaitSocket(reading))return 0;
        /* Bound writes for the legacy WinCE RFCOMM provider: larger writes
           produced truncated OBEX frames despite reporting send completion. */
        unsigned int amount=n-done;if(!reading&&amount>256)amount=256;
        got=reading?recv(channel,(char*)p+done,amount,0):send(channel,(const char*)p+done,amount,0);
        if(got<=0){if(got<0&&WSAGetLastError()==WSAEWOULDBLOCK)continue;return 0;}done+=got;}
    return 1;
}
static unsigned int Exchange(unsigned int size) {
    unsigned int n;
    if(!size||size>peerLimit||!TransferBytes(packet,size,0)||!TransferBytes(reply,3,1)) {
        WCHAR message[100];wsprintfW(message,L"OBEX exchange failed / socket error %d",WSAGetLastError());Note(message);return 0;
    }
    n=Obex16(reply+1);if(n<3||n>sizeof(reply)||!TransferBytes(reply+3,n-3,1))return 0;
    return n;
}
static int SafeName(const char *p) {
    unsigned int i,n=(unsigned int)strlen(p);if(!n||n>95||p[0]=='.')return 0;
    for(i=0;i<n;i++)if((unsigned char)p[i]<32||(unsigned char)p[i]>126||strchr("<>:\"/\\|?*",p[i]))return 0;
    return 1;
}
static HWND Backend(void) {
    HWND h=FindWindowW(NULL,L"ANW_PHONELINK");
    if(!h)h=FindWindowW(L"ANW_PHONELINK",NULL);
    return h;
}
static int StartBackend(void) {
    HWND h=Backend();DWORD begin;STARTUPINFOW si;PROCESS_INFORMATION pi;
    if(!h){
        const WCHAR *path=L"\\ResidentFlash2\\App\\BT\\PhoneLink.exe";
        if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)path=L"\\ResidentFlash\\App\\BT\\PhoneLink.exe";
        memset(&si,0,sizeof(si));memset(&pi,0,sizeof(pi));si.cb=sizeof(si);
        si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;
        if(!CreateProcessW(path,NULL,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi)){Note(L"OEM Bluetooth backend could not start");return 0;}
        CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
        begin=GetTickCount();while(!(h=Backend())&&GetTickCount()-begin<10000)Sleep(100);
    }
    if(!h){Note(L"OEM backend window missing; inspect mero-bt.log");return 0;}
    ShowWindow(h,SW_HIDE);
    /* PhoneLink MFC message map and handler prove WPARAM=1 requests radio on. */
    PostMessageW(h,RegisterWindowMessageW(L"WM_ANWBT_INMSG_BTPOWER"),1,0);
    Sleep(1800);
    return 1;
}
static int OpenPeer(void) {
    unsigned char address[40];unsigned int n,pos,payload,length,next;u_long nonblocking=1;int error,errorLen=sizeof(error);
#ifdef MERO_AGENT_BUILD
    static const unsigned char peer[8]={MERO_AGENT_HOST_BYTES};
#else
    static const unsigned char peer[8]={0x70,0x27,0x83,0xdd,0x86,0x8c,0,0};
#endif
    static const unsigned char ftpGuid[16]={6,0x11,0,0,0,0,0,0x10,0x80,0,0,0x80,0x5f,0x9b,0x34,0xfb};
    WCHAR message[180];
    ClosePeer();if(!StartBackend())return 0;
    /* AF_BT=32, RFCOMM=3, sockaddr length=40 and offsets verified in BTService.dll. */
    memset(address,0,sizeof(address));address[0]=32;memcpy(address+8,peer,8);memcpy(address+16,ftpGuid,16);
    DWORD begin=GetTickCount();
    do {
        channel=socket(32,SOCK_STREAM,3);
        if(channel!=INVALID_SOCKET||WSAGetLastError()!=WSAENETDOWN)break;
        Sleep(200);
    }while(GetTickCount()-begin<8000);
    if(channel==INVALID_SOCKET){wsprintfW(message,L"Bluetooth socket failed: %d",WSAGetLastError());Note(message);return 0;}
    ioctlsocket(channel,FIONBIO,&nonblocking);
    if(connect(channel,(struct sockaddr*)address,sizeof(address))==SOCKET_ERROR){
        error=WSAGetLastError();
        if(error!=WSAEWOULDBLOCK&&error!=WSAEINPROGRESS){wsprintfW(message,L"Bluetooth connect failed: %d",error);Note(message);ClosePeer();return 0;}
        if(!WaitSocket(0)||getsockopt(channel,SOL_SOCKET,SO_ERROR,(char*)&error,&errorLen)||error){
            wsprintfW(message,L"Bluetooth connect timed out / error %d",error);Note(message);ClosePeer();return 0;}
    }
    peerLimit=4096;n=Exchange(ObexConnect(packet));
    if(n<7||reply[0]!=0xa0){Note(L"OBEX FTP handshake rejected / timed out");ClosePeer();return 0;}
    peerLimit=Obex16(reply+5);if(peerLimit<255){Note(L"Invalid OBEX packet limit");ClosePeer();return 0;}
    if(peerLimit>4096)peerLimit=4096;
    pos=7;while(pos<n){next=ObexHeader(reply,n,pos,&payload,&length);if(!next){ClosePeer();return 0;}
        if(reply[pos]==0xcb){connectionId=Obex32(reply+payload);haveId=1;}pos=next;}
    wsprintfW(message,L"FTP handshake OK: MTU=%u, connection ID=%u",peerLimit,connectionId);
    Note(message);return 1;
}
static int GetBody(const char *name,const char *type,HANDLE file,unsigned int *xmlLength) {
    unsigned int n,pos,next,payload,length,total=0,requests=0;DWORD written;
    unsigned int request=ObexRequest(packet,0x83,haveId,connectionId,name,type);
    do{
        n=Exchange(request);if(!n||(reply[0]!=0x90&&reply[0]!=0xa0)) {
            WCHAR message[100];wsprintfW(message,L"FTP GET failed: response=%02X, packet bytes=%u",n?reply[0]:0,n);Note(message);return 0;
        }
        pos=3;while(pos<n){next=ObexHeader(reply,n,pos,&payload,&length);if(!next)return 0;
            if(reply[pos]==0x48||reply[pos]==0x49){
                if(total+length>16*1024*1024)return 0;
                if(file==INVALID_HANDLE_VALUE){if(total+length>=sizeof(xml))return 0;memcpy(xml+total,reply+payload,length);}
                else if(!WriteFile(file,reply+payload,length,&written,NULL)||written!=length)return 0;
                total+=length;}
            pos=next;}
        if(reply[0]==0xa0)break;
        if(++requests>32768)return 0;
        request=ObexRequest(packet,0x83,haveId,connectionId,NULL,NULL);
    }while(1);
    if(file==INVALID_HANDLE_VALUE){xml[total]=0;*xmlLength=total;}return 1;
}
static void ListFiles(void) {
    char *p,*end;unsigned int length;
    count=selected=0;
    if(!GetBody(NULL,"x-obex/folder-listing",INVALID_HANDLE_VALUE,&length)){Note(L"Folder listing failed; reconnect");ClosePeer();return;}
    p=xml;while(count<MAX_FILES&&(p=strstr(p,"<file"))){p+=5;end=strchr(p,'>');if(!end)break;
        char *name=strstr(p,"name=\"");if(!name||name>end)continue;name+=6;
        char *quote=strchr(name,'\"');if(!quote||quote>end||quote-name>95)continue;
        char value[96];memcpy(value,name,quote-name);value[quote-name]=0;
        /* Initial transfer directory uses ASCII names; XML entities are skipped. */
        if(SafeName(value)&&!strchr(value,'&'))strcpy(names[count++],value);
        p=end+1;}
    WCHAR msg[180];wsprintfW(msg,L"FTP listing: %d files. Select and download.",count);Note(msg);
}
static void Download(void) {
    HANDLE file;WCHAR path[260],final[260],name[96];unsigned int unused=0;DWORD tick=GetTickCount();
    if(!count){Note(L"No file selected");return;}
    MultiByteToWideChar(CP_ACP,0,names[selected],-1,name,96);
    CreateDirectoryW(L"\\SDMMC\\BT",NULL);CreateDirectoryW(L"\\SDMMC\\BT\\Downloads",NULL);
    wsprintfW(path,L"\\SDMMC\\BT\\Downloads\\partial-%lu-%s",tick,name);
    wsprintfW(final,L"\\SDMMC\\BT\\Downloads\\%lu-%s",tick,name);
    file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE){Note(L"Cannot create SD download; leave mass storage first");return;}
    int ok=GetBody(names[selected],NULL,file,&unused);if(ok)ok=FlushFileBuffers(file);CloseHandle(file);
    if(ok&&MoveFileW(path,final)){WCHAR msg[180];wsprintfW(msg,L"Saved: BT/Downloads/%lu-%s",tick,name);Note(msg);}
    else{Note(L"Download incomplete; partial file preserved. Reconnect.");ClosePeer();}
}
static void UploadTest(void) {
    static const char data[]="MERO FOSTON BLUETOOTH FILE TRANSFER TEST\r\n";
    unsigned int n=ObexRequest(packet,0x82,haveId,connectionId,"foston-bt-check.txt",NULL);
    packet[n++]=0xc3;ObexWrite32(packet+n,sizeof(data)-1);n+=4;
    packet[n++]=0x49;ObexWrite16(packet+n,sizeof(data)+2);n+=2;memcpy(packet+n,data,sizeof(data)-1);n+=sizeof(data)-1;ObexWrite16(packet+1,n);
    n=Exchange(n);if(n&&reply[0]==0xa0)Note(L"Test file uploaded: foston-bt-check.txt");
    else{Note(L"Upload test failed; reconnect");ClosePeer();}
}
#ifdef MERO_AGENT_BUILD
#include "mero-agent-runtime.h"
#endif
static DWORD WINAPI Worker(LPVOID value) {
    int action=(int)(LONG)value;
#ifdef MERO_AGENT_BUILD
    if(action==4){AgentLoop();InterlockedExchange(&busy,0);PostMessageW(window,WM_BT_RESULT,0,0);return 0;}
#endif
    if(action==0){if(OpenPeer())ListFiles();}
    else if(channel==INVALID_SOCKET)Note(L"Connect first");
    else if(action==1)ListFiles();else if(action==2)Download();else if(action==3)UploadTest();
    InterlockedExchange(&busy,0);PostMessageW(window,WM_BT_RESULT,0,0);return 0;
}
static void Run(int action) {
    if(InterlockedCompareExchange(&busy,1,0))return;
    Note(L"Working... Bluetooth timeout is 12 seconds");InvalidateRect(window,NULL,TRUE);
    HANDLE thread=CreateThread(NULL,0,Worker,(LPVOID)(LONG)action,0,NULL);
    if(thread)CloseHandle(thread);else{busy=0;Note(L"Cannot start transfer worker");}
}
static LRESULT CALLBACK WndProc(HWND h,UINT msg,WPARAM w,LPARAM l) {
#ifdef MERO_AGENT_BUILD
    if(msg==RegisterWindowMessageW(L"MERO_AGENT_CONNECT_V1")) {
        InterlockedExchange(&agentReconnect,1);
        InterlockedCompareExchange(&agentManualAction,0,-1);
        if(!agentOn&&!busy){InterlockedExchange(&agentOn,1);Run(4);}
        if(w){ShowWindow(h,SW_SHOW);SetForegroundWindow(h);}
        Note(L"Agent reconnect requested");InvalidateRect(h,NULL,TRUE);return 0;
    }
#endif
    if(msg==WM_CREATE){SetTimer(h,1,250,NULL);MeroLiveInit(h,NULL);return 0;}
    if(msg==WM_TIMER){if(w==MERO_LIVE_TIMER){MeroLiveTick(h);
#ifdef MERO_AGENT_BUILD
        InvalidateRect(h,NULL,FALSE);
#endif
        return 0;}
#ifdef MERO_AGENT_BUILD
        if(agentReconnect&&!agentOn&&!busy){InterlockedExchange(&agentOn,1);Run(4);}
#endif
        if(busy){HWND back=Backend();if(back)ShowWindow(back,SW_HIDE);
#ifdef MERO_AGENT_BUILD
            if(!agentOn)
#endif
                SetForegroundWindow(h);}
        if(lastRequest&&GetTickCount()-lastRequest<5000)SetForegroundWindow(h);
        return 0;}
    if(msg==WM_BT_RESULT){InvalidateRect(h,NULL,TRUE);return 0;}
    if(msg==WM_LBUTTONUP&&!mero_standby){int x=LOWORD(l),y=HIWORD(l);
#ifdef MERO_AGENT_BUILD
        if(x>=325&&y<33){if(agentOn){InterlockedExchange(&agentReconnect,0);InterlockedExchange(&agentOn,0);Note(L"Agent will pause after the current operation");}else if(!busy){InterlockedExchange(&agentOn,1);Run(4);}InvalidateRect(h,NULL,TRUE);return 0;}
        if(x>=330&&y>=193&&y<226){MeroReturnToShell();ShowWindow(h,SW_HIDE);return 0;}
#endif
        if(busy
#ifdef MERO_AGENT_BUILD
            && !agentOn
#endif
        )return 0;
        if(x<320&&y>=44&&y<194){int row=(y-44)/25;int start=selected/6*6;if(start+row<count)selected=start+row;InvalidateRect(h,NULL,TRUE);}
        else if(x>=330&&y>=38&&y<226){int b=(y-38)/31;
#ifdef MERO_AGENT_BUILD
            if(agentOn&&b<3) {
                if(!agentManualBusy&&InterlockedCompareExchange(&agentManualAction,b==0?0:b==1?2:3,-1)==-1)
                    Note(L"File action queued");
                return 0;
            }
#endif
            if(b==0){lastRequest=GetTickCount();Run(0);}else if(b==1)Run(2);else if(b==2)Run(3);
            else if(b==3){if(selected>0)selected--;InvalidateRect(h,NULL,TRUE);}else if(b==4){if(selected+1<count)selected++;InvalidateRect(h,NULL,TRUE);}else {MeroReturnToShell();DestroyWindow(h);}}
        return 0;}
    if(msg==WM_PAINT){PAINTSTRUCT ps;HDC screen=BeginPaint(h,&ps),dc=CreateCompatibleDC(screen);
        HBITMAP bitmap=CreateCompatibleBitmap(screen,480,272),previous=(HBITMAP)SelectObject(dc,bitmap);
        RECT r={0,0,480,272};HBRUSH black=CreateSolidBrush(RGB(8,14,20));FillRect(dc,&r,black);DeleteObject(black);
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(80,220,180));DrawTextW(dc,L"MERO BLUETOOTH FILES",-1,&(RECT){12,5,470,32},DT_LEFT);
#ifdef MERO_AGENT_BUILD
        SetTextColor(dc,RGB(255,210,100));DrawTextW(dc,agentOn?L"[AGENT ON]":L"[AGENT OFF]",-1,&(RECT){325,5,477,32},DT_LEFT);
#endif
        int i,start=selected/6*6;for(i=0;i<6&&start+i<count;i++){WCHAR line[100];MultiByteToWideChar(CP_ACP,0,names[start+i],-1,line,100);
            SetTextColor(dc,start+i==selected?RGB(255,230,120):RGB(200,210,220));DrawTextW(dc,line,-1,&(RECT){14,44+i*25,320,69+i*25},DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS);}
        const WCHAR *buttons[]={L"CONNECT / LIST",L"DOWNLOAD FILE",L"UPLOAD TEST",L"PREVIOUS",L"NEXT",L"BACK"};SetTextColor(dc,RGB(100,190,255));
        SelectClipRgn(dc,NULL);
        for(i=0;i<6;i++)ExtTextOutW(dc,330,38+i*31,0,NULL,buttons[i],lstrlenW(buttons[i]),NULL);
        SetTextColor(dc,RGB(210,210,210));
#ifdef MERO_AGENT_BUILD
        if(agentOn&&!agentManualBusy&&agentManualAction<0&&(!lastRequest||(DWORD)(GetTickCount()-lastRequest)>5000)) {
            WCHAR live[128];DWORD age=mero_received?(DWORD)(GetTickCount()-mero_received)/1000:99999;
            if(mero_received&&age<15)wsprintfW(live,L"CONNECTED // PC data %lus ago. File controls ready.",age);
            else lstrcpyW(live,L"CONNECTING // waiting for fresh PC data");
            SelectClipRgn(dc,NULL);ExtTextOutW(dc,10,240,0,NULL,live,lstrlenW(live),NULL);
        } else
#endif
        {SelectClipRgn(dc,NULL);ExtTextOutW(dc,10,240,0,NULL,status,lstrlenW(status),NULL);}
        BitBlt(screen,0,0,480,272,dc,0,0,SRCCOPY);SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);
        EndPaint(h,&ps);return 0;}
    if(msg==WM_DESTROY){MeroLiveClose(h);ClosePeer();PostQuitMessage(0);return 0;}
    return DefWindowProcW(h,msg,w,l);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE prev,LPWSTR cmd,int show) {
    WNDCLASSW wc;MSG m;WSADATA data;(void)prev;(void)cmd;(void)show;
    HWND existing=FindWindowW(BT_WINDOW_CLASS,NULL);
    if(existing){
#ifdef MERO_AGENT_BUILD
        if(cmd&&wcsstr(cmd,L"--connect"))PostMessageW(existing,RegisterWindowMessageW(L"MERO_AGENT_CONNECT_V1"),1,0);
#endif
        if(!cmd||!wcsstr(cmd,L"--background")){ShowWindow(existing,SW_SHOW);SetForegroundWindow(existing);}return 0;
    }
    if(WSAStartup(MAKEWORD(2,0),&data)){MessageBoxW(NULL,L"Winsock unavailable",L"Mero BT",MB_OK);return 1;}
    memset(&wc,0,sizeof(wc));wc.lpfnWndProc=WndProc;wc.hInstance=instance;wc.lpszClassName=BT_WINDOW_CLASS;RegisterClassW(&wc);
    window=CreateWindowExW(WS_EX_TOPMOST,wc.lpszClassName,L"Mero Bluetooth Files",WS_VISIBLE|WS_POPUP,0,0,480,272,NULL,NULL,instance,NULL);
#ifdef MERO_AGENT_BUILD
    InterlockedExchange(&agentOn,1);Run(4);
    if(cmd&&wcsstr(cmd,L"--background"))ShowWindow(window,SW_HIDE);
#endif
    while(GetMessageW(&m,NULL,0,0)){TranslateMessage(&m);DispatchMessageW(&m);}WSACleanup();return 0;
}
