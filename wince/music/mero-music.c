/* Small, truthful YouTube Music remote. The host alone selects/controls MPRIS. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../mero-live.h"
#include "../mero-shell-return.h"
#include "../mero-art.h"
static HWND mainWindow;
static HFONT titleFont, bodyFont, smallFont;
static char track[32], playback[24] = "UNAVAILABLE";
static WCHAR title[192] = L"Open YouTube Music on the PC", artist[192];
static DWORD received;
static unsigned int position, duration;
static int canNext, canPrevious, canPlay, canSeek, haveState;
static WCHAR notice[96] = L"Waiting for Bluetooth agent";
static char artHash[65],paintedArt[65];
static HDC artDC;
static HBITMAP artBitmap,artOldBitmap;
static DWORD *artBits;
static HFONT Font(int height,int weight) {
    LOGFONTW lf;memset(&lf,0,sizeof(lf));lf.lfHeight=height;lf.lfWeight=weight;
    lstrcpyW(lf.lfFaceName,L"Tahoma");return CreateFontIndirectW(&lf);
}

static void Text(HDC dc, HFONT font, COLORREF color, const WCHAR *s, int x,int y,int right,int bottom,UINT flags) {
    RECT r = {x,y,right,bottom}; SelectObject(dc,font); SetTextColor(dc,color);
    DrawTextW(dc,s,-1,&r,flags);
}
static void Wide(WCHAR *dest,int cap,const char *s) {
    WCHAR temp[512]; if(MultiByteToWideChar(CP_UTF8,0,s,-1,temp,512)) lstrcpynW(dest,temp,cap);
}
static void HostLine(char *line) {
    char *p,*end;
    char previousTrack[32],previousArt[65];
    if(strncmp(line,"MERO:NOW:",9))return;
    strcpy(previousTrack,track);strcpy(previousArt,artHash);
    p=line+9; canNext=canPrevious=canPlay=canSeek=0; duration=position=0; artHash[0]=0;
    while(*p) {
        end=strchr(p,'|');if(end)*end=0;
        if(!strncmp(p,"title=",6))Wide(title,192,p+6);
        else if(!strncmp(p,"artist=",7))Wide(artist,192,p+7);
        else if(!strncmp(p,"track=",6)){strncpy(track,p+6,sizeof(track)-1);track[sizeof(track)-1]=0;}
        else if(!strncmp(p,"status=",7)){strncpy(playback,p+7,sizeof(playback)-1);playback[sizeof(playback)-1]=0;}
        else if(!strncmp(p,"art=",4)){strncpy(artHash,p+4,sizeof(artHash)-1);artHash[sizeof(artHash)-1]=0;}
        else if(!strncmp(p,"pos=",4))position=strtoul(p+4,NULL,10);
        else if(!strncmp(p,"len=",4))duration=strtoul(p+4,NULL,10);
        else if(!strncmp(p,"next=",5))canNext=atoi(p+5)==1;
        else if(!strncmp(p,"previous=",9))canPrevious=atoi(p+9)==1;
        else if(!strncmp(p,"playpause=",10))canPlay=atoi(p+10)==1;
        else if(!strncmp(p,"seek=",5))canSeek=atoi(p+5)==1;
        if(!end)break;p=end+1;
    }
    if(strcmp(previousTrack,track)||strcmp(previousArt,artHash)) {
        if(artBits)memset(artBits,0,MERO_ART_SIDE*MERO_ART_SIDE*sizeof(DWORD));
        paintedArt[0]=0;
        if(MeroArtOpen()&&MeroBusLock()) {
            /* Keep only bytes already verified for this exact current cover. */
            if(!artHash[0]||strcmp(mero_art->track,track)||strcmp(mero_art->sha,artHash))
                memset(mero_art,0,sizeof(*mero_art));
            ReleaseMutex(mero_bus_mutex);
        }
    }
    if(!haveState)lstrcpyW(notice,L"Controls ready");
    received=GetTickCount(); haveState=1; InvalidateRect(mainWindow,NULL,FALSE);
}
static int Fresh(void) {return haveState && (DWORD)(GetTickCount()-received)<15000 && track[0];}
static unsigned int CurrentPosition(void) {
    unsigned int p=position,elapsed=(DWORD)(GetTickCount()-received)/1000;
    if(elapsed>15)elapsed=15;
    if(!strcmp(playback,"PLAYING"))p+=elapsed;
    if(duration && p>duration)p=duration;
    return p;
}
static void Artwork(HDC dc) {
    RECT area={16,43,128,155};HBRUSH brush=CreateSolidBrush(RGB(13,25,30));int ready=0;
    FillRect(dc,&area,brush);DeleteObject(brush);
    if(artHash[0]&&MeroArtOpen()&&MeroBusLock()) {
        if(!strcmp(mero_art->sha,artHash)&&!strcmp(mero_art->track,track)) {
            if(!artBitmap){BITMAPINFO info;memset(&info,0,sizeof(info));info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
                info.bmiHeader.biWidth=MERO_ART_SIDE;info.bmiHeader.biHeight=-MERO_ART_SIDE;info.bmiHeader.biPlanes=1;
                info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
                artBitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void**)&artBits,NULL,0);artDC=CreateCompatibleDC(dc);
                if(artBitmap&&artDC)artOldBitmap=(HBITMAP)SelectObject(artDC,artBitmap);
            }
            if(artBitmap&&artDC&&artBits){
                if(strcmp(paintedArt,artHash)){int i;for(i=0;i<MERO_ART_SIDE*MERO_ART_SIDE;i++) {
                    unsigned int c=mero_art->pixels[i];
                    artBits[i]=(((c>>11)*255/31)<<16)|((((c>>5)&63)*255/63)<<8)|((c&31)*255/31);
                }strcpy(paintedArt,artHash);}
                ready=1;
            }
        }
        ReleaseMutex(mero_bus_mutex);
    }
    if(ready)BitBlt(dc,16,43,MERO_ART_SIDE,MERO_ART_SIDE,artDC,0,0,SRCCOPY);
    else Text(dc,smallFont,RGB(110,155,165),artHash[0]?L"Cover loading":L"YouTube Music",20,80,124,122,DT_CENTER|DT_WORDBREAK);
}
static void Paint(HWND h) {
    PAINTSTRUCT ps; HDC screen=BeginPaint(h,&ps),dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,480,272),old=(HBITMAP)SelectObject(dc,bitmap);
    RECT all={0,0,480,272},progress={18,161,462,169};HBRUSH b;
    WCHAR timing[64],state[96];unsigned int pos=CurrentPosition();int i,pending=0;
    FillRect(dc,&all,(HBRUSH)GetStockObject(BLACK_BRUSH));SetBkMode(dc,TRANSPARENT);
    Text(dc,smallFont,RGB(60,220,190),L"YOUTUBE MUSIC",16,10,350,30,DT_SINGLELINE);
    Text(dc,bodyFont,RGB(255,120,110),L"SHELL",400,7,478,38,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    Artwork(dc);
    Text(dc,titleFont,RGB(235,244,250),title,144,45,466,106,DT_WORDBREAK|DT_END_ELLIPSIS);
    Text(dc,bodyFont,RGB(120,200,210),artist,145,113,462,152,DT_WORDBREAK|DT_END_ELLIPSIS);
    b=CreateSolidBrush(RGB(24,43,49));FillRect(dc,&progress,b);DeleteObject(b);
    if(duration){progress.right=18+(int)(444.0*pos/duration);b=CreateSolidBrush(RGB(60,220,190));FillRect(dc,&progress,b);DeleteObject(b);
        wsprintfW(timing,L"%u:%02u / %u:%02u",pos/60,pos%60,duration/60,duration%60);
    }else wsprintfW(timing,L"%u:%02u  /  timing unavailable",pos/60,pos%60);
    Text(dc,smallFont,RGB(150,170,180),timing,18,172,462,188,DT_SINGLELINE|DT_CENTER);
    if(MeroBusLock()) {
        pending=mero_bus->event[0] && strcmp(mero_bus->eventId,mero_bus->ack);
        if(pending)lstrcpyW(notice,L"Sending command...");
        else if(mero_bus->ack[0]) {
            if(!strcmp(mero_bus->result,"ok"))lstrcpyW(notice,L"Command received by PC");
            else if(!strcmp(mero_bus->result,"track_changed_press_again"))lstrcpyW(notice,L"Song changed; tap again");
            else if(!strcmp(mero_bus->result,"expired_press_again"))lstrcpyW(notice,L"That press expired; tap again");
            else if(!strcmp(mero_bus->result,"control_unavailable"))lstrcpyW(notice,L"The browser cannot use that control");
            else lstrcpyW(notice,L"Command did not finish; check the PC");
        }
        ReleaseMutex(mero_bus_mutex);
    }
    for(i=0;i<3;i++) {
        RECT r={12+i*156,193,156+i*156,237}; int enabled=Fresh()&&!pending&&(i==0?canPrevious:i==1?canPlay:canNext);
        b=CreateSolidBrush(enabled?RGB(15,58,62):RGB(19,26,30));FillRect(dc,&r,b);DeleteObject(b);
        Text(dc,bodyFont,enabled?RGB(170,255,235):RGB(90,105,110),i==0?L"PREVIOUS":i==2?L"NEXT":!strcmp(playback,"PLAYING")?L"PAUSE":L"PLAY",r.left,r.top,r.right,r.bottom,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    if(!haveState)lstrcpyW(state,L"Waiting for AGENT ON + PC bridge");
    else if(!Fresh())lstrcpyW(state,track[0]?L"Offline // last track cached":L"No active YouTube Music session");
    else wsprintfW(state,L"Bluetooth connected // %s",!strcmp(playback,"PLAYING")?L"playing":L"paused");
    Text(dc,smallFont,Fresh()?RGB(80,210,170):RGB(240,180,80),state,14,241,468,255,DT_SINGLELINE);
    Text(dc,smallFont,RGB(120,145,155),notice,14,257,468,271,DT_SINGLELINE|DT_END_ELLIPSIS);
    BitBlt(screen,0,0,480,272,dc,0,0,SRCCOPY);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);EndPaint(h,&ps);
}
static LRESULT CALLBACK WindowProc(HWND h,UINT msg,WPARAM w,LPARAM l) {
    if(msg==WM_CREATE){mainWindow=h;MeroLiveInit(h,HostLine);SetTimer(h,1,250,NULL);return 0;}
    if(msg==WM_TIMER){if(w==MERO_LIVE_TIMER)MeroLiveTick(h);if(!mero_standby)InvalidateRect(h,NULL,FALSE);return 0;}
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT){Paint(h);return 0;}
    if(msg==WM_LBUTTONDOWN&&!mero_standby){int x=LOWORD(l),y=HIWORD(l),ok=0;
        if(x>=392&&y<40){MeroReturnToShell();DestroyWindow(h);return 0;}
        if(Fresh()) {
            if(y>=193&&y<237){int i=(x-12)/156;if(x>=12&&x<468)
                if((i==0&&canPrevious)||(i==1&&canPlay)||(i==2&&canNext))ok=MeroBusCommand(i==0?"previous":i==1?"playpause":"next",track,0);}
            else if(y>=157&&y<191&&x>=18&&x<=462&&duration&&canSeek)
                ok=MeroBusCommand("seek",track,(unsigned int)((double)(x-18)*duration/444));
        }
        lstrcpyW(notice,ok?L"Sending command...":L"Waiting for link / previous command");InvalidateRect(h,NULL,FALSE);return 0;
    }
    if(msg==WM_CLOSE){MeroReturnToShell();DestroyWindow(h);return 0;}
    if(msg==WM_DESTROY){
        if(mero_art&&MeroBusLock()){memset(mero_art,0,sizeof(*mero_art));ReleaseMutex(mero_bus_mutex);}
        if(artBits)memset(artBits,0,MERO_ART_SIDE*MERO_ART_SIDE*sizeof(DWORD));
        MeroArtClose();if(artDC&&artOldBitmap)SelectObject(artDC,artOldBitmap);
        if(artBitmap)DeleteObject(artBitmap);if(artDC)DeleteDC(artDC);
        MeroLiveClose(h);KillTimer(h,1);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR args,int show) {
    WNDCLASSW wc;MSG msg;HWND existing=FindWindowW(L"MERO_MUSIC",NULL);(void)previous;(void)args;(void)show;
    if(existing){ShowWindow(existing,SW_SHOW);SetForegroundWindow(existing);return 0;}
    titleFont=Font(23,FW_BOLD);bodyFont=Font(18,FW_NORMAL);smallFont=Font(13,FW_NORMAL);
    memset(&wc,0,sizeof(wc));wc.lpfnWndProc=WindowProc;wc.hInstance=instance;wc.lpszClassName=L"MERO_MUSIC";
    RegisterClassW(&wc);mainWindow=CreateWindowExW(WS_EX_TOPMOST,wc.lpszClassName,L"Mero Music",WS_VISIBLE|WS_POPUP,0,0,480,272,NULL,NULL,instance,NULL);
    if(!mainWindow)return 1;
    while(GetMessageW(&msg,NULL,0,0)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    DeleteObject(titleFont);DeleteObject(bodyFont);DeleteObject(smallFont);return 0;
}
