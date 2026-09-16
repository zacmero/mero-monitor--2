/*
 * mero-media-ctrl.c - Cybernetic Companion Deck: YouTube Music Viewer & Cyberpunk Weather Terminal
 * Target: Foston FS-460BT (PE32 ARMv4 Windows CE 5.0 Core)
 * Screen: 480x272 16bpp TFT LCD
 * Aesthetic: Obsidian black, phosphor green (#00FF80), neon cyan (#00F0FF), amber (#FFB400)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#include "stb_image.h"

#define DECK_VERSION        L"2.0.0"
#define TIMER_ID_POLL       1
#define TIMER_ID_MARQUEE    2
#define TIMER_ID_CLOCK      3

#define NOW_PLAYING_SDMMC   L"\\SDMMC\\MERO\\now_playing.txt"
#define NOW_PLAYING_FLASH   L"\\ResidentFlash\\MERO\\now_playing.txt"
#define COVER_FILE_SDMMC    L"\\SDMMC\\MERO\\cover.jpg"
#define COVER_FILE_FLASH    L"\\ResidentFlash\\MERO\\cover.jpg"
#define WEATHER_FILE_SDMMC  L"\\SDMMC\\MERO\\weather.txt"
#define WEATHER_FILE_FLASH  L"\\ResidentFlash\\MERO\\weather.txt"
#define PATH_SHELL          L"\\SDMMC\\MERO\\mero-shell.exe"

typedef enum {
    VIEW_MUSIC   = 0,
    VIEW_WEATHER = 1
} DeckView;

static HINSTANCE      g_hInstance = NULL;
static HWND           g_hWnd = NULL;
static const int      g_screenW = 480;
static const int      g_screenH = 272;
static DeckView       g_currentView = VIEW_MUSIC;

/* --- Media State --- */
static WCHAR          g_title[128]  = L"Waiting for Host Stream...";
static WCHAR          g_artist[64]  = L"YouTube Music";
static WCHAR          g_album[64]   = L"Mero Companion Deck";
static BOOL           g_isPlaying   = FALSE;
static int            g_positionSec = 0;
static int            g_durationSec = 0;
static DWORD          g_lastMediaTick = 0;
static WCHAR          g_lastTrackTitle[128] = L"";

/* Marquee scrolling for long titles */
static int            g_scrollOffset = 0;
static int            g_scrollDir    = 1;
static int            g_scrollPause  = 0;

/* Artwork buffer */
#define ART_SIZE 180
static HBITMAP        g_hCoverBmp = NULL;
static DWORD         *g_pCoverBits = NULL;
static HDC            g_coverDC = NULL;
static HBITMAP        g_hOldCoverBmp = NULL;
static BOOL           g_hasCover = FALSE;
static WCHAR          g_lastCoverPath[MAX_PATH] = L"";

/* --- Weather Telemetry State --- */
static WCHAR          g_wxLocation[64]  = L"Canela, RS [BR]";
static WCHAR          g_wxCoord[64]     = L"LAT -29.36 | LON -50.81";
static WCHAR          g_wxCondition[64] = L"Clear / Sunny";
static int            g_wxTempC         = 13;
static int            g_wxFeelsC        = 13;
static int            g_wxHumidity      = 87;
static WCHAR          g_wxWind[48]      = L"6 km/h SE";
static WCHAR          g_wxPressure[32]  = L"1024 hPa";
static WCHAR          g_wxPrecip[32]    = L"0.0 mm";
static int            g_wxUV            = 0;
static WCHAR          g_wxUpdated[32]   = L"--:--";

/* 3-Day Forecast */
typedef struct {
    WCHAR day[24];
    WCHAR desc[32];
    int   minC;
    int   maxC;
} ForecastDay;

static ForecastDay    g_forecast[3] = {
    { L"TODAY", L"Sunny", 8, 15 },
    { L"TOMORROW", L"Partly Cloudy", 9, 20 },
    { L"DAY+2", L"Overcast", 13, 21 }
};

/* Calibrated Color Correction LUT */
static unsigned char  g_lut[256];
static int            g_satScale = 332;

static void LoadColorCalibration(void)
{
    int contrast = 125, brightness = 0, saturation = 130;
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
                line = strtok(NULL, "\r\n");
            }
        }
        CloseHandle(hFile);
    }

    int i;
    for (i = 0; i < 256; i++) {
        int v = i + brightness;
        v = 128 + ((v - 128) * contrast) / 100;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        g_lut[i] = (unsigned char)v;
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
    if (fileSize == 0 || fileSize > 8 * 1024 * 1024) {
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

    /* Nearest-neighbor scaling into 180x180 surface with color LUT */
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

/* --- Serial Receiver (ActiveSync mode) --- */
static HANDLE g_hSerial = INVALID_HANDLE_VALUE;
static char   g_rxLineBuf[512] = "";
static int    g_rxLineLen = 0;

static void ParseNowPlayingLine(char *payload)
{
    char *token = strtok(payload, "|");
    while (token) {
        if (strncmp(token, "status=", 7) == 0) {
            g_isPlaying = (_strnicmp(token + 7, "PLAY", 4) == 0);
        } else if (strncmp(token, "title=", 6) == 0) {
            MultiByteToWideChar(CP_UTF8, 0, token + 6, -1, g_title, 128);
        } else if (strncmp(token, "artist=", 7) == 0) {
            MultiByteToWideChar(CP_UTF8, 0, token + 7, -1, g_artist, 64);
        } else if (strncmp(token, "album=", 6) == 0) {
            MultiByteToWideChar(CP_UTF8, 0, token + 6, -1, g_album, 64);
        } else if (strncmp(token, "pos=", 4) == 0) {
            int rxPos = atoi(token + 4);
            if (rxPos > 0 || abs(rxPos - g_positionSec) > 3) {
                g_positionSec = rxPos;
                g_lastMediaTick = GetTickCount();
            }
        } else if (strncmp(token, "len=", 4) == 0) {
            g_durationSec = atoi(token + 4);
        }
        token = strtok(NULL, "|");
    }

    if (lstrcmpW(g_title, g_lastTrackTitle) != 0) {
        lstrcpynW(g_lastTrackTitle, g_title, 128);
        g_scrollOffset = 0;
        g_scrollDir = 1;
        g_scrollPause = 10;
        if (GetFileAttributesW(COVER_FILE_SDMMC) != 0xFFFFFFFF) LoadCoverArt(COVER_FILE_SDMMC);
        else if (GetFileAttributesW(COVER_FILE_FLASH) != 0xFFFFFFFF) LoadCoverArt(COVER_FILE_FLASH);
    }
}

static void ParseWeatherLine(char *payload)
{
    char *token = strtok(payload, "|");
    while (token) {
        if (strncmp(token, "temp=", 5) == 0) g_wxTempC = atoi(token + 5);
        else if (strncmp(token, "feels=", 6) == 0) g_wxFeelsC = atoi(token + 6);
        else if (strncmp(token, "hum=", 4) == 0) g_wxHumidity = atoi(token + 4);
        else if (strncmp(token, "cond=", 5) == 0) MultiByteToWideChar(CP_UTF8, 0, token + 5, -1, g_wxCondition, 64);
        else if (strncmp(token, "wind=", 5) == 0) MultiByteToWideChar(CP_UTF8, 0, token + 5, -1, g_wxWind, 48);
        else if (strncmp(token, "press=", 6) == 0) MultiByteToWideChar(CP_UTF8, 0, token + 6, -1, g_wxPressure, 32);
        else if (strncmp(token, "precip=", 7) == 0) MultiByteToWideChar(CP_UTF8, 0, token + 7, -1, g_wxPrecip, 32);
        else if (strncmp(token, "uv=", 3) == 0) g_wxUV = atoi(token + 3);
        else if (strncmp(token, "up=", 3) == 0) MultiByteToWideChar(CP_UTF8, 0, token + 3, -1, g_wxUpdated, 32);
        token = strtok(NULL, "|");
    }
}

static BOOL PollSerialStream(void)
{
    if (g_hSerial == INVALID_HANDLE_VALUE) return FALSE;

    char chunk[128];
    DWORD bytesRead = 0;
    BOOL gotPacket = FALSE;
    int loop = 4;

    while (loop-- > 0 && ReadFile(g_hSerial, chunk, sizeof(chunk) - 1, &bytesRead, NULL) && bytesRead > 0) {
        DWORD k;
        for (k = 0; k < bytesRead; k++) {
            char c = chunk[k];
            if (c == '\r') continue;
            if (c == '\n') {
                g_rxLineBuf[g_rxLineLen] = '\0';
                if (strncmp(g_rxLineBuf, "MERO:NOW:", 9) == 0) {
                    ParseNowPlayingLine(g_rxLineBuf + 9);
                    gotPacket = TRUE;
                } else if (strncmp(g_rxLineBuf, "MERO:WX:", 8) == 0) {
                    ParseWeatherLine(g_rxLineBuf + 8);
                    gotPacket = TRUE;
                }
                g_rxLineLen = 0;
            } else {
                if (g_rxLineLen < (int)sizeof(g_rxLineBuf) - 2) {
                    g_rxLineBuf[g_rxLineLen++] = c;
                }
            }
        }
    }
    return gotPacket;
}

/* --- Storage Polling (Mass Storage mode) --- */
static void PollStorageNowPlaying(void)
{
    HANDLE hFile;
    DWORD bytesRead;
    char buf[1024];

    hFile = CreateFileW(NOW_PLAYING_SDMMC, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        hFile = CreateFileW(NOW_PLAYING_FLASH, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
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
                    int rxPos = atoi(line + 9);
                    if (rxPos > 0 || abs(rxPos - g_positionSec) > 3) {
                        g_positionSec = rxPos;
                        g_lastMediaTick = GetTickCount();
                    }
                } else if (strncmp(line, "length=", 7) == 0) {
                    g_durationSec = atoi(line + 7);
                }
                line = strtok(NULL, "\r\n");
            }
        }
        CloseHandle(hFile);

        if (lstrcmpW(g_title, g_lastTrackTitle) != 0) {
            lstrcpynW(g_lastTrackTitle, g_title, 128);
            g_scrollOffset = 0;
            g_scrollDir = 1;
            g_scrollPause = 10;
            if (GetFileAttributesW(COVER_FILE_SDMMC) != 0xFFFFFFFF) LoadCoverArt(COVER_FILE_SDMMC);
            else if (GetFileAttributesW(COVER_FILE_FLASH) != 0xFFFFFFFF) LoadCoverArt(COVER_FILE_FLASH);
        } else if (!g_hasCover) {
            if (GetFileAttributesW(COVER_FILE_SDMMC) != 0xFFFFFFFF) LoadCoverArt(COVER_FILE_SDMMC);
            else if (GetFileAttributesW(COVER_FILE_FLASH) != 0xFFFFFFFF) LoadCoverArt(COVER_FILE_FLASH);
        }
    }
}

static void PollStorageWeather(void)
{
    HANDLE hFile;
    DWORD bytesRead;
    char buf[2048];

    hFile = CreateFileW(WEATHER_FILE_SDMMC, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        hFile = CreateFileW(WEATHER_FILE_FLASH, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    if (hFile != INVALID_HANDLE_VALUE) {
        memset(buf, 0, sizeof(buf));
        if (ReadFile(hFile, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
            char *line = strtok(buf, "\r\n");
            while (line) {
                if (strncmp(line, "location=", 9) == 0) MultiByteToWideChar(CP_UTF8, 0, line + 9, -1, g_wxLocation, 64);
                else if (strncmp(line, "coord=", 6) == 0) MultiByteToWideChar(CP_UTF8, 0, line + 6, -1, g_wxCoord, 64);
                else if (strncmp(line, "desc=", 5) == 0) MultiByteToWideChar(CP_UTF8, 0, line + 5, -1, g_wxCondition, 64);
                else if (strncmp(line, "temp_c=", 7) == 0) g_wxTempC = atoi(line + 7);
                else if (strncmp(line, "feels_like=", 11) == 0) g_wxFeelsC = atoi(line + 11);
                else if (strncmp(line, "humidity=", 9) == 0) g_wxHumidity = atoi(line + 9);
                else if (strncmp(line, "wind=", 5) == 0) MultiByteToWideChar(CP_UTF8, 0, line + 5, -1, g_wxWind, 48);
                else if (strncmp(line, "pressure=", 9) == 0) MultiByteToWideChar(CP_UTF8, 0, line + 9, -1, g_wxPressure, 32);
                else if (strncmp(line, "precip=", 7) == 0) MultiByteToWideChar(CP_UTF8, 0, line + 7, -1, g_wxPrecip, 32);
                else if (strncmp(line, "uv=", 3) == 0) g_wxUV = atoi(line + 3);
                else if (strncmp(line, "updated=", 8) == 0) MultiByteToWideChar(CP_UTF8, 0, line + 8, -1, g_wxUpdated, 32);
                /* Forecast days */
                else if (strncmp(line, "d1_name=", 8) == 0) MultiByteToWideChar(CP_UTF8, 0, line + 8, -1, g_forecast[0].day, 24);
                else if (strncmp(line, "d1_desc=", 8) == 0) MultiByteToWideChar(CP_UTF8, 0, line + 8, -1, g_forecast[0].desc, 32);
                else if (strncmp(line, "d1_min=", 7) == 0) g_forecast[0].minC = atoi(line + 7);
                else if (strncmp(line, "d1_max=", 7) == 0) g_forecast[0].maxC = atoi(line + 7);
                else if (strncmp(line, "d2_name=", 8) == 0) MultiByteToWideChar(CP_UTF8, 0, line + 8, -1, g_forecast[1].day, 24);
                else if (strncmp(line, "d2_desc=", 8) == 0) MultiByteToWideChar(CP_UTF8, 0, line + 8, -1, g_forecast[1].desc, 32);
                else if (strncmp(line, "d2_min=", 7) == 0) g_forecast[1].minC = atoi(line + 7);
                else if (strncmp(line, "d2_max=", 7) == 0) g_forecast[1].maxC = atoi(line + 7);
                else if (strncmp(line, "d3_name=", 8) == 0) MultiByteToWideChar(CP_UTF8, 0, line + 8, -1, g_forecast[2].day, 24);
                else if (strncmp(line, "d3_desc=", 8) == 0) MultiByteToWideChar(CP_UTF8, 0, line + 8, -1, g_forecast[2].desc, 32);
                else if (strncmp(line, "d3_min=", 7) == 0) g_forecast[2].minC = atoi(line + 7);
                else if (strncmp(line, "d3_max=", 7) == 0) g_forecast[2].maxC = atoi(line + 7);

                line = strtok(NULL, "\r\n");
            }
        }
        CloseHandle(hFile);
    }
}

/* UI Touch Target Rects */
static const RECT g_rcTabMusic   = { 12,  4, 150, 26 };
static const RECT g_rcTabWeather = { 156, 4, 294, 26 };
static const RECT g_rcBtnExit    = { 422, 4, 468, 26 };

/* --- RENDERERS --- */

/* Vector Weather Glyph Renderer */
static void DrawWeatherGlyph(HDC hdc, int x, int y, int w, int h, const WCHAR *cond)
{
    /* Bounding box */
    RECT glyphBox = { x, y, x + w, y + h };
    FillRect(hdc, &glyphBox, (HBRUSH)GetStockObject(BLACK_BRUSH));

    int cx = x + w / 2;
    int cy = y + h / 2;

    if (wcsstr(cond, L"Rain") || wcsstr(cond, L"Shower") || wcsstr(cond, L"Drizzle")) {
        /* Cloud + Rain Streaks */
        HPEN hCloudPen = CreatePen(PS_SOLID, 2, RGB(0, 200, 255));
        HPEN hOld = (HPEN)SelectObject(hdc, hCloudPen);
        HBRUSH hCloudBr = CreateSolidBrush(RGB(10, 25, 40));
        HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hCloudBr);

        Ellipse(hdc, cx - 40, cy - 25, cx + 10, cy + 15);
        Ellipse(hdc, cx - 15, cy - 35, cx + 35, cy + 15);
        RoundRect(hdc, cx - 45, cy - 10, cx + 45, cy + 16, 10, 10);

        SelectObject(hdc, hOldBr);
        DeleteObject(hCloudBr);

        /* Diagonal rain streaks */
        HPEN hRainPen = CreatePen(PS_SOLID, 2, RGB(0, 255, 200));
        SelectObject(hdc, hRainPen);
        int rx;
        for (rx = -30; rx <= 30; rx += 15) {
            MoveToEx(hdc, cx + rx, cy + 20, NULL);
            LineTo(hdc, cx + rx - 8, cy + 32);
        }
        SelectObject(hdc, hOld);
        DeleteObject(hRainPen);
        DeleteObject(hCloudPen);

    } else if (wcsstr(cond, L"Cloud") || wcsstr(cond, L"Overcast")) {
        /* Dual Cyber Clouds */
        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 255, 160));
        HPEN hOld = (HPEN)SelectObject(hdc, hPen);
        HBRUSH hBr = CreateSolidBrush(RGB(8, 28, 22));
        HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hBr);

        /* Back Cloud */
        Ellipse(hdc, cx - 20, cy - 30, cx + 25, cy + 10);
        RoundRect(hdc, cx - 25, cy - 12, cx + 35, cy + 12, 8, 8);

        /* Front Cloud */
        Ellipse(hdc, cx - 45, cy - 20, cx + 5, cy + 20);
        Ellipse(hdc, cx - 20, cy - 35, cx + 30, cy + 20);
        RoundRect(hdc, cx - 50, cy - 5, cx + 45, cy + 22, 12, 12);

        SelectObject(hdc, hOldBr);
        DeleteObject(hBr);
        SelectObject(hdc, hOld);
        DeleteObject(hPen);

    } else if (wcsstr(cond, L"Thunder") || wcsstr(cond, L"Storm")) {
        /* Storm Cloud + Lightning Bolt */
        HPEN hCloudPen = CreatePen(PS_SOLID, 2, RGB(180, 100, 255));
        HPEN hOld = (HPEN)SelectObject(hdc, hCloudPen);
        HBRUSH hCloudBr = CreateSolidBrush(RGB(20, 10, 35));
        HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hCloudBr);

        Ellipse(hdc, cx - 40, cy - 30, cx + 10, cy + 10);
        Ellipse(hdc, cx - 15, cy - 38, cx + 35, cy + 10);
        RoundRect(hdc, cx - 45, cy - 12, cx + 45, cy + 14, 10, 10);

        SelectObject(hdc, hOldBr);
        DeleteObject(hCloudBr);

        /* Lightning Bolt */
        HPEN hBoltPen = CreatePen(PS_SOLID, 2, RGB(255, 230, 0));
        SelectObject(hdc, hBoltPen);
        MoveToEx(hdc, cx - 5, cy + 14, NULL);
        LineTo(hdc, cx - 15, cy + 26);
        LineTo(hdc, cx - 5, cy + 26);
        LineTo(hdc, cx - 12, cy + 38);

        SelectObject(hdc, hOld);
        DeleteObject(hBoltPen);
        DeleteObject(hCloudPen);

    } else {
        /* Radiant Cyber Sun Core */
        HPEN hSunPen = CreatePen(PS_SOLID, 2, RGB(255, 180, 0));
        HPEN hOld = (HPEN)SelectObject(hdc, hSunPen);
        HBRUSH hSunBr = CreateSolidBrush(RGB(40, 25, 0));
        HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hSunBr);

        Ellipse(hdc, cx - 18, cy - 18, cx + 18, cy + 18);

        SelectObject(hdc, hOldBr);
        DeleteObject(hSunBr);

        /* 8 Radiating Flares */
        int r1 = 24, r2 = 34;
        /* Orthogonal */
        MoveToEx(hdc, cx, cy - r1, NULL); LineTo(hdc, cx, cy - r2);
        MoveToEx(hdc, cx, cy + r1, NULL); LineTo(hdc, cx, cy + r2);
        MoveToEx(hdc, cx - r1, cy, NULL); LineTo(hdc, cx - r2, cy);
        MoveToEx(hdc, cx + r1, cy, NULL); LineTo(hdc, cx + r2, cy);
        /* Diagonal */
        MoveToEx(hdc, cx - 17, cy - 17, NULL); LineTo(hdc, cx - 24, cy - 24);
        MoveToEx(hdc, cx + 17, cy - 17, NULL); LineTo(hdc, cx + 24, cy - 24);
        MoveToEx(hdc, cx - 17, cy + 17, NULL); LineTo(hdc, cx - 24, cy + 24);
        MoveToEx(hdc, cx + 17, cy + 17, NULL); LineTo(hdc, cx + 24, cy + 24);

        SelectObject(hdc, hOld);
        DeleteObject(hSunPen);
    }
}

/* Paint View 1: Lean YouTube Music Viewer */
static void PaintMusicView(HDC memDC)
{
    WCHAR buf[128];

    /* Left: Album Cover Art / Cyber Vinyl */
    int artX = 14, artY = 40;
    if (g_hasCover && g_coverDC) {
        BitBlt(memDC, artX, artY, ART_SIZE, ART_SIZE, g_coverDC, 0, 0, SRCCOPY);
    } else {
        /* Procedural Cybernetic Vinyl Disc */
        RECT artBox = { artX, artY, artX + ART_SIZE, artY + ART_SIZE };
        FillRect(memDC, &artBox, (HBRUSH)GetStockObject(BLACK_BRUSH));

        HPEN hVinylPen = CreatePen(PS_SOLID, 1, RGB(30, 40, 48));
        HPEN hOld = (HPEN)SelectObject(memDC, hVinylPen);
        int r;
        for (r = 15; r < 85; r += 10) {
            Ellipse(memDC, artX + 90 - r, artY + 90 - r, artX + 90 + r, artY + 90 + r);
        }

        /* Glowing Center Spindle */
        COLORREF labelCol = g_isPlaying ? RGB(0, 255, 160) : RGB(255, 60, 80);
        HBRUSH hLabelBr = CreateSolidBrush(labelCol);
        HBRUSH hOldBr = (HBRUSH)SelectObject(memDC, hLabelBr);
        Ellipse(memDC, artX + 90 - 22, artY + 90 - 22, artX + 90 + 22, artY + 90 + 22);
        SelectObject(memDC, hOldBr);
        DeleteObject(hLabelBr);

        HBRUSH hHoleBr = (HBRUSH)GetStockObject(BLACK_BRUSH);
        SelectObject(memDC, hHoleBr);
        Ellipse(memDC, artX + 90 - 6, artY + 90 - 6, artX + 90 + 6, artY + 90 + 6);

        SelectObject(memDC, hOld);
        DeleteObject(hVinylPen);
    }

    /* Cyber Corner Brackets around Artwork */
    {
        HPEN hBracket = CreatePen(PS_SOLID, 2, RGB(0, 255, 200));
        HPEN hOld = (HPEN)SelectObject(memDC, hBracket);
        int bLen = 14;
        /* TL */
        MoveToEx(memDC, artX - 3, artY + bLen, NULL);
        LineTo(memDC, artX - 3, artY - 3);
        LineTo(memDC, artX + bLen, artY - 3);
        /* TR */
        MoveToEx(memDC, artX + ART_SIZE + 3 - bLen, artY - 3, NULL);
        LineTo(memDC, artX + ART_SIZE + 3, artY - 3);
        LineTo(memDC, artX + ART_SIZE + 3, artY + bLen);
        /* BL */
        MoveToEx(memDC, artX - 3, artY + ART_SIZE + 3 - bLen, NULL);
        LineTo(memDC, artX - 3, artY + ART_SIZE + 3);
        LineTo(memDC, artX + bLen, artY + ART_SIZE + 3);
        /* BR */
        MoveToEx(memDC, artX + ART_SIZE + 3 - bLen, artY + ART_SIZE + 3, NULL);
        LineTo(memDC, artX + ART_SIZE + 3, artY + ART_SIZE + 3);
        LineTo(memDC, artX + ART_SIZE + 3, artY + ART_SIZE + 3 - bLen);
        SelectObject(memDC, hOld);
        DeleteObject(hBracket);
    }

    /* Sub-art label */
    SetTextColor(memDC, RGB(0, 140, 120));
    ExtTextOutW(memDC, artX + 22, artY + ART_SIZE + 7, 0, NULL, L"YOUTUBE MUSIC AUDIO STREAM", 26, NULL);

    /* Right: Metadata Matrix (x = 210 to 468) */
    int textX = 212;

    /* Playback Status Badge */
    RECT badgeRc = { textX, 38, textX + 105, 56 };
    COLORREF badgeBorderCol = g_isPlaying ? RGB(0, 255, 128) : RGB(255, 180, 40);
    COLORREF badgeBgCol     = g_isPlaying ? RGB(0, 35, 18)   : RGB(35, 24, 0);

    HBRUSH hBadgeBr = CreateSolidBrush(badgeBgCol);
    FillRect(memDC, &badgeRc, hBadgeBr);
    DeleteObject(hBadgeBr);

    HPEN hBadgePen = CreatePen(PS_SOLID, 1, badgeBorderCol);
    HPEN hOldPen = (HPEN)SelectObject(memDC, hBadgePen);
    RoundRect(memDC, badgeRc.left, badgeRc.top, badgeRc.right, badgeRc.bottom, 4, 4);
    SelectObject(memDC, hOldPen);
    DeleteObject(hBadgePen);

    SetTextColor(memDC, badgeBorderCol);
    DrawTextW(memDC, g_isPlaying ? L"● PLAYING" : L"❚❚ PAUSED", -1, &badgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    /* Digital Time position right beside badge */
    int curMin = g_positionSec / 60;
    int curSec = g_positionSec % 60;
    int totMin = g_durationSec / 60;
    int totSec = g_durationSec % 60;

    wsprintfW(buf, L"%02d:%02d / %02d:%02d", curMin, curSec, totMin, totSec);
    SetTextColor(memDC, RGB(0, 240, 255));
    ExtTextOutW(memDC, textX + 120, 40, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Track Title with Smooth Marquee */
    SetTextColor(memDC, RGB(120, 150, 160));
    ExtTextOutW(memDC, textX, 64, 0, NULL, L"TRACK:", 6, NULL);

    RECT titleClip = { textX, 80, 468, 106 };
    HRGN hRgn = CreateRectRgnIndirect(&titleClip);
    SelectClipRgn(memDC, hRgn);

    SetTextColor(memDC, RGB(255, 255, 255));
    ExtTextOutW(memDC, textX - g_scrollOffset, 80, 0, NULL, g_title, lstrlenW(g_title), NULL);

    SelectClipRgn(memDC, NULL);
    DeleteObject(hRgn);

    /* ARTIST */
    SetTextColor(memDC, RGB(120, 150, 160));
    ExtTextOutW(memDC, textX, 110, 0, NULL, L"ARTIST:", 7, NULL);
    SetTextColor(memDC, RGB(255, 215, 0));
    ExtTextOutW(memDC, textX + 55, 110, 0, NULL, g_artist, lstrlenW(g_artist), NULL);

    /* ALBUM */
    SetTextColor(memDC, RGB(120, 150, 160));
    ExtTextOutW(memDC, textX, 132, 0, NULL, L"ALBUM:", 6, NULL);
    SetTextColor(memDC, RGB(180, 220, 230));
    ExtTextOutW(memDC, textX + 55, 132, 0, NULL, g_album, lstrlenW(g_album), NULL);

    /* Sleek Cyber Progress Bar */
    int progY = 160;
    int barW = 250, barH = 8;
    RECT barRc = { textX, progY, textX + barW, progY + barH };
    FillRect(memDC, &barRc, (HBRUSH)GetStockObject(DKGRAY_BRUSH));

    if (g_durationSec > 0) {
        int fillW = (g_positionSec * barW) / g_durationSec;
        if (fillW > barW) fillW = barW;
        if (fillW > 0) {
            RECT fillRc = { textX, progY, textX + fillW, progY + barH };
            HBRUSH hFillBr = CreateSolidBrush(RGB(0, 240, 255));
            FillRect(memDC, &fillRc, hFillBr);
            DeleteObject(hFillBr);

            /* Glowing white leading head */
            if (fillW >= 3) {
                RECT headRc = { textX + fillW - 3, progY - 1, textX + fillW, progY + barH + 1 };
                HBRUSH hHeadBr = CreateSolidBrush(RGB(255, 255, 255));
                FillRect(memDC, &headRc, hHeadBr);
                DeleteObject(hHeadBr);
            }
        }
    }

    /* Ambient Telemetry Readout beneath Progress */
    SetTextColor(memDC, RGB(0, 180, 140));
    wsprintfW(buf, L"COMPANION HUD // SYNC: ACTIVE // TAP TO VIEW WEATHER");
    ExtTextOutW(memDC, textX, 180, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Bottom Decorative Divider */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(20, 40, 35));
        HPEN hOld = (HPEN)SelectObject(memDC, hPen);
        MoveToEx(memDC, textX, 205, NULL);
        LineTo(memDC, g_screenW - 14, 205);
        SelectObject(memDC, hOld);
        DeleteObject(hPen);
    }

    SetTextColor(memDC, RGB(100, 140, 130));
    ExtTextOutW(memDC, textX, 212, 0, NULL, L"WEATHER FORECAST QUICK TELEMETRY:", 33, NULL);
    wsprintfW(buf, L"%ls: %d°C (%ls) | WIND %ls", g_wxLocation, g_wxTempC, g_wxCondition, g_wxWind);
    SetTextColor(memDC, RGB(0, 255, 180));
    ExtTextOutW(memDC, textX, 226, 0, NULL, buf, lstrlenW(buf), NULL);
}

/* Paint View 2: Elegant Cyberpunk Weather Terminal */
static void PaintWeatherView(HDC memDC)
{
    WCHAR buf[128];

    /* Left Section: Core Atmosphere (x = 14 to 195) */
    int leftX = 14;

    /* Location Banner */
    SetTextColor(memDC, RGB(0, 255, 160));
    ExtTextOutW(memDC, leftX, 36, 0, NULL, g_wxLocation, lstrlenW(g_wxLocation), NULL);

    SetTextColor(memDC, RGB(0, 120, 80));
    ExtTextOutW(memDC, leftX, 50, 0, NULL, g_wxCoord, lstrlenW(g_wxCoord), NULL);

    /* Big Temperature Readout */
    wsprintfW(buf, L"%d°C", g_wxTempC);
    SetTextColor(memDC, RGB(0, 245, 255));
    ExtTextOutW(memDC, leftX, 68, 0, NULL, buf, lstrlenW(buf), NULL);

    wsprintfW(buf, L"FEELS %d°C", g_wxFeelsC);
    SetTextColor(memDC, RGB(140, 220, 210));
    ExtTextOutW(memDC, leftX + 75, 72, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Condition Pill */
    RECT condRc = { leftX, 94, leftX + 175, 114 };
    HBRUSH hCondBr = CreateSolidBrush(RGB(10, 28, 22));
    FillRect(memDC, &condRc, hCondBr);
    DeleteObject(hCondBr);

    HPEN hCondPen = CreatePen(PS_SOLID, 1, RGB(0, 200, 140));
    HPEN hOld = (HPEN)SelectObject(memDC, hCondPen);
    RoundRect(memDC, condRc.left, condRc.top, condRc.right, condRc.bottom, 4, 4);
    SelectObject(memDC, hOld);
    DeleteObject(hCondPen);

    SetTextColor(memDC, RGB(0, 255, 180));
    DrawTextW(memDC, g_wxCondition, -1, &condRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    /* Vector Cyber Weather Glyph (x=14, y=120, w=175, h=85) */
    DrawWeatherGlyph(memDC, leftX, 120, 175, 85, g_wxCondition);

    /* High / Low Today */
    wsprintfW(buf, L"TODAY: ▲ %d°C  |  ▼ %d°C", g_forecast[0].maxC, g_forecast[0].minC);
    SetTextColor(memDC, RGB(255, 215, 0));
    ExtTextOutW(memDC, leftX + 10, 214, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Vertical Divider between Left and Right Sections */
    {
        HPEN hDivPen = CreatePen(PS_SOLID, 1, RGB(25, 45, 40));
        HPEN hOldP = (HPEN)SelectObject(memDC, hDivPen);
        MoveToEx(memDC, 200, 36, NULL);
        LineTo(memDC, 200, 240);
        SelectObject(memDC, hOldP);
        DeleteObject(hDivPen);
    }

    /* Right Section: Telemetry Matrix & 3-Day Vector (x = 212 to 468) */
    int rightX = 212;

    SetTextColor(memDC, RGB(0, 255, 160));
    ExtTextOutW(memDC, rightX, 36, 0, NULL, L"ATMOSPHERIC TELEMETRY MATRIX", 28, NULL);

    /* Grid Row 1: Humidity */
    SetTextColor(memDC, RGB(120, 160, 150));
    ExtTextOutW(memDC, rightX, 54, 0, NULL, L"HUMIDITY:", 9, NULL);
    wsprintfW(buf, L"%d%%", g_wxHumidity);
    SetTextColor(memDC, RGB(0, 255, 220));
    ExtTextOutW(memDC, rightX + 65, 54, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Mini Cyber Bar for Humidity */
    int humBarX = rightX + 110, humBarY = 57, humBarW = 140, humBarH = 7;
    RECT humRc = { humBarX, humBarY, humBarX + humBarW, humBarY + humBarH };
    FillRect(memDC, &humRc, (HBRUSH)GetStockObject(DKGRAY_BRUSH));
    int humFill = (g_wxHumidity * humBarW) / 100;
    if (humFill > humBarW) humFill = humBarW;
    if (humFill > 0) {
        RECT humFillRc = { humBarX, humBarY, humBarX + humFill, humBarY + humBarH };
        HBRUSH hHBr = CreateSolidBrush(RGB(0, 255, 180));
        FillRect(memDC, &humFillRc, hHBr);
        DeleteObject(hHBr);
    }

    /* Grid Row 2: Wind */
    SetTextColor(memDC, RGB(120, 160, 150));
    ExtTextOutW(memDC, rightX, 70, 0, NULL, L"WIND:", 5, NULL);
    SetTextColor(memDC, RGB(0, 240, 255));
    ExtTextOutW(memDC, rightX + 65, 70, 0, NULL, g_wxWind, lstrlenW(g_wxWind), NULL);

    /* Grid Row 3: Barometer & Precipitation */
    SetTextColor(memDC, RGB(120, 160, 150));
    ExtTextOutW(memDC, rightX, 86, 0, NULL, L"PRESSURE:", 9, NULL);
    SetTextColor(memDC, RGB(180, 240, 200));
    ExtTextOutW(memDC, rightX + 65, 86, 0, NULL, g_wxPressure, lstrlenW(g_wxPressure), NULL);

    SetTextColor(memDC, RGB(120, 160, 150));
    ExtTextOutW(memDC, rightX + 155, 86, 0, NULL, L"PRECIP:", 7, NULL);
    SetTextColor(memDC, RGB(180, 240, 200));
    ExtTextOutW(memDC, rightX + 205, 86, 0, NULL, g_wxPrecip, lstrlenW(g_wxPrecip), NULL);

    /* Grid Row 4: UV Index */
    SetTextColor(memDC, RGB(120, 160, 150));
    ExtTextOutW(memDC, rightX, 102, 0, NULL, L"UV INDEX:", 9, NULL);
    wsprintfW(buf, L"%d [LOW]", g_wxUV);
    SetTextColor(memDC, RGB(255, 215, 0));
    ExtTextOutW(memDC, rightX + 65, 102, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Forecast Divider */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(25, 45, 40));
        HPEN hOldP = (HPEN)SelectObject(memDC, hPen);
        MoveToEx(memDC, rightX, 120, NULL);
        LineTo(memDC, g_screenW - 14, 120);
        SelectObject(memDC, hOldP);
        DeleteObject(hPen);
    }

    SetTextColor(memDC, RGB(0, 255, 160));
    ExtTextOutW(memDC, rightX, 126, 0, NULL, L"SYNOPTIC OUTLOOK // 3-DAY VECTOR", 32, NULL);

    /* 3 Forecast Cards Side-by-Side */
    int cardY = 144, cardW = 80, cardH = 92, cardGap = 8;
    int f;
    for (f = 0; f < 3; f++) {
        int cardX = rightX + f * (cardW + cardGap);
        RECT cRc = { cardX, cardY, cardX + cardW, cardY + cardH };

        /* Card background & outline */
        HBRUSH hCardBr = CreateSolidBrush(RGB(8, 20, 18));
        FillRect(memDC, &cRc, hCardBr);
        DeleteObject(hCardBr);

        HPEN hCardPen = CreatePen(PS_SOLID, 1, RGB(0, 180, 120));
        HPEN hOldP = (HPEN)SelectObject(memDC, hCardPen);
        RoundRect(memDC, cRc.left, cRc.top, cRc.right, cRc.bottom, 4, 4);
        SelectObject(memDC, hOldP);
        DeleteObject(hCardPen);

        /* Day Name */
        SetTextColor(memDC, RGB(0, 255, 240));
        RECT dayRc = { cardX + 2, cardY + 6, cardX + cardW - 2, cardY + 22 };
        DrawTextW(memDC, g_forecast[f].day, -1, &dayRc, DT_CENTER | DT_TOP | DT_SINGLELINE);

        /* Temp Range */
        wsprintfW(buf, L"%d° / %d°", g_forecast[f].maxC, g_forecast[f].minC);
        SetTextColor(memDC, RGB(255, 215, 0));
        RECT tempRc = { cardX + 2, cardY + 28, cardX + cardW - 2, cardY + 44 };
        DrawTextW(memDC, buf, -1, &tempRc, DT_CENTER | DT_TOP | DT_SINGLELINE);

        /* Description */
        SetTextColor(memDC, RGB(140, 210, 190));
        RECT descRc = { cardX + 4, cardY + 50, cardX + cardW - 4, cardY + cardH - 4 };
        DrawTextW(memDC, g_forecast[f].desc, -1, &descRc, DT_CENTER | DT_WORDBREAK);
    }
}

/* Main Paint Routine */
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

    /* --- TOP NAVIGATION BAR --- */
    /* Tab 1: NOW PLAYING */
    BOOL isMusicActive = (g_currentView == VIEW_MUSIC);
    COLORREF musicTabCol = isMusicActive ? RGB(0, 255, 220) : RGB(0, 140, 120);
    COLORREF musicBgCol  = isMusicActive ? RGB(0, 40, 36)   : RGB(10, 18, 16);

    HBRUSH hMBr = CreateSolidBrush(musicBgCol);
    FillRect(memDC, &g_rcTabMusic, hMBr);
    DeleteObject(hMBr);

    HPEN hMPen = CreatePen(PS_SOLID, 1, musicTabCol);
    HPEN hOldP = (HPEN)SelectObject(memDC, hMPen);
    RoundRect(memDC, g_rcTabMusic.left, g_rcTabMusic.top, g_rcTabMusic.right, g_rcTabMusic.bottom, 4, 4);
    SelectObject(memDC, hOldP);
    DeleteObject(hMPen);

    SetTextColor(memDC, musicTabCol);
    DrawTextW(memDC, L"♫ NOW PLAYING", -1, (LPRECT)&g_rcTabMusic, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    /* Tab 2: WEATHER HUD */
    BOOL isWxActive = (g_currentView == VIEW_WEATHER);
    COLORREF wxTabCol = isWxActive ? RGB(0, 255, 160) : RGB(0, 140, 120);
    COLORREF wxBgCol  = isWxActive ? RGB(0, 36, 26)   : RGB(10, 18, 16);

    HBRUSH hWBr = CreateSolidBrush(wxBgCol);
    FillRect(memDC, &g_rcTabWeather, hWBr);
    DeleteObject(hWBr);

    HPEN hWPen = CreatePen(PS_SOLID, 1, wxTabCol);
    hOldP = (HPEN)SelectObject(memDC, hWPen);
    RoundRect(memDC, g_rcTabWeather.left, g_rcTabWeather.top, g_rcTabWeather.right, g_rcTabWeather.bottom, 4, 4);
    SelectObject(memDC, hOldP);
    DeleteObject(hWPen);

    SetTextColor(memDC, wxTabCol);
    DrawTextW(memDC, L"⚡ WEATHER HUD", -1, (LPRECT)&g_rcTabWeather, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    /* Live Digital Clock in Header Center */
    SYSTEMTIME st;
    GetLocalTime(&st);
    wsprintfW(buf, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    SetTextColor(memDC, RGB(0, 255, 200));
    ExtTextOutW(memDC, 310, 7, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Exit / Hub Button */
    HBRUSH hExBr = CreateSolidBrush(RGB(35, 15, 15));
    FillRect(memDC, &g_rcBtnExit, hExBr);
    DeleteObject(hExBr);

    HPEN hExPen = CreatePen(PS_SOLID, 1, RGB(255, 70, 70));
    hOldP = (HPEN)SelectObject(memDC, hExPen);
    RoundRect(memDC, g_rcBtnExit.left, g_rcBtnExit.top, g_rcBtnExit.right, g_rcBtnExit.bottom, 4, 4);
    SelectObject(memDC, hOldP);
    DeleteObject(hExPen);

    SetTextColor(memDC, RGB(255, 80, 80));
    DrawTextW(memDC, L"HUB [X]", -1, (LPRECT)&g_rcBtnExit, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    /* Top Divider Line */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(25, 45, 40));
        hOldP = (HPEN)SelectObject(memDC, hPen);
        MoveToEx(memDC, 12, 29, NULL);
        LineTo(memDC, g_screenW - 12, 29);
        SelectObject(memDC, hOldP);
        DeleteObject(hPen);
    }

    /* --- RENDER ACTIVE VIEW --- */
    if (g_currentView == VIEW_MUSIC) {
        PaintMusicView(memDC);
    } else {
        PaintWeatherView(memDC);
    }

    /* --- FOOTER STATUS BAR --- */
    {
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(20, 35, 30));
        hOldP = (HPEN)SelectObject(memDC, hPen);
        MoveToEx(memDC, 12, 246, NULL);
        LineTo(memDC, g_screenW - 12, 246);
        SelectObject(memDC, hOldP);
        DeleteObject(hPen);
    }

    SetTextColor(memDC, RGB(80, 140, 120));
    if (g_currentView == VIEW_MUSIC) {
        wsprintfW(buf, L"MERO DECK [v%s] // YOUTUBE MUSIC // TAP TO TOGGLE WEATHER", DECK_VERSION);
    } else {
        wsprintfW(buf, L"MERO DECK [v%s] // METEOROLOGY // SYNCED: %ls // TAP FOR MUSIC", DECK_VERSION, g_wxUpdated);
    }
    ExtTextOutW(memDC, 14, 252, 0, NULL, buf, lstrlenW(buf), NULL);

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

    /* Tab: Music */
    if (PtInRect(&g_rcTabMusic, pt)) {
        g_currentView = VIEW_MUSIC;
        InvalidateRect(g_hWnd, NULL, FALSE);
        return;
    }

    /* Tab: Weather */
    if (PtInRect(&g_rcTabWeather, pt)) {
        g_currentView = VIEW_WEATHER;
        InvalidateRect(g_hWnd, NULL, FALSE);
        return;
    }

    /* Exit to Hub */
    if (PtInRect(&g_rcBtnExit, pt)) {
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

    /* Tapping body toggles between Music and Weather */
    if (y > 30 && y < 246) {
        g_currentView = (g_currentView == VIEW_MUSIC) ? VIEW_WEATHER : VIEW_MUSIC;
        InvalidateRect(g_hWnd, NULL, FALSE);
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        g_hWnd = hWnd;
        LoadColorCalibration();

        /* Create 180x180 32bpp DIB Section for Artwork */
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

        /* Initial data poll */
        PollStorageNowPlaying();
        PollStorageWeather();

        /* Timers: Poll (1s), Marquee (150ms), Clock (1s) */
        SetTimer(hWnd, TIMER_ID_POLL, 1000, NULL);
        SetTimer(hWnd, TIMER_ID_MARQUEE, 150, NULL);
        SetTimer(hWnd, TIMER_ID_CLOCK, 1000, NULL);
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_ID_POLL) {
            /* Check serial if active, else storage */
            if (!PollSerialStream()) {
                PollStorageNowPlaying();
                PollStorageWeather();
            }

            /* Smooth local clock tick for song position */
            if (g_isPlaying) {
                DWORD nowTick = GetTickCount();
                if (g_lastMediaTick == 0) g_lastMediaTick = nowTick;
                DWORD elapsed = nowTick - g_lastMediaTick;
                if (elapsed >= 1000) {
                    int sec = (int)(elapsed / 1000);
                    g_positionSec += sec;
                    if (g_durationSec > 0 && g_positionSec > g_durationSec) {
                        g_positionSec = g_durationSec;
                    }
                    g_lastMediaTick = nowTick - (elapsed % 1000);
                }
            } else {
                g_lastMediaTick = GetTickCount();
            }

            InvalidateRect(hWnd, NULL, FALSE);
        } else if (wParam == TIMER_ID_MARQUEE) {
            /* Marquee scroll title when long */
            if (g_currentView == VIEW_MUSIC && lstrlenW(g_title) > 18) {
                if (g_scrollPause > 0) {
                    g_scrollPause--;
                } else {
                    g_scrollOffset += (g_scrollDir * 2);
                    int maxScroll = (lstrlenW(g_title) * 8) - 180;
                    if (maxScroll < 0) maxScroll = 0;

                    if (g_scrollOffset > maxScroll) {
                        g_scrollOffset = maxScroll;
                        g_scrollDir = -1;
                        g_scrollPause = 12;
                    } else if (g_scrollOffset < 0) {
                        g_scrollOffset = 0;
                        g_scrollDir = 1;
                        g_scrollPause = 12;
                    }
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
        } else if (wParam == TIMER_ID_CLOCK) {
            /* Update clock in header */
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
        KillTimer(hWnd, TIMER_ID_POLL);
        KillTimer(hWnd, TIMER_ID_MARQUEE);
        KillTimer(hWnd, TIMER_ID_CLOCK);

        if (g_coverDC) {
            SelectObject(g_coverDC, g_hOldCoverBmp);
            DeleteDC(g_coverDC);
            g_coverDC = NULL;
        }
        if (g_hCoverBmp) {
            DeleteObject(g_hCoverBmp);
            g_hCoverBmp = NULL;
        }
        if (g_hSerial != INVALID_HANDLE_VALUE) {
            CloseHandle(g_hSerial);
            g_hSerial = INVALID_HANDLE_VALUE;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;

    g_hInstance = hInstance;

    /* Prevent multiple instances */
    HWND hExisting = FindWindowW(L"MERO_COMPANION_DECK", NULL);
    if (hExisting) {
        SetForegroundWindow(hExisting);
        return 0;
    }

    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"MERO_COMPANION_DECK";

    if (!RegisterClassW(&wc)) return 1;

    HWND hWnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"MERO_COMPANION_DECK",
        L"Mero Companion Deck",
        WS_VISIBLE | WS_POPUP,
        0, 0, g_screenW, g_screenH,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) return 1;

    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
