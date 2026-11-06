/* Read-only screen capture helper, launched by the agent. No window or UI. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
int WINAPI WinMain(HINSTANCE a,HINSTANCE b,LPWSTR args,int show) {
    HDC screen=GetDC(NULL),dc;HBITMAP bitmap,old;BITMAPINFO info;void *bits=NULL;
    HANDLE file;DWORD done;int x,y,ok=1;unsigned short row[480*2];
    unsigned char header[12]={'M','E','R','O','3','2','R','1',224,1,16,1};
    (void)a;(void)b;(void)args;(void)show;
    if(!screen)return 1;
    memset(&info,0,sizeof(info));info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=480;info.bmiHeader.biHeight=-272;info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&bits,NULL,0);dc=CreateCompatibleDC(screen);
    if(!bitmap||!dc||!bits){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);ReleaseDC(NULL,screen);return 2;}
    old=(HBITMAP)SelectObject(dc,bitmap);ok=BitBlt(dc,0,0,480,272,screen,0,0,SRCCOPY);
    CreateDirectoryW(L"\\SDMMC\\MERO",NULL);
    file=CreateFileW(L"\\SDMMC\\MERO\\screen.rle",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)ok=0;
    if(ok)ok=WriteFile(file,header,sizeof(header),&done,NULL)&&done==sizeof(header);
    for(y=0;ok&&y<272;y++) {
        unsigned int *pixels=(unsigned int*)bits+y*480;int n=0;
        for(x=0;x<480;) {
            int end=x+1;unsigned int rgb=pixels[x];
            unsigned short color=(unsigned short)(((rgb>>19)&31)<<11|((rgb>>10)&63)<<5|((rgb>>3)&31));
            while(end<480&&pixels[end]==rgb)end++;
            row[n++]=(unsigned short)(end-x);row[n++]=color;x=end;
        }
        ok=WriteFile(file,row,n*2,&done,NULL)&&done==(DWORD)(n*2);
    }
    if(file!=INVALID_HANDLE_VALUE){if(ok)ok=FlushFileBuffers(file);CloseHandle(file);}
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(NULL,screen);return ok?0:3;
}
