/* Local OEM backlight controls. Bluetooth is only needed for live companion state. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../mero-live.h"
#include "../mero-cinema-backlight.h"
#include "../mero-shell-return.h"
static HFONT largeFont,bodyFont,smallFont;
static DWORD currentRaw,originalRaw;
static int ready,haveOriginal,portrait=1;
static WCHAR message[128]=L"Compare MINIMUM and MAXIMUM on the panel.";
static COLORREF colors[6]={RGB(255,197,93),RGB(110,225,179),RGB(193,154,255),RGB(255,132,119),RGB(117,209,255),RGB(249,150,207)};
static const WCHAR *labels[]={L"-  DIMMER",L"+  BRIGHTER",L"MINIMUM",L"MAXIMUM",L"RESTORE",L"SHELL  >"};
static void SetLevel(int level){
    DWORD value,after;
    if(!ready)return;
    if(level<0)level=0;if(level>10)level=10;
    value=14+5*level;
    if(!MeroBrightness(&value,1)||!MeroBrightness(&after,0)||after!=value){
        lstrcpyW(message,L"Driver did not confirm this level.");return;
    }
    currentRaw=after;lstrcpyW(message,L"Level accepted. Check the panel's actual glow.");
}
static void Act(int action){
    int level=(int)(currentRaw-14)/5;
    if(action==0)SetLevel(level-1);
    else if(action==1)SetLevel(level+1);
    else if(action==2)SetLevel(0);
    else if(action==3)SetLevel(10);
    else if(action==4&&haveOriginal)SetLevel((int)(originalRaw-14)/5);
}
static void Refresh(void){
    DWORD raw;
    ready=MeroBrightness(&raw,0);
    if(ready){currentRaw=raw;if(!haveOriginal){originalRaw=raw;haveOriginal=1;}}
    else lstrcpyW(message,L"Backlight control unavailable. SHELL still works.");
}
static HFONT Font(int size){LOGFONTW lf;memset(&lf,0,sizeof(lf));lf.lfHeight=size;lf.lfWeight=FW_NORMAL;lstrcpyW(lf.lfFaceName,L"Tahoma");return CreateFontIndirectW(&lf);}
static void Fill(HDC dc,int x,int y,int r,int b,COLORREF color){RECT a={x,y,r,b};HBRUSH brush=CreateSolidBrush(color);FillRect(dc,&a,brush);DeleteObject(brush);}
static void Text(HDC dc,HFONT font,COLORREF color,const WCHAR *s,int x,int y,int r,int b){RECT a={x,y,r,b};SelectObject(dc,font);SetTextColor(dc,color);DrawTextW(dc,s,-1,&a,DT_CENTER|DT_VCENTER|DT_WORDBREAK);}
static RECT Button(int i){int w=portrait?272:480,y=portrait?218:114,h=portrait?62:42;RECT r={8+(i%2)*(w/2),y+(i/2)*h,w/2-8+(i%2)*(w/2),y+(i/2)*h+h-6};return r;}
static void HostLine(char *s){if(!strncmp(s,"MERO:AMB:",9)){char *p=strstr(s,"portrait=");if(p)portrait=atoi(p+9)!=0;}}
static void Paint(HWND window){
    PAINTSTRUCT ps;HDC screen=BeginPaint(window,&ps),dc=CreateCompatibleDC(screen);int w=portrait?272:480,h=portrait?480:272,i;
    BITMAPINFO bi;DWORD *pixels;memset(&bi,0,sizeof(bi));bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    HBITMAP bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,(void**)&pixels,NULL,0),old=(HBITMAP)SelectObject(dc,bitmap);WCHAR text[128];
    SetBkMode(dc,TRANSPARENT);Fill(dc,0,0,w,h,RGB(12,17,22));
    Text(dc,bodyFont,RGB(255,211,142),L"BACKLIGHT",10,8,w-10,35);
    if(ready)wsprintfW(text,L"LEVEL %d / 10",(currentRaw-14)/5);else lstrcpyW(text,L"UNAVAILABLE");
    Text(dc,largeFont,RGB(235,240,235),text,10,portrait?45:34,w-10,portrait?95:70);
    if(haveOriginal)wsprintfW(text,L"Starting level: %d / 10",(originalRaw-14)/5);else lstrcpyW(text,L"Waiting for backlight driver");
    Text(dc,smallFont,RGB(157,183,179),text,10,portrait?99:72,w-10,portrait?120:90);
    /* Constant white/gray patches make a physical brightness comparison easier. */
    int sy=portrait?135:94,sw=(w-32)/3;
    for(i=0;i<3;i++)Fill(dc,12+i*(sw+4),sy,12+i*(sw+4)+sw,sy+(portrait?42:12),RGB(255-i*70,255-i*70,255-i*70));
    if(portrait)Text(dc,smallFont,RGB(169,188,192),L"Compare the light from these patches.",10,179,w-10,207);
    for(i=0;i<6;i++){
        RECT r=Button(i);COLORREF c=colors[i];int enabled=ready||i==5;
        Fill(dc,r.left,r.top,r.right,r.bottom,enabled?RGB(GetRValue(c)/6,GetGValue(c)/6,GetBValue(c)/6):RGB(22,26,30));
        Text(dc,bodyFont,enabled?c:RGB(90,100,110),labels[i],r.left,r.top,r.right,r.bottom);
    }
    Text(dc,smallFont,RGB(200,212,205),message,8,portrait?408:241,w-8,portrait?446:258);
    Text(dc,smallFont,RGB(137,164,170),L"Close keeps this level. RESTORE uses the starting level.",8,portrait?448:257,w-8,h-2);
    if(portrait&&pixels){DWORD *out;HDC rot=CreateCompatibleDC(screen);bi.bmiHeader.biWidth=480;bi.bmiHeader.biHeight=-272;
        HBITMAP rb=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,(void**)&out,NULL,0),ro=(HBITMAP)SelectObject(rot,rb);
        if(out){int x,y;for(y=0;y<480;y++)for(x=0;x<272;x++)out[x*480+479-y]=pixels[y*272+x];BitBlt(screen,0,0,480,272,rot,0,0,SRCCOPY);}
        SelectObject(rot,ro);DeleteObject(rb);DeleteDC(rot);
    }else BitBlt(screen,0,0,480,272,dc,0,0,SRCCOPY);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);EndPaint(window,&ps);
}
static LRESULT CALLBACK WndProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    if(msg==WM_CREATE){Refresh();MeroLiveInit(h,HostLine);SetTimer(h,1,500,NULL);return 0;}
    if(msg==WM_TIMER){if(w==MERO_LIVE_TIMER)MeroLiveTick(h);else Refresh();if(!mero_standby)InvalidateRect(h,NULL,FALSE);return 0;}
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT){Paint(h);return 0;}
    if(msg==WM_CHAR&&!mero_standby){if(w=='-')Act(0);else if(w=='+')Act(1);else if(w=='0')Act(2);else if(w=='9')Act(3);else if(w=='r'||w=='R')Act(4);InvalidateRect(h,NULL,FALSE);return 0;}
    if(msg==WM_KEYDOWN&&w==VK_ESCAPE&&!mero_standby){MeroReturnToShell();DestroyWindow(h);return 0;}
    if(msg==WM_LBUTTONDOWN&&!mero_standby){POINT p={LOWORD(l),HIWORD(l)};int i;if(portrait){int x=p.x;p.x=p.y;p.y=479-x;}
        for(i=0;i<6;i++){RECT r=Button(i);if(PtInRect(&r,p)){if(i==5){MeroReturnToShell();DestroyWindow(h);}else{Act(i);InvalidateRect(h,NULL,FALSE);}break;}}return 0;
    }
    if(msg==WM_CLOSE){MeroReturnToShell();DestroyWindow(h);return 0;}
    if(msg==WM_DESTROY){MeroLiveClose(h);KillTimer(h,1);PostQuitMessage(0);return 0;}
    return DefWindowProcW(h,msg,w,l);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR args,int show){
    WNDCLASSW wc;MSG msg;HWND h=FindWindowW(L"MERO_BACKLIGHT",NULL);(void)previous;(void)args;(void)show;
    if(h){ShowWindow(h,SW_SHOW);SetForegroundWindow(h);return 0;}
    largeFont=Font(29);bodyFont=Font(18);smallFont=Font(13);
    memset(&wc,0,sizeof(wc));wc.lpfnWndProc=WndProc;wc.hInstance=instance;wc.lpszClassName=L"MERO_BACKLIGHT";RegisterClassW(&wc);
    h=CreateWindowExW(WS_EX_TOPMOST,wc.lpszClassName,L"Backlight",WS_VISIBLE|WS_POPUP,0,0,480,272,NULL,NULL,instance,NULL);if(!h)return 1;
    while(GetMessageW(&msg,NULL,0,0)){TranslateMessage(&msg);DispatchMessageW(&msg);}DeleteObject(largeFont);DeleteObject(bodyFont);DeleteObject(smallFont);return 0;
}
