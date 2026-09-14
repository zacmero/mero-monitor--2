/*
 * mero-shell.c - Custom native Windows CE launcher & OS shell
 * Project: mero-monitor-#2
 * Target: Foston FS-460BT (PE32 ARMv4 Windows CE 5.0)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <string.h>

#define SHELL_VERSION       L"0.1.0"
#define TIMER_ID_TICK       1
#define TIMER_ID_COUNTDOWN  2

#define CFG_DIR             L"\\SDMMC\\MERO"
#define CFG_FILE            L"\\SDMMC\\MERO\\shell.cfg"

#define PATH_TERMINAL       L"\\SDMMC\\MERO\\mero-terminal.exe"
#define PATH_PROBE          L"\\SDMMC\\MERO\\mero-probe.exe"
#define PATH_GPS            L"\\SDMMC\\MERO\\mero-gps.exe"
#define PATH_IGO8           L"\\SDMMC\\IGO8\\iGO8.exe"

typedef enum {
    AUTOLAUNCH_NONE = 0,
    AUTOLAUNCH_TERMINAL = 1,
    AUTOLAUNCH_GPS = 2,
    AUTOLAUNCH_PROBE = 3,
    AUTOLAUNCH_COUNT = 4
} AutoLaunchPref;

static const WCHAR *g_prefNames[] = {
    L"NONE (Direct Shell)",
    L"THOUGHT TERMINAL",
    L"GPS MONITOR",
    L"HARDWARE PROBE"
};

typedef struct {
    RECT rc;
    const WCHAR *title;
    const WCHAR *sub;
    COLORREF color;
} ShellButton;

static HINSTANCE      g_hInstance = NULL;
static HWND           g_hWnd = NULL;
static int            g_screenW = 480;
static int            g_screenH = 272;

static AutoLaunchPref g_pref = AUTOLAUNCH_NONE;
static int            g_countdownSeconds = 0;
static BOOL           g_countdownActive = FALSE;
static WCHAR          g_statusMsg[128] = L"READY // TAP A MODULE TO LAUNCH";

static DWORD          g_memAvailMB = 0;
static DWORD          g_memTotalMB = 0;
static int            g_batteryPercent = -1;
static BOOL           g_isAC = FALSE;

static ShellButton g_buttons[6];

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

/* Launch an external application */
static BOOL LaunchApp(const WCHAR *path)
{
    PROCESS_INFORMATION pi;
    BOOL ret;

    memset(&pi, 0, sizeof(pi));
    ret = CreateProcessW(path, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
    if (ret) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return ret;
}

/* Execute preferred application */
static void ExecutePreferredApp(void)
{
    const WCHAR *target = NULL;

    switch (g_pref) {
    case AUTOLAUNCH_TERMINAL:
        target = PATH_TERMINAL;
        break;
    case AUTOLAUNCH_GPS:
        target = PATH_GPS;
        break;
    case AUTOLAUNCH_PROBE:
        target = PATH_PROBE;
        break;
    default:
        return;
    }

    if (!LaunchApp(target)) {
        /* If specific app failed or not found, notify user */
        wsprintfW(g_statusMsg, L"ERR: Cannot execute %s", target);
        g_countdownActive = FALSE;
        InvalidateRect(g_hWnd, NULL, FALSE);
    }
}

/* Layout button geometry */
static void InitButtons(void)
{
    int colW = 215;
    int rowH = 46;
    int xLeft = 18;
    int xRight = 247;
    int y0 = 34;
    int spacing = 8;

    /* Left column */
    /* Button 0: Thought Terminal */
    SetRect(&g_buttons[0].rc, xLeft, y0, xLeft + colW, y0 + rowH);
    g_buttons[0].title = L"[1] THOUGHT TERMINAL";
    g_buttons[0].sub   = L"Mero Mind / Artifact Stream";
    g_buttons[0].color = RGB(0, 255, 128);

    /* Button 1: GPS Monitor */
    SetRect(&g_buttons[1].rc, xLeft, y0 + (rowH + spacing), xLeft + colW, y0 + (rowH + spacing) + rowH);
    g_buttons[1].title = L"[2] GPS SENSOR";
    g_buttons[1].sub   = L"COM1: 9600 Baud NMEA Stream";
    g_buttons[1].color = RGB(0, 220, 255);

    /* Button 2: Hardware Probe */
    SetRect(&g_buttons[2].rc, xLeft, y0 + (rowH + spacing)*2, xLeft + colW, y0 + (rowH + spacing)*2 + rowH);
    g_buttons[2].title = L"[3] SYSTEM PROBE";
    g_buttons[2].sub   = L"Hardware & Screen Diagnostics";
    g_buttons[2].color = RGB(255, 180, 0);

    /* Right column */
    /* Button 3: Boot Preference */
    SetRect(&g_buttons[3].rc, xRight, y0, xRight + colW, y0 + rowH);
    g_buttons[3].title = L"[4] DEFAULT APP";
    g_buttons[3].sub   = g_prefNames[g_pref];
    g_buttons[3].color = RGB(180, 140, 255);

    /* Button 4: Vendor Navigation */
    SetRect(&g_buttons[4].rc, xRight, y0 + (rowH + spacing), xRight + colW, y0 + (rowH + spacing) + rowH);
    g_buttons[4].title = L"[5] FACTORY IGO8";
    g_buttons[4].sub   = L"Original Navigation System";
    g_buttons[4].color = RGB(140, 180, 160);

    /* Button 5: Exit to WinCE */
    SetRect(&g_buttons[5].rc, xRight, y0 + (rowH + spacing)*2, xRight + colW, y0 + (rowH + spacing)*2 + rowH);
    g_buttons[5].title = L"[6] EXIT TO WINCE";
    g_buttons[5].sub   = L"Return to Device Desktop";
    g_buttons[5].color = RGB(255, 80, 80);
}

/* Paint Custom Mero Shell */
static void OnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rc;
    WCHAR buf[256];
    int i;

    hdc = BeginPaint(hWnd, &ps);
    GetClientRect(hWnd, &rc);

    /* Solid Black Background */
    FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(hdc, TRANSPARENT);

    /* TOP STATUS BAR */
    SetTextColor(hdc, RGB(0, 255, 128));
    wsprintfW(buf, L"MERO // OS SHELL [v%s]", SHELL_VERSION);
    ExtTextOutW(hdc, 18, 8, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Power / Battery */
    if (g_batteryPercent >= 0) {
        SetTextColor(hdc, g_batteryPercent < 20 ? RGB(255, 80, 80) : RGB(255, 200, 40));
        wsprintfW(buf, L"%s %d%%", g_isAC ? L"[AC]" : L"[BAT]", g_batteryPercent);
    } else {
        SetTextColor(hdc, RGB(255, 200, 40));
        wsprintfW(buf, L"%s", g_isAC ? L"[AC ON]" : L"[BAT]");
    }
    ExtTextOutW(hdc, 230, 8, 0, NULL, buf, lstrlenW(buf), NULL);

    /* RAM Status */
    SetTextColor(hdc, RGB(0, 200, 255));
    wsprintfW(buf, L"RAM: %luM/%luM", g_memAvailMB, g_memTotalMB);
    ExtTextOutW(hdc, 340, 8, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Header Divider Line */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(40, 90, 60));
        HPEN hOld = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, 18, 26, NULL);
        LineTo(hdc, g_screenW - 18, 26);
        SelectObject(hdc, hOld);
        DeleteObject(hPen);
    }

    /* Update button subtitle for preference */
    g_buttons[3].sub = g_prefNames[g_pref];

    /* DRAW APPLICATION BUTTONS */
    for (i = 0; i < 6; i++) {
        RECT btnRc = g_buttons[i].rc;
        HPEN hPen = CreatePen(PS_SOLID, 1, g_buttons[i].color);
        HPEN hOld = (HPEN)SelectObject(hdc, hPen);

        /* Draw bounding box */
        MoveToEx(hdc, btnRc.left, btnRc.top, NULL);
        LineTo(hdc, btnRc.right, btnRc.top);
        LineTo(hdc, btnRc.right, btnRc.bottom);
        LineTo(hdc, btnRc.left, btnRc.bottom);
        LineTo(hdc, btnRc.left, btnRc.top);

        SelectObject(hdc, hOld);
        DeleteObject(hPen);

        /* Button Title */
        SetTextColor(hdc, g_buttons[i].color);
        ExtTextOutW(hdc, btnRc.left + 10, btnRc.top + 7, 0, NULL,
                   g_buttons[i].title, lstrlenW(g_buttons[i].title), NULL);

        /* Button Subtitle */
        SetTextColor(hdc, RGB(180, 180, 180));
        ExtTextOutW(hdc, btnRc.left + 10, btnRc.top + 25, 0, NULL,
                   g_buttons[i].sub, lstrlenW(g_buttons[i].sub), NULL);
    }

    /* FOOTER STATUS BAR */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(30, 60, 40));
        HPEN hOld = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, 18, 202, NULL);
        LineTo(hdc, g_screenW - 18, 202);
        SelectObject(hdc, hOld);
        DeleteObject(hPen);
    }

    if (g_countdownActive) {
        SetTextColor(hdc, RGB(255, 220, 40));
    } else {
        SetTextColor(hdc, RGB(140, 180, 160));
    }
    ExtTextOutW(hdc, 18, 212, 0, NULL, g_statusMsg, lstrlenW(g_statusMsg), NULL);

    /* Footer Hint */
    SetTextColor(hdc, RGB(90, 120, 100));
    wsprintfW(buf, L"Foston FS-460BT // WinCE 5.0 Core // SDMMC: Active");
    ExtTextOutW(hdc, 18, 234, 0, NULL, buf, lstrlenW(buf), NULL);

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
            switch (i) {
            case 0: /* Thought Terminal */
                wsprintfW(g_statusMsg, L"Launching Thought Terminal...");
                InvalidateRect(g_hWnd, NULL, FALSE);
                UpdateWindow(g_hWnd);
                if (!LaunchApp(PATH_TERMINAL)) {
                    wsprintfW(g_statusMsg, L"Terminal binary pending: %s", PATH_TERMINAL);
                    InvalidateRect(g_hWnd, NULL, FALSE);
                }
                break;

            case 1: /* GPS Monitor */
                wsprintfW(g_statusMsg, L"Launching GPS Monitor...");
                InvalidateRect(g_hWnd, NULL, FALSE);
                UpdateWindow(g_hWnd);
                if (!LaunchApp(PATH_GPS)) {
                    wsprintfW(g_statusMsg, L"GPS monitor binary pending: %s", PATH_GPS);
                    InvalidateRect(g_hWnd, NULL, FALSE);
                }
                break;

            case 2: /* System Probe */
                wsprintfW(g_statusMsg, L"Launching Hardware Probe...");
                InvalidateRect(g_hWnd, NULL, FALSE);
                UpdateWindow(g_hWnd);
                if (!LaunchApp(PATH_PROBE)) {
                    wsprintfW(g_statusMsg, L"Probe not found at: %s", PATH_PROBE);
                    InvalidateRect(g_hWnd, NULL, FALSE);
                }
                break;

            case 3: /* Cycle Boot Preference */
                g_pref = (AutoLaunchPref)((g_pref + 1) % AUTOLAUNCH_COUNT);
                SaveConfig();
                wsprintfW(g_statusMsg, L"Default launch set to: %s", g_prefNames[g_pref]);
                InvalidateRect(g_hWnd, NULL, FALSE);
                break;

            case 4: /* Vendor Navigation */
                wsprintfW(g_statusMsg, L"Launching original iGO8...");
                InvalidateRect(g_hWnd, NULL, FALSE);
                UpdateWindow(g_hWnd);
                if (!LaunchApp(PATH_IGO8)) {
                    wsprintfW(g_statusMsg, L"Original iGO8 not found at: %s", PATH_IGO8);
                    InvalidateRect(g_hWnd, NULL, FALSE);
                }
                break;

            case 5: /* Exit to WinCE */
                DestroyWindow(g_hWnd);
                break;
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
        UpdateSystemStatus();
        InitButtons();
        LoadConfig();
        SetTimer(hWnd, TIMER_ID_TICK, 1000, NULL);
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_ID_TICK) {
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
                    ExecutePreferredApp();
                }
            }
            InvalidateRect(hWnd, NULL, FALSE);
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

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
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
