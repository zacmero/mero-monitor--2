/* OEM Gui.dll GetBackLight 0x1001a394 / SetBackLight 0x1001a440.
 * BKL1: GET=0x22000c (DWORD out), SET=0x220008 (DWORD in).
 * Vendor slider raw=14+5*level, levels 0..10. Its driver reports zero
 * bytes returned even on success; the OEM checks BOOL, not byte count.
 * These codes are for BKL1 only, never for the USB controller.
 */
static int MeroBrightnessValid(DWORD raw){return raw>=14&&raw<=64&&(raw-14)%5==0;}
static int MeroBrightness(DWORD *raw,int write){
    DWORD n=0;int ok;HANDLE h;
    if(write&&!MeroBrightnessValid(*raw))return 0;
    h=CreateFileW(L"BKL1:",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,0,NULL);
    if(h==INVALID_HANDLE_VALUE)return 0;
    if(write)ok=DeviceIoControl(h,0x220008,raw,4,NULL,0,NULL,NULL);
    else{*raw=0xffffffff;ok=DeviceIoControl(h,0x22000c,NULL,0,raw,4,&n,NULL);}
    CloseHandle(h);return ok&&MeroBrightnessValid(*raw);
}
static void MeroCinemaBacklight(int dim){
    static int initialized,armed;static DWORD original;
    DWORD data[2]={0,0},n;HANDLE file;
    const WCHAR *path=L"\\SDMMC\\MERO\\cinema-backlight.state";
    if(!initialized){
        file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
        if(file!=INVALID_HANDLE_VALUE){
            if(ReadFile(file,data,sizeof(data),&n,NULL)&&n==sizeof(data)){
                if(data[0]==0x324c4b42&&MeroBrightnessValid(data[1])){original=data[1];armed=1;}
                else if(data[0]==0x314c4b42&&data[1]<=4){
                    typedef DWORD (*SetPower)(LPCWSTR,DWORD,DWORD);
                    SetPower set=(SetPower)GetProcAddress(GetModuleHandleW(L"coredll.dll"),L"SetDevicePower");
                    if(set)set(L"BKL1:",1,data[1]);
                }
            }CloseHandle(file);
        }initialized=1;
    }
    if(dim==armed)return;
    if(dim){
        if(!MeroBrightness(&original,0))return;
        data[0]=0x324c4b42;data[1]=original;
        file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);if(file==INVALID_HANDLE_VALUE)return;
        int saved=WriteFile(file,data,sizeof(data),&n,NULL)&&n==sizeof(data)&&FlushFileBuffers(file);CloseHandle(file);
        if(!saved)return;
        armed=1;DWORD minimum=14;MeroBrightness(&minimum,1);
    }else if(MeroBrightness(&original,1)){
        armed=0;data[0]=data[1]=0;
        file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
        if(file!=INVALID_HANDLE_VALUE){WriteFile(file,data,sizeof(data),&n,NULL);FlushFileBuffers(file);CloseHandle(file);}
    }
}
