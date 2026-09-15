/*
 * mero-media-ctrl.c - Native Cybernetic YouTube Music & Media Controller
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
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#include "stb_image.h"

#define MEDIA_VERSION       L"0.1.0"
#define TIMER_ID_POLL       1
#define TIMER_ID_MARQUEE    2

#define NOW_PLAYING_SDMMC   L"\\SDMMC\\Stream\\now_playing.txt"
#define NOW_PLAYING_FLASH   L"\\ResidentFlash\\Stream\\now_playing.txt"
#define NOW_PLAYING_MERO    L"\\SDMMC\\MERO\\now_playing.txt"

#define COVER_FILE_SDMMC    L"\\SDMMC\\Stream\\cover.jpg"
#define COVER_FILE_FLASH    L"\\ResidentFlash\\Stream\\cover.jpg"

#define CMD_FILE_SDMMC      L"\\SDMMC\\MERO\\media_cmd.txt"
#define CMD_FILE_FLASH      L"\\ResidentFlash\\MERO\\media_cmd.txt"

#define PATH_SHELL          L"\\SDMMC\\MERO\\mero-shell.exe"

static HINSTANCE      g_hInstance = NULL;
static HWND           g_hWnd = NULL;
static int            g_screenW = 480;
static int            g_screenH = 272;

/* Now Playing State */
static WCHAR          g_title[128]  = L"Ready for Stream";
static WCHAR          g_artist[64]  = L"YouTube Music Bridge";
static WCHAR          g_album[64]   = L"Mero Monitor #2";
static BOOL           g_isPlaying   = FALSE;
static int            g_positionSec = 0;
static int            g_durationSec = 0;
static WCHAR          g_statusMsg[128] = L"MERO MEDIA DECK // CONNECTING TO HOST BRIDGE...";

/* Marquee scrolling for long titles */
static int            g_scrollOffset = 0;
static int            g_scrollDir    = 1;

/* Artwork buffer */
#define ART_SIZE 180
static HBITMAP        g_hCoverBmp = NULL;
static DWORD         *g_pCoverBits = NULL;
static HDC            g_coverDC = NULL;
static HBITMAP        g_hOldCoverBmp = NULL;
static BOOL           g_hasCover = FALSE;
static WCHAR          g_lastCoverPath[MAX_PATH] = L"";

/* Calibrated Color Correction LUT */
static unsigned char  g_lut[256];
static int            g_satScale = 332;

static void LoadColorCalibration(void)
{
    int contrast = 125, brightness = 0, gamma = 115, saturation = 130;
    HANDLE hFile = CreateFileW(L"\\SDMMC\\MERO\\display.cfg", GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        hFile = CreateFileW(L"\\ResidentFlash\\MERO\\display.cfg", GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    if (hFile != INVALID_HANDLE_VALUE) {
        char buf[512];
        DWORD bytesRead;
        memset(buf, 0, sizeof(buf));
        if (ReadFile(hFile, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
            char *line = strtok(buf, "\r\n");
            while (line) {
                int val = 0;
                if (sscanf(line, "contrast=%d", &val) == 1) contrast = val;
                else if (sscanf(line, "brightness=%d", &val) == 1) brightness = val;
                else if (sscanf(line, "saturation=%d", &val) == 1) saturation = val;
                else if (sscanf(line, "gamma=%d", &val) == 1) gamma = val;
                line = strtok(NULL, "\r\n");
            }
        }
        CloseHandle(hFile);
    }

    float c = (float)contrast / 100.0f;
    float gammaInv = 100.0f / (float)gamma;
    int i;
    for (i = 0; i < 256; i++) {
        float val = (float)i + (float)brightness;
        val = (val - 128.0f) * c + 128.0f;
        if (val < 0.0f) val = 0.0f;
        if (val > 255.0f) val = 255.0f;
        float norm = val / 255.0f;
        float out = powf(norm, gammaInv) * 255.0f;
        int res = (int)(out + 0.5f);
        if (res < 0) { res = 0; }
        else if (res > 255) { res = 255; }
        g_lut[i] = (unsigned char)res;
    }
    g_satScale = (saturation * 256) / 100;
}

/* Load and scale album cover art into 180x180 32bpp DIB with calibrated LUT */
static void LoadCoverArt(const WCHAR *path)
{
    HANDLE hFile;
    DWORD fileSize, bytesRead;
    unsigned char *fileBuffer;
    int srcW, srcH, channels;
    unsigned char *pixels;

    hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        g_hasCover = FALSE;
        return;
    }

    fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0 || fileSize > 10 * 1024 * 1024) {
        CloseHandle(hFile);
        g_hasCover = FALSE;
        return;
    }

    fileBuffer = (unsigned char*)malloc(fileSize);
    if (!fileBuffer) {
        CloseHandle(hFile);
        g_hasCover = FALSE;
        return;
    }

    if (!ReadFile(hFile, fileBuffer, fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
        free(fileBuffer);
        CloseHandle(hFile);
        g_hasCover = FALSE;
        return;
    }
    CloseHandle(hFile);

    pixels = stbi_load_from_memory(fileBuffer, (int)fileSize, &srcW, &srcH, &channels, 4);
    free(fileBuffer);

    if (!pixels || srcW <= 0 || srcH <= 0) {
        if (pixels) stbi_image_free(pixels);
        g_hasCover = FALSE;
        return;
    }

    /* Scale into 180x180 surface with color LUT */
    int dy, dx;
    for (dy = 0; dy < ART_SIZE; dy++) {
        int sy = (dy * srcH) / ART_SIZE;
        DWORD *row = &g_pCoverBits[dy * ART_SIZE];
        for (dx = 0; dx < ART_SIZE; dx++) {
            int sx = (dx * srcW) / ART_SIZE;
            unsigned char *p = &pixels[(sy * srcW + sx) * 4];

            int r = g_lut[p[0]];
            int g = g_lut[p[1]];
            int b = g_lut[p[2]];

            if (g_satScale != 256) {
                int y = (77 * r + 150 * g + 29 * b) >> 8;
                r = y + (((r - y) * g_satScale) >> 8);
                g = y + (((g - y) * g_satScale) >> 8);
                b = y + (((b - y) * g_satScale) >> 8);
                if (r < 0) r = 0; else if (r > 255) r = 255;
                if (g < 0) g = 0; else if (g > 255) g = 255;
                if (b < 0) b = 0; else if (b > 255) b = 255;
            }

            row[dx] = ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
        }
    }

    stbi_image_free(pixels);
    g_hasCover = TRUE;
    lstrcpynW(g_lastCoverPath, path, MAX_PATH);
}

/* Dispatch command to host bridge */
static void SendMediaCommand(const char *cmdName)
{
    HANDLE hFile;
    DWORD written;

    CreateDirectoryW(L"\\SDMMC\\MERO", NULL);
    CreateDirectoryW(L"\\ResidentFlash\\MERO", NULL);

    hFile = CreateFileW(CMD_FILE_SDMMC, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, cmdName, (DWORD)strlen(cmdName), &written, NULL);
        CloseHandle(hFile);
    }

    hFile = CreateFileW(CMD_FILE_FLASH, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, cmdName, (DWORD)strlen(cmdName), &written, NULL);
        CloseHandle(hFile);
    }

    /* Tactile status feedback */
    wsprintfW(g_statusMsg, L"DISPATCHED -> %hs", cmdName);
    InvalidateRect(g_hWnd, NULL, FALSE);
}

/* Poll now_playing.txt for track updates */
static void PollNowPlaying(void)
{
    HANDLE hFile;
    DWORD bytesRead;
    char buf[1024];
    WCHAR coverPath[MAX_PATH] = L"";

    hFile = CreateFileW(NOW_PLAYING_SDMMC, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        hFile = CreateFileW(NOW_PLAYING_FLASH, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    if (hFile == INVALID_HANDLE_VALUE) {
        hFile = CreateFileW(NOW_PLAYING_MERO, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    if (hFile != INVALID_HANDLE_VALUE) {
        memset(buf, 0, sizeof(buf));
        if (ReadFile(hFile, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
            char *line = strtok(buf, "\r\n");
            while (line) {
                if (strncmp(line, "title=", 6) == 0) {
                    MultiByteToWideChar(CP_UTF8, 0, line + 6, -1, g_title, 128);
                } else if (strncmp(line, "artist=", 7) == 0) {
                    MultiByteToWideChar(CP_UTF8, 0, line + 7, -1, g_artist, 64);
                } else if (strncmp(line, "album=", 6) == 0) {
                    MultiByteToWideChar(CP_UTF8, 0, line + 6, -1, g_album, 64);
                } else if (strncmp(line, "status=", 7) == 0) {
                    g_isPlaying = (_strnicmp(line + 7, "PLAY", 4) == 0);
                } else if (strncmp(line, "position=", 9) == 0) {
                    g_positionSec = atoi(line + 9);
                } else if (strncmp(line, "length=", 7) == 0) {
                    g_durationSec = atoi(line + 7);
                } else if (strncmp(line, "cover=", 6) == 0) {
                    MultiByteToWideChar(CP_UTF8, 0, line + 6, -1, coverPath, MAX_PATH);
                }
                line = strtok(NULL, "\r\n");
            }
        }
        CloseHandle(hFile);

        /* Refresh artwork if path changed or new */
        if (coverPath[0] != L'\0' && _wcsicmp(coverPath, g_lastCoverPath) != 0) {
            LoadCoverArt(coverPath);
        } else if (!g_hasCover) {
            if (GetFileAttributesW(COVER_FILE_SDMMC) != 0xFFFFFFFF) LoadCoverArt(COVER_FILE_SDMMC);
            else if (GetFileAttributesW(COVER_FILE_FLASH) != 0xFFFFFFFF) LoadCoverArt(COVER_FILE_FLASH);
        }
    } else {
        if (!g_hasCover) {
            if (GetFileAttributesW(COVER_FILE_SDMMC) != 0xFFFFFFFF) LoadCoverArt(COVER_FILE_SDMMC);
            else if (GetFileAttributesW(COVER_FILE_FLASH) != 0xFFFFFFFF) LoadCoverArt(COVER_FILE_FLASH);
        }
    }

    if (g_isPlaying && g_durationSec > 0) {
        g_positionSec++;
        if (g_positionSec > g_durationSec) g_positionSec = g_durationSec;
    }
}

/* UI Layout Rects */
static RECT g_rcPrev    = { 215, 172, 275, 218 };
static RECT g_rcPlay    = { 285, 168, 385, 222 };
static RECT g_rcNext    = { 395, 172, 455, 218 };
static RECT g_rcVolDown = { 430, 88,  466, 118 };
static RECT g_rcVolUp   = { 430, 48,  466, 78 };
static RECT g_rcHub     = { 385, 230, 465, 264 };

static void OnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc, memDC;
    HBITMAP memBmp, oldBmp;
    RECT rc;
    WCHAR buf[128];

    hdc = BeginPaint(hWnd, &ps);
    GetClientRect(hWnd, &rc);

    memDC = CreateCompatibleDC(hdc);
    memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    FillRect(memDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(memDC, TRANSPARENT);

    /* TOP HEADER BAR */
    SetTextColor(memDC, RGB(255, 60, 60));
    wsprintfW(buf, L"MERO // YOUTUBE MUSIC DECK [v%s]", MEDIA_VERSION);
    ExtTextOutW(memDC, 15, 8, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Playback Status Pill */
    if (g_isPlaying) {
        SetTextColor(memDC, RGB(0, 255, 128));
        ExtTextOutW(memDC, 380, 8, 0, NULL, L"[PLAYING]", 9, NULL);
    } else {
        SetTextColor(memDC, RGB(255, 180, 40));
        ExtTextOutW(memDC, 385, 8, 0, NULL, L"[PAUSED]", 8, NULL);
    }

    /* Top Divider Line */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(60, 40, 40));
        HPEN hOld = (HPEN)SelectObject(memDC, hPen);
        MoveToEx(memDC, 15, 26, NULL);
        LineTo(memDC, g_screenW - 15, 26);
        SelectObject(memDC, hOld);
        DeleteObject(hPen);
    }

    /* LEFT: ALBUM ARTWORK / VINYL DISC */
    int artX = 15, artY = 36;
    if (g_hasCover && g_coverDC) {
        BitBlt(memDC, artX, artY, ART_SIZE, ART_SIZE, g_coverDC, 0, 0, SRCCOPY);
    } else {
        /* Vector Procedural Vinyl Disc */
        RECT artBox = { artX, artY, artX + ART_SIZE, artY + ART_SIZE };
        FillRect(memDC, &artBox, (HBRUSH)GetStockObject(BLACK_BRUSH));

        HPEN hVinylPen = CreatePen(PS_SOLID, 1, RGB(35, 35, 45));
        HPEN hOld = (HPEN)SelectObject(memDC, hVinylPen);
        int r;
        for (r = 15; r < 85; r += 10) {
            Ellipse(memDC, artX + 90 - r, artY + 90 - r, artX + 90 + r, artY + 90 + r);
        }

        /* Glowing Center Label */
        COLORREF labelCol = RGB(255, 50, 50);
        HBRUSH hLabelBr = CreateSolidBrush(labelCol);
        HBRUSH hOldBr = (HBRUSH)SelectObject(memDC, hLabelBr);
        Ellipse(memDC, artX + 90 - 24, artY + 90 - 24, artX + 90 + 24, artY + 90 + 24);
        SelectObject(memDC, hOldBr);
        DeleteObject(hLabelBr);

        /* Center Spindle Hole */
        HBRUSH hHoleBr = (HBRUSH)GetStockObject(BLACK_BRUSH);
        SelectObject(memDC, hHoleBr);
        Ellipse(memDC, artX + 90 - 6, artY + 90 - 6, artX + 90 + 6, artY + 90 + 6);

        SelectObject(memDC, hOld);
        DeleteObject(hVinylPen);
    }

    /* Cybernetic Corner Brackets around Artwork */
    {
        HPEN hBracket = CreatePen(PS_SOLID, 2, RGB(0, 255, 200));
        HPEN hOld = (HPEN)SelectObject(memDC, hBracket);
        int bLen = 14;
        /* Top-Left */
        MoveToEx(memDC, artX - 3, artY + bLen, NULL);
        LineTo(memDC, artX - 3, artY - 3);
        LineTo(memDC, artX + bLen, artY - 3);
        /* Top-Right */
        MoveToEx(memDC, artX + ART_SIZE + 3 - bLen, artY - 3, NULL);
        LineTo(memDC, artX + ART_SIZE + 3, artY - 3);
        LineTo(memDC, artX + ART_SIZE + 3, artY + bLen);
        /* Bottom-Left */
        MoveToEx(memDC, artX - 3, artY + ART_SIZE + 3 - bLen, NULL);
        LineTo(memDC, artX - 3, artY + ART_SIZE + 3);
        LineTo(memDC, artX + bLen, artY + ART_SIZE + 3);
        /* Bottom-Right */
        MoveToEx(memDC, artX + ART_SIZE + 3 - bLen, artY + ART_SIZE + 3, NULL);
        LineTo(memDC, artX + ART_SIZE + 3, artY + ART_SIZE + 3);
        LineTo(memDC, artX + ART_SIZE + 3, artY + ART_SIZE + 3 - bLen);
        SelectObject(memDC, hOld);
        DeleteObject(hBracket);
    }

    /* RIGHT: METADATA & MARQUEE TITLE */
    int textX = 215;

    /* Label TRACK */
    SetTextColor(memDC, RGB(140, 140, 140));
    ExtTextOutW(memDC, textX, 36, 0, NULL, L"TRACK:", 6, NULL);

    /* Track Title with Marquee support */
    RECT titleClip = { textX, 52, 420, 78 };
    HRGN hRgn = CreateRectRgnIndirect(&titleClip);
    SelectClipRgn(memDC, hRgn);

    SetTextColor(memDC, RGB(0, 255, 255));
    ExtTextOutW(memDC, textX - g_scrollOffset, 52, 0, NULL, g_title, lstrlenW(g_title), NULL);

    SelectClipRgn(memDC, NULL);
    DeleteObject(hRgn);

    /* ARTIST */
    SetTextColor(memDC, RGB(140, 140, 140));
    ExtTextOutW(memDC, textX, 82, 0, NULL, L"ARTIST:", 7, NULL);
    SetTextColor(memDC, RGB(255, 215, 0));
    ExtTextOutW(memDC, textX, 98, 0, NULL, g_artist, lstrlenW(g_artist), NULL);

    /* ALBUM */
    SetTextColor(memDC, RGB(140, 140, 140));
    ExtTextOutW(memDC, textX, 122, 0, NULL, L"ALBUM:", 6, NULL);
    SetTextColor(memDC, RGB(200, 200, 200));
    ExtTextOutW(memDC, textX + 55, 122, 0, NULL, g_album, lstrlenW(g_album), NULL);

    /* PROGRESS BAR & TIMING */
    int progY = 145;
    int curMin = g_positionSec / 60;
    int curSec = g_positionSec % 60;
    int totMin = g_durationSec / 60;
    int totSec = g_durationSec % 60;

    SetTextColor(memDC, RGB(0, 200, 255));
    wsprintfW(buf, L"%02d:%02d / %02d:%02d", curMin, curSec, totMin, totSec);
    ExtTextOutW(memDC, textX, progY, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Progress bar background */
    int barW = 240, barH = 6;
    int barY = progY + 16;
    RECT barRc = { textX, barY, textX + barW, barY + barH };
    FillRect(memDC, &barRc, (HBRUSH)GetStockObject(DKGRAY_BRUSH));

    if (g_durationSec > 0) {
        int fillW = (g_positionSec * barW) / g_durationSec;
        if (fillW > barW) fillW = barW;
        if (fillW > 0) {
            RECT fillRc = { textX, barY, textX + fillW, barY + barH };
            HBRUSH hFillBr = CreateSolidBrush(RGB(255, 60, 60));
            FillRect(memDC, &fillRc, hFillBr);
            DeleteObject(hFillBr);
        }
    }

    /* TRANSPORT BUTTONS */
    #define DRAW_BTN(btnRc, txt, col) do { \
        HPEN hP = CreatePen(PS_SOLID, 1, (col)); \
        HPEN hOldP = (HPEN)SelectObject(memDC, hP); \
        MoveToEx(memDC, (btnRc).left, (btnRc).top, NULL); \
        LineTo(memDC, (btnRc).right, (btnRc).top); \
        LineTo(memDC, (btnRc).right, (btnRc).bottom); \
        LineTo(memDC, (btnRc).left, (btnRc).bottom); \
        LineTo(memDC, (btnRc).left, (btnRc).top); \
        SelectObject(memDC, hOldP); \
        DeleteObject(hP); \
        SetTextColor(memDC, (col)); \
        DrawTextW(memDC, (txt), -1, &(btnRc), DT_CENTER | DT_VCENTER | DT_SINGLELINE); \
    } while(0)

    DRAW_BTN(g_rcPrev,    L"|<<",                  RGB(0, 255, 200));
    DRAW_BTN(g_rcPlay,    g_isPlaying ? L"|| PAUSE" : L"> PLAY", g_isPlaying ? RGB(0, 255, 128) : RGB(255, 180, 40));
    DRAW_BTN(g_rcNext,    L">>|",                  RGB(0, 255, 200));
    DRAW_BTN(g_rcVolUp,   L"+",                    RGB(255, 215, 0));
    DRAW_BTN(g_rcVolDown, L"-",                    RGB(255, 215, 0));
    DRAW_BTN(g_rcHub,     L"<< HUB",               RGB(255, 80, 80));

    #undef DRAW_BTN

    /* FOOTER STATUS BAR */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(30, 40, 35));
        HPEN hOld = (HPEN)SelectObject(memDC, hPen);
        MoveToEx(memDC, 15, 222, NULL);
        LineTo(memDC, g_screenW - 15, 222);
        SelectObject(memDC, hOld);
        DeleteObject(hPen);
    }
    SetTextColor(memDC, RGB(120, 180, 160));
    ExtTextOutW(memDC, 15, 238, 0, NULL, g_statusMsg, lstrlenW(g_statusMsg), NULL);

    /* Atomic BitBlt to display */
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hWnd, &ps);
}

static void OnTouch(int x, int y)
{
    POINT pt = { x, y };

    if (PtInRect(&g_rcPrev, pt)) {
        SendMediaCommand("PREV");
    } else if (PtInRect(&g_rcPlay, pt)) {
        g_isPlaying = !g_isPlaying;
        SendMediaCommand("PLAY_PAUSE");
    } else if (PtInRect(&g_rcNext, pt)) {
        SendMediaCommand("NEXT");
    } else if (PtInRect(&g_rcVolUp, pt)) {
        SendMediaCommand("VOL_UP");
    } else if (PtInRect(&g_rcVolDown, pt)) {
        SendMediaCommand("VOL_DOWN");
    } else if (PtInRect(&g_rcHub, pt)) {
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

    InvalidateRect(g_hWnd, NULL, FALSE);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        g_hWnd = hWnd;
        LoadColorCalibration();

        /* Create 180x180 32bpp DIB for Artwork */
        {
            HDC hdc = GetDC(hWnd);
            BITMAPINFO bmi;
            memset(&bmi, 0, sizeof(bmi));
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = ART_SIZE;
            bmi.bmiHeader.biHeight = -ART_SIZE;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            g_hCoverBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&g_pCoverBits, NULL, 0);
            g_coverDC = CreateCompatibleDC(hdc);
            g_hOldCoverBmp = (HBITMAP)SelectObject(g_coverDC, g_hCoverBmp);
            ReleaseDC(hWnd, hdc);
        }

        PollNowPlaying();
        SetTimer(hWnd, TIMER_ID_POLL, 1000, NULL);
        SetTimer(hWnd, TIMER_ID_MARQUEE, 150, NULL);
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_ID_POLL) {
            PollNowPlaying();
            InvalidateRect(hWnd, NULL, FALSE);
        } else if (wParam == TIMER_ID_MARQUEE) {
            /* Smoothly animate text marquee if title exceeds width */
            int titleLen = lstrlenW(g_title);
            if (titleLen > 18) {
                g_scrollOffset += 3 * g_scrollDir;
                if (g_scrollOffset > (titleLen - 16) * 10) {
                    g_scrollDir = -1;
                } else if (g_scrollOffset <= 0) {
                    g_scrollOffset = 0;
                    g_scrollDir = 1;
                }
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        return 0;

    case WM_LBUTTONDOWN:
        OnTouch(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        OnPaint(hWnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, TIMER_ID_POLL);
        KillTimer(hWnd, TIMER_ID_MARQUEE);
        if (g_coverDC && g_hOldCoverBmp) SelectObject(g_coverDC, g_hOldCoverBmp);
        if (g_hCoverBmp) DeleteObject(g_hCoverBmp);
        if (g_coverDC) DeleteDC(g_coverDC);
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

    HWND hExisting = FindWindowW(L"MeroMediaWndClass", L"Mero Media");
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
    wc.lpszClassName = L"MeroMediaWndClass";

    if (!RegisterClassW(&wc)) return 1;

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"MeroMediaWndClass",
        L"Mero Media",
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
