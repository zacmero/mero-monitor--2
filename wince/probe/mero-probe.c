/*
 * mero-probe.c - Native Windows CE ARM hardware diagnostic probe
 * Project: mero-monitor-#2
 * Target: Foston FS-460BT (PE32 ARMv4 Windows CE)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <string.h>

#define PROBE_VERSION       L"0.1.0"
#define IDC_BTN_EXIT        1001

/* Target paths */
#define SD_LOG_DIR          L"\\SDMMC\\MERO"
#define SD_LOG_FILE         L"\\SDMMC\\MERO\\probe.txt"
#define LOCAL_LOG_FILE      L"probe.txt"

/* Global state */
static HINSTANCE g_hInstance = NULL;
static HWND      g_hWnd = NULL;
static HWND      g_hBtnExit = NULL;

static int       g_screenWidth = 0;
static int       g_screenHeight = 0;
static int       g_bitsPerPixel = 0;

static DWORD     g_osMajor = 0;
static DWORD     g_osMinor = 0;
static DWORD     g_osBuild = 0;
static WCHAR     g_osExtra[128] = { 0 };

static DWORD     g_memLoad = 0;
static DWORD     g_memTotalMB = 0;
static DWORD     g_memAvailMB = 0;

static int       g_touchCount = 0;
static int       g_lastTouchX = -1;
static int       g_lastTouchY = -1;
static WCHAR     g_logStatus[128] = L"Logging pending...";

/* Write diagnostic information to log file */
static void WriteDiagnosticLog(void)
{
    HANDLE hFile;
    DWORD bytesWritten;
    char buffer[1024];
    int len;

    /* Ensure target directory exists on SD card */
    CreateDirectoryW(SD_LOG_DIR, NULL);

    /* Try writing to \\SDMMC\\MERO\\probe.txt first */
    hFile = CreateFileW(
        SD_LOG_FILE,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        /* Fallback to local working directory */
        hFile = CreateFileW(
            LOCAL_LOG_FILE,
            GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        if (hFile == INVALID_HANDLE_VALUE) {
            wsprintfW(g_logStatus, L"Log write FAILED (error: %lu)", GetLastError());
            return;
        } else {
            wsprintfW(g_logStatus, L"Logged to local %s", LOCAL_LOG_FILE);
        }
    } else {
        wsprintfW(g_logStatus, L"Logged to %s", SD_LOG_FILE);
    }

    len = sprintf(
        buffer,
        "=== MERO PROBE v%ls ===\r\n"
        "Platform: Foston FS-460BT class / Windows CE\r\n"
        "TickCount: %lu ms\r\n"
        "\r\n"
        "[OS Version]\r\n"
        "Version: %lu.%lu (Build %lu)\r\n"
        "Extra: %ls\r\n"
        "\r\n"
        "[Display]\r\n"
        "Resolution: %d x %d\r\n"
        "Color Depth: %d bpp\r\n"
        "\r\n"
        "[Memory]\r\n"
        "Total Physical: %lu MB\r\n"
        "Avail Physical: %lu MB\r\n"
        "Memory Load: %lu%%\r\n"
        "\r\n"
        "Status: OK\r\n"
        "========================\r\n",
        PROBE_VERSION,
        GetTickCount(),
        g_osMajor, g_osMinor, g_osBuild,
        g_osExtra,
        g_screenWidth, g_screenHeight,
        g_bitsPerPixel,
        g_memTotalMB, g_memAvailMB,
        g_memLoad
    );

    WriteFile(hFile, buffer, (DWORD)len, &bytesWritten, NULL);
    CloseHandle(hFile);
}

/* Query system metrics */
static void CollectSystemMetrics(HWND hWnd)
{
    OSVERSIONINFO vi;
    MEMORYSTATUS ms;
    HDC hdc;

    /* Screen resolution */
    g_screenWidth = GetSystemMetrics(SM_CXSCREEN);
    g_screenHeight = GetSystemMetrics(SM_CYSCREEN);

    /* Display color depth */
    hdc = GetDC(hWnd);
    if (hdc) {
        g_bitsPerPixel = GetDeviceCaps(hdc, BITSPIXEL);
        ReleaseDC(hWnd, hdc);
    }

    /* OS Version */
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    if (GetVersionExW(&vi)) {
        g_osMajor = vi.dwMajorVersion;
        g_osMinor = vi.dwMinorVersion;
        g_osBuild = vi.dwBuildNumber;
        lstrcpynW(g_osExtra, vi.szCSDVersion, sizeof(g_osExtra)/sizeof(g_osExtra[0]));
    }

    /* Memory status */
    memset(&ms, 0, sizeof(ms));
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    g_memLoad = ms.dwMemoryLoad;
    g_memTotalMB = (DWORD)(ms.dwTotalPhys / (1024 * 1024));
    g_memAvailMB = (DWORD)(ms.dwAvailPhys / (1024 * 1024));
}

/* Paint diagnostic UI */
static void OnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rc;
    WCHAR buf[256];
    int y = 10;
    int lineHeight = 18;

    hdc = BeginPaint(hWnd, &ps);
    GetClientRect(hWnd, &rc);

    /* Background: pure black */
    FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(hdc, TRANSPARENT);

    /* Header title: Neon green */
    SetTextColor(hdc, RGB(0, 255, 128));
    wsprintfW(buf, L"MERO PROBE // FS-460BT [v%s]", PROBE_VERSION);
    ExtTextOutW(hdc, 16, y, 0, NULL, buf, lstrlenW(buf), NULL);
    y += lineHeight + 8;

    /* Divider line */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(40, 80, 50));
        HPEN hOld = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, 16, y, NULL);
        LineTo(hdc, g_screenWidth - 16, y);
        SelectObject(hdc, hOld);
        DeleteObject(hPen);
    }
    y += 10;

    /* Section: OS Information */
    SetTextColor(hdc, RGB(0, 200, 255));
    wsprintfW(buf, L"OS: Windows CE %lu.%lu (Build %lu) %s",
              g_osMajor, g_osMinor, g_osBuild, g_osExtra);
    ExtTextOutW(hdc, 16, y, 0, NULL, buf, lstrlenW(buf), NULL);
    y += lineHeight;

    /* Section: Display Metrics */
    SetTextColor(hdc, RGB(220, 220, 220));
    wsprintfW(buf, L"Display: %d x %d (%d bpp) [Target: 480x272 RGB565]",
              g_screenWidth, g_screenHeight, g_bitsPerPixel);
    ExtTextOutW(hdc, 16, y, 0, NULL, buf, lstrlenW(buf), NULL);
    y += lineHeight;

    /* Section: Memory Metrics */
    wsprintfW(buf, L"Memory: %lu MB total / %lu MB free (%lu%% used)",
              g_memTotalMB, g_memAvailMB, g_memLoad);
    ExtTextOutW(hdc, 16, y, 0, NULL, buf, lstrlenW(buf), NULL);
    y += lineHeight;

    /* Section: Touch Digitizer State */
    SetTextColor(hdc, RGB(255, 200, 50));
    if (g_touchCount > 0) {
        wsprintfW(buf, L"Touch: X=%03d  Y=%03d  (events: %d)",
                  g_lastTouchX, g_lastTouchY, g_touchCount);
    } else {
        wsprintfW(buf, L"Touch: TAP ANYWHERE TO TEST DIGITIZER");
    }
    ExtTextOutW(hdc, 16, y, 0, NULL, buf, lstrlenW(buf), NULL);
    y += lineHeight;

    /* Section: Log File Status */
    SetTextColor(hdc, RGB(160, 160, 160));
    wsprintfW(buf, L"Log: %s", g_logStatus);
    ExtTextOutW(hdc, 16, y, 0, NULL, buf, lstrlenW(buf), NULL);
    y += lineHeight + 6;

    /* Next Steps / Peripheral notice */
    SetTextColor(hdc, RGB(100, 140, 120));
    wsprintfW(buf, L"Next: COM1 GPS @ 9600 baud | ActiveSync ipaq->ttyUSB0");
    ExtTextOutW(hdc, 16, y, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Draw visual touch cursor marker */
    if (g_lastTouchX >= 0 && g_lastTouchY >= 0) {
        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 0));
        HPEN hOld = (HPEN)SelectObject(hdc, hPen);

        MoveToEx(hdc, g_lastTouchX - 12, g_lastTouchY, NULL);
        LineTo(hdc, g_lastTouchX + 13, g_lastTouchY);
        MoveToEx(hdc, g_lastTouchX, g_lastTouchY - 12, NULL);
        LineTo(hdc, g_lastTouchX, g_lastTouchY + 13);

        SelectObject(hdc, hOld);
        DeleteObject(hPen);
    }

    EndPaint(hWnd, &ps);
}

/* Window procedure */
static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        CollectSystemMetrics(hWnd);
        WriteDiagnosticLog();

        /* Create native Win32 exit button at top-right */
        g_hBtnExit = CreateWindowW(
            L"BUTTON",
            L"[ EXIT ]",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            g_screenWidth - 110,
            8,
            96,
            36,
            hWnd,
            (HMENU)IDC_BTN_EXIT,
            g_hInstance,
            NULL
        );
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BTN_EXIT) {
            DestroyWindow(hWnd);
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
    case WM_MOUSEMOVE:
        if (wParam & MK_LBUTTON) {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            g_lastTouchX = x;
            g_lastTouchY = y;
            g_touchCount++;

            /* Redundant touch check for exit button area in case button child window is bypassed */
            if (x >= (g_screenWidth - 120) && y <= 50) {
                DestroyWindow(hWnd);
                return 0;
            }

            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;

    case WM_PAINT:
        OnPaint(hWnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

/* Windows CE Entry Point */
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPWSTR    lpCmdLine,
    int       nCmdShow)
{
    WNDCLASS wc;
    MSG msg;
    int screenW, screenH;

    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    g_hInstance = hInstance;

    screenW = GetSystemMetrics(SM_CXSCREEN);
    screenH = GetSystemMetrics(SM_CYSCREEN);

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"MeroProbeWndClass";

    if (!RegisterClassW(&wc)) {
        return 1;
    }

    /* Create borderless fullscreen popup window covering entire 480x272 LCD */
    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"MeroProbeWndClass",
        L"Mero Probe",
        WS_VISIBLE | WS_POPUP,
        0, 0,
        screenW, screenH,
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
