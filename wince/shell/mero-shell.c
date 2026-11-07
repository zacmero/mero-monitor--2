/*
 * mero-shell.c - Custom native Windows CE launcher & OS shell
 * Project: mero-monitor-#2
 * Target: Foston FS-460BT (PE32 ARMv4 Windows CE 5.0)
 */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <tlhelp32.h>
#include <mmsystem.h>
#include <tchar.h>
#include <stdio.h>
#include <string.h>
#include "../mero-live.h"
#include "../mero-agent-start.h"
#include "../mero-usb.h"

#define SHELL_VERSION       L"0.3.6"
#define TIMER_ID_TICK       1

#define IOCTL_HAL_REBOOT    0x0001003C
#define POWER_STATE_RESET   0x00800000

/* Windows CE Coredll exports */
extern BOOL WINAPI KernelIoControl(DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);
extern DWORD WINAPI SetSystemPowerState(LPCWSTR pwsState, DWORD StateFlags, DWORD Options);
extern BOOL WINAPI SetCleanRebootFlag(void);

#define CFG_DIR             L"\\SDMMC\\MERO"
#define CFG_FILE            L"\\SDMMC\\MERO\\shell.cfg"
#define DUMP_FILE           L"\\SDMMC\\MERO\\system_dump.txt"

#define PATH_GALLERY        L"\\SDMMC\\MERO\\mero-gallery.exe"
#define PATH_MEDIA_CTRL     L"\\SDMMC\\MERO\\mero-media-ctrl.exe"
#define PATH_TUNER          L"\\SDMMC\\MERO\\mero-tuner.exe"
#define PATH_BT             L"\\SDMMC\\MERO\\mero-bt.exe"
#define PATH_CAM            L"\\SDMMC\\MERO\\mero-cam.exe"
#define PATH_VIS            L"\\SDMMC\\MERO\\mero-vis.exe"
#define PATH_TERMINAL       L"\\SDMMC\\MERO\\mero-terminal.exe"
#define PATH_PROBE          L"\\SDMMC\\MERO\\mero-probe.exe"
#define PATH_CMD            L"\\SDMMC\\MERO\\mero-cmd.exe"
#define PATH_GPS            L"\\SDMMC\\MERO\\mero-gps.exe"
#define PATH_IGO8           L"\\SDMMC\\IGO8\\iGO8.exe"
#define PATH_CONTROL        L"\\Windows\\control.exe"
#define PATH_EXPLORER       L"\\Windows\\explorer.exe"

typedef enum {
    BOOT_FLASH = 0,    /* \ResidentFlash\MERO\mero-shell.exe */
    BOOT_SDMMC = 1,    /* \SDMMC\MERO\mero-shell.exe */
    BOOT_DESKTOP = 2,  /* explorer.exe */
    BOOT_VENDOR = 3,   /* launch.exe */
    BOOT_COUNT = 4
} BootTarget;

static BootTarget     g_bootTarget = BOOT_FLASH;
static const WCHAR   *g_bootTargetNames[] = {
    L"Flash: \\ResidentFlash",
    L"SDMMC: \\SDMMC\\MERO",
    L"WinCE Desktop",
    L"Vendor (launch.exe)"
};

typedef enum {
    AUTOLAUNCH_NONE     = 0,
    AUTOLAUNCH_GALLERY  = 1,
    AUTOLAUNCH_MEDIA    = 2,
    AUTOLAUNCH_TUNER    = 3,
    AUTOLAUNCH_CAM      = 4,
    AUTOLAUNCH_CMD      = 5,
    AUTOLAUNCH_PROBE    = 6,
    AUTOLAUNCH_WEATHER  = 7,
    AUTOLAUNCH_MUSIC    = 8,
    AUTOLAUNCH_FM       = 9,
    AUTOLAUNCH_GPS      = 10,
    AUTOLAUNCH_BT       = 11,
    AUTOLAUNCH_VIS      = 12,
    AUTOLAUNCH_TERMINAL = 13,
    AUTOLAUNCH_NAV      = 14,
    AUTOLAUNCH_AMBIENT  = 15,
    AUTOLAUNCH_BACKLIGHT = 16,
    AUTOLAUNCH_COUNT    = 17
} AutoLaunchPref;

typedef struct { const WCHAR *name, *path, *args, *description; } CompanionApp;
static const CompanionApp g_apps[AUTOLAUNCH_COUNT] = {
    {L"Mero Shell",NULL,NULL,L"No automatic app"},
    {L"Local Gallery",PATH_GALLERY,NULL,L"Existing on-device photo folders"},
    {L"Sensor Terminal",PATH_MEDIA_CTRL,NULL,L"Clock, GPS, FM and weather"},
    {L"Display Tuner",PATH_TUNER,NULL,L"Screen, color and backlight"},
    {L"Camera Monitor",PATH_CAM,NULL,L"Existing camera bridge"},
    {L"Mero CMD",PATH_CMD,NULL,L"Files and process diagnostics"},
    {L"System Probe",PATH_PROBE,NULL,L"Hardware diagnostics"},
    {L"Weather Window",PATH_MEDIA_CTRL,L"--weather",L"Live weather and forecast"},
    {L"YouTube Music",L"\\SDMMC\\MERO\\mero-music.exe",NULL,L"Track, progress and PC controls"},
    {L"Frequency Matrix",PATH_MEDIA_CTRL,L"--fm",L"Ambient audio and live waveform"},
    {L"GPS Radar",PATH_MEDIA_CTRL,L"--gps",L"Native GPS; needs sky visibility"},
    {L"Bluetooth Files",PATH_BT,NULL,L"Files and agent connection"},
    {L"Audio Visualizer",PATH_VIS,NULL,L"Optional existing application"},
    {L"Terminal",PATH_TERMINAL,NULL,L"Optional existing application"},
    {L"iGO Navigation",PATH_IGO8,NULL,L"Existing navigation application"},
    {L"Ambient Home",L"\\SDMMC\\MERO\\mero-ambient.exe",NULL,L"Stream PC folders with live weather"},
    {L"Backlight",L"\\SDMMC\\MERO\\mero-backlight.exe",NULL,L"Compare minimum and maximum brightness"}
};
static const int g_appOrder[] = {15,16,7,8,1,9,2,10,11,3,4,5,6,12,13,14};
static int g_appPage;
static BOOL g_selectDefault, g_returningHome;
static DWORD g_bootStarted,g_agentAttempt,g_usbAttempt;
static BOOL g_usbStartupReady;
static BOOL g_sdPrefsLoaded,g_allowBootReload=TRUE;
static int g_launchAttempts;

typedef enum {
    USB_MODE_MASS_STORAGE = 0,
    USB_MODE_ACTIVESYNC   = 1,
    USB_MODE_COUNT        = 2
} UsbMode;

static UsbMode g_usbMode = USB_MODE_MASS_STORAGE;
static const WCHAR *g_usbModeNames[] = {
    L"MASS STORAGE (U-Disk)",
    L"ACTIVESYNC (PC Link)"
};

typedef struct {
    RECT rc;
    WCHAR title[48];
    WCHAR sub[64];
    COLORREF color;
} ShellButton;

static HINSTANCE      g_hInstance = NULL;
static HWND           g_hWnd = NULL;
static int            g_screenW = 480;
static int            g_screenH = 272;

typedef enum {
    PAGE_MAIN        = 0,  /* Mero OS Core Hub */
    PAGE_COMPANIONS  = 1,  /* Custom Companion Apps Suite */
    PAGE_SYSTEM      = 2,  /* System & Hardware Tools */
    PAGE_ADVANCED    = 3,  /* Low-level & Diagnostic Tools */
    PAGE_BLUETOOTH   = 4,
    PAGE_COUNT       = 5
} ShellPage;

static ShellPage      g_currentPage = PAGE_MAIN;
static AutoLaunchPref g_pref = AUTOLAUNCH_NONE;
static int            g_countdownSeconds = 0;
static BOOL           g_countdownActive = FALSE;
static WCHAR          g_statusMsg[160] = L"READY // TAP A MODULE TO LAUNCH";

static DWORD          g_memAvailMB = 0;
static DWORD          g_memTotalMB = 0;
static int            g_batteryPercent = -1;
static BOOL           g_isAC = FALSE;
static int            g_volumeLevel = 3; /* 0=Mute, 1=25%, 2=50%, 3=75%, 4=100% */

static ShellButton    g_buttons[6];

/* Audio volume adjustment via standard waveOut */
static void SetMasterVolume(int level)
{
    DWORD vol;
    switch (level) {
    case 0: vol = 0x00000000; break;
    case 1: vol = 0x40004000; break;
    case 2: vol = 0x80008000; break;
    case 3: vol = 0xC000C000; break;
    case 4: default: vol = 0xFFFFFFFF; break;
    }
    waveOutSetVolume(0, vol);
}

/* Read shell configuration */
static void LoadConfig(void)
{
    HANDLE f; DWORD n; char buffer[256]; BOOL found=FALSE;
    g_pref=AUTOLAUNCH_NONE; g_countdownSeconds=0; g_countdownActive=FALSE;
    f=CreateFileW(CFG_FILE,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    g_sdPrefsLoaded=(f!=INVALID_HANDLE_VALUE);
    if(f==INVALID_HANDLE_VALUE)f=CreateFileW(L"\\ResidentFlash\\MERO\\shell.cfg",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(f!=INVALID_HANDLE_VALUE){
        int val; found=TRUE; memset(buffer,0,sizeof(buffer));
        if(ReadFile(f,buffer,sizeof(buffer)-1,&n,NULL)&&n&&sscanf(buffer,"autolaunch=%d",&val)==1&&val>=0&&val<AUTOLAUNCH_COUNT)g_pref=(AutoLaunchPref)val;
        CloseHandle(f);
    }
    /* A saved zero is authoritative. Registry is a read-only legacy fallback. */
    if(!found){HKEY key;if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"Software\\Mero",0,KEY_READ,&key)==ERROR_SUCCESS){
        DWORD val=0,size=sizeof(val);
        if(RegQueryValueExW(key,L"AutoLaunch",NULL,NULL,(LPBYTE)&val,&size)==ERROR_SUCCESS&&val<AUTOLAUNCH_COUNT)g_pref=(AutoLaunchPref)val;
        RegCloseKey(key);
    }}
    if(g_pref!=AUTOLAUNCH_NONE&&!g_returningHome){g_countdownSeconds=3;g_countdownActive=TRUE;
        wsprintfW(g_statusMsg,L"START %s IN 3s // TAP TO CANCEL",g_apps[g_pref].name);}
}
static BOOL SaveConfig(void)
{
    HANDLE f;DWORD done;char buffer[128];WCHAR backup[160];BOOL ok,existed;
    int len=sprintf(buffer,"autolaunch=%d\r\ntimeout=3\r\n",(int)g_pref);
    CreateDirectoryW(CFG_DIR,NULL);
    f=CreateFileW(L"\\SDMMC\\MERO\\shell.cfg.next",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(f==INVALID_HANDLE_VALUE)return FALSE;
    ok=WriteFile(f,buffer,len,&done,NULL)&&done==(DWORD)len&&FlushFileBuffers(f);
    CloseHandle(f);if(!ok)return FALSE;
    existed=GetFileAttributesW(CFG_FILE)!=INVALID_FILE_ATTRIBUTES;
    wsprintfW(backup,L"\\SDMMC\\MERO\\shell-pref-%lu.cfg",GetTickCount());
    if(existed&&!MoveFileW(CFG_FILE,backup))return FALSE;
    if(!MoveFileW(L"\\SDMMC\\MERO\\shell.cfg.next",CFG_FILE)){
        if(existed)MoveFileW(backup,CFG_FILE);return FALSE;
    }
    return TRUE;
}

/* Query system metrics */
static void UpdateSystemStatus(void)
{
    MEMORYSTATUS ms;
    SYSTEM_POWER_STATUS_EX2 sps;

    memset(&ms, 0, sizeof(ms));
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    g_memTotalMB = (DWORD)(ms.dwTotalPhys / (1024 * 1024));
    g_memAvailMB = (DWORD)(ms.dwAvailPhys / (1024 * 1024));

    memset(&sps, 0, sizeof(sps));
    if (GetSystemPowerStatusEx2(&sps, sizeof(sps), FALSE)) {
        g_isAC = (sps.ACLineStatus == 1);
        if (sps.BatteryLifePercent <= 100) {
            g_batteryPercent = sps.BatteryLifePercent;
        } else {
            g_batteryPercent = -1;
        }
    }
}

/* Hide and disable vendor UI shell window */
static void HideVendorUI(void)
{
    HWND hWndVendor = FindWindowW(NULL, L"Launch");
    if (!hWndVendor) hWndVendor = FindWindowW(L"Launch", NULL);
    if (!hWndVendor) hWndVendor = FindWindowW(NULL, L"Main");
    if (!hWndVendor) hWndVendor = FindWindowW(L"Main", NULL);
    if (hWndVendor) {
        ShowWindow(hWndVendor, SW_HIDE);
        EnableWindow(hWndVendor, FALSE);
    }
}

/* Suppress and terminate Apical watchdog and vendor UI processes */
static int KillVendorWatchdog(void)
{
    HANDLE hSnap;
    PROCESSENTRY32 pe;
    int count = 0;

    HideVendorUI();

    HWND hWndDog = FindWindowW(NULL, L"ANWWATCHDOG");
    if (hWndDog) {
        ShowWindow(hWndDog, SW_HIDE);
        EnableWindow(hWndDog, FALSE);
    }
    HWND hWndComm = FindWindowW(NULL, L"Cannot start communications with the desktop computer.");
    if (hWndComm) PostMessageW(hWndComm, WM_CLOSE, 0, 0);
    HWND hWndComm2 = FindWindowW(NULL, L"Communications Error");
    if (hWndComm2) PostMessageW(hWndComm2, WM_CLOSE, 0, 0);

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        pe.dwSize = sizeof(pe);
        if (Process32First(hSnap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"ANWDOG.exe") == 0 ||
                    _wcsicmp(pe.szExeFile, L"Launch.exe") == 0 ||
                    _wcsicmp(pe.szExeFile, L"Main.exe") == 0 ||
                    _wcsicmp(pe.szExeFile, L"YFMenu.exe") == 0) {
                    
                    HANDLE hProc = OpenProcess(0x0001 /* PROCESS_TERMINATE */, FALSE, pe.th32ProcessID);
                    if (hProc) {
                        TerminateProcess(hProc, 0);
                        CloseHandle(hProc);
                        count++;
                    }
                }
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
    return count;
}

/* Create desktop shortcut (.lnk) files on Windows CE desktop */
/* Create desktop shortcut (.lnk) files on Windows CE desktop */
/* Copy all latest binaries to ResidentFlash to keep offline storage current */
static void SyncToResidentFlash(void)
{
    CreateDirectoryW(L"\\ResidentFlash", NULL);
    CreateDirectoryW(L"\\ResidentFlash\\MERO", NULL);

    SetFileAttributesW(L"\\ResidentFlash\\MERO\\mero-shell.exe", FILE_ATTRIBUTE_NORMAL);
    SetFileAttributesW(L"\\ResidentFlash\\MERO\\mero-cmd.exe", FILE_ATTRIBUTE_NORMAL);
    SetFileAttributesW(L"\\ResidentFlash\\MERO\\mero-gallery.exe", FILE_ATTRIBUTE_NORMAL);
    SetFileAttributesW(L"\\ResidentFlash\\MERO\\mero-tuner.exe", FILE_ATTRIBUTE_NORMAL);
    SetFileAttributesW(L"\\ResidentFlash\\MERO\\mero-media-ctrl.exe", FILE_ATTRIBUTE_NORMAL);
    SetFileAttributesW(L"\\ResidentFlash\\MERO\\mero-flash.exe", FILE_ATTRIBUTE_NORMAL);

    CopyFileW(L"\\SDMMC\\MERO\\mero-shell.exe", L"\\ResidentFlash\\MERO\\mero-shell.exe", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\mero-cmd.exe", L"\\ResidentFlash\\MERO\\mero-cmd.exe", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\mero-gallery.exe", L"\\ResidentFlash\\MERO\\mero-gallery.exe", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\mero-tuner.exe", L"\\ResidentFlash\\MERO\\mero-tuner.exe", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\mero-media-ctrl.exe", L"\\ResidentFlash\\MERO\\mero-media-ctrl.exe", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\mero-flash.exe", L"\\ResidentFlash\\MERO\\mero-flash.exe", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\display.cfg", L"\\ResidentFlash\\MERO\\display.cfg", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\fm.cfg", L"\\ResidentFlash\\MERO\\fm.cfg", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\shell.cfg", L"\\ResidentFlash\\MERO\\shell.cfg", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\mero-shell.ico", L"\\ResidentFlash\\MERO\\mero-shell.ico", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\mero-cmd.ico", L"\\ResidentFlash\\MERO\\mero-cmd.ico", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\mero-gallery.ico", L"\\ResidentFlash\\MERO\\mero-gallery.ico", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\mero-flash.ico", L"\\ResidentFlash\\MERO\\mero-flash.ico", FALSE);
}

/* Create desktop shortcut (.lnk) files on Windows CE desktop */
static void CreateDesktopShortcuts(void)
{
    HANDLE hFile;
    DWORD written;
    const char *lnkShell   = "26#\\SDMMC\\MERO\\mero-shell.exe";
    const char *lnkCmd     = "24#\\SDMMC\\MERO\\mero-cmd.exe";
    const char *lnkGallery = "28#\\SDMMC\\MERO\\mero-gallery.exe";
    const char *lnkTuner   = "26#\\SDMMC\\MERO\\mero-tuner.exe";
    const char *lnkMedia   = "31#\\SDMMC\\MERO\\mero-media-ctrl.exe";

    /* Preserve the existing internal recovery copy. */

    CreateDirectoryW(L"\\Windows\\Desktop", NULL);

    /* Delete old/stale shortcuts that might be on desktop */
    DeleteFileW(L"\\Windows\\Desktop\\YT Music Deck.lnk");
    DeleteFileW(L"\\Windows\\Desktop\\YouTube Music Deck.lnk");

    hFile = CreateFileW(L"\\Windows\\Desktop\\Mero Shell.lnk", GENERIC_WRITE, FILE_SHARE_READ,
                        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, lnkShell, (DWORD)strlen(lnkShell), &written, NULL);
        CloseHandle(hFile);
    }

    hFile = CreateFileW(L"\\Windows\\Desktop\\Mero Cmd.lnk", GENERIC_WRITE, FILE_SHARE_READ,
                        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, lnkCmd, (DWORD)strlen(lnkCmd), &written, NULL);
        CloseHandle(hFile);
    }

    hFile = CreateFileW(L"\\Windows\\Desktop\\mero-gallery.lnk", GENERIC_WRITE, FILE_SHARE_READ,
                        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, lnkGallery, (DWORD)strlen(lnkGallery), &written, NULL);
        CloseHandle(hFile);
    }

    hFile = CreateFileW(L"\\Windows\\Desktop\\mero-tuner.lnk", GENERIC_WRITE, FILE_SHARE_READ,
                        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, lnkTuner, (DWORD)strlen(lnkTuner), &written, NULL);
        CloseHandle(hFile);
    }

    hFile = CreateFileW(L"\\Windows\\Desktop\\mero-media-ctrl.lnk", GENERIC_WRITE, FILE_SHARE_READ,
                        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, lnkMedia, (DWORD)strlen(lnkMedia), &written, NULL);
        CloseHandle(hFile);
    }
}

/* Query active HKLM\init\Launch50 boot target */
static void QueryCurrentBootTarget(void)
{
    HKEY hKey;
    WCHAR val[256];
    DWORD valLen = sizeof(val);
    DWORD type = 0;

    g_bootTarget = BOOT_VENDOR;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"init", 0, 0, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"Launch50", NULL, &type, (LPBYTE)val, &valLen) == ERROR_SUCCESS && type == REG_SZ) {
            if (wcsstr(val, L"ResidentFlash")) {
                g_bootTarget = BOOT_FLASH;
            } else if (wcsstr(val, L"SDMMC")) {
                g_bootTarget = BOOT_SDMMC;
            } else if (wcsstr(val, L"explorer")) {
                g_bootTarget = BOOT_DESKTOP;
            } else {
                g_bootTarget = BOOT_VENDOR;
            }
        }
        RegCloseKey(hKey);
    }
}

/* Query active USB Function client driver */
static void QueryCurrentUsbMode(void)
{
    WCHAR client[128];
    if(MeroUsbQuery(client)) {
        g_usbMode=!_wcsicmp(client,L"Serial_Class")?USB_MODE_ACTIVESYNC:USB_MODE_MASS_STORAGE;
        return;
    }
    HKEY hKey;
    g_usbMode = USB_MODE_MASS_STORAGE;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Drivers\\USB\\FunctionDrivers", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        WCHAR client[64];
        DWORD size = sizeof(client);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, L"DefaultClientDriver", NULL, &type, (LPBYTE)client, &size) == ERROR_SUCCESS) {
            if (wcsstr(client, L"Serial") || wcsstr(client, L"serial") ||
                wcsstr(client, L"RNDIS") || wcsstr(client, L"rndis")) {
                g_usbMode = USB_MODE_ACTIVESYNC;
            } else {
                g_usbMode = USB_MODE_MASS_STORAGE;
            }
        }
        RegCloseKey(hKey);
    }
}

/* Toggle USB Function mode between Mass Storage and ActiveSync */
static void ToggleUsbMode(void)
{
    UsbMode wanted=(UsbMode)((g_usbMode+1)%USB_MODE_COUNT);
    const WCHAR *driver=wanted==USB_MODE_ACTIVESYNC?L"Serial_Class":L"Mass_Storage_Class";
    if(MeroUsbSet(driver)) {
        g_usbMode=wanted;g_usbStartupReady=TRUE;
        wsprintfW(g_statusMsg,L"USB preference + active driver: %s",g_usbModeNames[g_usbMode]);
    } else wsprintfW(g_statusMsg,L"USB change failed (%lu); previous preference preserved",GetLastError());
    InvalidateRect(g_hWnd,NULL,FALSE);
}

/* Apply new HKLM\init\Launch50 boot target and persist to flash */
static BOOL ApplyBootTarget(BootTarget target)
{
    HKEY hKey;
    const WCHAR *launchPath = NULL;
    BOOL ok = FALSE;

    switch (target) {
    case BOOT_FLASH:
        CreateDirectoryW(L"\\ResidentFlash\\MERO", NULL);
        if (!CopyFileW(L"\\SDMMC\\MERO\\mero-shell.exe", L"\\ResidentFlash\\MERO\\mero-shell.exe", FALSE)) return FALSE;
        CopyFileW(L"\\SDMMC\\MERO\\mero-cmd.exe", L"\\ResidentFlash\\MERO\\mero-cmd.exe", FALSE);
        CopyFileW(L"\\SDMMC\\MERO\\mero-flash.exe", L"\\ResidentFlash\\MERO\\mero-flash.exe", FALSE);
        CopyFileW(L"\\SDMMC\\MERO\\mero-shell.ico", L"\\ResidentFlash\\MERO\\mero-shell.ico", FALSE);
        CopyFileW(L"\\SDMMC\\MERO\\mero-cmd.ico", L"\\ResidentFlash\\MERO\\mero-cmd.ico", FALSE);
        CopyFileW(L"\\SDMMC\\MERO\\mero-flash.ico", L"\\ResidentFlash\\MERO\\mero-flash.ico", FALSE);
        launchPath = L"\\ResidentFlash\\MERO\\mero-shell.exe";
        break;
    case BOOT_SDMMC:
        launchPath = L"\\SDMMC\\MERO\\mero-shell.exe";
        break;
    case BOOT_DESKTOP:
        launchPath = L"explorer.exe";
        break;
    case BOOT_VENDOR:
    default:
        launchPath = L"launch.exe";
        break;
    }

    if ((target == BOOT_FLASH || target == BOOT_SDMMC) && GetFileAttributesW(launchPath) == 0xFFFFFFFF) return FALSE;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"init", 0, 0, &hKey) == ERROR_SUCCESS) {
        DWORD len = (lstrlenW(launchPath) + 1) * sizeof(WCHAR);
        if (RegSetValueExW(hKey, L"Launch50", 0, REG_SZ, (const BYTE*)launchPath, len) == ERROR_SUCCESS) {
            RegFlushKey(HKEY_LOCAL_MACHINE);
            ok = TRUE;
        }
        RegCloseKey(hKey);
    }

    if (ok) g_bootTarget = target;

    /* Registry persistence verification canary */
    if (ok && RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Mero", 0, NULL, 0, 0, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD flag = (DWORD)target + 100;
        RegSetValueExW(hKey, L"BootConfig", 0, REG_DWORD, (const BYTE*)&flag, sizeof(DWORD));
        RegFlushKey(HKEY_LOCAL_MACHINE);
        RegCloseKey(hKey);
    }

    return ok;
}

static BOOL CALLBACK EnumMinimizeAllProc(HWND hWnd, LPARAM lParam)
{
    (void)lParam;
    WCHAR cls[64];
    if (IsWindowVisible(hWnd) && hWnd != g_hWnd) {
        GetClassNameW(hWnd, cls, sizeof(cls) / sizeof(cls[0]));
        if (_wcsicmp(cls, L"HHTaskBar") != 0 && _wcsicmp(cls, L"DesktopExplorerWindow") != 0) {
            ShowWindow(hWnd, SW_MINIMIZE);
        }
    }
    return TRUE;
}

/* Forward declaration */
static BOOL LaunchApp(const WCHAR *path, BOOL exitShell);

/* Expose Windows CE Desktop & Taskbar cleanly */
static void ShowDesktop(void)
{
    HWND hTaskbar = FindWindowW(L"HHTaskBar", NULL);
    HWND hDesktop = FindWindowW(L"DesktopExplorerWindow", NULL);
    if (!hTaskbar || !hDesktop) {
        /* Explorer shell not started yet; launch it to create desktop & taskbar */
        LaunchApp(PATH_EXPLORER, FALSE);
        Sleep(500);
        hTaskbar = FindWindowW(L"HHTaskBar", NULL);
        hDesktop = FindWindowW(L"DesktopExplorerWindow", NULL);
    }

    KillVendorWatchdog();
    CreateDesktopShortcuts();
    EnumWindows(EnumMinimizeAllProc, 0);

    if (hDesktop) {
        ShowWindow(hDesktop, SW_SHOW);
        SetWindowPos(hDesktop, HWND_BOTTOM, 0, 0, g_screenW, g_screenH, SWP_SHOWWINDOW);
        InvalidateRect(hDesktop, NULL, TRUE);
        UpdateWindow(hDesktop);
    }

    if (hTaskbar) {
        ShowWindow(hTaskbar, SW_SHOW);
        SetWindowPos(hTaskbar, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        InvalidateRect(hTaskbar, NULL, TRUE);
        UpdateWindow(hTaskbar);
    }

    RedrawWindow(NULL, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);

    /* Cleanly destroy Mero Shell so WinCE Desktop and Explorer have full control */
    if (g_hWnd) {
        DestroyWindow(g_hWnd);
    }
}

/* Hardware cold reboot via IOCTL_HAL_REBOOT */
static void HardwareReboot(void)
{
    DWORD bytesRet = 0;
    SetCleanRebootFlag();
    KernelIoControl(IOCTL_HAL_REBOOT, NULL, 0, NULL, 0, &bytesRet);
    SetSystemPowerState(NULL, POWER_STATE_RESET, 0);
}

/* Launch an external application with automatic SDMMC/ResidentFlash fallback */
static BOOL LaunchApp(const WCHAR *path, BOOL exitShell)
{
    PROCESS_INFORMATION pi;
    BOOL ret;

    memset(&pi, 0, sizeof(pi));
    ret = CreateProcessW(path, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
    if (!ret) {
        /* Fallback between \SDMMC and \ResidentFlash */
        if (wcsstr(path, L"\\SDMMC\\MERO\\")) {
            WCHAR altPath[MAX_PATH];
            wsprintfW(altPath, L"\\ResidentFlash\\MERO\\%s", wcsrchr(path,L'\\')+1);
            ret = CreateProcessW(altPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
        } else if (wcsstr(path, L"\\ResidentFlash\\MERO\\")) {
            WCHAR altPath[MAX_PATH];
            wsprintfW(altPath, L"\\SDMMC\\MERO\\%s", wcsrchr(path,L'\\')+1);
            ret = CreateProcessW(altPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
        }
    }
    if (ret) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        if (exitShell && g_hWnd) {
            /* Destroy shell cleanly so launched child owns the screen & RAM */
            ShowWindow(g_hWnd, SW_HIDE);
        }
    }
    return ret;
}

static BOOL LaunchCompanion(int id)
{
    PROCESS_INFORMATION pi;const CompanionApp *app;
    if(id<=0||id>=AUTOLAUNCH_COUNT)return FALSE;
    app=&g_apps[id];
    if(id==AUTOLAUNCH_BT){HWND bt=FindWindowW(L"MERO_BT_FILES",NULL);
        (void)bt;
        if(MeroUsbSet(L"Serial_Class")&&MeroStartAgent(TRUE,TRUE)){
            g_usbStartupReady=TRUE;g_usbMode=USB_MODE_ACTIVESYNC;
            ShowWindow(g_hWnd,SW_HIDE);return TRUE;
        }return FALSE;}
    if(!app->args)return LaunchApp(app->path,TRUE);
    memset(&pi,0,sizeof(pi));
    if(!CreateProcessW(app->path,(LPWSTR)app->args,NULL,NULL,FALSE,0,NULL,NULL,NULL,&pi))return FALSE;
    CloseHandle(pi.hProcess);CloseHandle(pi.hThread);ShowWindow(g_hWnd,SW_HIDE);return TRUE;
}
static int CompanionAt(int slot)
{
    int index=g_appPage*4+slot;
    if(g_selectDefault){if(index==0)return 0;index--;}
    {unsigned int i;for(i=0;i<sizeof(g_appOrder)/sizeof(g_appOrder[0]);i++) {
        int id=g_appOrder[i];
        if((id==AUTOLAUNCH_VIS||id==AUTOLAUNCH_TERMINAL) && GetFileAttributesW(g_apps[id].path)==INVALID_FILE_ATTRIBUTES)continue;
        if(!index--)return id;
    }}return -1;
}
static int CompanionPages(void) {
    int n=g_selectDefault?1:0;unsigned int i;
    for(i=0;i<sizeof(g_appOrder)/sizeof(g_appOrder[0]);i++) {
        int id=g_appOrder[i];
        if((id==AUTOLAUNCH_VIS||id==AUTOLAUNCH_TERMINAL) && GetFileAttributesW(g_apps[id].path)==INVALID_FILE_ATTRIBUTES)continue;
        n++;
    }
    return (n+3)/4;
}

/* Restore or launch factory vendor UI (Launch.exe) */
static void LaunchVendorUI(void)
{
    HWND hWndVendor = FindWindowW(NULL, L"Launch");
    if (!hWndVendor) hWndVendor = FindWindowW(L"Launch", NULL);
    if (!hWndVendor) hWndVendor = FindWindowW(NULL, L"Main");
    if (!hWndVendor) hWndVendor = FindWindowW(L"Main", NULL);
    if (hWndVendor) {
        EnableWindow(hWndVendor, TRUE);
        ShowWindow(hWndVendor, SW_SHOWNORMAL);
        SetForegroundWindow(hWndVendor);
        if (g_hWnd) {
            DestroyWindow(g_hWnd);
        }
        return;
    }

    /* Process not running; spawn directly from Windows folder */
    if (!LaunchApp(L"\\Windows\\Launch.exe", TRUE)) {
        if (!LaunchApp(L"\\Windows\\Main.exe", TRUE)) {
            if (!LaunchApp(L"\\Windows\\YFMenu.exe", TRUE)) {
                wsprintfW(g_statusMsg, L"Vendor UI binary not found in \\Windows");
                InvalidateRect(g_hWnd, NULL, FALSE);
            }
        }
    }
}

static void DumpLaunchStrings(void);

/* Perform a deep hardware & system discovery scan to SD card */
static void PerformSystemDump(void)
{
    HANDLE hFile;
    DWORD bytesWritten;
    char line[1024];
    WIN32_FIND_DATAW wfd;
    HANDLE hFind;
    HKEY hKey;

    CreateDirectoryW(CFG_DIR, NULL);
    hFile = CreateFileW(
        DUMP_FILE,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        wsprintfW(g_statusMsg, L"ERR: Cannot write to %s", DUMP_FILE);
        return;
    }

    #define WRITE_STR(s) do { \
        WriteFile(hFile, (s), (DWORD)strlen(s), &bytesWritten, NULL); \
    } while(0)

    WRITE_STR("========================================\r\n");
    WRITE_STR("   MERO MONITOR #2: SYSTEM DISCOVERY    \r\n");
    WRITE_STR("========================================\r\n\r\n");

    /* 1. Root Storage Scan */
    WRITE_STR("[ROOT DIRECTORIES & STORAGE VOLUMES]\r\n");
    hFind = FindFirstFileW(L"\\*.*", &wfd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                snprintf(line, sizeof(line), "DIR: \\%ls\r\n", wfd.cFileName);
                WRITE_STR(line);
            }
        } while (FindNextFileW(hFind, &wfd));
        FindClose(hFind);
    }
    WRITE_STR("\r\n");

    /* 2. Registry HKLM\\init (The Boot sequence) */
    WRITE_STR("[REGISTRY: HKEY_LOCAL_MACHINE\\init (Startup Launch Sequence)]\r\n");
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"init", 0, 0, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        WCHAR valName[128];
        BYTE  data[256];
        DWORD valLen, dataLen, type;

        while (1) {
            valLen = sizeof(valName) / sizeof(valName[0]);
            dataLen = sizeof(data);
            if (RegEnumValueW(hKey, index++, valName, &valLen, NULL, &type, data, &dataLen) != ERROR_SUCCESS) {
                break;
            }
            if (type == REG_SZ) {
                snprintf(line, sizeof(line), "  %ls = \"%ls\"\r\n", valName, (WCHAR*)data);
                WRITE_STR(line);
            } else if (type == REG_DWORD) {
                snprintf(line, sizeof(line), "  %ls = 0x%08lX\r\n", valName, *(DWORD*)data);
                WRITE_STR(line);
            }
        }
        RegCloseKey(hKey);
    } else {
        WRITE_STR("  [!] Failed to open HKLM\\init\r\n");
    }
    WRITE_STR("\r\n");

    /* 3. Registry HKLM\\Drivers\\BuiltIn (Hardware Peripherals) */
    WRITE_STR("[REGISTRY: HKEY_LOCAL_MACHINE\\Drivers\\BuiltIn (Hardware Subsystems)]\r\n");
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Drivers\\BuiltIn", 0, 0, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        WCHAR subKeyName[128];
        DWORD subLen;

        while (1) {
            subLen = sizeof(subKeyName) / sizeof(subKeyName[0]);
            if (RegEnumKeyExW(hKey, index++, subKeyName, &subLen, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
                break;
            }
            snprintf(line, sizeof(line), "  DRIVER: %ls\r\n", subKeyName);
            WRITE_STR(line);
        }
        RegCloseKey(hKey);
    }
    WRITE_STR("\r\n");

    /* 3b. Registry HKLM\\Drivers\\Active (Currently Loaded Stream Drivers) */
    WRITE_STR("[REGISTRY: HKEY_LOCAL_MACHINE\\Drivers\\Active (Loaded Drivers & COM Ports)]\r\n");
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Drivers\\Active", 0, 0, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        WCHAR subKeyName[128];
        DWORD subLen;

        while (1) {
            subLen = sizeof(subKeyName) / sizeof(subKeyName[0]);
            if (RegEnumKeyExW(hKey, index++, subKeyName, &subLen, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
                break;
            }
            HKEY hSub;
            if (RegOpenKeyExW(hKey, subKeyName, 0, 0, &hSub) == ERROR_SUCCESS) {
                WCHAR keyVal[128] = { 0 };
                WCHAR nameVal[64] = { 0 };
                DWORD kLen = sizeof(keyVal);
                DWORD nLen = sizeof(nameVal);
                RegQueryValueExW(hSub, L"Key", NULL, NULL, (BYTE*)keyVal, &kLen);
                RegQueryValueExW(hSub, L"Name", NULL, NULL, (BYTE*)nameVal, &nLen);
                RegCloseKey(hSub);
                snprintf(line, sizeof(line), "  ACTIVE: [%ls] Name=\"%ls\" Key=\"%ls\"\r\n", subKeyName, nameVal, keyVal);
                WRITE_STR(line);
            }
        }
        RegCloseKey(hKey);
    }
    WRITE_STR("\r\n");

    /* 3c. Registry HKLM\\Drivers\\USB\\FunctionDrivers */
    WRITE_STR("[REGISTRY: HKEY_LOCAL_MACHINE\\Drivers\\USB\\FunctionDrivers]\r\n");
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Drivers\\USB\\FunctionDrivers", 0, 0, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        WCHAR subKeyName[128];
        DWORD subLen;
        WCHAR defClient[64] = { 0 };
        DWORD defLen = sizeof(defClient);
        RegQueryValueExW(hKey, L"DefaultClientDriver", NULL, NULL, (BYTE*)defClient, &defLen);
        snprintf(line, sizeof(line), "  DefaultClientDriver = \"%ls\"\r\n", defClient);
        WRITE_STR(line);

        while (1) {
            subLen = sizeof(subKeyName) / sizeof(subKeyName[0]);
            if (RegEnumKeyExW(hKey, index++, subKeyName, &subLen, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
                break;
            }
            HKEY hSub;
            if (RegOpenKeyExW(hKey, subKeyName, 0, 0, &hSub) == ERROR_SUCCESS) {
                WCHAR dllVal[64] = { 0 }, prefixVal[32] = { 0 };
                DWORD idxVal = 0;
                DWORD dLen = sizeof(dllVal), pLen = sizeof(prefixVal), iLen = sizeof(idxVal);
                RegQueryValueExW(hSub, L"Dll", NULL, NULL, (BYTE*)dllVal, &dLen);
                RegQueryValueExW(hSub, L"Prefix", NULL, NULL, (BYTE*)prefixVal, &pLen);
                RegQueryValueExW(hSub, L"Index", NULL, NULL, (BYTE*)&idxVal, &iLen);
                RegCloseKey(hSub);
                snprintf(line, sizeof(line), "  USB FN: [%ls] Dll=\"%ls\" Prefix=\"%ls\" Index=%lu\r\n",
                         subKeyName, dllVal, prefixVal, idxVal);
                WRITE_STR(line);
            }
        }
        RegCloseKey(hKey);
    }
    WRITE_STR("\r\n");

    /* 4. Scan \\ResidentFlash for vendor apps, inis, bmps */
    WRITE_STR("[FILES IN \\ResidentFlash (Internal Storage)]\r\n");
    hFind = FindFirstFileW(L"\\ResidentFlash\\*.*", &wfd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            snprintf(line, sizeof(line), "  \\ResidentFlash\\%ls %s\r\n",
                     wfd.cFileName,
                     (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "<DIR>" : "");
            WRITE_STR(line);
        } while (FindNextFileW(hFind, &wfd));
        FindClose(hFind);
    } else {
        WRITE_STR("  (No \\ResidentFlash volume found or empty)\r\n");
    }
    WRITE_STR("\r\n");

    /* 5. Scan \\Windows for Executables */
    WRITE_STR("[KEY EXECUTABLES IN \\Windows]\r\n");
    hFind = FindFirstFileW(L"\\Windows\\*.exe", &wfd);
    if (hFind != INVALID_HANDLE_VALUE) {
        int count = 0;
        do {
            snprintf(line, sizeof(line), "%ls%s", wfd.cFileName, (++count % 4 == 0) ? "\r\n" : "  |  ");
            WRITE_STR(line);
        } while (FindNextFileW(hFind, &wfd));
        FindClose(hFind);
        WRITE_STR("\r\n");
    }
    WRITE_STR("\r\n=== END OF DISCOVERY DUMP ===\r\n");

    #undef WRITE_STR

    CloseHandle(hFile);

    /* Extract strings from \Windows\Launch.exe */
    DumpLaunchStrings();

    wsprintfW(g_statusMsg, L"SUCCESS // Dump & strings written to \\SDMMC\\MERO");
}

/* Extract interesting string references from vendor \\Windows\\Launch.exe */
static void DumpLaunchStrings(void)
{
    HANDLE hIn, hOut;
    DWORD bytesRead, bytesWritten;
    static char buf[4096];
    char line[512];
    int i, len;

    hIn = CreateFileW(L"\\Windows\\Launch.exe", GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hIn == INVALID_HANDLE_VALUE) return;

    hOut = CreateFileW(L"\\SDMMC\\MERO\\launch_strings.txt", GENERIC_WRITE, FILE_SHARE_READ,
                       NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOut == INVALID_HANDLE_VALUE) {
        CloseHandle(hIn);
        return;
    }

    const char *hdr = "=== STRINGS EXTRACTED FROM \\Windows\\Launch.exe ===\r\n\r\n";
    WriteFile(hOut, hdr, (DWORD)strlen(hdr), &bytesWritten, NULL);

    while (ReadFile(hIn, buf, sizeof(buf) - 2, &bytesRead, NULL) && bytesRead > 0) {
        for (i = 0; i < (int)bytesRead - 4; i++) {
            if (buf[i] >= 32 && buf[i] <= 126) {
                len = 0;
                while (i + len < (int)bytesRead && buf[i + len] >= 32 && buf[i + len] <= 126 && len < 200) {
                    len++;
                }
                if (len >= 5) {
                    char temp[256];
                    memcpy(temp, &buf[i], len);
                    temp[len] = '\0';
                    if (strstr(temp, ".ini") || strstr(temp, ".INI") ||
                        strstr(temp, ".bmp") || strstr(temp, ".BMP") ||
                        strstr(temp, ".exe") || strstr(temp, ".EXE") ||
                        strstr(temp, "SDMMC") || strstr(temp, "Resident") ||
                        strstr(temp, "Navi") || strstr(temp, "navi") ||
                        strstr(temp, "Auto") || strstr(temp, "auto") ||
                        strstr(temp, "Path") || strstr(temp, "Init") ||
                        strstr(temp, "Software") || strstr(temp, "Menu")) {
                        snprintf(line, sizeof(line), "ASCII: %s\r\n", temp);
                        WriteFile(hOut, line, (DWORD)strlen(line), &bytesWritten, NULL);
                    }
                    i += len;
                }
            }
        }
    }

    CloseHandle(hIn);
    CloseHandle(hOut);
}

/* Update button text & colors based on current page */
typedef int (WINAPI *MeroBthGetMode)(DWORD *);
typedef int (WINAPI *MeroBthSetMode)(DWORD);
static HMODULE g_bthModule;
static MeroBthGetMode g_bthGet;
static MeroBthSetMode g_bthSet;
static DWORD g_bthMode;
static BOOL g_bthKnown;

static void BluetoothInspect(void);
static void BluetoothRefresh(void);
static void BluetoothSet(DWORD mode);
static void BluetoothPairing(void);

static COLORREF MenuColor(int n){static const COLORREF colors[]={RGB(255,197,93),RGB(255,132,119),RGB(110,225,179),RGB(193,154,255),RGB(117,209,255),RGB(249,150,207)};return colors[n%6];}
static int g_portrait;
static void ShellLiveLine(char *line){
 if(!strncmp(line,"MERO:AMB:",9)){char *p=strstr(line,"portrait=");int next=p?atoi(p+9):0;
 if(next!=g_portrait){g_portrait=next;g_screenW=next?272:480;g_screenH=next?480:272;InvalidateRect(g_hWnd,NULL,FALSE);}}
}
static void UpdateButtons(void)
{
    int colW = 215;
    int rowH = 46;
    int xLeft = 18;
    int xRight = 247;
    int y0 = 34;
    int spacing = 8;

    /* Left column */
    SetRect(&g_buttons[0].rc, xLeft, y0, xLeft + colW, y0 + rowH);
    SetRect(&g_buttons[1].rc, xLeft, y0 + (rowH + spacing), xLeft + colW, y0 + (rowH + spacing) + rowH);
    SetRect(&g_buttons[2].rc, xLeft, y0 + (rowH + spacing)*2, xLeft + colW, y0 + (rowH + spacing)*2 + rowH);

    /* Right column */
    SetRect(&g_buttons[3].rc, xRight, y0, xRight + colW, y0 + rowH);
    SetRect(&g_buttons[4].rc, xRight, y0 + (rowH + spacing), xRight + colW, y0 + (rowH + spacing) + rowH);
    SetRect(&g_buttons[5].rc, xRight, y0 + (rowH + spacing)*2, xRight + colW, y0 + (rowH + spacing)*2 + rowH);

    if(g_portrait){int i;for(i=0;i<6;i++)SetRect(&g_buttons[i].rc,18,48+i*58,254,100+i*58);}
    switch (g_currentPage) {
    case PAGE_MAIN:
        lstrcpyW(g_buttons[0].title,L"01 - COMPANION LABS >>");
        lstrcpyW(g_buttons[0].sub,L"All applications, one place");
        lstrcpyW(g_buttons[1].title,L"02 - DEFAULT APP >>");
        lstrcpyW(g_buttons[1].sub,g_apps[g_pref].name);
        lstrcpyW(g_buttons[2].title,L"03 - BLUETOOTH LINK >>");
        lstrcpyW(g_buttons[2].sub,L"Agent and connection settings");
        lstrcpyW(g_buttons[3].title,L"04 - SYSTEM TOOLS >>");
        lstrcpyW(g_buttons[3].sub,L"Hardware and recovery settings");
        lstrcpyW(g_buttons[4].title,L"05 - WINCE DESKTOP");
        lstrcpyW(g_buttons[4].sub,L"Leave the launcher");
        lstrcpyW(g_buttons[5].title,L"06 - ORIENTATION");
        lstrcpyW(g_buttons[5].sub,L"Portrait / landscape display");
        {int b;for(b=0;b<6;b++)g_buttons[b].color=MenuColor(b);}
        break;
    case PAGE_COMPANIONS:
        {int slot;for(slot=0;slot<4;slot++){
            int b=slot<2?slot:slot+1,id=CompanionAt(slot);
            if(id<0){lstrcpyW(g_buttons[b].title,L"");lstrcpyW(g_buttons[b].sub,L"");}
            else{
                wsprintfW(g_buttons[b].title,L"%s%s",g_pref==id?L"* ":L"",g_apps[id].name);
                lstrcpyW(g_buttons[b].sub,g_selectDefault?L"Tap to use at startup":g_apps[id].description);
            }
            g_buttons[b].color=MenuColor(id);
        }}
        wsprintfW(g_buttons[2].title,L"MORE APPS >> [%d/%d]",g_appPage+1,CompanionPages());
        lstrcpyW(g_buttons[2].sub,L"Next page");g_buttons[2].color=RGB(140,190,255);
        lstrcpyW(g_buttons[5].title,L"<< BACK TO MAIN");
        lstrcpyW(g_buttons[5].sub,g_selectDefault?L"Cancel default selection":L"Return to shell home");
        g_buttons[5].color=RGB(180,180,180);break;
    case PAGE_SYSTEM:
        /* PAGE 2: System Tools & Hardware Options */
        lstrcpyW(g_buttons[0].title, L"[1] AUTONOMOUS BOOT");
        lstrcpyW(g_buttons[0].sub,   g_bootTargetNames[g_bootTarget]);
        g_buttons[0].color = RGB(0, 255, 128);

        lstrcpyW(g_buttons[1].title, L"[2] USB STORAGE MODE");
        lstrcpyW(g_buttons[1].sub,   g_usbModeNames[g_usbMode]);
        g_buttons[1].color = RGB(0, 220, 255);

        lstrcpyW(g_buttons[2].title, L"[3] VENDOR GPS UI");
        lstrcpyW(g_buttons[2].sub,   L"Original Factory Navigation UI");
        g_buttons[2].color = RGB(255, 200, 40);

        lstrcpyW(g_buttons[3].title, L"[4] BLUETOOTH >>");
        lstrcpyW(g_buttons[3].sub,   L"Radio, visibility, pairing, status");
        g_buttons[3].color = RGB(255, 140, 40);

        lstrcpyW(g_buttons[4].title, L"[5] ADVANCED TOOLS >>");
        lstrcpyW(g_buttons[4].sub,   L"Watchdog, Hardware Dump, Reset");
        g_buttons[4].color = RGB(120, 200, 255);

        lstrcpyW(g_buttons[5].title, L"[6] << BACK TO MAIN");
        lstrcpyW(g_buttons[5].sub,   L"Return to Main OS Hub");
        g_buttons[5].color = RGB(160, 160, 160);
        break;

    case PAGE_BLUETOOTH:
        lstrcpyW(g_buttons[0].title, L"[1] CONNECT / AGENT >>");
        lstrcpyW(g_buttons[0].sub, L"Start or reconnect the saved agent");
        lstrcpyW(g_buttons[1].title, L"[2] OEM RADIO ON");
        lstrcpyW(g_buttons[1].sub, L"Request power through actual backend");
        lstrcpyW(g_buttons[2].title, L"[3] OEM RADIO OFF");
        lstrcpyW(g_buttons[2].sub, L"Disconnect Bluetooth via OEM backend");
        lstrcpyW(g_buttons[3].title, L"[4] BACKEND STATUS");
        lstrcpyW(g_buttons[3].sub, L"Check installed backend process");
        lstrcpyW(g_buttons[4].title, L"[5] EXPORT VENDOR FILES");
        lstrcpyW(g_buttons[4].sub, L"Capture actual app / DLLs to SD");
        lstrcpyW(g_buttons[5].title, L"[6] << BACK TO TOOLS");
        lstrcpyW(g_buttons[5].sub, L"System and advanced controls");
        { int b; for (b = 0; b < 6; b++) g_buttons[b].color = MenuColor(b); }
        break;

    case PAGE_ADVANCED:
    default:
        /* PAGE 3: Advanced Diagnostics & Subsystem Tools */
        lstrcpyW(g_buttons[0].title, L"[1] KILL VENDOR DOG");
        lstrcpyW(g_buttons[0].sub,   L"Stop watchdog / launcher");
        g_buttons[0].color = RGB(255, 80, 80);

        lstrcpyW(g_buttons[1].title, L"[2] HARDWARE DUMP");
        lstrcpyW(g_buttons[1].sub,   L"Registry & Peripheral Scan");
        g_buttons[1].color = RGB(0, 220, 255);

        lstrcpyW(g_buttons[2].title, L"[3] WINCE EXPLORER");
        lstrcpyW(g_buttons[2].sub,   L"Standard Windows CE Shell");
        g_buttons[2].color = RGB(255, 180, 0);

        {
            WCHAR volBuf[32];
            wsprintfW(volBuf, L"Master Level: %d%%", g_volumeLevel * 25);
            lstrcpyW(g_buttons[3].title, L"[4] VOLUME TOGGLE");
            lstrcpyW(g_buttons[3].sub, volBuf);
            g_buttons[3].color = RGB(180, 140, 255);
        }

        lstrcpyW(g_buttons[4].title, L"[5] REBOOT DEVICE");
        lstrcpyW(g_buttons[4].sub,   L"Cold Hardware Reset (IOCTL)");
        g_buttons[4].color = RGB(255, 120, 80);

        lstrcpyW(g_buttons[5].title, L"[6] << BACK TO TOOLS");
        lstrcpyW(g_buttons[5].sub,   L"Return to System Tools");
        g_buttons[5].color = RGB(160, 160, 160);
        break;
    }
}

/* Paint Custom Mero Shell (Double Buffered - Zero Blinking) */

static void BluetoothRefresh(void)
{
    if (!g_bthModule) {
        g_bthModule = LoadLibraryW(L"bthutil.dll");
        if (g_bthModule) {
            g_bthGet = (MeroBthGetMode)GetProcAddress(g_bthModule, L"BthGetMode");
            g_bthSet = (MeroBthSetMode)GetProcAddress(g_bthModule, L"BthSetMode");
        }
    }
    g_bthKnown = g_bthGet && g_bthGet(&g_bthMode) == 0 && g_bthMode <= 2;
    if (!g_bthKnown) lstrcpyW(g_statusMsg, L"OEM Bluetooth API unavailable; use pairing panel");
    else wsprintfW(g_statusMsg, L"Bluetooth: %s", g_bthMode == 0 ? L"OFF" :
                   g_bthMode == 1 ? L"ON / hidden" : L"ON / discoverable");
}

static void BluetoothSet(DWORD mode)
{
    BluetoothRefresh();
    if (!g_bthKnown || !g_bthSet) {
        lstrcpyW(g_statusMsg, L"Radio control unsupported by this OEM stack");
        return;
    }
    int result = g_bthSet(mode);
    BluetoothRefresh();
    if (result || !g_bthKnown || g_bthMode != mode)
        wsprintfW(g_statusMsg, L"Bluetooth change not confirmed (error %d)", result);
}



static void BluetoothEvidenceLine(HANDLE log, const char *line)
{
    DWORD written;
    WriteFile(log, line, (DWORD)strlen(line), &written, NULL);
}

/* Copy the readable file stream, without trying to reproduce ROM attributes. */
static void BluetoothCaptureFile(HANDLE log, const WCHAR *source, const WCHAR *dest,
                                 DWORD *budget)
{
    HANDLE input, output;
    DWORD size, high, got, written, total = 0, error = 0;
    BYTE buffer[4096];
    char line[640];
    input = CreateFileW(source, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (input == INVALID_HANDLE_VALUE) { error = GetLastError(); goto report; }
    size = GetFileSize(input, &high);
    if (high || size == INVALID_FILE_SIZE || size > *budget) {
        error = ERROR_DISK_FULL; CloseHandle(input); goto report;
    }
    output = CreateFileW(dest, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) { error = GetLastError(); CloseHandle(input); goto report; }
    *budget -= size;
    while (total < size) {
        DWORD want = size - total;
        if (want > sizeof(buffer)) want = sizeof(buffer);
        if (!ReadFile(input, buffer, want, &got, NULL)) { error = GetLastError(); break; }
        if (!got) { error = ERROR_HANDLE_EOF; break; }
        if (!WriteFile(output, buffer, got, &written, NULL)) { error = GetLastError(); break; }
        if (written != got) { error = ERROR_WRITE_FAULT; break; }
        total += written;
    }
    if (!FlushFileBuffers(output) && !error) error = GetLastError();
    CloseHandle(output);
    CloseHandle(input);
report:
    snprintf(line, sizeof(line), "Capture %ls -> %ls bytes=%lu error=%lu\r\n", source, dest, total, error);
    BluetoothEvidenceLine(log, line);
}

static void BluetoothCaptureTree(HANDLE log, const WCHAR *source, const WCHAR *dest,
                                 DWORD *budget, int depth)
{
    WIN32_FIND_DATAW fd;
    HANDLE find;
    WCHAR pattern[260], path[260], target[260];
    char line[640];
    if (depth > 4 || wcslen(source) > 180 || wcslen(dest) > 180) return;
    CreateDirectoryW(dest, NULL);
    wsprintfW(pattern, L"%s\\*", source);
    find = FindFirstFileW(pattern, &fd);
    if (find == INVALID_HANDLE_VALUE) {
        snprintf(line, sizeof(line), "Directory %ls unavailable error=%lu\r\n", source, GetLastError());
        BluetoothEvidenceLine(log, line); return;
    }
    do {
        const WCHAR *ext;
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        if (wcslen(fd.cFileName) > 70) continue;
        wsprintfW(path, L"%s\\%s", source, fd.cFileName);
        wsprintfW(target, L"%s\\%s", dest, fd.cFileName);
        snprintf(line, sizeof(line), "OEM file %ls size=%lu attributes=%08lx\r\n",
                 path, fd.nFileSizeLow, fd.dwFileAttributes);
        BluetoothEvidenceLine(log, line);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            BluetoothCaptureTree(log, path, target, budget, depth + 1);
        } else {
            ext = wcsrchr(fd.cFileName, L'.');
            if (ext && (!_wcsicmp(ext, L".exe") || !_wcsicmp(ext, L".dll")))
                BluetoothCaptureFile(log, path, target, budget);
        }
    } while (FindNextFileW(find, &fd));
    FindClose(find);
}

static void BluetoothCaptureVendor(void)
{
    const WCHAR *dlls[] = { L"btdrt.dll", L"hciext.dll", L"mtk_bthuart.dll",
                            L"btsvc.dll", L"btd.dll", L"bthuniv.dll", L"bta2dp.dll" };
    WCHAR root[128], path[260], dest[260];
    HANDLE log, snap;
    DWORD budget = 32 * 1024 * 1024;
    ULARGE_INTEGER available;
    unsigned int i;
    char line[640];
    if (!GetDiskFreeSpaceExW(CFG_DIR, &available, NULL, NULL) ||
        available.QuadPart < 4 * 1024 * 1024) {
        lstrcpyW(g_statusMsg, L"Export needs mounted SD and at least 4 MB free"); return;
    }
    if (available.QuadPart < budget + 1024 * 1024)
        budget = (DWORD)(available.QuadPart - 1024 * 1024);
    wsprintfW(root, L"\\SDMMC\\MERO\\oem-bt-%lu", GetTickCount());
    CreateDirectoryW(root, NULL);
    wsprintfW(path, L"%s\\vendor-inventory.txt", root);
    log = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (log == INVALID_HANDLE_VALUE) { lstrcpyW(g_statusMsg, L"Cannot create OEM capture"); return; }
    BluetoothEvidenceLine(log, "OEM Bluetooth capture v2: file reads only; no controller commands\r\n");
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
        if (Process32First(snap, &pe)) do {
            snprintf(line, sizeof(line), "Process %lu %ls\r\n", pe.th32ProcessID, pe.szExeFile);
            BluetoothEvidenceLine(log, line);
        } while (Process32Next(snap, &pe));
        CloseHandle(snap);
    }
    wsprintfW(dest, L"%s\\ResidentFlash2", root);
    BluetoothCaptureTree(log, L"\\ResidentFlash2", dest, &budget, 0);
    wsprintfW(dest, L"%s\\ResidentFlash", root);
    BluetoothCaptureTree(log, L"\\ResidentFlash", dest, &budget, 0);
    wsprintfW(dest, L"%s\\Windows", root); CreateDirectoryW(dest, NULL);
    for (i = 0; i < sizeof(dlls)/sizeof(dlls[0]); i++) {
        wsprintfW(path, L"\\Windows\\%s", dlls[i]);
        wsprintfW(dest, L"%s\\Windows\\%s", root, dlls[i]);
        BluetoothCaptureFile(log, path, dest, &budget);
    }
    FlushFileBuffers(log); CloseHandle(log);
    lstrcpyW(g_statusMsg, L"Vendor capture saved to SD: MERO/oem-bt-* ");
}

/* Read ROM/configuration only. Export files to SD; never execute OEM initializers. */
static void BluetoothInspect(void)
{
    static const WCHAR *keys[] = {
        L"Drivers\\BuiltIn\\HciExt", L"Drivers\\BuiltIn\\BtA2dpSnd"
    };
    static const WCHAR *files[] = {
        L"Launch.exe", L"btpower.exe", L"btdrt.dll", L"bthutil.dll"
    };
    WCHAR root[128] = L"\\SDMMC\\MERO\\bluetooth-inspect";
    WCHAR path[260], dest[260];
    WIN32_FIND_DATAW fd;
    HANDLE find, log;
    char line[1024];
    DWORD written;
    unsigned int i;
    CreateDirectoryW(CFG_DIR, NULL);
    CreateDirectoryW(root, NULL);
    wsprintfW(path, L"%s\\inventory.txt", root);
    log = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL, NULL);
    if (log == INVALID_HANDLE_VALUE) {
        lstrcpyW(g_statusMsg, L"Export needs SD card outside USB mass storage");
        return;
    }
#define BT_WRITE() WriteFile(log, line, (DWORD)strlen(line), &written, NULL)
    snprintf(line, sizeof(line), "Mero Bluetooth OEM evidence; shell 0.2.11\r\n"); BT_WRITE();
    snprintf(line, sizeof(line), "bthutil getter=%d setter=%d known=%d mode=%lu\r\n",
             g_bthGet != NULL, g_bthSet != NULL, g_bthKnown, g_bthMode); BT_WRITE();
    find = FindFirstFileW(L"\\Windows\\*", &fd);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            snprintf(line, sizeof(line), "Windows: %ls size=%lu attr=%08lx\r\n",
                     fd.cFileName, fd.nFileSizeLow, fd.dwFileAttributes); BT_WRITE();
        } while (FindNextFileW(find, &fd));
        FindClose(find);
    }
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        HKEY key;
        snprintf(line, sizeof(line), "HKLM\\%ls\r\n", keys[i]); BT_WRITE();
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keys[i], 0, 0, &key) == ERROR_SUCCESS) {
            DWORD index = 0, result;
            do {
                WCHAR name[128];
                BYTE data[514];
                DWORD nameLen = 128, dataLen = 512, type;
                memset(data, 0, sizeof(data));
                result = RegEnumValueW(key, index++, name, &nameLen, NULL, &type, data, &dataLen);
                if (result == ERROR_SUCCESS) {
                    if (type == REG_SZ) snprintf(line, sizeof(line), "  %ls=%ls\r\n", name, (WCHAR*)data);
                    else if (type == REG_DWORD && dataLen == sizeof(DWORD)) {
                        DWORD value; memcpy(&value, data, sizeof(value));
                        snprintf(line, sizeof(line), "  %ls=%08lx\r\n", name, value);
                    } else snprintf(line, sizeof(line), "  %ls type=%lu bytes=%lu\r\n", name, type, dataLen);
                    BT_WRITE();
                }
            } while (result == ERROR_SUCCESS || result == ERROR_MORE_DATA);
            RegCloseKey(key);
        }
    }
    for (i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        DWORD error = ERROR_FILE_NOT_FOUND;
        BOOL copied = FALSE;
        wsprintfW(path, L"\\Windows\\%s", files[i]);
        wsprintfW(dest, L"%s\\%s", root, files[i]);
        find = FindFirstFileW(path, &fd);
        if (find != INVALID_HANDLE_VALUE) {
            FindClose(find);
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.nFileSizeHigh == 0 &&
                fd.nFileSizeLow <= 8 * 1024 * 1024) {
                /* Keep previous captures; do not overwrite while a host reads them. */
                copied = CopyFileW(path, dest, TRUE);
                error = copied ? 0 : GetLastError();
            } else error = ERROR_INVALID_DATA;
        }
        snprintf(line, sizeof(line), "Export %ls copied=%d error=%lu\r\n", files[i], copied, error); BT_WRITE();
    }
    FlushFileBuffers(log);
    CloseHandle(log);
#undef BT_WRITE
    lstrcpyW(g_statusMsg, L"OEM details saved: SDMMC/MERO/bluetooth-inspect");
}

static void BluetoothPairing(void)
{
    /* Actual device inventory confirms this WinCE applet. No guessed applet index. */
    if (GetFileAttributesW(L"\\Windows\\bthpnl.cpl") != INVALID_FILE_ATTRIBUTES) {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        WCHAR args[] = L"\\Windows\\bthpnl.cpl";
        memset(&si, 0, sizeof(si));
        memset(&pi, 0, sizeof(pi));
        si.cb = sizeof(si);
        if (CreateProcessW(L"\\Windows\\ctlpnl.exe", args, NULL, NULL, FALSE,
                           0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            lstrcpyW(g_statusMsg, L"Opened WinCE Bluetooth settings");
            return;
        }
        wsprintfW(g_statusMsg, L"Bluetooth applet launch failed: %lu", GetLastError());
        return;
    }
    /* Reuse only an installed pairing app, never the complete vendor launcher. */
    HWND phone = FindWindowW(NULL, L"ANW_PHONELINK");
    if (phone) {
        EnableWindow(phone, TRUE);
        ShowWindow(phone, SW_SHOW);
        SetForegroundWindow(phone);
        return;
    }
    if (GetFileAttributesW(L"\\Windows\\PhoneLink.exe") != INVALID_FILE_ATTRIBUTES &&
        LaunchApp(L"\\Windows\\PhoneLink.exe", FALSE)) return;
    lstrcpyW(g_statusMsg, L"OEM panel not identified; tap EXPORT OEM DETAILS");
}


/* Confirmed PhoneLink registered-message handlers: WPARAM=1 on, WPARAM=0 off. */
static int g_oemPending = -1;
static DWORD g_oemStarted;
static HWND BluetoothOemWindow(void)
{
    HWND h = FindWindowW(NULL, L"ANW_PHONELINK");
    if (!h) h = FindWindowW(L"ANW_PHONELINK", NULL);
    return h;
}
static void BluetoothOemPower(int on)
{
    HWND h = BluetoothOemWindow();
    if (!h) {
        if (!on) { lstrcpyW(g_statusMsg, L"OEM backend is not running"); return; }
        const WCHAR *path = L"\\ResidentFlash2\\App\\BT\\PhoneLink.exe";
        if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
            path = L"\\ResidentFlash\\App\\BT\\PhoneLink.exe";
        if (!LaunchApp(path, FALSE)) { lstrcpyW(g_statusMsg, L"OEM backend start failed"); return; }
        g_oemPending = on; g_oemStarted = GetTickCount();
        lstrcpyW(g_statusMsg, L"Starting OEM Bluetooth backend...");
        return;
    }
    ShowWindow(h, SW_HIDE);
    if (PostMessageW(h, RegisterWindowMessageW(L"WM_ANWBT_INMSG_BTPOWER"), on, 0))
        lstrcpyW(g_statusMsg, on ? L"OEM radio ON requested; test link in Bluetooth Files" : L"OEM radio OFF requested");
    else lstrcpyW(g_statusMsg, L"OEM power request failed");
    SetForegroundWindow(g_hWnd);
}
static void BluetoothOemTick(void)
{
    if (g_oemPending < 0) return;
    if (BluetoothOemWindow()) {
        int on = g_oemPending; g_oemPending = -1;
        BluetoothOemPower(on); InvalidateRect(g_hWnd, NULL, FALSE);
    } else if (GetTickCount() - g_oemStarted > 10000) {
        g_oemPending = -1;
        lstrcpyW(g_statusMsg, L"OEM backend did not create its window");
        InvalidateRect(g_hWnd, NULL, FALSE);
    }
}

static void OnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc;
    HDC memDC;
    HBITMAP memBmp, oldBmp;
    RECT rc;
    WCHAR buf[256];
    int i;

    if (!IsWindowVisible(hWnd)) {
        hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        return;
    }

    hdc = BeginPaint(hWnd, &ps);
    GetClientRect(hWnd, &rc);
    if(g_portrait){rc.right=272;rc.bottom=480;}

    memDC = CreateCompatibleDC(hdc);
    BITMAPINFO bi;DWORD *pixels;memset(&bi,0,sizeof(bi));bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=rc.right;bi.bmiHeader.biHeight=-rc.bottom;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    memBmp = CreateDIBSection(hdc,&bi,DIB_RGB_COLORS,(void**)&pixels,NULL,0);
    oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    /* Solid Black Background */
    FillRect(memDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(memDC, TRANSPARENT);

    /* TOP STATUS BAR */
    SetTextColor(memDC, RGB(0, 255, 128));
    const WCHAR *pageTitle = L"OS SHELL";
    if (g_currentPage == PAGE_COMPANIONS) pageTitle = L"COMPANION LABS";
    else if (g_currentPage == PAGE_SYSTEM) pageTitle = L"SYSTEM TOOLS";
    else if (g_currentPage == PAGE_BLUETOOTH) pageTitle = L"BLUETOOTH CONTROL";
    else if (g_currentPage == PAGE_ADVANCED) pageTitle = L"ADVANCED TOOLS";
    wsprintfW(buf, L"MERO // %s [v%s]", pageTitle, SHELL_VERSION);
    ExtTextOutW(memDC, 18, 8, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Power / Battery */
    if (g_batteryPercent >= 0) {
        SetTextColor(memDC, g_batteryPercent < 20 ? RGB(255, 80, 80) : RGB(255, 200, 40));
        wsprintfW(buf, L"%s %d%%", g_isAC ? L"[AC]" : L"[BAT]", g_batteryPercent);
    } else {
        SetTextColor(memDC, RGB(255, 200, 40));
        wsprintfW(buf, L"%s", g_isAC ? L"[AC ON]" : L"[BAT]");
    }
    ExtTextOutW(memDC, g_portrait?18:230, g_portrait?28:8, 0, NULL, buf, lstrlenW(buf), NULL);

    /* RAM Status */
    SetTextColor(memDC, RGB(0, 200, 255));
    wsprintfW(buf, L"RAM: %luM/%luM", g_memAvailMB, g_memTotalMB);
    ExtTextOutW(memDC, g_portrait?130:340, g_portrait?28:8, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Header Divider Line */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(40, 90, 60));
        HPEN hOld = (HPEN)SelectObject(memDC, hPen);
        MoveToEx(memDC, 18, 26, NULL);
        LineTo(memDC, g_screenW - 18, 26);
        SelectObject(memDC, hOld);
        DeleteObject(hPen);
    }

    UpdateButtons();

    /* DRAW APPLICATION BUTTONS */
    for (i = 0; i < 6; i++) {
        RECT btnRc = g_buttons[i].rc;
        COLORREF color=g_buttons[i].color;int selected=g_buttons[i].title[0]==L'*';
        HBRUSH tint=CreateSolidBrush(RGB(GetRValue(color)/(selected?4:10),GetGValue(color)/(selected?4:10),GetBValue(color)/(selected?4:10)));
        FillRect(memDC,&btnRc,tint);DeleteObject(tint);
        HPEN hPen = CreatePen(PS_SOLID, selected?3:1, color);
        HPEN hOld = (HPEN)SelectObject(memDC, hPen);

        /* Draw bounding box */
        MoveToEx(memDC, btnRc.left, btnRc.top, NULL);
        LineTo(memDC, btnRc.right, btnRc.top);
        LineTo(memDC, btnRc.right, btnRc.bottom);
        LineTo(memDC, btnRc.left, btnRc.bottom);
        LineTo(memDC, btnRc.left, btnRc.top);

        SelectObject(memDC, hOld);
        DeleteObject(hPen);

        /* Button Title */
        SetTextColor(memDC, g_buttons[i].color);
        ExtTextOutW(memDC, btnRc.left + 10, btnRc.top + 7, 0, NULL,
                   g_buttons[i].title, lstrlenW(g_buttons[i].title), NULL);

        /* Button Subtitle */
        SetTextColor(memDC, RGB(180, 180, 180));
        ExtTextOutW(memDC, btnRc.left + 10, btnRc.top + 25, 0, NULL,
                   g_buttons[i].sub, lstrlenW(g_buttons[i].sub), NULL);
    }

    /* FOOTER STATUS BAR */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(30, 60, 40));
        HPEN hOld = (HPEN)SelectObject(memDC, hPen);
        MoveToEx(memDC, 18, g_portrait?408:202, NULL);
        LineTo(memDC, g_screenW - 18, g_portrait?408:202);
        SelectObject(memDC, hOld);
        DeleteObject(hPen);
    }

    if (g_countdownActive) {
        SetTextColor(memDC, RGB(255, 220, 40));
    } else {
        SetTextColor(memDC, RGB(140, 180, 160));
    }
    ExtTextOutW(memDC, 18, g_portrait?420:212, 0, NULL, g_statusMsg, lstrlenW(g_statusMsg), NULL);

    /* Footer Hint */
    SetTextColor(memDC, RGB(90, 120, 100));
    const WCHAR *pageShort = L"Main";
    if (g_currentPage == PAGE_COMPANIONS) pageShort = L"Companions";
    else if (g_currentPage == PAGE_SYSTEM) pageShort = L"System";
    else if (g_currentPage == PAGE_BLUETOOTH) pageShort = L"Bluetooth";
    else if (g_currentPage == PAGE_ADVANCED) pageShort = L"Advanced";
    wsprintfW(buf, L"Foston FS-460BT // WinCE 5.0 Core // SDMMC: Active // %s (%d/4)", pageShort, (int)g_currentPage + 1);
    ExtTextOutW(memDC, 18, g_portrait?450:234, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Atomic BitBlt to display - zero tearing, zero blinking */
    if(g_portrait&&pixels){DWORD *bits;HDC rot=CreateCompatibleDC(hdc);bi.bmiHeader.biWidth=480;bi.bmiHeader.biHeight=-272;
        HBITMAP rb=CreateDIBSection(hdc,&bi,DIB_RGB_COLORS,(void**)&bits,NULL,0),old=(HBITMAP)SelectObject(rot,rb);
        if(bits){int x,y;for(y=0;y<480;y++)for(x=0;x<272;x++)bits[x*480+479-y]=pixels[y*272+x];BitBlt(hdc,0,0,480,272,rot,0,0,SRCCOPY);}
        SelectObject(rot,old);DeleteObject(rb);DeleteDC(rot);
    }else BitBlt(hdc,0,0,480,272,memDC,0,0,SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hWnd, &ps);
}

/* Handle touch tap events */
static void OnTouch(int x, int y)
{
    int i;
    g_allowBootReload=FALSE;

    /* Any touch cancels countdown */
    if (g_countdownActive) {
        g_countdownActive = FALSE;
        wsprintfW(g_statusMsg, L"Auto-launch aborted. Shell ready.");
        InvalidateRect(g_hWnd, NULL, FALSE);
        return;
    }

    for (i = 0; i < 6; i++) {
        if (PtInRect(&g_buttons[i].rc, (POINT){ x, y })) {
            switch (g_currentPage) {
            case PAGE_MAIN:
                if(i==0||i==1){g_currentPage=PAGE_COMPANIONS;g_appPage=0;g_selectDefault=(i==1);
                    lstrcpyW(g_statusMsg,g_selectDefault?L"Choose startup app // * = current default":L"All apps // * = startup default");}
                else if(i==2)g_currentPage=PAGE_BLUETOOTH;
                else if(i==3)g_currentPage=PAGE_SYSTEM;
                else if(i==4){ShowDesktop();DestroyWindow(g_hWnd);}
                else {
                    char token[17]="";int sent=0;
                    if(MeroBusLock()){char *line=strstr(mero_bus->lines,"MERO:AMB:");char *key=line?strstr(line,"token="):NULL;
                        if(key)sscanf(key+6,"%16[0-9a-f]",token);ReleaseMutex(mero_bus_mutex);}
                    if(token[0])sent=MeroBusCommand("ambient-rotate",token,0);
                    lstrcpyW(g_statusMsg,sent?L"Ambient orientation toggle sent":L"Wait for Bluetooth / previous request");
                }
                InvalidateRect(g_hWnd,NULL,FALSE);break;
            case PAGE_COMPANIONS:
                if(i==5){g_currentPage=PAGE_MAIN;g_selectDefault=FALSE;}
                else if(i==2)g_appPage=(g_appPage+1)%CompanionPages();
                else{
                    int id=CompanionAt(i<2?i:i-1);
                    if(id>=0){
                        if(g_selectDefault){AutoLaunchPref previous=g_pref;g_pref=(AutoLaunchPref)id;
                            if(SaveConfig()){wsprintfW(g_statusMsg,L"Startup default saved: %s",g_apps[id].name);g_currentPage=PAGE_MAIN;g_selectDefault=FALSE;}
                            else{g_pref=previous;lstrcpyW(g_statusMsg,L"Could not save. Keep SD writable / ActiveSync.");}}
                        else if(!LaunchCompanion(id))wsprintfW(g_statusMsg,L"Unavailable on SD: %s",g_apps[id].name);
                    }
                }
                InvalidateRect(g_hWnd,NULL,FALSE);break;
            case PAGE_SYSTEM:
                switch (i) {
                case 0: /* Autonomous Boot Toggle */
                    g_bootTarget = (BootTarget)((g_bootTarget + 1) % BOOT_COUNT);
                    ApplyBootTarget(g_bootTarget);
                    wsprintfW(g_statusMsg, L"BOOT TARGET: %s (SAVED TO HKLM)", g_bootTargetNames[g_bootTarget]);
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    break;

                case 1: /* Toggle USB Storage Mode */
                    ToggleUsbMode();
                    break;

                case 2: /* Launch Original Vendor GPS UI */
                    wsprintfW(g_statusMsg, L"Restoring Vendor GPS UI (Launch.exe)...");
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    UpdateWindow(g_hWnd);
                    LaunchVendorUI();
                    break;

                case 3:
                    g_currentPage = PAGE_BLUETOOTH;
                    lstrcpyW(g_statusMsg, L"Bluetooth files: native client + OEM background backend");
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    break;

                case 4: /* Switch to Advanced Tools */
                    g_currentPage = PAGE_ADVANCED;
                    wsprintfW(g_statusMsg, L"Advanced Tools: Watchdog, Hardware Dump, Explorer, Volume");
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    break;

                case 5: /* Back to Main Hub */
                    g_currentPage = PAGE_MAIN;
                    wsprintfW(g_statusMsg, L"Mero OS Hub ready");
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    break;
                }
                break;

            case PAGE_BLUETOOTH:
                switch (i) {
                case 0:
                    {
                    if(MeroUsbSet(L"Serial_Class")&&MeroStartAgent(TRUE,TRUE)){
                        g_usbStartupReady=TRUE;g_usbMode=USB_MODE_ACTIVESYNC;
                        lstrcpyW(g_statusMsg,L"Agent connect requested // ActiveSync ready");
                    }
                    else wsprintfW(g_statusMsg,L"Agent start failed (%lu). Check ActiveSync and SD access.",GetLastError());
                    break;
                }
                case 1: BluetoothOemPower(1); break;
                case 2: BluetoothOemPower(0); break;
                case 3:
                    if(MeroBusLock()) {
                        DWORD age=(DWORD)(GetTickCount()-mero_bus->tick)/1000;
                        if(mero_bus->generation&&age<15)wsprintfW(g_statusMsg,L"Bluetooth agent LIVE // PC state %lus ago",age);
                        else lstrcpyW(g_statusMsg,L"No fresh PC state // tap CONNECT / AGENT");
                        ReleaseMutex(mero_bus_mutex);
                    }
                    break;
                case 4: BluetoothCaptureVendor(); break;
                case 5: g_currentPage = PAGE_SYSTEM; break;
                }
                InvalidateRect(g_hWnd, NULL, FALSE);
                break;

            case PAGE_ADVANCED:
            default:
                switch (i) {
                case 0: /* Kill Vendor Watchdog */
                    {
                        int killed = KillVendorWatchdog();
                        wsprintfW(g_statusMsg, L"Stopped %d vendor watchdog / launcher tasks", killed);
                        InvalidateRect(g_hWnd, NULL, FALSE);
                    }
                    break;

                case 1: /* Deep Hardware Discovery Dump */
                    wsprintfW(g_statusMsg, L"Dumping registry and file hierarchy...");
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    UpdateWindow(g_hWnd);
                    PerformSystemDump();
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    break;

                case 2: /* WinCE Explorer */
                    wsprintfW(g_statusMsg, L"Launching Windows CE Explorer...");
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    UpdateWindow(g_hWnd);
                    if (!LaunchApp(PATH_EXPLORER, FALSE)) {
                        wsprintfW(g_statusMsg, L"Explorer launch failed");
                        InvalidateRect(g_hWnd, NULL, FALSE);
                    }
                    break;

                case 3: /* Volume Toggle */
                    g_volumeLevel = (g_volumeLevel + 1) % 5;
                    SetMasterVolume(g_volumeLevel);
                    wsprintfW(g_statusMsg, L"Master audio volume set to %d%%", g_volumeLevel * 25);
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    break;

                case 4: /* Hardware Cold Reboot */
                    wsprintfW(g_statusMsg, L"Reboot triggered. Tap Power if battery is dead.");
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    UpdateWindow(g_hWnd);
                    HardwareReboot();
                    break;

                case 5: /* Back to System Tools */
                    g_currentPage = PAGE_SYSTEM;
                    wsprintfW(g_statusMsg, L"System Tools: USB Mode, Vendor GPS UI, Audio Volume");
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    break;
                }
                break;
            }
            return;
        }
    }
}

/* Window procedure */
static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(uMsg==RegisterWindowMessageW(L"MERO_SHELL_HOME_V1")){
        g_countdownActive=FALSE;g_currentPage=PAGE_MAIN;g_selectDefault=FALSE;
        lstrcpyW(g_statusMsg,L"Home // startup app paused");
        ShowWindow(hWnd,SW_SHOW);SetForegroundWindow(hWnd);InvalidateRect(hWnd,NULL,FALSE);return 0;
    }

    switch (uMsg) {
    case WM_CREATE:
        MeroLiveInit(hWnd, ShellLiveLine);
        g_hWnd = hWnd;
        HideVendorUI();
        UpdateSystemStatus();
        SetMasterVolume(g_volumeLevel);
        QueryCurrentBootTarget();
        QueryCurrentUsbMode();
        UpdateButtons();
        g_bootStarted=GetTickCount();g_agentAttempt=g_usbAttempt=g_bootStarted;
        g_usbStartupReady=MeroUsbSet(L"Serial_Class");
        if(g_usbStartupReady)g_usbMode=USB_MODE_ACTIVESYNC;
        LoadConfig();
        if(g_usbStartupReady)MeroStartAgent(FALSE,FALSE);
        if (GetFileAttributesW(L"\\SDMMC\\MERO\\launch_strings.txt") == 0xFFFFFFFF) {
            PerformSystemDump();
        }
        /* Internal recovery binaries are updated only by an explicit deployment. */

        SetTimer(hWnd, TIMER_ID_TICK, 1000, NULL);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER:
        if (wParam == MERO_LIVE_TIMER) { MeroLiveTick(hWnd); return 0; }
        if (wParam == TIMER_ID_TICK) {
            DWORD now=GetTickCount();BluetoothOemTick();
            if((DWORD)(now-g_bootStarted)<60000 && (DWORD)(now-g_agentAttempt)>=2000) {
                g_agentAttempt=now;
                if(!g_usbStartupReady){g_usbAttempt=now;g_usbStartupReady=MeroUsbSet(L"Serial_Class");if(g_usbStartupReady)g_usbMode=USB_MODE_ACTIVESYNC;}
                if(g_usbStartupReady)MeroStartAgent(FALSE,FALSE);
                if(!g_sdPrefsLoaded&&g_allowBootReload&&!g_returningHome&&GetFileAttributesW(CFG_FILE)!=INVALID_FILE_ATTRIBUTES)LoadConfig();
            }
        }
        if (mero_standby) return 0;
        if (wParam == TIMER_ID_TICK) {
            if (!IsWindowVisible(hWnd)) return 0;
            DWORD prevAvail = g_memAvailMB;
            int prevBat = g_batteryPercent;
            BOOL prevAC = g_isAC;
            UpdateSystemStatus();

            if (g_countdownActive) {
                if (g_countdownSeconds > 1) {
                    g_countdownSeconds--;
                    wsprintfW(g_statusMsg, L"AUTOLAUNCH: %s IN %d SEC... [TAP TO CANCEL]",
                              g_apps[g_pref].name, g_countdownSeconds);
                } else {
                    g_countdownActive = FALSE;
                    wsprintfW(g_statusMsg, L"Auto-launching %s...", g_apps[g_pref].name);
                    InvalidateRect(hWnd, NULL, FALSE);
                    UpdateWindow(hWnd);
                    if(!LaunchCompanion(g_pref)) {
                        if(++g_launchAttempts<15){g_countdownActive=TRUE;g_countdownSeconds=2;lstrcpyW(g_statusMsg,L"Waiting for startup app / SD; retrying...");}
                        else lstrcpyW(g_statusMsg,L"Startup app unavailable // shell remains usable");
                    }
                }
                InvalidateRect(hWnd, NULL, FALSE);
            } else if (g_memAvailMB != prevAvail || g_batteryPercent != prevBat || g_isAC != prevAC) {
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        return 0;

    case WM_LBUTTONDOWN:
        if(g_portrait)OnTouch(HIWORD(lParam),479-LOWORD(lParam));else OnTouch(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_PAINT:
        OnPaint(hWnd);
        return 0;

    case WM_DESTROY:
        MeroLiveClose(hWnd);
        KillTimer(hWnd, TIMER_ID_TICK);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

/* Entry point */
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPWSTR    lpCmdLine,
    int       nCmdShow)
{
    WNDCLASS wc;
    MSG msg;

    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    g_returningHome=lpCmdLine && wcsstr(lpCmdLine,L"--home");
    g_hInstance = hInstance;
    g_screenW = GetSystemMetrics(SM_CXSCREEN);
    g_screenH = GetSystemMetrics(SM_CYSCREEN);

    /* Chainloader: If running from ResidentFlash, hand off to SDMMC if present */
    {
        WCHAR myPath[MAX_PATH];
        GetModuleFileNameW(hInstance, myPath, MAX_PATH);
        if (wcsstr(myPath, L"ResidentFlash") || wcsstr(myPath, L"residentflash")) {
            if (GetFileAttributesW(L"\\SDMMC\\MERO\\mero-shell.exe") != 0xFFFFFFFF) {
                PROCESS_INFORMATION pi;
                memset(&pi, 0, sizeof(pi));
                if (CreateProcessW(L"\\SDMMC\\MERO\\mero-shell.exe", lpCmdLine, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    return 0; /* Handed off to SDMMC! */
                }
            }
        }
    }

    /* Single instance check: if shell already running (e.g. hidden for desktop), restore it */
    HWND hExisting = FindWindowW(L"MeroShellWndClass", L"Mero Shell");
    if (hExisting) {
        PostMessageW(hExisting, RegisterWindowMessageW(L"MERO_SHELL_HOME_V1"), 0, 0);
        ShowWindow(hExisting, SW_SHOWNORMAL);
        SetWindowPos(hExisting, HWND_TOPMOST, 0, 0, g_screenW, g_screenH, SWP_SHOWWINDOW);
        SetForegroundWindow(hExisting);
        InvalidateRect(hExisting, NULL, TRUE);
        UpdateWindow(hExisting);
        return 0;
    }

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"MeroShellWndClass";

    if (!RegisterClassW(&wc)) {
        return 1;
    }

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"MeroShellWndClass",
        L"Mero Shell",
        WS_VISIBLE | WS_POPUP,
        0, 0,
        g_screenW, g_screenH,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!g_hWnd) {
        return 2;
    }

    ShowWindow(g_hWnd, SW_SHOWNORMAL);
    UpdateWindow(g_hWnd);

    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
