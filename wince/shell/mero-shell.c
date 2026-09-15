/*
 * mero-shell.c - Custom native Windows CE launcher & OS shell
 * Project: mero-monitor-#2
 * Target: Foston FS-460BT (PE32 ARMv4 Windows CE 5.0)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <mmsystem.h>
#include <tchar.h>
#include <stdio.h>
#include <string.h>

#define SHELL_VERSION       L"0.2.5"
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
    AUTOLAUNCH_NONE = 0,
    AUTOLAUNCH_GALLERY = 1,
    AUTOLAUNCH_CMD = 2,
    AUTOLAUNCH_PROBE = 3,
    AUTOLAUNCH_COUNT = 4
} AutoLaunchPref;

static const WCHAR *g_prefNames[] = {
    L"NONE (Direct Shell)",
    L"MEDIA VISUALIZER",
    L"MERO CMD SHELL",
    L"HARDWARE PROBE"
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

static int            g_currentPage = 0; /* 0 = Mero Apps, 1 = System Tools */
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
    HANDLE hFile;
    DWORD bytesRead;
    char buffer[256];

    g_pref = AUTOLAUNCH_NONE;
    g_countdownSeconds = 0;
    g_countdownActive = FALSE;

    hFile = CreateFileW(
        CFG_FILE,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        memset(buffer, 0, sizeof(buffer));
        if (ReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            int val = 0;
            if (sscanf(buffer, "autolaunch=%d", &val) == 1) {
                if (val >= 0 && val < AUTOLAUNCH_COUNT) {
                    g_pref = (AutoLaunchPref)val;
                }
            }
        }
        CloseHandle(hFile);
    }

    if (g_pref != AUTOLAUNCH_NONE) {
        g_countdownSeconds = 3;
        g_countdownActive = TRUE;
        wsprintfW(g_statusMsg, L"AUTOLAUNCH: %s IN %d SEC... [TAP TO CANCEL]",
                  g_prefNames[g_pref], g_countdownSeconds);
    }
}

/* Save shell configuration */
static void SaveConfig(void)
{
    HANDLE hFile;
    DWORD bytesWritten;
    char buffer[128];
    int len;

    CreateDirectoryW(CFG_DIR, NULL);

    hFile = CreateFileW(
        CFG_FILE,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        len = sprintf(buffer, "autolaunch=%d\r\ntimeout=3\r\n", (int)g_pref);
        WriteFile(hFile, buffer, (DWORD)len, &bytesWritten, NULL);
        CloseHandle(hFile);
    }
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
    HWND hWndPhone = FindWindowW(NULL, L"ANW_PHONELINK");
    if (hWndPhone) {
        ShowWindow(hWndPhone, SW_HIDE);
        EnableWindow(hWndPhone, FALSE);
    }

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        pe.dwSize = sizeof(pe);
        if (Process32First(hSnap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"ANWDOG.exe") == 0 ||
                    _wcsicmp(pe.szExeFile, L"PhoneLink.exe") == 0 ||
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
static void CreateDesktopShortcuts(void)
{
    HANDLE hFile;
    DWORD written;
    const char *lnkShell = "34#\\ResidentFlash\\MERO\\mero-flash.exe";
    const char *lnkCmd   = "32#\\ResidentFlash\\MERO\\mero-cmd.exe";
    const char *lnkFlash = "34#\\ResidentFlash\\MERO\\mero-flash.exe";

    CreateDirectoryW(L"\\ResidentFlash\\MERO", NULL);
    CopyFileW(L"\\SDMMC\\MERO\\mero-shell.exe", L"\\ResidentFlash\\MERO\\mero-shell.exe", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\mero-cmd.exe", L"\\ResidentFlash\\MERO\\mero-cmd.exe", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\mero-flash.exe", L"\\ResidentFlash\\MERO\\mero-flash.exe", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\mero-shell.ico", L"\\ResidentFlash\\MERO\\mero-shell.ico", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\mero-cmd.ico", L"\\ResidentFlash\\MERO\\mero-cmd.ico", FALSE);
    CopyFileW(L"\\SDMMC\\MERO\\mero-flash.ico", L"\\ResidentFlash\\MERO\\mero-flash.ico", FALSE);

    CreateDirectoryW(L"\\Windows\\Desktop", NULL);

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

    hFile = CreateFileW(L"\\Windows\\Desktop\\Resident Shell.lnk", GENERIC_WRITE, FILE_SHARE_READ,
                        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, lnkFlash, (DWORD)strlen(lnkFlash), &written, NULL);
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

/* Apply new HKLM\init\Launch50 boot target and persist to flash */
static BOOL ApplyBootTarget(BootTarget target)
{
    HKEY hKey;
    const WCHAR *launchPath = NULL;
    BOOL ok = FALSE;

    g_bootTarget = target;

    switch (target) {
    case BOOT_FLASH:
        CreateDirectoryW(L"\\ResidentFlash\\MERO", NULL);
        CopyFileW(L"\\SDMMC\\MERO\\mero-shell.exe", L"\\ResidentFlash\\MERO\\mero-shell.exe", FALSE);
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

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"init", 0, 0, &hKey) == ERROR_SUCCESS) {
        DWORD len = (lstrlenW(launchPath) + 1) * sizeof(WCHAR);
        if (RegSetValueExW(hKey, L"Launch50", 0, REG_SZ, (const BYTE*)launchPath, len) == ERROR_SUCCESS) {
            RegFlushKey(HKEY_LOCAL_MACHINE);
            ok = TRUE;
        }
        RegCloseKey(hKey);
    }

    /* Registry persistence verification canary */
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Mero", 0, NULL, 0, 0, NULL, &hKey, NULL) == ERROR_SUCCESS) {
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
            wsprintfW(altPath, L"\\ResidentFlash\\MERO\\%s", path + 12);
            ret = CreateProcessW(altPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
        } else if (wcsstr(path, L"\\ResidentFlash\\MERO\\")) {
            WCHAR altPath[MAX_PATH];
            wsprintfW(altPath, L"\\SDMMC\\MERO\\%s", path + 20);
            ret = CreateProcessW(altPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
        }
    }
    if (ret) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        if (exitShell && g_hWnd) {
            /* Destroy shell cleanly so launched child owns the screen & RAM */
            DestroyWindow(g_hWnd);
        }
    }
    return ret;
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

    if (g_currentPage == 0) {
        /* PAGE 0: Mero Applications */
        lstrcpyW(g_buttons[0].title, L"[1] MEDIA VISUALIZER");
        lstrcpyW(g_buttons[0].sub,   L"Living Picture Frame & Gallery");
        g_buttons[0].color = RGB(0, 255, 128);

        lstrcpyW(g_buttons[1].title, L"[2] MERO CMD SHELL");
        lstrcpyW(g_buttons[1].sub,   L"Process Mgr & Interactive CLI");
        g_buttons[1].color = RGB(0, 220, 255);

        lstrcpyW(g_buttons[2].title, L"[3] SYSTEM PROBE");
        lstrcpyW(g_buttons[2].sub,   L"Hardware & Screen Diagnostics");
        g_buttons[2].color = RGB(255, 180, 0);

        lstrcpyW(g_buttons[3].title, L"[4] DEFAULT BOOT APP");
        lstrcpyW(g_buttons[3].sub,   g_prefNames[g_pref]);
        g_buttons[3].color = RGB(180, 140, 255);

        lstrcpyW(g_buttons[4].title, L"[5] SYSTEM TOOLS >>");
        lstrcpyW(g_buttons[4].sub,   L"Explorer, Volume, Discovery Dump");
        g_buttons[4].color = RGB(120, 200, 255);

        lstrcpyW(g_buttons[5].title, L"[6] EXIT TO DESKTOP");
        lstrcpyW(g_buttons[5].sub,   L"Leave Shell & Show WinCE Desktop");
        g_buttons[5].color = RGB(255, 80, 80);
    } else {
        /* PAGE 1: System Tools & Hardware Options */
        lstrcpyW(g_buttons[0].title, L"[1] AUTONOMOUS BOOT");
        lstrcpyW(g_buttons[0].sub,   g_bootTargetNames[g_bootTarget]);
        g_buttons[0].color = RGB(0, 255, 128);

        lstrcpyW(g_buttons[1].title, L"[2] VENDOR GPS UI");
        lstrcpyW(g_buttons[1].sub,   L"Launch Factory Foston Interface");
        g_buttons[1].color = RGB(255, 140, 40);

        {
            WCHAR volBuf[32];
            wsprintfW(volBuf, L"Master Level: %d%%", g_volumeLevel * 25);
            lstrcpyW(g_buttons[2].title, L"[3] VOLUME TOGGLE");
            lstrcpyW(g_buttons[2].sub, volBuf);
            g_buttons[2].color = RGB(255, 200, 40);
        }

        lstrcpyW(g_buttons[3].title, L"[4] KILL VENDOR DOG");
        lstrcpyW(g_buttons[3].sub,   L"Terminate ANWDOG & PhoneLink");
        g_buttons[3].color = RGB(255, 80, 80);

        lstrcpyW(g_buttons[4].title, L"[5] REBOOT DEVICE");
        lstrcpyW(g_buttons[4].sub,   L"Soft Reset (Power Button If Off)");
        g_buttons[4].color = RGB(255, 120, 80);

        lstrcpyW(g_buttons[5].title, L"[6] << BACK TO MAIN");
        lstrcpyW(g_buttons[5].sub,   L"Return to Mero Applications");
        g_buttons[5].color = RGB(160, 160, 160);
    }
}

/* Paint Custom Mero Shell (Double Buffered - Zero Blinking) */
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

    memDC = CreateCompatibleDC(hdc);
    memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    /* Solid Black Background */
    FillRect(memDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(memDC, TRANSPARENT);

    /* TOP STATUS BAR */
    SetTextColor(memDC, RGB(0, 255, 128));
    wsprintfW(buf, L"MERO // %s [v%s]",
              g_currentPage == 0 ? L"OS SHELL" : L"SYSTEM TOOLS",
              SHELL_VERSION);
    ExtTextOutW(memDC, 18, 8, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Power / Battery */
    if (g_batteryPercent >= 0) {
        SetTextColor(memDC, g_batteryPercent < 20 ? RGB(255, 80, 80) : RGB(255, 200, 40));
        wsprintfW(buf, L"%s %d%%", g_isAC ? L"[AC]" : L"[BAT]", g_batteryPercent);
    } else {
        SetTextColor(memDC, RGB(255, 200, 40));
        wsprintfW(buf, L"%s", g_isAC ? L"[AC ON]" : L"[BAT]");
    }
    ExtTextOutW(memDC, 230, 8, 0, NULL, buf, lstrlenW(buf), NULL);

    /* RAM Status */
    SetTextColor(memDC, RGB(0, 200, 255));
    wsprintfW(buf, L"RAM: %luM/%luM", g_memAvailMB, g_memTotalMB);
    ExtTextOutW(memDC, 340, 8, 0, NULL, buf, lstrlenW(buf), NULL);

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
        HPEN hPen = CreatePen(PS_SOLID, 1, g_buttons[i].color);
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
        MoveToEx(memDC, 18, 202, NULL);
        LineTo(memDC, g_screenW - 18, 202);
        SelectObject(memDC, hOld);
        DeleteObject(hPen);
    }

    if (g_countdownActive) {
        SetTextColor(memDC, RGB(255, 220, 40));
    } else {
        SetTextColor(memDC, RGB(140, 180, 160));
    }
    ExtTextOutW(memDC, 18, 212, 0, NULL, g_statusMsg, lstrlenW(g_statusMsg), NULL);

    /* Footer Hint */
    SetTextColor(memDC, RGB(90, 120, 100));
    wsprintfW(buf, L"Foston FS-460BT // WinCE 5.0 Core // SDMMC: Active // Page %d/2", g_currentPage + 1);
    ExtTextOutW(memDC, 18, 234, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Atomic BitBlt to display - zero tearing, zero blinking */
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hWnd, &ps);
}

/* Handle touch tap events */
static void OnTouch(int x, int y)
{
    int i;

    /* Any touch cancels countdown */
    if (g_countdownActive) {
        g_countdownActive = FALSE;
        wsprintfW(g_statusMsg, L"Auto-launch aborted. Shell ready.");
        InvalidateRect(g_hWnd, NULL, FALSE);
        return;
    }

    for (i = 0; i < 6; i++) {
        if (PtInRect(&g_buttons[i].rc, (POINT){ x, y })) {
            if (g_currentPage == 0) {
                /* PAGE 0 ACTIONS */
                switch (i) {
                case 0: /* Media Visualizer */
                    wsprintfW(g_statusMsg, L"Launching Media Visualizer...");
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    UpdateWindow(g_hWnd);
                    if (!LaunchApp(PATH_GALLERY, TRUE)) {
                        wsprintfW(g_statusMsg, L"Gallery binary not found: %s", PATH_GALLERY);
                        InvalidateRect(g_hWnd, NULL, FALSE);
                    }
                    break;

                case 1: /* Mero CMD Shell */
                    wsprintfW(g_statusMsg, L"Launching Mero CMD Shell...");
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    UpdateWindow(g_hWnd);
                    if (!LaunchApp(PATH_CMD, TRUE)) {
                        wsprintfW(g_statusMsg, L"CMD binary not found: %s", PATH_CMD);
                        InvalidateRect(g_hWnd, NULL, FALSE);
                    }
                    break;

                case 2: /* System Probe */
                    wsprintfW(g_statusMsg, L"Launching Hardware Probe...");
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    UpdateWindow(g_hWnd);
                    if (!LaunchApp(PATH_PROBE, TRUE)) {
                        wsprintfW(g_statusMsg, L"Probe not found: %s", PATH_PROBE);
                        InvalidateRect(g_hWnd, NULL, FALSE);
                    }
                    break;

                case 3: /* Cycle Boot Preference */
                    g_pref = (AutoLaunchPref)((g_pref + 1) % AUTOLAUNCH_COUNT);
                    SaveConfig();
                    wsprintfW(g_statusMsg, L"Default launch set to: %s", g_prefNames[g_pref]);
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    break;

                case 4: /* Switch to Page 1 (System Tools) */
                    g_currentPage = 1;
                    wsprintfW(g_statusMsg, L"System Tools: Explorer, Control Panel, Architecture Dump");
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    break;

                case 5: /* Exit to WinCE Desktop */
                    ShowDesktop();
                    DestroyWindow(g_hWnd);
                    break;
                }
            } else {
                /* PAGE 1 ACTIONS */
                switch (i) {
                case 0: /* Autonomous Boot Toggle */
                    g_bootTarget = (BootTarget)((g_bootTarget + 1) % BOOT_COUNT);
                    ApplyBootTarget(g_bootTarget);
                    wsprintfW(g_statusMsg, L"BOOT TARGET: %s (SAVED TO HKLM)", g_bootTargetNames[g_bootTarget]);
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    break;

                case 1: /* Launch Original Vendor UI */
                    wsprintfW(g_statusMsg, L"Launching Factory Vendor UI (Launch.exe)...");
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    UpdateWindow(g_hWnd);
                    LaunchVendorUI();
                    break;

                case 2: /* Volume Toggle */
                    g_volumeLevel = (g_volumeLevel + 1) % 5;
                    SetMasterVolume(g_volumeLevel);
                    wsprintfW(g_statusMsg, L"Master audio volume set to %d%%", g_volumeLevel * 25);
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    break;

                case 3: /* Kill Vendor Watchdog */
                    {
                        int killed = KillVendorWatchdog();
                        wsprintfW(g_statusMsg, L"Killed %d vendor daemons (ANWDOG/PhoneLink suppressed)", killed);
                        InvalidateRect(g_hWnd, NULL, FALSE);
                    }
                    break;

                case 4: /* Hardware Cold Reboot */
                    wsprintfW(g_statusMsg, L"Reboot triggered. Tap Power if battery is dead.");
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    UpdateWindow(g_hWnd);
                    HardwareReboot();
                    break;

                case 5: /* Back to Page 0 */
                    g_currentPage = 0;
                    wsprintfW(g_statusMsg, L"Mero Applications ready");
                    InvalidateRect(g_hWnd, NULL, FALSE);
                    break;
                }
            }
            return;
        }
    }
}

/* Window procedure */
static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        g_hWnd = hWnd;
        HideVendorUI();
        UpdateSystemStatus();
        SetMasterVolume(g_volumeLevel);
        QueryCurrentBootTarget();
        UpdateButtons();
        LoadConfig();
        if (GetFileAttributesW(L"\\SDMMC\\MERO\\launch_strings.txt") == 0xFFFFFFFF) {
            PerformSystemDump();
        }
        /* Auto-sync: If running from SDMMC, keep ResidentFlash updated in background */
        {
            WCHAR myPath[MAX_PATH];
            GetModuleFileNameW(g_hInstance, myPath, MAX_PATH);
            if (wcsstr(myPath, L"SDMMC") || wcsstr(myPath, L"sdmmc")) {
                CreateDirectoryW(L"\\ResidentFlash\\MERO", NULL);
                CopyFileW(L"\\SDMMC\\MERO\\mero-shell.exe", L"\\ResidentFlash\\MERO\\mero-shell.exe", FALSE);
                CopyFileW(L"\\SDMMC\\MERO\\mero-cmd.exe", L"\\ResidentFlash\\MERO\\mero-cmd.exe", FALSE);
                CopyFileW(L"\\SDMMC\\MERO\\mero-gallery.exe", L"\\ResidentFlash\\MERO\\mero-gallery.exe", FALSE);
                CopyFileW(L"\\SDMMC\\MERO\\mero-flash.exe", L"\\ResidentFlash\\MERO\\mero-flash.exe", FALSE);
                CopyFileW(L"\\SDMMC\\MERO\\mero-shell.ico", L"\\ResidentFlash\\MERO\\mero-shell.ico", FALSE);
                CopyFileW(L"\\SDMMC\\MERO\\mero-cmd.ico", L"\\ResidentFlash\\MERO\\mero-cmd.ico", FALSE);
                CopyFileW(L"\\SDMMC\\MERO\\mero-gallery.ico", L"\\ResidentFlash\\MERO\\mero-gallery.ico", FALSE);
                CopyFileW(L"\\SDMMC\\MERO\\mero-flash.ico", L"\\ResidentFlash\\MERO\\mero-flash.ico", FALSE);

                /* Mirror Stream folder to ResidentFlash for USB/offline resilience */
                CreateDirectoryW(L"\\ResidentFlash\\Stream", NULL);
                {
                    WIN32_FIND_DATAW wfdStr;
                    HANDLE hFStr = FindFirstFileW(L"\\SDMMC\\Stream\\*.*", &wfdStr);
                    if (hFStr != INVALID_HANDLE_VALUE) {
                        do {
                            if (!(wfdStr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                                WCHAR sPath[MAX_PATH], dPath[MAX_PATH];
                                wsprintfW(sPath, L"\\SDMMC\\Stream\\%s", wfdStr.cFileName);
                                wsprintfW(dPath, L"\\ResidentFlash\\Stream\\%s", wfdStr.cFileName);
                                CopyFileW(sPath, dPath, FALSE);
                            }
                        } while (FindNextFileW(hFStr, &wfdStr));
                        FindClose(hFStr);
                    }
                }
            }
        }
        SetTimer(hWnd, TIMER_ID_TICK, 1000, NULL);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER:
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
                              g_prefNames[g_pref], g_countdownSeconds);
                } else {
                    g_countdownActive = FALSE;
                    wsprintfW(g_statusMsg, L"Auto-launching %s...", g_prefNames[g_pref]);
                    InvalidateRect(hWnd, NULL, FALSE);
                    UpdateWindow(hWnd);
                    /* Execute preferred */
                    if (g_pref == AUTOLAUNCH_GALLERY) LaunchApp(PATH_GALLERY, TRUE);
                    else if (g_pref == AUTOLAUNCH_CMD) LaunchApp(PATH_CMD, TRUE);
                    else if (g_pref == AUTOLAUNCH_PROBE) LaunchApp(PATH_PROBE, TRUE);
                }
                InvalidateRect(hWnd, NULL, FALSE);
            } else if (g_memAvailMB != prevAvail || g_batteryPercent != prevBat || g_isAC != prevAC) {
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        return 0;

    case WM_LBUTTONDOWN:
        OnTouch(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_PAINT:
        OnPaint(hWnd);
        return 0;

    case WM_DESTROY:
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
                if (CreateProcessW(L"\\SDMMC\\MERO\\mero-shell.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
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
