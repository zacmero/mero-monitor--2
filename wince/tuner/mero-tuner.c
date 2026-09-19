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

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_JPEG
#include "stb_image.h"
#include "suzy_sample.h"

#define TUNER_VERSION       L"0.2.0"
#define CFG_DIR             L"\\SDMMC\\MERO"
#define CFG_FILE_SDMMC      L"\\SDMMC\\MERO\\display.cfg"
#define CFG_FILE_FLASH      L"\\ResidentFlash\\MERO\\display.cfg"
#define PATH_GALLERY_SD     L"\\SDMMC\\MERO\\mero-gallery.exe"
#define PATH_GALLERY_FLASH  L"\\ResidentFlash\\MERO\\mero-gallery.exe"
#define PATH_SHELL_SD       L"\\SDMMC\\MERO\\mero-shell.exe"
#define PATH_SHELL_FLASH    L"\\ResidentFlash\\MERO\\mero-shell.exe"

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
static WCHAR          g_statusMsg[128] = L"DISPLAY TUNER // TAP [-] / [+] TO CALIBRATE SUZY PREVIEW";
static BOOL           g_saveFlash = FALSE;

/* Sample Preview (190x125 24bpp) */
#define PREV_W 190
#define PREV_H 125
static unsigned char *g_sampleRgb = NULL;
static HBITMAP        g_hPrevBmp  = NULL;
static DWORD         *g_pPrevBits = NULL;
static HDC            g_prevDC    = NULL;
static HBITMAP        g_hOldPrev  = NULL;

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
        FlushFileBuffers(hFile);
        CloseHandle(hFile);
    }

    hFile = CreateFileW(CFG_FILE_FLASH, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, buf, (DWORD)strlen(buf), &written, NULL);
        FlushFileBuffers(hFile);
        CloseHandle(hFile);
    }

    g_saveFlash = TRUE;
    wsprintfW(g_statusMsg, L"[✓] CALIBRATION SAVED TO SDMMC & FLASH! (LIVE APPLIED)");
    MessageBeep(MB_OK);
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

    int yStart = 36;
    int yStep  = 33;
    int btnW   = 32;
    int btnH   = 26;

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
    int bY2 = 264;
    ADD_BTN(BTN_SAVE,    14,  bY1, 126, bY2, L"SAVE PROFILE", RGB(0, 255, 128));
    ADD_BTN(BTN_RESET,   134, bY1, 246, bY2, L"RESET DEFAULT", RGB(255, 180, 0));
    ADD_BTN(BTN_GALLERY, 254, bY1, 370, bY2, L"OPEN GALLERY", RGB(0, 220, 255));
    ADD_BTN(BTN_EXIT,    378, bY1, 466, bY2, L"EXIT TO HUB", RGB(255, 80, 80));

    #undef ADD_BTN
}

/* Render the live calibrated Suzy Mero Steele photo into 190x125 DIB */
static void RenderSamplePhoto(void)
{
    if (!g_sampleRgb || !g_pPrevBits) return;

    int y, x;
    for (y = 0; y < PREV_H; y++) {
        DWORD *dst = &g_pPrevBits[y * PREV_W];
        int rowIdx = y * PREV_W * 3;
        for (x = 0; x < PREV_W; x++) {
            int p = rowIdx + x * 3;
            int r = g_lut[g_sampleRgb[p]];
            int g = g_lut[g_sampleRgb[p + 1]];
            int b = g_lut[g_sampleRgb[p + 2]];

            if (g_satScale != 256) {
                int lum = (77 * r + 150 * g + 29 * b) >> 8;
                r = lum + (((r - lum) * g_satScale) >> 8);
                g = lum + (((g - lum) * g_satScale) >> 8);
                b = lum + (((b - lum) * g_satScale) >> 8);
                if (r < 0) r = 0; else if (r > 255) r = 255;
                if (g < 0) g = 0; else if (g > 255) g = 255;
                if (b < 0) b = 0; else if (b > 255) b = 255;
            }

            dst[x] = ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
        }
    }
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
    ExtTextOutW(memDC, 14, 6, 0, NULL, buf, lstrlenW(buf), NULL);

    SetTextColor(memDC, RGB(0, 200, 255));
    wsprintfW(buf, L"TFT 480x272 // %d%% BKL", g_backlight * 10);
    ExtTextOutW(memDC, 340, 6, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Header Line */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(40, 90, 60));
        HPEN hOld = (HPEN)SelectObject(memDC, hPen);
        MoveToEx(memDC, 14, 24, NULL);
        LineTo(memDC, g_screenW - 14, 24);
        SelectObject(memDC, hOld);
        DeleteObject(hPen);
    }

    /* LEFT COLUMN: 5 CONTROLS */
    int yStart = 34;
    int yStep  = 33;

    /* 1. Contrast */
    SetTextColor(memDC, RGB(200, 200, 200));
    ExtTextOutW(memDC, 16, yStart + 5, 0, NULL, L"CONTRAST:", 9, NULL);
    SetTextColor(memDC, RGB(0, 255, 200));
    wsprintfW(buf, L"%d%%", g_contrast);
    ExtTextOutW(memDC, 168, yStart + 5, 0, NULL, buf, lstrlenW(buf), NULL);

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
    ExtTextOutW(memDC, 168, yStart + yStep*2 + 5, 0, NULL, buf, lstrlenW(buf), NULL);

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

    /* RIGHT COLUMN: LIVE TEST CANVAS & SUZY PHOTO PREVIEW */
    int px = 270, py = 30, pw = 196;

    /* Grayscale Gradient Ramp (16 blocks) */
    int rampY = py;
    int rampH = 14;
    int blockW = pw / 16;
    for (i = 0; i < 16; i++) {
        int rawVal = i * 17;
        COLORREF c = CalibrateColor(RGB(rawVal, rawVal, rawVal));
        HBRUSH hBr = CreateSolidBrush(c);
        RECT blkRc = { px + i * blockW, rampY, px + (i + 1) * blockW, rampY + rampH };
        FillRect(memDC, &blkRc, hBr);
        DeleteObject(hBr);
    }

    /* Color Bars (6 blocks: Red, Green, Blue, Cyan, Magenta, Yellow) */
    int barY = rampY + rampH + 3;
    int barH = 14;
    COLORREF rawBars[6] = {
        RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255),
        RGB(0, 255, 255), RGB(255, 0, 255), RGB(255, 255, 0)
    };
    int colW = pw / 6;
    for (i = 0; i < 6; i++) {
        COLORREF c = CalibrateColor(rawBars[i]);
        HBRUSH hBr = CreateSolidBrush(c);
        RECT blkRc = { px + i * colW, barY, px + (i + 1) * colW, barY + barH };
        FillRect(memDC, &blkRc, hBr);
        DeleteObject(hBr);
    }

    /* REAL-TIME SUZY PHOTO PREVIEW */
    int photoY = barY + barH + 4;
    RenderSamplePhoto();
    if (g_prevDC) {
        BitBlt(memDC, px + (pw - PREV_W) / 2, photoY, PREV_W, PREV_H, g_prevDC, 0, 0, SRCCOPY);
    }

    /* Cybernetic Corner Brackets around Photo */
    {
        HPEN hBracket = CreatePen(PS_SOLID, 1, RGB(0, 255, 200));
        HPEN hOld = (HPEN)SelectObject(memDC, hBracket);
        int bx = px + (pw - PREV_W) / 2;
        int by = photoY;
        int bLen = 10;
        MoveToEx(memDC, bx - 2, by + bLen, NULL); LineTo(memDC, bx - 2, by - 2); LineTo(memDC, bx + bLen, by - 2);
        MoveToEx(memDC, bx + PREV_W + 1 - bLen, by - 2, NULL); LineTo(memDC, bx + PREV_W + 1, by - 2); LineTo(memDC, bx + PREV_W + 1, by + bLen);
        MoveToEx(memDC, bx - 2, by + PREV_H + 1 - bLen, NULL); LineTo(memDC, bx - 2, by + PREV_H + 1); LineTo(memDC, bx + bLen, by + PREV_H + 1);
        MoveToEx(memDC, bx + PREV_W + 1 - bLen, by + PREV_H + 1, NULL); LineTo(memDC, bx + PREV_W + 1, by + PREV_H + 1); LineTo(memDC, bx + PREV_W + 1, by + PREV_H + 1 - bLen);
        SelectObject(memDC, hOld);
        DeleteObject(hBracket);
    }

    /* VISIBLE STATUS LINE (Between Controls and Bottom Deck) */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(40, 70, 50));
        HPEN hOld = (HPEN)SelectObject(memDC, hPen);
        MoveToEx(memDC, 14, 202, NULL);
        LineTo(memDC, g_screenW - 14, 202);
        SelectObject(memDC, hOld);
        DeleteObject(hPen);
    }

    SetTextColor(memDC, g_saveFlash ? RGB(0, 255, 128) : RGB(140, 200, 170));
    ExtTextOutW(memDC, 14, 206, 0, NULL, g_statusMsg, lstrlenW(g_statusMsg), NULL);

    /* DRAW INTERACTIVE TOUCH BUTTONS */
    for (i = 0; i < g_buttonCount; i++) {
        RECT bRc = g_buttons[i].rc;
        COLORREF bCol = g_buttons[i].color;

        if (g_buttons[i].id == BTN_SAVE && g_saveFlash) {
            bCol = RGB(0, 255, 128);
            HBRUSH hFill = CreateSolidBrush(RGB(10, 60, 30));
            FillRect(memDC, &bRc, hFill);
            DeleteObject(hFill);
        }

        HPEN hPen = CreatePen(PS_SOLID, 1, bCol);
        HPEN hOld = (HPEN)SelectObject(memDC, hPen);

        MoveToEx(memDC, bRc.left, bRc.top, NULL);
        LineTo(memDC, bRc.right, bRc.top);
        LineTo(memDC, bRc.right, bRc.bottom);
        LineTo(memDC, bRc.left, bRc.bottom);
        LineTo(memDC, bRc.left, bRc.top);

        SelectObject(memDC, hOld);
        DeleteObject(hPen);

        SetTextColor(memDC, (g_buttons[i].id == BTN_SAVE && g_saveFlash) ? RGB(255, 255, 255) : bCol);
        DrawTextW(memDC, g_buttons[i].label, -1, &bRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

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
            g_saveFlash = FALSE;
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
                    BOOL ok = CreateProcessW(PATH_GALLERY_SD, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
                    if (!ok) {
                        ok = CreateProcessW(PATH_GALLERY_FLASH, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
                    }
                    if (ok) {
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                        DestroyWindow(g_hWnd);
                        return;
                    } else {
                        wsprintfW(g_statusMsg, L"ERR // Gallery binary not found in SDMMC or Flash");
                    }
                }
                break;

            case BTN_EXIT:
                {
                    PROCESS_INFORMATION pi;
                    memset(&pi, 0, sizeof(pi));
                    BOOL ok = CreateProcessW(PATH_SHELL_SD, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
                    if (!ok) {
                        ok = CreateProcessW(PATH_SHELL_FLASH, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
                    }
                    if (ok) {
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                        DestroyWindow(g_hWnd);
                        return;
                    } else {
                        wsprintfW(g_statusMsg, L"ERR // Shell binary not found in SDMMC or Flash");
                    }
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

        /* Decode Suzy Mero Steele sample preview from embedded memory */
        {
            int w = 0, h = 0, ch = 0;
            unsigned char *decoded = stbi_load_from_memory(g_suzySampleJpeg, SUZY_SAMPLE_LEN, &w, &h, &ch, 3);
            if (decoded && w == PREV_W && h == PREV_H) {
                g_sampleRgb = decoded;
            } else if (decoded) {
                stbi_image_free(decoded);
            }
        }

        /* Create 190x125 32bpp DIB for real-time sample rendering */
        {
            HDC hdc = GetDC(hWnd);
            BITMAPINFO bmi;
            memset(&bmi, 0, sizeof(bmi));
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = PREV_W;
            bmi.bmiHeader.biHeight = -PREV_H; /* Top-down DIB */
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            g_hPrevBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&g_pPrevBits, NULL, 0);
            g_prevDC = CreateCompatibleDC(hdc);
            g_hOldPrev = (HBITMAP)SelectObject(g_prevDC, g_hPrevBmp);
            ReleaseDC(hWnd, hdc);
        }

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
        if (g_prevDC && g_hOldPrev) SelectObject(g_prevDC, g_hOldPrev);
        if (g_hPrevBmp) DeleteObject(g_hPrevBmp);
        if (g_prevDC) DeleteDC(g_prevDC);
        if (g_sampleRgb) stbi_image_free(g_sampleRgb);
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
