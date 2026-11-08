/* Mero Terminal: persistent PC session over the signed companion bus. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../mero-live.h"
#include "../mero-shell-return.h"
#include "mero-terminal-font.h"
static HFONT small;
static int portrait=1,keyboard,physical,shift,symbols,ctrl,cols=32,rows=26,cx,cy,scrolled,active;
static int viewSent=-1,requested=-1;
static int modeRequest=-1;
static DWORD modeRequestTick;
static char token[17],screenText[2200],status[96]="Waiting for PC Bluetooth",input[256];
static unsigned int inputLen,screenLength;
static int screenUtf16;
static DWORD received;
static DWORD inputTick;
static int touchPressed,touchX,touchY;
static char notice[96];
static const char *letters[]={"1234567890","qwertyuiop","asdfghjkl;","zxcvbnm,./"};
static const char *punctuation[]={"!@#$%^&*()","[]{}<>\\|`~","-_=+:'\"?;:","0123456789"};
static int Decode(const char *text,char *out,unsigned int cap) {
    unsigned int bits=0,n=0,have=0;const char *alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    while(*text){const char *p;if(*text=='=')break;p=strchr(alphabet,*text++);if(!p)return 0;
        bits=(bits<<6)|(unsigned int)(p-alphabet);have+=6;
        if(have>=8){have-=8;if(n+1>=cap)return 0;out[n++]=(char)((bits>>have)&255);}}
    out[n]=0;return (int)n+1;
}
static int Enqueue(const char *bytes,unsigned int count) {
    if(!token[0]||(DWORD)(GetTickCount()-received)>15000){strcpy(notice,"Offline: input not sent");return 0;}
    if(!count||inputLen+count>sizeof(input)){strcpy(notice,"Input queue full: wait for PC");return 0;}
    if(!inputLen)inputTick=GetTickCount();
    memcpy(input+inputLen,bytes,count);inputLen+=count;notice[0]=0;return 1;
}
static void Character(WCHAR ch) {
    char bytes[4];int count;
    if(ch==8)ch=127;if(ch==10)ch=13;
    if(ctrl&&(ch==' '||ch=='@'||ch=='2'))ch=0;
    else if(ctrl&&ch>='a'&&ch<='z')ch=ch-'a'+1;
    else if(ctrl&&ch>='A'&&ch<='Z')ch=ch-'A'+1;
    ctrl=0;count=WideCharToMultiByte(CP_UTF8,0,&ch,1,bytes,sizeof(bytes),NULL,NULL);
    if(count>0)Enqueue(bytes,count);
}
static void HostLine(char *line) {
    char *p,*end;
    if(!strncmp(line,"MERO:AMB:",9)){
        p=strstr(line,"portrait=");if(p)portrait=atoi(p+9)!=0;return;
    }
    if(strncmp(line,"MERO:TERM:",10))return;
    screenUtf16=0;
    for(p=line+10;*p;p=end+1){
        end=strchr(p,'|');if(end)*end=0;
        if(!strncmp(p,"token=",6)&&strlen(p+6)==16){
            if(strcmp(token,p+6)){if(inputLen)strcpy(notice,"PC restarted: queued input cleared");inputLen=0;viewSent=-1;strcpy(token,p+6);}
        }else if(!strncmp(p,"cols=",5))cols=atoi(p+5);
        else if(!strncmp(p,"rows=",5))rows=atoi(p+5);
        else if(!strncmp(p,"cx=",3))cx=atoi(p+3);
        else if(!strncmp(p,"cy=",3))cy=atoi(p+3);
        else if(!strncmp(p,"scroll=",7))scrolled=atoi(p+7);
        else if(!strncmp(p,"active=",7))active=atoi(p+7);
        else if(!strncmp(p,"physical=",9)){int next=atoi(p+9)!=0;
            if(modeRequest==next)modeRequest=-1;
            if(next!=physical){physical=next;if(next)keyboard=0;viewSent=-1;}}
        else if(!strncmp(p,"input=",6)){if(p[6]&&strcmp(notice,p+6)&&!physical){modeRequest=-1;}strncpy(notice,p+6,sizeof(notice)-1);notice[sizeof(notice)-1]=0;}
        else if(!strncmp(p,"status=",7)){strncpy(status,p+7,sizeof(status)-1);status[sizeof(status)-1]=0;}
        else if(!strncmp(p,"encoding=",9))screenUtf16=!strcmp(p+9,"u16");
        else if(!strncmp(p,"text=",5)){int n=Decode(p+5,screenText,sizeof(screenText));screenLength=n?n-1:0;if(screenUtf16&&(screenLength&1))screenLength=0;}
        if(!end)break;
    }
    received=GetTickCount();
}
static void Flush(void) {
    unsigned int i,n;char track[100];int view=portrait|(keyboard?2:0);
    if(modeRequest>=0&&(DWORD)(GetTickCount()-modeRequestTick)>15000){modeRequest=-1;keyboard=0;}
    if(inputLen&&(DWORD)(GetTickCount()-inputTick)>15000){inputLen=0;strcpy(notice,"Old input cleared: check PC prompt");}
    if(!token[0]||(DWORD)(GetTickCount()-received)>15000)return;
    if(requested>=3){if(MeroBusCommand(requested==3?"terminal-keyboard":"terminal-touch",token,0))requested=-1;return;}
    if(view!=viewSent){if(MeroBusCommand("terminal-view",token,view))viewSent=view;return;}
    if(!active)return;
    if(requested>=0){if(MeroBusCommand("terminal-scroll",token,requested))requested=-1;return;}
    if(!inputLen)return;
    n=inputLen>32?32:inputLen;
    /* Do not split a UTF-8 sequence between packets. */
    while(n<inputLen&&n&&(input[n]&0xc0)==0x80)n--;
    strcpy(track,token);strcat(track,".");
    for(i=0;i<n;i++)sprintf(track+17+i*2,"%02x",(unsigned char)input[i]);
    if(n&&MeroBusCommand("terminal-input",track,0)){
        memmove(input,input+n,inputLen-n);inputLen-=n;
    }
}
static HFONT Font(int height,int width) {
    LOGFONTW lf;memset(&lf,0,sizeof(lf));lf.lfHeight=height;lf.lfWidth=width;lf.lfWeight=FW_NORMAL;
    lf.lfPitchAndFamily=FIXED_PITCH;lstrcpyW(lf.lfFaceName,L"Courier New");return CreateFontIndirectW(&lf);
}
static void Fill(HDC dc,int x,int y,int r,int b,COLORREF color){RECT rc={x,y,r,b};HBRUSH brush=CreateSolidBrush(color);FillRect(dc,&rc,brush);DeleteObject(brush);}
static void Label(HDC dc,const char *text,int x,int y,int r,int b,COLORREF color,UINT align){
    WCHAR wide[160];RECT rc={x,y,r,b};MultiByteToWideChar(CP_UTF8,0,text,-1,wide,160);
    SetTextColor(dc,color);DrawTextW(dc,wide,-1,&rc,align|DT_SINGLELINE);
}
static int KeyHeight(void){return portrait?32:24;}
static int KeyTop(void){return (portrait?480:272)-18-6*KeyHeight();}
static const char *ControlLabel(int row,int key){
    static const char *a[]={"SHIFT","SYM","CTRL","BKSP","ENTER"};
    static const char *b[]={"ESC","TAB","SPACE","UP","DOWN"};
    if(row==5&&symbols&&key>=3)return key==3?"LEFT":"RIGHT";
    return row==4?a[key]:b[key];
}
static void Touch(int x,int y,HWND h){
    int width=portrait?272:480,key;
    if(y<30){key=x*5/width;
        if(key==0){MeroReturnToShell();DestroyWindow(h);return;}
        if(key==1){if((modeRequest>=0?modeRequest:physical)){requested=4;modeRequest=0;modeRequestTick=GetTickCount();keyboard=1;}else keyboard=!keyboard;}
        if(key==2)requested=1;
        if(key==3){if(!active)viewSent=-1;else requested=scrolled?2:0;}
        if(key==4){int previous=modeRequest>=0?modeRequest:physical;modeRequest=!previous;modeRequestTick=GetTickCount();requested=previous?4:3;keyboard=0;}
        return;
    }
    if(!keyboard||y<KeyTop()||y>=KeyTop()+6*KeyHeight())return;
    int row=(y-KeyTop())/KeyHeight();
    if(row<4){char ch;key=x*10/width;ch=(symbols?punctuation:letters)[row][key];
        if(shift&&ch>='a'&&ch<='z')ch-=32;Character(ch);}
    else {key=x*5/width;
        if(row==4){if(key==0)shift=!shift;else if(key==1)symbols=!symbols;else if(key==2)ctrl=!ctrl;else Character(key==3?8:13);}
        else {if(key<3)Character(key==0?27:key==1?9:32);else Enqueue(symbols?(key==3?"\033[D":"\033[C"):(key==3?"\033[A":"\033[B"),3);}
    }
}
/* Resistive panels can repeat DOWN messages while the stylus is held. */
static int TouchDown(int x,int y){if(touchPressed)return 0;touchPressed=1;touchX=x;touchY=y;return 1;}
static void TouchUp(void){touchPressed=0;}
static void Paint(HWND h){
    PAINTSTRUCT ps;HDC screen=BeginPaint(h,&ps),dc=CreateCompatibleDC(screen);BITMAPINFO bi;DWORD *bits;
    int width=portrait?272:480,height=portrait?480:272,x,y,row,col=0;
    memset(&bi,0,sizeof(bi));bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    HBITMAP bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,(void**)&bits,NULL,0),old=(HBITMAP)SelectObject(dc,bitmap);
    Fill(dc,0,0,width,height,RGB(9,15,21));SetBkMode(dc,TRANSPARENT);SelectObject(dc,small);
    const char *labels[]={"SHELL",keyboard?"HIDE KB":"SHOW KB","SCROLL",active?"LIVE":"CONNECT",(modeRequest>=0?modeRequest:physical)?"PC OFF":"PC KEYS"};COLORREF accents[]={RGB(249,150,207),RGB(255,197,93),RGB(193,154,255),RGB(110,225,179),RGB(117,209,255)};
    for(x=0;x<5;x++){int left=x*width/5,right=(x+1)*width/5;Fill(dc,left,0,right-1,29,RGB(25,36,43));Label(dc,labels[x],left,8,right,28,accents[x],DT_CENTER);}
    {unsigned int pos=0;row=0;while(pos<screenLength&&row<rows&&row<26){
        unsigned int ch=(unsigned char)screenText[pos++];if(screenUtf16){if(pos>=screenLength)break;ch|=(unsigned int)(unsigned char)screenText[pos++]<<8;}if(ch=='\n'){row++;col=0;continue;}
        y=36+row*14;x=8+col*8;
        if(col<cols&&x+8<=width-4&&y+14<(keyboard?KeyTop():height-18)){
            if(bits&&ch>=32){const unsigned char *glyph=MeroTerminalGlyph(ch);int gx,gy;for(gy=0;gy<14;gy++)for(gx=0;gx<8;gx++)
                if(glyph[gy]&(1<<(7-gx)))bits[(y+gy)*width+x+gx]=0x00b7e7b9;}
        }col++;
    }}
    if(active&&!scrolled&&cx>=0&&cx<cols&&cy>=0&&cy<rows&&36+cy*14+14<(keyboard?KeyTop():height-18))
        Fill(dc,8+cx*8,48+cy*14,15+cx*8,50+cy*14,RGB(255,197,93));
    SelectObject(dc,small);
    if(keyboard)for(row=0;row<6;row++){
        int count=row<4?10:5;
        for(col=0;col<count;col++){
            char label[2]={0,0};const char *text;
            if(row<4){label[0]=(symbols?punctuation:letters)[row][col];if(shift&&label[0]>='a'&&label[0]<='z')label[0]-=32;text=label;}
            else text=ControlLabel(row,col);
            x=col*width/count;y=KeyTop()+row*KeyHeight();
            int down=touchPressed&&touchX>=x&&touchX<(col+1)*width/count&&touchY>=y&&touchY<y+KeyHeight();
            Fill(dc,x+1,y+1,(col+1)*width/count-1,y+KeyHeight()-1,down?RGB(79,91,66):RGB(31,42,52));
            Label(dc,text,x,y+(KeyHeight()-13)/2,(col+1)*width/count,y+KeyHeight(),
                  row==4&&((col==0&&shift)||(col==1&&symbols)||(col==2&&ctrl))?RGB(255,197,93):RGB(207,218,230),DT_CENTER);
        }
    }
    {char footer[160];int fresh=received&&(DWORD)(GetTickCount()-received)<15000,sending=0;
     if(MeroBusLock()){sending=mero_bus->event[0]&&strcmp(mero_bus->eventId,mero_bus->ack)&&strstr(mero_bus->event,"\naction=terminal-input\n");ReleaseMutex(mero_bus_mutex);}
     snprintf(footer,sizeof(footer),"%s%s | %u queued%s",notice[0]?notice:fresh?status:"OFFLINE - last screen",scrolled?" / UP":"",inputLen,sending?" / SENDING":"");
     Label(dc,footer,6,height-16,width-4,height,RGB(117,209,255),DT_LEFT|DT_END_ELLIPSIS);}
    if(portrait&&bits){DWORD *rotated;HDC rot=CreateCompatibleDC(screen);bi.bmiHeader.biWidth=480;bi.bmiHeader.biHeight=-272;
        HBITMAP rb=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,(void**)&rotated,NULL,0),ro=(HBITMAP)SelectObject(rot,rb);
        if(rotated){for(y=0;y<480;y++)for(x=0;x<272;x++)rotated[x*480+479-y]=bits[y*272+x];BitBlt(screen,0,0,480,272,rot,0,0,SRCCOPY);}
        SelectObject(rot,ro);DeleteObject(rb);DeleteDC(rot);
    }else BitBlt(screen,0,0,480,272,dc,0,0,SRCCOPY);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);EndPaint(h,&ps);
}
static LRESULT CALLBACK WndProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    if(msg==WM_CREATE){MeroLiveInit(h,HostLine);SetTimer(h,MERO_LIVE_TIMER,50,NULL);SetTimer(h,1,100,NULL);return 0;}
    if(msg==WM_TIMER){if(w==MERO_LIVE_TIMER)MeroLiveTick(h);if(w==1&&!mero_standby&&GetForegroundWindow()==h){Flush();InvalidateRect(h,NULL,FALSE);}return 0;}
    if(msg==WM_PAINT){Paint(h);return 0;}
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_LBUTTONDOWN&&!mero_standby){int x=LOWORD(l),y=HIWORD(l);if(portrait){int old=x;x=y;y=479-old;}if(TouchDown(x,y)){SetCapture(h);Touch(x,y,h);}InvalidateRect(h,NULL,FALSE);return 0;}
    if(msg==WM_LBUTTONUP||msg==WM_KILLFOCUS){TouchUp();if(GetCapture()==h)ReleaseCapture();InvalidateRect(h,NULL,FALSE);if(msg==WM_LBUTTONUP)return 0;}
    if(msg==WM_CHAR&&!mero_standby){Character((WCHAR)w);return 0;}
    if(msg==WM_KEYDOWN&&!mero_standby){const char *sequence=NULL;
        switch(w){case VK_UP:sequence="\033[A";break;case VK_DOWN:sequence="\033[B";break;
        case VK_LEFT:sequence="\033[D";break;case VK_RIGHT:sequence="\033[C";break;
        case VK_HOME:sequence="\033[H";break;case VK_END:sequence="\033[F";break;
        case VK_PRIOR:sequence="\033[5~";break;case VK_NEXT:sequence="\033[6~";break;case VK_DELETE:sequence="\033[3~";break;}
        if(sequence){Enqueue(sequence,strlen(sequence));return 0;}
    }
    if(msg==WM_CLOSE){MeroReturnToShell();DestroyWindow(h);return 0;}
    if(msg==WM_DESTROY){memset(input,0,sizeof(input));memset(screenText,0,sizeof(screenText));MeroLiveClose(h);KillTimer(h,1);PostQuitMessage(0);return 0;}
    return DefWindowProcW(h,msg,w,l);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR args,int show){
    WNDCLASSW wc;MSG msg;HWND h=FindWindowW(L"MERO_TERMINAL",NULL);(void)previous;(void)args;(void)show;
    if(h){ShowWindow(h,SW_SHOW);SetForegroundWindow(h);return 0;}
    small=Font(12,0);memset(&wc,0,sizeof(wc));wc.lpfnWndProc=WndProc;wc.hInstance=instance;wc.lpszClassName=L"MERO_TERMINAL";RegisterClassW(&wc);
    h=CreateWindowExW(WS_EX_TOPMOST,wc.lpszClassName,L"Mero Terminal",WS_POPUP|WS_VISIBLE,0,0,480,272,NULL,NULL,instance,NULL);if(!h)return 1;
    SetForegroundWindow(h);
    while(GetMessageW(&msg,NULL,0,0)){TranslateMessage(&msg);DispatchMessageW(&msg);}DeleteObject(small);return 0;
}
