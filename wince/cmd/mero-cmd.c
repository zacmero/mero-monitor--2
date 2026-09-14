/*
 * mero-cmd.c - Native interactive command processor & process manager
 * Project: mero-monitor-#2
 * Target: Foston FS-460BT (PE32 ARMv4 Windows CE 5.0)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <stdio.h>
#include <string.h>

#define CMD_VERSION         L"0.1.0"
#define MAX_LINES           120
#define LINE_LEN            128
#define LINES_PER_SCREEN    11

typedef struct {
    RECT rc;
    const WCHAR *label;
    int cmdId;
    COLORREF color;
} DockButton;

enum {
    CMD_ID_LS = 1,
    CMD_ID_PWD,
    CMD_ID_MEM,
    CMD_ID_PS,
    CMD_ID_KILL_LAUNCH,
    CMD_ID_REG_INIT,
    CMD_ID_CAT_DUMP,
    CMD_ID_CAT_PROBE,
    CMD_ID_EXPLORER,
    CMD_ID_CONTROL,
    CMD_ID_CLEAR,
    CMD_ID_EXIT,
    CMD_ID_SCROLL_UP,
    CMD_ID_SCROLL_DN
};

static HINSTANCE g_hInstance = NULL;
static HWND      g_hWnd = NULL;
static int       g_screenW = 480;
static int       g_screenH = 272;

static WCHAR     g_history[MAX_LINES][LINE_LEN];
static int       g_historyCount = 0;
static int       g_scrollOffset = 0;

static WCHAR     g_currentDir[MAX_PATH] = L"\\SDMMC\\MERO";
static DockButton g_dockBtns[14];

/* Append line to terminal buffer */
static void TermPrint(const WCHAR *text)
{
    if (g_historyCount < MAX_LINES) {
        lstrcpynW(g_history[g_historyCount], text, LINE_LEN);
        g_historyCount++;
    } else {
        /* Shift buffer up */
        int i;
        for (i = 0; i < MAX_LINES - 1; i++) {
            memcpy(g_history[i], g_history[i+1], sizeof(g_history[0]));
        }
        lstrcpynW(g_history[MAX_LINES - 1], text, LINE_LEN);
    }

    /* Auto scroll to bottom */
    if (g_historyCount > LINES_PER_SCREEN) {
        g_scrollOffset = g_historyCount - LINES_PER_SCREEN;
    } else {
        g_scrollOffset = 0;
    }

    if (g_hWnd) {
        InvalidateRect(g_hWnd, NULL, FALSE);
    }
}

/* Clear buffer */
static void TermClear(void)
{
    g_historyCount = 0;
    g_scrollOffset = 0;
    TermPrint(L"MERO COMMAND PROCESSOR [v" CMD_VERSION L"]");
    TermPrint(L"Type or tap quick commands below. Ready.");
    TermPrint(L"------------------------------------------------");
}

/* Command: mem */
static void CmdMem(void)
{
    MEMORYSTATUS ms;
    WCHAR buf[128];
    memset(&ms, 0, sizeof(ms));
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);

    TermPrint(L"[MEMORY]");
    wsprintfW(buf, L"  RAM Total: %lu MB | Free: %lu MB (%lu%% load)",
              (DWORD)(ms.dwTotalPhys / (1024 * 1024)),
              (DWORD)(ms.dwAvailPhys / (1024 * 1024)),
              ms.dwMemoryLoad);
    TermPrint(buf);
}

/* Command: pwd */
static void CmdPwd(void)
{
    WCHAR buf[MAX_PATH + 16];
    wsprintfW(buf, L"[PWD] %s", g_currentDir);
    TermPrint(buf);
}

/* Command: ls */
static void CmdLs(void)
{
    WIN32_FIND_DATAW wfd;
    HANDLE hFind;
    WCHAR pattern[MAX_PATH + 8];
    WCHAR line[128];
    int count = 0;

    wsprintfW(pattern, L"%s\\*.*", g_currentDir);
    TermPrint(L"[LS] Directory contents:");

    hFind = FindFirstFileW(pattern, &wfd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                wsprintfW(line, L"  <DIR> %s", wfd.cFileName);
            } else {
                DWORD sizeKB = (wfd.nFileSizeLow + 1023) / 1024;
                wsprintfW(line, L"  %6luK  %s", sizeKB, wfd.cFileName);
            }
            TermPrint(line);
            count++;
        } while (FindNextFileW(hFind, &wfd));
        FindClose(hFind);
    } else {
        TermPrint(L"  (Empty or access denied)");
    }
    wsprintfW(line, L"  Total items: %d", count);
    TermPrint(line);
}

/* Command: ps */
static void CmdPs(void)
{
    HANDLE hSnap;
    PROCESSENTRY32 pe;
    WCHAR line[128];
    int count = 0;

    TermPrint(L"[PROCESS TABLE]");
    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        TermPrint(L"  [!] Failed to create toolhelp snapshot");
        return;
    }

    pe.dwSize = sizeof(pe);
    if (Process32First(hSnap, &pe)) {
        do {
            wsprintfW(line, L"  PID: 0x%08X  %s", pe.th32ProcessID, pe.szExeFile);
            TermPrint(line);
            count++;
        } while (Process32Next(hSnap, &pe));
    }
    CloseHandle(hSnap);

    wsprintfW(line, L"  Active processes: %d", count);
    TermPrint(line);
}

/* Command: kill Launch.exe */
static void CmdKillLaunch(void)
{
    HANDLE hSnap;
    PROCESSENTRY32 pe;
    BOOL found = FALSE;

    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        TermPrint(L"[KILL] Failed to create process snapshot");
        return;
    }

    pe.dwSize = sizeof(pe);
    if (Process32First(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"Launch.exe") == 0 ||
                _wcsicmp(pe.szExeFile, L"Main.exe") == 0) {
                HANDLE hProc = OpenProcess(0, FALSE, pe.th32ProcessID);
                WCHAR line[128];
                if (hProc) {
                    TerminateProcess(hProc, 0);
                    CloseHandle(hProc);
                    wsprintfW(line, L"[KILL] Terminated %s (PID 0x%08X)!",
                              pe.szExeFile, pe.th32ProcessID);
                    TermPrint(line);
                } else {
                    wsprintfW(line, L"[KILL] Cannot open process PID 0x%08X", pe.th32ProcessID);
                    TermPrint(line);
                }
                found = TRUE;
                break;
            }
        } while (Process32Next(hSnap, &pe));
    }
    CloseHandle(hSnap);

    if (!found) {
        TermPrint(L"[KILL] Launch.exe / Main.exe not found in process table");
    }
}

/* Command: cat file */
static void CmdCat(const WCHAR *path)
{
    HANDLE hFile;
    char buffer[1024];
    DWORD bytesRead;
    WCHAR line[128];
    char *p, *lineStart;

    hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        wsprintfW(line, L"[CAT] File not found: %s", path);
        TermPrint(line);
        return;
    }

    wsprintfW(line, L"[CAT] Viewing %s:", path);
    TermPrint(line);

    while (ReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        lineStart = buffer;
        for (p = buffer; *p; p++) {
            if (*p == '\n') {
                *p = '\0';
                if (p > buffer && *(p-1) == '\r') *(p-1) = '\0';
                MultiByteToWideChar(CP_ACP, 0, lineStart, -1, line, sizeof(line)/sizeof(line[0]));
                TermPrint(line);
                lineStart = p + 1;
            }
        }
        if (*lineStart) {
            MultiByteToWideChar(CP_ACP, 0, lineStart, -1, line, sizeof(line)/sizeof(line[0]));
            TermPrint(line);
        }
    }
    CloseHandle(hFile);
}

/* Command: reg init */
static void CmdRegInit(void)
{
    HKEY hKey;
    WCHAR line[128];

    TermPrint(L"[REGISTRY] HKLM\\init startup entries:");
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"init", 0, 0, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        WCHAR valName[128];
        BYTE data[256];
        DWORD valLen, dataLen, type;

        while (1) {
            valLen = sizeof(valName) / sizeof(valName[0]);
            dataLen = sizeof(data);
            if (RegEnumValueW(hKey, index++, valName, &valLen, NULL, &type, data, &dataLen) != ERROR_SUCCESS) {
                break;
            }
            if (type == REG_SZ) {
                wsprintfW(line, L"  %s = \"%s\"", valName, (WCHAR*)data);
                TermPrint(line);
            }
        }
        RegCloseKey(hKey);
    } else {
        TermPrint(L"  [!] Failed to open HKLM\\init");
    }
}

/* Initialize touch dock buttons */
static void InitDockButtons(void)
{
    int btnW = 75;
    int btnH = 28;
    int x0 = 10;
    int y1 = 206;
    int y2 = 238;
    int spacing = 3;

    /* Row 1 */
    SetRect(&g_dockBtns[0].rc, x0 + (btnW+spacing)*0, y1, x0 + (btnW+spacing)*0 + btnW, y1 + btnH);
    g_dockBtns[0].label = L"ls";
    g_dockBtns[0].cmdId = CMD_ID_LS;
    g_dockBtns[0].color = RGB(0, 255, 128);

    SetRect(&g_dockBtns[1].rc, x0 + (btnW+spacing)*1, y1, x0 + (btnW+spacing)*1 + btnW, y1 + btnH);
    g_dockBtns[1].label = L"pwd";
    g_dockBtns[1].cmdId = CMD_ID_PWD;
    g_dockBtns[1].color = RGB(0, 220, 255);

    SetRect(&g_dockBtns[2].rc, x0 + (btnW+spacing)*2, y1, x0 + (btnW+spacing)*2 + btnW, y1 + btnH);
    g_dockBtns[2].label = L"mem";
    g_dockBtns[2].cmdId = CMD_ID_MEM;
    g_dockBtns[2].color = RGB(0, 220, 255);

    SetRect(&g_dockBtns[3].rc, x0 + (btnW+spacing)*3, y1, x0 + (btnW+spacing)*3 + btnW, y1 + btnH);
    g_dockBtns[3].label = L"ps";
    g_dockBtns[3].cmdId = CMD_ID_PS;
    g_dockBtns[3].color = RGB(255, 200, 40);

    SetRect(&g_dockBtns[4].rc, x0 + (btnW+spacing)*4, y1, x0 + (btnW+spacing)*4 + btnW, y1 + btnH);
    g_dockBtns[4].label = L"kill UI";
    g_dockBtns[4].cmdId = CMD_ID_KILL_LAUNCH;
    g_dockBtns[4].color = RGB(255, 80, 80);

    SetRect(&g_dockBtns[5].rc, x0 + (btnW+spacing)*5, y1, x0 + (btnW+spacing)*5 + btnW, y1 + btnH);
    g_dockBtns[5].label = L"reg init";
    g_dockBtns[5].cmdId = CMD_ID_REG_INIT;
    g_dockBtns[5].color = RGB(180, 140, 255);

    /* Row 2 */
    SetRect(&g_dockBtns[6].rc, x0 + (btnW+spacing)*0, y2, x0 + (btnW+spacing)*0 + btnW, y2 + btnH);
    g_dockBtns[6].label = L"cat dump";
    g_dockBtns[6].cmdId = CMD_ID_CAT_DUMP;
    g_dockBtns[6].color = RGB(255, 140, 200);

    SetRect(&g_dockBtns[7].rc, x0 + (btnW+spacing)*1, y2, x0 + (btnW+spacing)*1 + btnW, y2 + btnH);
    g_dockBtns[7].label = L"cat probe";
    g_dockBtns[7].cmdId = CMD_ID_CAT_PROBE;
    g_dockBtns[7].color = RGB(255, 140, 200);

    SetRect(&g_dockBtns[8].rc, x0 + (btnW+spacing)*2, y2, x0 + (btnW+spacing)*2 + btnW, y2 + btnH);
    g_dockBtns[8].label = L"explorer";
    g_dockBtns[8].cmdId = CMD_ID_EXPLORER;
    g_dockBtns[8].color = RGB(100, 200, 255);

    SetRect(&g_dockBtns[9].rc, x0 + (btnW+spacing)*3, y2, x0 + (btnW+spacing)*3 + btnW, y2 + btnH);
    g_dockBtns[9].label = L"control";
    g_dockBtns[9].cmdId = CMD_ID_CONTROL;
    g_dockBtns[9].color = RGB(100, 200, 255);

    SetRect(&g_dockBtns[10].rc, x0 + (btnW+spacing)*4, y2, x0 + (btnW+spacing)*4 + btnW, y2 + btnH);
    g_dockBtns[10].label = L"clear";
    g_dockBtns[10].cmdId = CMD_ID_CLEAR;
    g_dockBtns[10].color = RGB(160, 160, 160);

    SetRect(&g_dockBtns[11].rc, x0 + (btnW+spacing)*5, y2, x0 + (btnW+spacing)*5 + btnW, y2 + btnH);
    g_dockBtns[11].label = L"EXIT";
    g_dockBtns[11].cmdId = CMD_ID_EXIT;
    g_dockBtns[11].color = RGB(255, 60, 60);

    /* Header scroll buttons */
    SetRect(&g_dockBtns[12].rc, 390, 2, 430, 20);
    g_dockBtns[12].label = L"UP";
    g_dockBtns[12].cmdId = CMD_ID_SCROLL_UP;
    g_dockBtns[12].color = RGB(160, 180, 200);

    SetRect(&g_dockBtns[13].rc, 435, 2, 475, 20);
    g_dockBtns[13].label = L"DN";
    g_dockBtns[13].cmdId = CMD_ID_SCROLL_DN;
    g_dockBtns[13].color = RGB(160, 180, 200);
}

/* Paint Terminal */
static void OnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rc;
    WCHAR buf[128];
    int i;
    int y;

    hdc = BeginPaint(hWnd, &ps);
    GetClientRect(hWnd, &rc);

    FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(hdc, TRANSPARENT);

    /* Header Bar */
    SetTextColor(hdc, RGB(0, 255, 128));
    wsprintfW(buf, L"MERO // CMD [v%s]", CMD_VERSION);
    ExtTextOutW(hdc, 10, 4, 0, NULL, buf, lstrlenW(buf), NULL);

    SetTextColor(hdc, RGB(160, 160, 160));
    wsprintfW(buf, L"%d lines", g_historyCount);
    ExtTextOutW(hdc, 220, 4, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Header divider */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(40, 80, 50));
        HPEN hOld = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, 10, 22, NULL);
        LineTo(hdc, g_screenW - 10, 22);
        SelectObject(hdc, hOld);
        DeleteObject(hPen);
    }

    /* Terminal output area (11 lines visible) */
    y = 26;
    for (i = 0; i < LINES_PER_SCREEN; i++) {
        int idx = g_scrollOffset + i;
        if (idx < g_historyCount) {
            const WCHAR *txt = g_history[idx];
            if (txt[0] == L'[') {
                SetTextColor(hdc, RGB(0, 255, 128));
            } else if (wcsstr(txt, L"PID:")) {
                SetTextColor(hdc, RGB(255, 220, 60));
            } else if (wcsstr(txt, L"KILL")) {
                SetTextColor(hdc, RGB(255, 80, 80));
            } else {
                SetTextColor(hdc, RGB(220, 220, 220));
            }
            ExtTextOutW(hdc, 12, y, 0, NULL, txt, lstrlenW(txt), NULL);
        }
        y += 15;
    }

    /* Dock divider */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(40, 80, 50));
        HPEN hOld = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, 10, 198, NULL);
        LineTo(hdc, g_screenW - 10, 198);
        SelectObject(hdc, hOld);
        DeleteObject(hPen);
    }

    /* Draw Touch Buttons */
    for (i = 0; i < 14; i++) {
        RECT bRc = g_dockBtns[i].rc;
        HPEN hPen = CreatePen(PS_SOLID, 1, g_dockBtns[i].color);
        HPEN hOld = (HPEN)SelectObject(hdc, hPen);

        MoveToEx(hdc, bRc.left, bRc.top, NULL);
        LineTo(hdc, bRc.right, bRc.top);
        LineTo(hdc, bRc.right, bRc.bottom);
        LineTo(hdc, bRc.left, bRc.bottom);
        LineTo(hdc, bRc.left, bRc.top);

        SelectObject(hdc, hOld);
        DeleteObject(hPen);

        SetTextColor(hdc, g_dockBtns[i].color);
        ExtTextOutW(hdc, bRc.left + 6, bRc.top + (i >= 12 ? 2 : 6), 0, NULL,
                    g_dockBtns[i].label, lstrlenW(g_dockBtns[i].label), NULL);
    }

    EndPaint(hWnd, &ps);
}

/* Handle button press */
static void HandleCommand(int cmdId)
{
    switch (cmdId) {
    case CMD_ID_LS:
        CmdLs();
        break;
    case CMD_ID_PWD:
        CmdPwd();
        break;
    case CMD_ID_MEM:
        CmdMem();
        break;
    case CMD_ID_PS:
        CmdPs();
        break;
    case CMD_ID_KILL_LAUNCH:
        CmdKillLaunch();
        break;
    case CMD_ID_REG_INIT:
        CmdRegInit();
        break;
    case CMD_ID_CAT_DUMP:
        CmdCat(L"\\SDMMC\\MERO\\system_dump.txt");
        break;
    case CMD_ID_CAT_PROBE:
        CmdCat(L"\\SDMMC\\MERO\\probe.txt");
        break;
    case CMD_ID_EXPLORER:
        {
            PROCESS_INFORMATION pi;
            memset(&pi, 0, sizeof(pi));
            if (CreateProcessW(L"\\Windows\\explorer.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                ShowWindow(g_hWnd, SW_MINIMIZE);
            } else {
                TermPrint(L"[!] Failed to launch explorer.exe");
            }
        }
        break;
    case CMD_ID_CONTROL:
        {
            PROCESS_INFORMATION pi;
            memset(&pi, 0, sizeof(pi));
            if (CreateProcessW(L"\\Windows\\control.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                ShowWindow(g_hWnd, SW_MINIMIZE);
            } else {
                TermPrint(L"[!] Failed to launch control.exe");
            }
        }
        break;
    case CMD_ID_CLEAR:
        TermClear();
        break;
    case CMD_ID_EXIT:
        DestroyWindow(g_hWnd);
        break;
    case CMD_ID_SCROLL_UP:
        if (g_scrollOffset > 0) {
            g_scrollOffset -= 3;
            if (g_scrollOffset < 0) g_scrollOffset = 0;
            InvalidateRect(g_hWnd, NULL, FALSE);
        }
        break;
    case CMD_ID_SCROLL_DN:
        if (g_scrollOffset + LINES_PER_SCREEN < g_historyCount) {
            g_scrollOffset += 3;
            if (g_scrollOffset + LINES_PER_SCREEN > g_historyCount) {
                g_scrollOffset = g_historyCount - LINES_PER_SCREEN;
            }
            InvalidateRect(g_hWnd, NULL, FALSE);
        }
        break;
    }
}

/* Touch handler */
static void OnTouch(int x, int y)
{
    int i;
    for (i = 0; i < 14; i++) {
        if (PtInRect(&g_dockBtns[i].rc, (POINT){ x, y })) {
            HandleCommand(g_dockBtns[i].cmdId);
            return;
        }
    }
}

/* Window procedure */
static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        InitDockButtons();
        TermClear();
        return 0;

    case WM_LBUTTONDOWN:
        OnTouch(LOWORD(lParam), HIWORD(lParam));
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
    wc.lpszClassName = L"MeroCmdWndClass";

    if (!RegisterClassW(&wc)) {
        return 1;
    }

    g_hWnd = CreateWindowExW(
        0,
        L"MeroCmdWndClass",
        L"Mero Cmd",
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
