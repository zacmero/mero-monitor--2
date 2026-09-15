/*
 * mero-tuner.c - Interactive Display & Color Calibrator
 * Project: mero-monitor-#2
 * Target: Foston FS-460BT (PE32 ARMv4 Windows CE 5.0)
 * Screen: 480x272 16bpp TFT LCD
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TUNER_VERSION       L"0.1.0"
#define CFG_DIR             L"\\SDMMC\\MERO"
#define CFG_FILE_SDMMC      L"\\SDMMC\\MERO\\display.cfg"
#define CFG_FILE_FLASH      L"\\ResidentFlash\\MERO\\display.cfg"
#define PATH_GALLERY        L"\\SDMMC\\MERO\\mero-gallery.exe"
#define PATH_SHELL          L"\\SDMMC\\MERO\\mero-shell.exe"

static HINSTANCE      g_hInstance = NULL;
static HWND           g_hWnd = NULL;
static int            g_screenW = 480;
static int            g_screenH = 272;

/* Calibration Parameters */
static int            g_contrast   = 125;  /* 50% to 200% (125 = 1.25x) */
static int            g_brightness = 0;    /* -50 to +50 */
static int            g_saturation = 130;  /* 50% to 200% (130 = 1.30x) */
static int            g_gamma      = 115;  /* 60 to 180 (115 = 1.15) */
static int            g_backlight  = 8;    /* 1 to 10 */

static unsigned char  g_lut[256];
static int            g_satScale = 332;
static WCHAR          g_statusMsg[128] = L"DISPLAY TUNER // TAP [-] / [+] TO ADJUST IN REAL-TIME";

/* UI Touch Target structure */
typedef struct {
    RECT rc;
    int  id;
    WCHAR label[24];
    COLORREF color;
} TouchButton;

#define BTN_CONTRAST_DEC    1
#define BTN_CONTRAST_INC    2
#define BTN_BRIGHT_DEC      3
#define BTN_BRIGHT_INC      4
#define BTN_SAT_DEC         5
#define BTN_SAT_INC         6
#define BTN_GAMMA_DEC       7
#define BTN_GAMMA_INC       8
#define BTN_BKL_DEC         9
#define BTN_BKL_INC         10
#define BTN_SAVE            11
#define BTN_RESET           12
#define BTN_GALLERY         13
#define BTN_EXIT            14

static TouchButton    g_buttons[14];
static int            g_buttonCount = 0;

static void RebuildLut(void)
{
    float c = (float)g_contrast / 100.0f;
    float gammaInv = 100.0f / (float)g_gamma;
    int i;

    for (i = 0; i < 256; i++) {
        float val = (float)i + (float)g_brightness;
        val = (val - 128.0f) * c + 128.0f;
        if (val < 0.0f) val = 0.0f;
        if (val > 255.0f) val = 255.0f;

        float norm = val / 255.0f;
        float out = powf(norm, gammaInv) * 255.0f;

        int res = (int)(out + 0.5f);
        if (res < 0) res = 0;
        if (res > 255) res = 255;
        g_lut[i] = (unsigned char)res;
    }

    g_satScale = (g_saturation * 256) / 100;
}

static COLORREF CalibrateColor(COLORREF inCol)
{
    int r = g_lut[GetRValue(inCol)];
    int g = g_lut[GetGValue(inCol)];
    int b = g_lut[GetBValue(inCol)];

    if (g_satScale != 256) {
        int y = (77 * r + 150 * g + 29 * b) >> 8;
        r = y + (((r - y) * g_satScale) >> 8);
        g = y + (((g - y) * g_satScale) >> 8);
        b = y + (((b - y) * g_satScale) >> 8);
        if (r < 0) r = 0; else if (r > 255) r = 255;
        if (g < 0) g = 0; else if (g > 255) g = 255;
        if (b < 0) b = 0; else if (b > 255) b = 255;
    }

    return RGB(r, g, b);
}

/* Set Hardware LCD Backlight via registry & system event */
static void ApplyHardwareBacklight(int level)
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"ControlPanel\\BackLight", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD val = (DWORD)level;
        RegSetValueExW(hKey, L"Bright", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegSetValueExW(hKey, L"Brightness", 0, REG_DWORD, (const BYTE*)&val, sizeof(DWORD));
        RegFlushKey(hKey);
        RegCloseKey(hKey);
    }

    /* Trigger WinCE backlight driver notification event */
    HANDLE hEvt = CreateEventW(NULL, FALSE, FALSE, L"BackLightChangeEvent");
    if (hEvt) {
        SetEvent(hEvt);
        CloseHandle(hEvt);
    }
}

static void LoadConfig(void)
{
    HANDLE hFile;
    DWORD bytesRead;
    char buf[512];

    hFile = CreateFileW(CFG_FILE_SDMMC, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        hFile = CreateFileW(CFG_FILE_FLASH, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    if (hFile != INVALID_HANDLE_VALUE) {
        memset(buf, 0, sizeof(buf));
        if (ReadFile(hFile, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
            char *line = strtok(buf, "\r\n");
            while (line) {
                int val = 0;
                if (sscanf(line, "contrast=%d", &val) == 1) g_contrast = val;
                else if (sscanf(line, "brightness=%d", &val) == 1) g_brightness = val;
                else if (sscanf(line, "saturation=%d", &val) == 1) g_saturation = val;
                else if (sscanf(line, "gamma=%d", &val) == 1) g_gamma = val;
                else if (sscanf(line, "backlight=%d", &val) == 1) g_backlight = val;
                line = strtok(NULL, "\r\n");
            }
        }
        CloseHandle(hFile);
    }

    RebuildLut();
    ApplyHardwareBacklight(g_backlight);
}

static void SaveConfig(void)
{
    char buf[256];
    DWORD written;
    HANDLE hFile;

    snprintf(buf, sizeof(buf),
             "contrast=%d\r\nbrightness=%d\r\nsaturation=%d\r\ngamma=%d\r\nbacklight=%d\r\n",
             g_contrast, g_brightness, g_saturation, g_gamma, g_backlight);

    CreateDirectoryW(CFG_DIR, NULL);
    CreateDirectoryW(L"\\ResidentFlash\\MERO", NULL);

    hFile = CreateFileW(CFG_FILE_SDMMC, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, buf, (DWORD)strlen(buf), &written, NULL);
        CloseHandle(hFile);
    }

    hFile = CreateFileW(CFG_FILE_FLASH, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, buf, (DWORD)strlen(buf), &written, NULL);
        CloseHandle(hFile);
    }

    wsprintfW(g_statusMsg, L"SUCCESS // Calibration saved to SDMMC & ResidentFlash!");
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void SetupLayout(void)
{
    g_buttonCount = 0;
    #define ADD_BTN(bId, rx1, ry1, rx2, ry2, bLbl, bCol) do { \
        if (g_buttonCount < 14) { \
            SetRect(&g_buttons[g_buttonCount].rc, (rx1), (ry1), (rx2), (ry2)); \
            g_buttons[g_buttonCount].id = (bId); \
            lstrcpynW(g_buttons[g_buttonCount].label, (bLbl), 24); \
            g_buttons[g_buttonCount].color = (bCol); \
            g_buttonCount++; \
        } \
    } while(0)

    int yStart = 38;
    int yStep  = 36;
    int btnW   = 32;
    int btnH   = 28;

    /* 1. Contrast */
    ADD_BTN(BTN_CONTRAST_DEC, 130, yStart, 130 + btnW, yStart + btnH, L" - ", RGB(0, 255, 200));
    ADD_BTN(BTN_CONTRAST_INC, 218, yStart, 218 + btnW, yStart + btnH, L" + ", RGB(0, 255, 200));

    /* 2. Brightness */
    ADD_BTN(BTN_BRIGHT_DEC,   130, yStart + yStep, 130 + btnW, yStart + yStep + btnH, L" - ", RGB(0, 255, 200));
    ADD_BTN(BTN_BRIGHT_INC,   218, yStart + yStep, 218 + btnW, yStart + yStep + btnH, L" + ", RGB(0, 255, 200));

    /* 3. Saturation */
    ADD_BTN(BTN_SAT_DEC,      130, yStart + yStep*2, 130 + btnW, yStart + yStep*2 + btnH, L" - ", RGB(0, 255, 200));
    ADD_BTN(BTN_SAT_INC,      218, yStart + yStep*2, 218 + btnW, yStart + yStep*2 + btnH, L" + ", RGB(0, 255, 200));

    /* 4. Gamma */
    ADD_BTN(BTN_GAMMA_DEC,    130, yStart + yStep*3, 130 + btnW, yStart + yStep*3 + btnH, L" - ", RGB(0, 255, 200));
    ADD_BTN(BTN_GAMMA_INC,    218, yStart + yStep*3, 218 + btnW, yStart + yStep*3 + btnH, L" + ", RGB(0, 255, 200));

    /* 5. Backlight */
    ADD_BTN(BTN_BKL_DEC,      130, yStart + yStep*4, 130 + btnW, yStart + yStep*4 + btnH, L" - ", RGB(0, 255, 200));
    ADD_BTN(BTN_BKL_INC,      218, yStart + yStep*4, 218 + btnW, yStart + yStep*4 + btnH, L" + ", RGB(0, 255, 200));

    /* Bottom Action Deck */
    int bY1 = 226;
    int bY2 = 262;
    ADD_BTN(BTN_SAVE,    14,  bY1, 122, bY2, L"SAVE PROFILE", RGB(0, 255, 128));
    ADD_BTN(BTN_RESET,   130, bY1, 238, bY2, L"RESET DEFAULTS", RGB(255, 180, 0));
    ADD_BTN(BTN_GALLERY, 246, bY1, 370, bY2, L"OPEN GALLERY", RGB(0, 220, 255));
    ADD_BTN(BTN_EXIT,    378, bY1, 466, bY2, L"EXIT TO HUB", RGB(255, 80, 80));

    #undef ADD_BTN
}

static void OnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc, memDC;
    HBITMAP memBmp, oldBmp;
    RECT rc;
    WCHAR buf[128];
    int i;

    hdc = BeginPaint(hWnd, &ps);
    GetClientRect(hWnd, &rc);

    memDC = CreateCompatibleDC(hdc);
    memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    FillRect(memDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(memDC, TRANSPARENT);

    /* Header Bar */
    SetTextColor(memDC, RGB(0, 255, 128));
    wsprintfW(buf, L"MERO // DISPLAY & COLOR TUNER [v%s]", TUNER_VERSION);
    ExtTextOutW(memDC, 14, 8, 0, NULL, buf, lstrlenW(buf), NULL);

    SetTextColor(memDC, RGB(0, 200, 255));
    wsprintfW(buf, L"TFT 480x272 // %d%% BKL", g_backlight * 10);
    ExtTextOutW(memDC, 340, 8, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Header Line */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(40, 90, 60));
        HPEN hOld = (HPEN)SelectObject(memDC, hPen);
        MoveToEx(memDC, 14, 26, NULL);
        LineTo(memDC, g_screenW - 14, 26);
        SelectObject(memDC, hOld);
        DeleteObject(hPen);
    }

    /* LEFT COLUMN: 5 CONTROLS */
    int yStart = 38;
    int yStep  = 36;

    /* 1. Contrast */
    SetTextColor(memDC, RGB(200, 200, 200));
    ExtTextOutW(memDC, 16, yStart + 5, 0, NULL, L"CONTRAST:", 9, NULL);
    SetTextColor(memDC, RGB(0, 255, 200));
    wsprintfW(buf, L"%d%%", g_contrast);
    ExtTextOutW(memDC, 170, yStart + 5, 0, NULL, buf, lstrlenW(buf), NULL);

    /* 2. Brightness */
    SetTextColor(memDC, RGB(200, 200, 200));
    ExtTextOutW(memDC, 16, yStart + yStep + 5, 0, NULL, L"BRIGHTNESS:", 11, NULL);
    SetTextColor(memDC, RGB(0, 255, 200));
    wsprintfW(buf, g_brightness >= 0 ? L"+%d" : L"%d", g_brightness);
    ExtTextOutW(memDC, 172, yStart + yStep + 5, 0, NULL, buf, lstrlenW(buf), NULL);

    /* 3. Saturation */
    SetTextColor(memDC, RGB(200, 200, 200));
    ExtTextOutW(memDC, 16, yStart + yStep*2 + 5, 0, NULL, L"SATURATION:", 11, NULL);
    SetTextColor(memDC, RGB(0, 255, 200));
    wsprintfW(buf, L"%d%%", g_saturation);
    ExtTextOutW(memDC, 170, yStart + yStep*2 + 5, 0, NULL, buf, lstrlenW(buf), NULL);

    /* 4. Gamma */
    SetTextColor(memDC, RGB(200, 200, 200));
    ExtTextOutW(memDC, 16, yStart + yStep*3 + 5, 0, NULL, L"GAMMA CURVE:", 12, NULL);
    SetTextColor(memDC, RGB(0, 255, 200));
    wsprintfW(buf, L"%d.%02d", g_gamma / 100, g_gamma % 100);
    ExtTextOutW(memDC, 168, yStart + yStep*3 + 5, 0, NULL, buf, lstrlenW(buf), NULL);

    /* 5. Backlight */
    SetTextColor(memDC, RGB(200, 200, 200));
    ExtTextOutW(memDC, 16, yStart + yStep*4 + 5, 0, NULL, L"BACKLIGHT:", 10, NULL);
    SetTextColor(memDC, RGB(0, 255, 200));
    wsprintfW(buf, L"LVL %d", g_backlight);
    ExtTextOutW(memDC, 170, yStart + yStep*4 + 5, 0, NULL, buf, lstrlenW(buf), NULL);

    /* RIGHT COLUMN: LIVE TEST CANVAS */
    int px = 265, py = 36, pw = 200, ph = 175;
    {
        /* Bounding Box for Test Canvas */
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(50, 100, 80));
        HPEN hOld = (HPEN)SelectObject(memDC, hPen);
        MoveToEx(memDC, px, py, NULL);
        LineTo(memDC, px + pw, py);
        LineTo(memDC, px + pw, py + ph);
        LineTo(memDC, px, py + ph);
        LineTo(memDC, px, py);
        SelectObject(memDC, hOld);
        DeleteObject(hPen);
    }

    /* Grayscale Gradient Ramp (16 blocks) */
    int rampY = py + 4;
    int rampH = 22;
    int blockW = (pw - 8) / 16;
    for (i = 0; i < 16; i++) {
        int rawVal = i * 17;
        COLORREF c = CalibrateColor(RGB(rawVal, rawVal, rawVal));
        HBRUSH hBr = CreateSolidBrush(c);
        RECT blkRc = { px + 4 + i * blockW, rampY, px + 4 + (i + 1) * blockW, rampY + rampH };
        FillRect(memDC, &blkRc, hBr);
        DeleteObject(hBr);
    }

    /* Color Bars (6 blocks: Red, Green, Blue, Cyan, Magenta, Yellow) */
    int barY = rampY + rampH + 4;
    int barH = 20;
    COLORREF rawBars[6] = {
        RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255),
        RGB(0, 255, 255), RGB(255, 0, 255), RGB(255, 255, 0)
    };
    int colW = (pw - 8) / 6;
    for (i = 0; i < 6; i++) {
        COLORREF c = CalibrateColor(rawBars[i]);
        HBRUSH hBr = CreateSolidBrush(c);
        RECT blkRc = { px + 4 + i * colW, barY, px + 4 + (i + 1) * colW, barY + barH };
        FillRect(memDC, &blkRc, hBr);
        DeleteObject(hBr);
    }

    /* Dynamic Cybernetic Procedural Landscape (Tests contrast, gradients, and skin/sky tones) */
    int imgY = barY + barH + 4;
    int imgH = (py + ph - 4) - imgY;
    int dy;
    for (dy = 0; dy < imgH; dy++) {
        float f = (float)dy / (float)imgH;
        /* Sky gradient: deep blue into warm orange sunset */
        int rSky = (int)(20.0f + f * 210.0f);
        int gSky = (int)(30.0f + f * 110.0f);
        int bSky = (int)(140.0f - f * 110.0f);
        COLORREF skyCol = CalibrateColor(RGB(rSky, gSky, bSky));

        HPEN hPen = CreatePen(PS_SOLID, 1, skyCol);
        HPEN hOld = (HPEN)SelectObject(memDC, hPen);
        MoveToEx(memDC, px + 4, imgY + dy, NULL);
        LineTo(memDC, px + pw - 4, imgY + dy);
        SelectObject(memDC, hOld);
        DeleteObject(hPen);
    }

    /* Sun sphere in landscape */
    {
        COLORREF sunCol = CalibrateColor(RGB(255, 220, 60));
        HBRUSH hSun = CreateSolidBrush(sunCol);
        HBRUSH hOldBr = (HBRUSH)SelectObject(memDC, hSun);
        HPEN hNull = (HPEN)GetStockObject(NULL_PEN);
        HPEN hOldPen = (HPEN)SelectObject(memDC, hNull);
        Ellipse(memDC, px + 75, imgY + 12, px + 125, imgY + 62);
        SelectObject(memDC, hOldPen);
        SelectObject(memDC, hOldBr);
        DeleteObject(hSun);
    }

    /* Silhouette Mountains for shadow/black depth test */
    {
        COLORREF darkCol = CalibrateColor(RGB(15, 12, 25));
        HBRUSH hMtn = CreateSolidBrush(darkCol);
        POINT pts[5] = {
            { px + 4, imgY + imgH },
            { px + 50, imgY + imgH - 35 },
            { px + 110, imgY + imgH - 18 },
            { px + 170, imgY + imgH - 45 },
            { px + pw - 4, imgY + imgH }
        };
        HBRUSH hOldBr = (HBRUSH)SelectObject(memDC, hMtn);
        HPEN hNull = (HPEN)GetStockObject(NULL_PEN);
        HPEN hOldPen = (HPEN)SelectObject(memDC, hNull);
        Polygon(memDC, pts, 5);
        SelectObject(memDC, hOldPen);
        SelectObject(memDC, hOldBr);
        DeleteObject(hMtn);
    }

    /* DRAW INTERACTIVE TOUCH BUTTONS */
    for (i = 0; i < g_buttonCount; i++) {
        RECT bRc = g_buttons[i].rc;
        HPEN hPen = CreatePen(PS_SOLID, 1, g_buttons[i].color);
        HPEN hOld = (HPEN)SelectObject(memDC, hPen);

        MoveToEx(memDC, bRc.left, bRc.top, NULL);
        LineTo(memDC, bRc.right, bRc.top);
        LineTo(memDC, bRc.right, bRc.bottom);
        LineTo(memDC, bRc.left, bRc.bottom);
        LineTo(memDC, bRc.left, bRc.top);

        SelectObject(memDC, hOld);
        DeleteObject(hPen);

        SetTextColor(memDC, g_buttons[i].color);
        DrawTextW(memDC, g_buttons[i].label, -1, &bRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* Status footer */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(30, 60, 40));
        HPEN hOld = (HPEN)SelectObject(memDC, hPen);
        MoveToEx(memDC, 14, 218, NULL);
        LineTo(memDC, g_screenW - 14, 218);
        SelectObject(memDC, hOld);
        DeleteObject(hPen);
    }
    SetTextColor(memDC, RGB(120, 180, 150));
    ExtTextOutW(memDC, 14, 268, 0, NULL, g_statusMsg, lstrlenW(g_statusMsg), NULL);

    /* Atomic Blit to LCD */
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hWnd, &ps);
}

static void OnTouch(int x, int y)
{
    int i;
    for (i = 0; i < g_buttonCount; i++) {
        if (PtInRect(&g_buttons[i].rc, (POINT){ x, y })) {
            switch (g_buttons[i].id) {
            case BTN_CONTRAST_DEC:
                if (g_contrast > 50) g_contrast -= 5;
                RebuildLut();
                wsprintfW(g_statusMsg, L"CONTRAST: %d%%", g_contrast);
                break;
            case BTN_CONTRAST_INC:
                if (g_contrast < 200) g_contrast += 5;
                RebuildLut();
                wsprintfW(g_statusMsg, L"CONTRAST: %d%%", g_contrast);
                break;

            case BTN_BRIGHT_DEC:
                if (g_brightness > -50) g_brightness -= 5;
                RebuildLut();
                wsprintfW(g_statusMsg, L"BRIGHTNESS: %d", g_brightness);
                break;
            case BTN_BRIGHT_INC:
                if (g_brightness < 50) g_brightness += 5;
                RebuildLut();
                wsprintfW(g_statusMsg, L"BRIGHTNESS: %d", g_brightness);
                break;

            case BTN_SAT_DEC:
                if (g_saturation > 50) g_saturation -= 5;
                RebuildLut();
                wsprintfW(g_statusMsg, L"SATURATION: %d%%", g_saturation);
                break;
            case BTN_SAT_INC:
                if (g_saturation < 200) g_saturation += 5;
                RebuildLut();
                wsprintfW(g_statusMsg, L"SATURATION: %d%%", g_saturation);
                break;

            case BTN_GAMMA_DEC:
                if (g_gamma > 60) g_gamma -= 5;
                RebuildLut();
                wsprintfW(g_statusMsg, L"GAMMA: %d.%02d", g_gamma / 100, g_gamma % 100);
                break;
            case BTN_GAMMA_INC:
                if (g_gamma < 180) g_gamma += 5;
                RebuildLut();
                wsprintfW(g_statusMsg, L"GAMMA: %d.%02d", g_gamma / 100, g_gamma % 100);
                break;

            case BTN_BKL_DEC:
                if (g_backlight > 1) g_backlight--;
                ApplyHardwareBacklight(g_backlight);
                wsprintfW(g_statusMsg, L"BACKLIGHT: LEVEL %d", g_backlight);
                break;
            case BTN_BKL_INC:
                if (g_backlight < 10) g_backlight++;
                ApplyHardwareBacklight(g_backlight);
                wsprintfW(g_statusMsg, L"BACKLIGHT: LEVEL %d", g_backlight);
                break;

            case BTN_SAVE:
                SaveConfig();
                break;

            case BTN_RESET:
                g_contrast   = 125;
                g_brightness = 0;
                g_saturation = 130;
                g_gamma      = 115;
                g_backlight  = 8;
                RebuildLut();
                ApplyHardwareBacklight(g_backlight);
                wsprintfW(g_statusMsg, L"RESET: Defaults restored (125%% contrast, 130%% sat, 1.15 gamma)");
                break;

            case BTN_GALLERY:
                {
                    PROCESS_INFORMATION pi;
                    memset(&pi, 0, sizeof(pi));
                    if (CreateProcessW(PATH_GALLERY, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                        DestroyWindow(g_hWnd);
                        return;
                    }
                }
                break;

            case BTN_EXIT:
                {
                    PROCESS_INFORMATION pi;
                    memset(&pi, 0, sizeof(pi));
                    if (!CreateProcessW(PATH_SHELL, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
                        CreateProcessW(L"\\ResidentFlash\\MERO\\mero-shell.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
                    }
                    if (pi.hProcess) {
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                    }
                    DestroyWindow(g_hWnd);
                    return;
                }
                break;
            }

            InvalidateRect(g_hWnd, NULL, FALSE);
            return;
        }
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        g_hWnd = hWnd;
        SetupLayout();
        LoadConfig();
        return 0;

    case WM_ERASEBKGND:
        return 1;

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

    HWND hExisting = FindWindowW(L"MeroTunerWndClass", L"Mero Tuner");
    if (hExisting) {
        ShowWindow(hExisting, SW_SHOWNORMAL);
        SetForegroundWindow(hExisting);
        return 0;
    }

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"MeroTunerWndClass";

    if (!RegisterClassW(&wc)) return 1;

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"MeroTunerWndClass",
        L"Mero Tuner",
        WS_VISIBLE | WS_POPUP,
        0, 0,
        g_screenW, g_screenH,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWnd) return 2;

    ShowWindow(g_hWnd, SW_SHOWNORMAL);
    UpdateWindow(g_hWnd);

    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
