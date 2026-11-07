/* Ambient Home: PC-selected folders, verified RAM frames, honest live weather. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../mero-live.h"
#include "../mero-shell-return.h"
#include "../mero-ambient.h"
static int portrait;
static HFONT largeFont,bodyFont,smallFont;
static HDC photoDC;
static HBITMAP photoBitmap,oldPhoto;
static DWORD *photoBits;
static char wanted[65],painted[65],token[32],paused[8],clockText[12];
static WCHAR folder[64]=L"PC photo folders",caption[96]=L"Connecting to your PC",state[96]=L"Waiting for live Bluetooth";
static WCHAR folders[8][64],temperature[16],description[96],location[96],low[12],high[12];
static DWORD ambientTick,weatherTick,lastPing;
static unsigned int weatherAge,frameTick,generation,shownGeneration;
static int haveAmbient,haveWeather,folderMenu,folderCount,weatherVisible=1,menuVisible,selectedFolder,expectedFolder=-1;
static WCHAR notice[96];
static char queuedAction[32],queuedToken[32];
static unsigned int queuedValue;
static DWORD queuedTick,expectedTick;
static HFONT Font(int height,int weight) {
    LOGFONTW lf;memset(&lf,0,sizeof(lf));lf.lfHeight=height;lf.lfWeight=weight;
    lstrcpyW(lf.lfFaceName,L"Tahoma");return CreateFontIndirectW(&lf);
}
static void Wide(WCHAR *dest,int cap,const char *value) {
    WCHAR temp[512];if(MultiByteToWideChar(CP_UTF8,0,value,-1,temp,512))lstrcpynW(dest,temp,cap);
}
static void Text(HDC dc,HFONT font,COLORREF color,const WCHAR *value,int x,int y,int right,int bottom,UINT flags) {
    RECT rect={x,y,right,bottom};SelectObject(dc,font);SetTextColor(dc,color);
    DrawTextW(dc,value,-1,&rect,flags);
}
static void Fill(HDC dc,int x,int y,int right,int bottom,COLORREF color) {
    RECT rect={x,y,right,bottom};HBRUSH b=CreateSolidBrush(color);FillRect(dc,&rect,b);DeleteObject(b);
}
static int Fresh(void) {return haveAmbient&&(DWORD)(GetTickCount()-ambientTick)<15000;}
static int Queue(const char *action,unsigned int value) {
    if(!Fresh()||queuedAction[0])return 0;
    strncpy(queuedAction,action,sizeof(queuedAction)-1);queuedValue=value;queuedTick=GetTickCount();
    strcpy(queuedToken,token);
    return 1;
}
static void FlushQueue(void) {
    if(!queuedAction[0])return;
    if((DWORD)(GetTickCount()-queuedTick)>15000) {
        queuedAction[0]=0;expectedFolder=-1;lstrcpyW(notice,L"Request expired; please tap again");return;
    }
    if(Fresh()&&MeroBusCommand(queuedAction,queuedToken,queuedValue)) {
        queuedAction[0]=0;lastPing=GetTickCount();
    }
}
static void WeatherIcon(HDC dc) {
    WCHAR condition[96];int i,rain,sunny;HPEN pen,oldPen;HBRUSH brush,oldBrush;
    lstrcpynW(condition,description,96);
    for(i=0;condition[i];i++)if(condition[i]>=L'A'&&condition[i]<=L'Z')condition[i]+=L'a'-L'A';
    rain=wcsstr(condition,L"rain")||wcsstr(condition,L"drizzle")||wcsstr(condition,L"shower");
    sunny=wcsstr(condition,L"sun")||wcsstr(condition,L"clear");
    pen=CreatePen(PS_SOLID,1,sunny?RGB(246,212,146):RGB(165,199,194));oldPen=(HPEN)SelectObject(dc,pen);
    brush=CreateSolidBrush(sunny?RGB(246,212,146):RGB(134,170,168));oldBrush=(HBRUSH)SelectObject(dc,brush);
    if(sunny) {
        int radius=9+(frameTick/8)%2;
        Ellipse(dc,167-radius,94-radius,167+radius,94+radius);
        MoveToEx(dc,167,73,NULL);LineTo(dc,167,79);MoveToEx(dc,167,109,NULL);LineTo(dc,167,115);
        MoveToEx(dc,146,94,NULL);LineTo(dc,152,94);MoveToEx(dc,182,94,NULL);LineTo(dc,188,94);
        MoveToEx(dc,152,79,NULL);LineTo(dc,156,83);MoveToEx(dc,178,105,NULL);LineTo(dc,182,109);
        MoveToEx(dc,152,109,NULL);LineTo(dc,156,105);MoveToEx(dc,178,83,NULL);LineTo(dc,182,79);
    } else {
        Ellipse(dc,147,87,172,105);Ellipse(dc,157,78,182,104);Ellipse(dc,172,88,192,105);
    }
    SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(brush);DeleteObject(pen);
    if(rain) {
        pen=CreatePen(PS_SOLID,1,RGB(117,190,221));oldPen=(HPEN)SelectObject(dc,pen);
        for(i=0;i<4;i++){int y=105+(frameTick+i*3)%9;MoveToEx(dc,152+i*10,y,NULL);LineTo(dc,150+i*10,y+4);}
        SelectObject(dc,oldPen);DeleteObject(pen);
    }
}
static void HostLine(char *line) {
    char *p,*end;int weather=!strncmp(line,"MERO:WX:",8);
    if(!weather&&strncmp(line,"MERO:AMB:",9))return;
    p=line+(weather?8:9);
    if(!weather){wanted[0]=0;folderCount=0;memset(folders,0,sizeof(folders));}
    while(*p) {
        end=strchr(p,'|');if(end)*end=0;
        if(weather) {
            if(!strncmp(p,"temp_c=",7))Wide(temperature,16,p+7);
            else if(!strncmp(p,"desc=",5))Wide(description,96,p+5);
            else if(!strncmp(p,"location=",9))Wide(location,96,p+9);
            else if(!strncmp(p,"age=",4))weatherAge=strtoul(p+4,NULL,10);
            else if(!strncmp(p,"d1_min=",7))Wide(low,12,p+7);
            else if(!strncmp(p,"d1_max=",7))Wide(high,12,p+7);
        } else {
            if(!strncmp(p,"sha=",4)){strncpy(wanted,p+4,64);wanted[64]=0;}
            else if(!strncmp(p,"token=",6)){strncpy(token,p+6,31);token[31]=0;}
            else if(!strncmp(p,"name=",5))Wide(folder,64,p+5);
            else if(!strncmp(p,"title=",6))Wide(caption,96,p+6);
            else if(!strncmp(p,"status=",7))Wide(state,96,p+7);
            else if(!strncmp(p,"paused=",7)){strncpy(paused,p+7,7);paused[7]=0;}
            else if(!strncmp(p,"portrait=",9)){int next=atoi(p+9);if(next!=portrait){portrait=next;painted[0]=0;if(photoDC){SelectObject(photoDC,oldPhoto);DeleteObject(photoBitmap);DeleteDC(photoDC);photoDC=NULL;photoBits=NULL;}}}
            else if(!strncmp(p,"folder=",7))selectedFolder=atoi(p+7);
            else if(!strncmp(p,"generation=",11))generation=strtoul(p+11,NULL,10);
            else if(!strncmp(p,"clock=",6)){strncpy(clockText,p+6,11);clockText[11]=0;}
            else if(p[0]=='f'&&p[1]>='0'&&p[1]<='7'&&p[2]=='=') {
                int i=p[1]-'0';Wide(folders[i],64,p+3);if(folderCount<i+1)folderCount=i+1;
            }
        }
        if(!end)break;p=end+1;
    }
    if(weather){weatherTick=GetTickCount();haveWeather=1;}
    else {
        ambientTick=GetTickCount();haveAmbient=1;
        if(expectedFolder>=0) {
            if(selectedFolder==expectedFolder||(DWORD)(GetTickCount()-expectedTick)>15000)expectedFolder=-1;
            else wanted[0]=0;
        }
        /* Retain the last verified photo during the next transfer; no loading flash. */
        if(!wanted[0]&&!wcscmp(state,L"No images in this folder"))painted[0]=0;
    }
}
static void ReadPhoto(HDC dc) {
    int i;BITMAPINFO info;
    if(!MeroAmbientOpen()||!MeroBusLock())return;
    if(!wanted[0]||strcmp(wanted,mero_ambient->sha)||!strcmp(wanted,painted)){ReleaseMutex(mero_bus_mutex);return;}
    if(!photoDC) {
        photoDC=CreateCompatibleDC(dc);memset(&info,0,sizeof(info));info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=portrait?136:240;info.bmiHeader.biHeight=portrait?-240:-136;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        photoBitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void**)&photoBits,NULL,0);
        if(photoBitmap)oldPhoto=(HBITMAP)SelectObject(photoDC,photoBitmap);
    }
    if(photoBits) {
        for(i=0;i<240*136;i++) {
            unsigned short v=mero_ambient->pixels[i];
            unsigned int r=(v>>11)&31,g=(v>>5)&63,b=v&31;
            photoBits[portrait?((i%240)*136+135-i/240):i]=((r*255/31)<<16)|((g*255/63)<<8)|(b*255/31);
        }
        strcpy(painted,wanted);
    }
    ReleaseMutex(mero_bus_mutex);
}
static void Paint(HWND h) {
    PAINTSTRUCT ps;HDC screen=BeginPaint(h,&ps),dc=CreateCompatibleDC(screen);
    int width=portrait?272:480,height=portrait?480:272;DWORD *canvas;BITMAPINFO bi;
    memset(&bi,0,sizeof(bi));bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    HBITMAP bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,(void**)&canvas,NULL,0),old=(HBITMAP)SelectObject(dc,bitmap);
    WCHAR text[192];int i;SetBkMode(dc,TRANSPARENT);
    Fill(dc,0,0,width,height,RGB(12,20,25));ReadPhoto(dc);
    if(photoDC&&painted[0]) {
        /* Slow local drift keeps stills alive without another transfer. */
        int drift=frameTick%48;if(drift>24)drift=48-drift;drift/=4;
        StretchBlt(dc,-drift,-drift,width+12,height+8,photoDC,0,0,portrait?136:240,portrait?240:136,SRCCOPY);
    }
    if(menuVisible) {
        Fill(dc,0,0,width,36,RGB(10,20,24));
        Text(dc,smallFont,RGB(210,233,217),L"FULL",5,10,portrait?72:160,32,DT_CENTER|DT_SINGLELINE);
        Text(dc,smallFont,RGB(210,233,217),L"ROTATE",portrait?72:160,10,portrait?144:240,32,DT_CENTER|DT_SINGLELINE);
        Text(dc,smallFont,RGB(210,233,217),L"WX",portrait?144:240,10,portrait?208:392,32,DT_CENTER|DT_SINGLELINE);
        Text(dc,smallFont,RGB(210,233,217),L"SHELL",portrait?208:392,10,width,32,DT_CENTER|DT_SINGLELINE);
    }
    if(weatherVisible&&!folderMenu) {
        Fill(dc,12,50,205,174,RGB(12,27,31));
        if(haveWeather) {
            unsigned int age=weatherAge+(DWORD)(GetTickCount()-weatherTick)/1000;
            int stale=(DWORD)(GetTickCount()-weatherTick)>15000||age>1200;
            Text(dc,smallFont,RGB(151,189,182),location,22,58,195,76,DT_SINGLELINE|DT_END_ELLIPSIS);
            wsprintfW(text,L"%s C",temperature);Text(dc,largeFont,RGB(244,219,164),text,22,80,193,116,DT_SINGLELINE);
            WeatherIcon(dc);
            Text(dc,bodyFont,RGB(221,234,220),description,22,117,195,140,DT_SINGLELINE|DT_END_ELLIPSIS);
            wsprintfW(text,L"%s - %s C   /   %s %um",low,high,stale?L"CACHED":L"FETCHED",age/60);
            Text(dc,smallFont,stale?RGB(249,182,109):RGB(155,201,181),text,22,146,198,166,DT_SINGLELINE|DT_END_ELLIPSIS);
        } else Text(dc,bodyFont,RGB(182,203,197),L"Weather pending",22,86,195,136,DT_SINGLELINE);
    }
    if(menuVisible)for(i=0;i<4;i++) {
        int x=portrait?8+(i%2)*132:8+i*118,y=portrait?400+(i/2)*40:240,w=portrait?124:108;
        COLORREF colors[]={RGB(255,197,93),RGB(193,154,255),RGB(110,225,179),RGB(249,150,207)};COLORREF color=colors[i];
        Fill(dc,x,y,x+w,y+30,RGB(GetRValue(color)/7,GetGValue(color)/7,GetBValue(color)/7));
        Text(dc,smallFont,color,i==0?L"PREVIOUS":i==1?(!strcmp(paused,"1")?L"RESUME":L"PAUSE"):i==2?L"NEXT":L"FOLDERS",x,y+6,x+w,y+28,DT_CENTER|DT_SINGLELINE);
    }
    if(folderMenu) {
        Fill(dc,8,40,width-8,237,RGB(12,29,33));
        for(i=0;i<folderCount;i++) {
            Fill(dc,14,44+i*23,width-14,66+i*23,i==selectedFolder?RGB(92,62,22):RGB(35,27,46));
            Text(dc,bodyFont,i==selectedFolder?RGB(255,217,130):RGB(213,187,245),folders[i],25,45+i*23,width-25,67+i*23,DT_SINGLELINE|DT_END_ELLIPSIS);
        }
        if(!folderCount)Text(dc,bodyFont,RGB(220,200,160),L"Configure ~/.config/mero/ambient.json on PC",20,100,460,170,DT_WORDBREAK);
    }
    if(folderMenu)Text(dc,smallFont,RGB(230,190,130),queuedAction[0]?L"Sending request...":Fresh()?state:L"Offline / last photo cached",14,222,466,238,DT_SINGLELINE);
    if(portrait&&canvas){DWORD *bits;HDC rotated=CreateCompatibleDC(screen);bi.bmiHeader.biWidth=480;bi.bmiHeader.biHeight=-272;
        HBITMAP rb=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,(void**)&bits,NULL,0),ro=(HBITMAP)SelectObject(rotated,rb);
        if(bits){int x,y;for(y=0;y<480;y++)for(x=0;x<272;x++)bits[x*480+479-y]=canvas[y*272+x];BitBlt(screen,0,0,480,272,rotated,0,0,SRCCOPY);}
        SelectObject(rotated,ro);DeleteObject(rb);DeleteDC(rotated);
    }else BitBlt(screen,0,0,480,272,dc,0,0,SRCCOPY);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);EndPaint(h,&ps);
}
/* Keep the same hit area when hidden so a second tap restores the widget. */
static int WeatherTap(int x,int y){
    if(folderMenu||x<12||x>=205||y<50||y>=174)return 0;
    weatherVisible=!weatherVisible;return 1;
}
static LRESULT CALLBACK WindowProc(HWND h,UINT msg,WPARAM w,LPARAM l) {
    if(msg==WM_CREATE){MeroLiveInit(h,HostLine);SetTimer(h,1,250,NULL);return 0;}
    if(msg==WM_TIMER) {
        if(w==MERO_LIVE_TIMER)MeroLiveTick(h);
        if(w==1) {
            frameTick++;
            FlushQueue();
            if(!mero_standby&&GetForegroundWindow()==h&&MeroAmbientOpen()&&MeroBusLock()) {
                mero_ambient->demandTick=GetTickCount();ReleaseMutex(mero_bus_mutex);
            }
            if(!queuedAction[0]&&!mero_standby&&GetForegroundWindow()==h&&Fresh()&&token[0]) {
                int shown=painted[0]&&!strcmp(painted,wanted)&&generation;
                if(!lastPing||(DWORD)(GetTickCount()-lastPing)>20000||(shown&&shownGeneration!=generation))
                    if(MeroBusCommand(shown?"ambient-shown":"ambient-ping",token,shown?generation:0)) {
                        lastPing=GetTickCount();if(shown)shownGeneration=generation;
                    }
            }
            if(!mero_standby)InvalidateRect(h,NULL,FALSE);
        }
        return 0;
    }
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT){Paint(h);return 0;}
    if(msg==WM_LBUTTONDOWN&&!mero_standby) {
        int x=LOWORD(l),y=HIWORD(l),ok=0;if(portrait){int px=x;x=y;y=479-px;}
        if(WeatherTap(x,y)){InvalidateRect(h,NULL,FALSE);return 0;}
        if(!menuVisible){menuVisible=1;InvalidateRect(h,NULL,FALSE);return 0;}
        if(y<36){
            if(x>=(portrait?208:392)){MeroReturnToShell();DestroyWindow(h);return 0;}
            if(x>=(portrait?144:240))weatherVisible=!weatherVisible;
            else if(x>=(portrait?72:160))Queue("ambient-rotate",0);
            else menuVisible=folderMenu=0;
            InvalidateRect(h,NULL,FALSE);return 0;
        }
        if(folderMenu&&y>=44&&y<44+23*folderCount) {
            ok=Queue("ambient-folder",(y-44)/23);
            if(ok){expectedFolder=(y-44)/23;expectedTick=GetTickCount();folderMenu=menuVisible=0;wanted[0]=0;lstrcpyW(state,L"Loading selected folder...");}
        } else if(y>=(portrait?400:240)) {
            int button=portrait?(y-400)/40*2+x/136:x/118;
            if(button==3)folderMenu=!folderMenu;
            else ok=Queue(button==0?"ambient-prev":button==1?"ambient-pause":"ambient-next",0);
        } else {folderMenu=menuVisible=0;}
        lstrcpyW(notice,ok?L"Sent to PC":L"Waiting for link / previous request");InvalidateRect(h,NULL,FALSE);return 0;
    }
    if(msg==WM_CLOSE){MeroReturnToShell();DestroyWindow(h);return 0;}
    if(msg==WM_DESTROY) {
        if(mero_ambient&&MeroBusLock()){memset(mero_ambient,0,sizeof(*mero_ambient));ReleaseMutex(mero_bus_mutex);}
        if(photoBits)memset(photoBits,0,240*136*4);
        MeroAmbientClose();if(photoDC&&oldPhoto)SelectObject(photoDC,oldPhoto);
        if(photoBitmap)DeleteObject(photoBitmap);if(photoDC)DeleteDC(photoDC);
        MeroLiveClose(h);KillTimer(h,1);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR args,int show) {
    WNDCLASSW wc;MSG msg;HWND h=FindWindowW(L"MERO_AMBIENT",NULL);(void)previous;(void)args;(void)show;
    if(h){ShowWindow(h,SW_SHOW);SetForegroundWindow(h);return 0;}
    largeFont=Font(32,FW_BOLD);bodyFont=Font(18,FW_NORMAL);smallFont=Font(13,FW_NORMAL);
    memset(&wc,0,sizeof(wc));wc.lpfnWndProc=WindowProc;wc.hInstance=instance;wc.lpszClassName=L"MERO_AMBIENT";RegisterClassW(&wc);
    h=CreateWindowExW(WS_EX_TOPMOST,wc.lpszClassName,L"Ambient Home",WS_VISIBLE|WS_POPUP,0,0,480,272,NULL,NULL,instance,NULL);
    if(!h)return 1;
    while(GetMessageW(&msg,NULL,0,0)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    DeleteObject(largeFont);DeleteObject(bodyFont);DeleteObject(smallFont);return 0;
}
