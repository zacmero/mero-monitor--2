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
static WCHAR          g_lastTrackTitle[128] = L"";

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

static int    g_cmdSeq = 0;
static HANDLE g_hSerial = INVALID_HANDLE_VALUE;
static WCHAR  g_activeSerialPort[32] = L"";
static DWORD  g_lastSerialRxTime = 0;
static int    g_candidateIdx = 0;
static DWORD  g_rxPacketCount = 0;
static DWORD  g_lastLocalTick = 0;

static WCHAR  g_candidatePorts[12][32];
static int    g_candidateCount = 0;

static void InitCandidatePorts(void)
{
    g_candidateCount = 0;

    /* 1. Discover active serial stream drivers directly from HKLM\Drivers\Active */
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Drivers\\Active", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        WCHAR subKey[64];
        DWORD subLen;

        while (1) {
            subLen = sizeof(subKey) / sizeof(subKey[0]);
            if (RegEnumKeyExW(hKey, index++, subKey, &subLen, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
                break;
            }

            HKEY hSub;
            if (RegOpenKeyExW(hKey, subKey, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
                WCHAR keyVal[128] = { 0 };
                WCHAR nameVal[64] = { 0 };
                DWORD kLen = sizeof(keyVal);
                DWORD nLen = sizeof(nameVal);

                RegQueryValueExW(hSub, L"Key", NULL, NULL, (BYTE*)keyVal, &kLen);
                RegQueryValueExW(hSub, L"Name", NULL, NULL, (BYTE*)nameVal, &nLen);
                RegCloseKey(hSub);

                /* Filter out GPS (COM1), UART (COM2), TMC (COM3) to avoid hardware bus lockup */
                if (wcsicmp(nameVal, L"COM1:") == 0 || wcsicmp(nameVal, L"COM2:") == 0 || wcsicmp(nameVal, L"COM3:") == 0) {
                    continue;
                }

                if (nameVal[0] != L'\0' && (wcsstr(nameVal, L"COM") || wcsstr(nameVal, L"USB") || wcsstr(nameVal, L"VCP"))) {
                    if (g_candidateCount < 12) {
                        lstrcpynW(g_candidatePorts[g_candidateCount++], nameVal, 32);
                    }
                }
            }
        }
        RegCloseKey(hKey);
    }

    /* 2. Add standard known virtual USB COM candidates if not already present */
    static const WCHAR *defaults[] = {
        L"COM4:", L"COM5:", L"COM7:", L"COM8:", L"COM9:", L"COM6:", L"COM0:",
        L"USBSER1:", L"USBFN1:", L"$device\\COM4", L"$device\\COM5", L"$device\\COM7"
    };
    int i, j;
    for (i = 0; i < 12 && g_candidateCount < 12; i++) {
        BOOL exists = FALSE;
        for (j = 0; j < g_candidateCount; j++) {
            if (wcsicmp(g_candidatePorts[j], defaults[i]) == 0) {
                exists = TRUE;
                break;
            }
        }
        if (!exists) {
            lstrcpynW(g_candidatePorts[g_candidateCount++], defaults[i], 32);
        }
    }
}

static BOOL OpenSerialPort(const WCHAR *portName)
{
    if (g_hSerial != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hSerial);
        g_hSerial = INVALID_HANDLE_VALUE;
    }

    g_hSerial = CreateFileW(
        portName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (g_hSerial == INVALID_HANDLE_VALUE) return FALSE;

    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (GetCommState(g_hSerial, &dcb)) {
        dcb.BaudRate = CBR_115200;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fBinary = TRUE;
        dcb.fParity = FALSE;
        dcb.fOutxCtsFlow = FALSE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fDtrControl = DTR_CONTROL_DISABLE;
        dcb.fDsrSensitivity = FALSE;
        dcb.fTXContinueOnXoff = TRUE;
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
        dcb.fErrorChar = FALSE;
        dcb.fNull = FALSE;
        dcb.fRtsControl = RTS_CONTROL_DISABLE;
        dcb.fAbortOnError = FALSE;
        SetCommState(g_hSerial, &dcb);
    }

    /* True non-blocking timeouts: read returns immediately, write caps at 25ms */
    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 25;
    SetCommTimeouts(g_hSerial, &timeouts);

    PurgeComm(g_hSerial, PURGE_TXCLEAR | PURGE_RXCLEAR);

    lstrcpynW(g_activeSerialPort, portName, 32);
    return TRUE;
}

static void EnsureSerialConnected(void)
{
    DWORD now = GetTickCount();

    if (g_candidateCount <= 0) {
        InitCandidatePorts();
    }

    if (g_hSerial != INVALID_HANDLE_VALUE) {
        /* If port opened but received no valid packets in 4s, cycle to next candidate */
        if (g_lastSerialRxTime > 0 && (now - g_lastSerialRxTime) > 4000) {
            CloseHandle(g_hSerial);
            g_hSerial = INVALID_HANDLE_VALUE;
            if (g_candidateCount > 0) {
                g_candidateIdx = (g_candidateIdx + 1) % g_candidateCount;
            }
            g_lastSerialRxTime = 0;
        } else {
            return;
        }
    }

    if (g_candidateCount <= 0) return;

    int tries;
    for (tries = 0; tries < g_candidateCount; tries++) {
        const WCHAR *port = g_candidatePorts[g_candidateIdx];
        if (OpenSerialPort(port)) {
            DWORD written = 0;
            WriteFile(g_hSerial, "MERO:PING\n", 10, &written, NULL);
            /* NO FlushFileBuffers */
            g_lastSerialRxTime = now;
            break;
        }
        g_candidateIdx = (g_candidateIdx + 1) % g_candidateCount;
    }
}

static char g_rxLineBuf[512] = "";
static int  g_rxLineLen = 0;

static BOOL PollSerialNowPlaying(void)
{
    if (g_hSerial == INVALID_HANDLE_VALUE) {
        EnsureSerialConnected();
        if (g_hSerial == INVALID_HANDLE_VALUE) return FALSE;
    }

    char chunk[128];
    DWORD bytesRead = 0;
    BOOL gotPacket = FALSE;
    int readLoop = 4;

    while (readLoop-- > 0 && ReadFile(g_hSerial, chunk, sizeof(chunk) - 1, &bytesRead, NULL) && bytesRead > 0) {
        DWORD k;
        for (k = 0; k < bytesRead; k++) {
            char c = chunk[k];
            if (c == '\r') continue;
            if (c == '\n') {
                g_rxLineBuf[g_rxLineLen] = '\0';
                if (strncmp(g_rxLineBuf, "MERO:NOW:", 9) == 0) {
                    char *payload = g_rxLineBuf + 9;
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
                            /* Only accept the remote position if:
                               - it is positive (not a stale 0 from Firefox MPRIS), OR
                               - local clock has drifted more than 3s from it (resync) */
                            if (rxPos > 0 || abs(rxPos - g_positionSec) > 3) {
                                g_positionSec = rxPos;
                                g_lastLocalTick = GetTickCount();
                            }
                        } else if (strncmp(token, "len=", 4) == 0) {
                            g_durationSec = atoi(token + 4);
                        }
                        token = strtok(NULL, "|");
                    }
                    gotPacket = TRUE;
                    g_rxPacketCount++;
                }
                g_rxLineLen = 0;
            } else {
                if (g_rxLineLen < (int)sizeof(g_rxLineBuf) - 2) {
                    g_rxLineBuf[g_rxLineLen++] = c;
                }
            }
        }
    }

    if (gotPacket) {
        g_lastSerialRxTime = GetTickCount();

        /* Reload cover art whenever title changes (serial path never did this before) */
        if (lstrcmpW(g_title, g_lastTrackTitle) != 0) {
            lstrcpynW(g_lastTrackTitle, g_title, 128);
            if (GetFileAttributesW(COVER_FILE_SDMMC) != 0xFFFFFFFF)
                LoadCoverArt(COVER_FILE_SDMMC);
            else if (GetFileAttributesW(COVER_FILE_FLASH) != 0xFFFFFFFF)
                LoadCoverArt(COVER_FILE_FLASH);
        } else if (!g_hasCover) {
            if (GetFileAttributesW(COVER_FILE_SDMMC) != 0xFFFFFFFF)
                LoadCoverArt(COVER_FILE_SDMMC);
        }

        InvalidateRect(g_hWnd, NULL, FALSE);
    }
    return gotPacket;
}

/* Dispatch command to host bridge */
static void SendMediaCommand(const char *cmdName)
{
    g_cmdSeq++;

    /* 1. DISPATCH OVER SERIAL (ActiveSync / Vsync mode) */
    EnsureSerialConnected();
    if (g_hSerial != INVALID_HANDLE_VALUE) {
        char sMsg[64];
        DWORD sWritten = 0;
        sprintf(sMsg, "MERO:CMD:%s:%d\n", cmdName, g_cmdSeq);
        WriteFile(g_hSerial, sMsg, (DWORD)strlen(sMsg), &sWritten, NULL);
        /* NO FlushFileBuffers! */
    }

    /* 2. DISPATCH OVER STORAGE MAILBOX (Mass Storage mode) */
    HANDLE hFile;
    DWORD written;
    char payload[128];
    sprintf(payload, "seq=%d\r\ncmd=%s\r\n", g_cmdSeq, cmdName);

    CreateDirectoryW(L"\\SDMMC\\MERO", NULL);
    CreateDirectoryW(L"\\ResidentFlash\\MERO", NULL);
    CreateDirectoryW(L"\\SDMMC\\Stream", NULL);
    CreateDirectoryW(L"\\ResidentFlash\\Stream", NULL);

    /* Dispatch to SDMMC MERO */
    hFile = CreateFileW(CMD_FILE_SDMMC, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, payload, (DWORD)strlen(payload), &written, NULL);
        CloseHandle(hFile);
    }

    /* Dispatch to SDMMC Stream */
    hFile = CreateFileW(L"\\SDMMC\\Stream\\media_cmd.txt", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, payload, (DWORD)strlen(payload), &written, NULL);
        CloseHandle(hFile);
    }

    /* Dispatch to ResidentFlash MERO */
    hFile = CreateFileW(CMD_FILE_FLASH, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, payload, (DWORD)strlen(payload), &written, NULL);
        CloseHandle(hFile);
    }

    /* Dispatch to ResidentFlash Stream */
    hFile = CreateFileW(L"\\ResidentFlash\\Stream\\media_cmd.txt", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, payload, (DWORD)strlen(payload), &written, NULL);
        CloseHandle(hFile);
    }

    /* Tactile status feedback */
    wsprintfW(g_statusMsg, L"CMD #%d: %hs [SENT]", g_cmdSeq, cmdName);
    InvalidateRect(g_hWnd, NULL, FALSE);
}

/* Poll track updates via Serial or SDMMC/Flash */
static void PollNowPlaying(void)
{
    /* Check serial first (ActiveSync mode) */
    if (PollSerialNowPlaying()) {
        return;
    }

    /* Fallback to storage polling (Mass Storage mode) */
    HANDLE hFile;
    DWORD bytesRead;
    char buf[1024];
    WCHAR coverPath[MAX_PATH] = L"";
    BOOL gotData = FALSE;

    hFile = CreateFileW(NOW_PLAYING_SDMMC, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        hFile = CreateFileW(NOW_PLAYING_FLASH, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    }
    if (hFile == INVALID_HANDLE_VALUE) {
        hFile = CreateFileW(NOW_PLAYING_MERO, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    }

    if (hFile != INVALID_HANDLE_VALUE) {
        memset(buf, 0, sizeof(buf));
        if (ReadFile(hFile, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
            char *line = strtok(buf, "\r\n");
            while (line) {
                if (strncmp(line, "title=", 6) == 0) {
                    MultiByteToWideChar(CP_UTF8, 0, line + 6, -1, g_title, 128);
                    gotData = TRUE;
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
                    }
                } else if (strncmp(line, "length=", 7) == 0) {
                    g_durationSec = atoi(line + 7);
                } else if (strncmp(line, "cover=", 6) == 0) {
                    MultiByteToWideChar(CP_UTF8, 0, line + 6, -1, coverPath, MAX_PATH);
                }
                line = strtok(NULL, "\r\n");
            }
        }
        CloseHandle(hFile);

        /* Refresh artwork if track changed, path changed, or not yet loaded */
        BOOL trackChanged = (lstrcmpW(g_title, g_lastTrackTitle) != 0);
        if (trackChanged) {
            lstrcpynW(g_lastTrackTitle, g_title, 128);
        }
        if (coverPath[0] != L'\0' && (trackChanged || _wcsicmp(coverPath, g_lastCoverPath) != 0)) {
            LoadCoverArt(coverPath);
        } else if (!g_hasCover) {
            if (GetFileAttributesW(COVER_FILE_SDMMC) != 0xFFFFFFFF) LoadCoverArt(COVER_FILE_SDMMC);
            else if (GetFileAttributesW(COVER_FILE_FLASH) != 0xFFFFFFFF) LoadCoverArt(COVER_FILE_FLASH);
        }

        if (gotData && wcsstr(g_statusMsg, L"CMD #") == NULL) {
            wsprintfW(g_statusMsg, L"STREAM LIVE // %s", g_isPlaying ? L"PLAYING" : L"PAUSED");
        }
    } else {
        if (wcsstr(g_statusMsg, L"CMD #") == NULL) {
            wsprintfW(g_statusMsg, L"WAITING FOR BRIDGE (SERIAL: %ls)...",
                      g_activeSerialPort[0] ? g_activeSerialPort : L"SCANNING");
        }
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

    WCHAR footerBuf[128];
    if (g_rxPacketCount > 0) {
        wsprintfW(footerBuf, L"LIVE // %ls (%lu pkts) | %s", g_activeSerialPort, g_rxPacketCount, g_statusMsg);
    } else if (g_hSerial != INVALID_HANDLE_VALUE) {
        wsprintfW(footerBuf, L"SERIAL // %ls (connecting...) | %s", g_activeSerialPort, g_statusMsg);
    } else {
        wsprintfW(footerBuf, L"STORAGE // %s", g_statusMsg);
    }
    ExtTextOutW(memDC, 15, 238, 0, NULL, footerBuf, lstrlenW(footerBuf), NULL);

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
        g_lastLocalTick = GetTickCount();
        InvalidateRect(g_hWnd, NULL, FALSE);
        UpdateWindow(g_hWnd);
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
        SetTimer(hWnd, TIMER_ID_POLL, 500, NULL);
        SetTimer(hWnd, TIMER_ID_MARQUEE, 150, NULL);
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_ID_POLL) {
            DWORD nowTick = GetTickCount();
            if (g_lastLocalTick == 0) g_lastLocalTick = nowTick;
            DWORD elapsed = nowTick - g_lastLocalTick;

            /* Advance local clock every ~1000ms if playing */
            if (g_isPlaying && elapsed >= 1000) {
                int sec = (int)(elapsed / 1000);
                g_positionSec += sec;
                if (g_durationSec > 0 && g_positionSec > g_durationSec) {
                    g_positionSec = g_durationSec;
                }
                g_lastLocalTick = nowTick - (elapsed % 1000);
            } else if (!g_isPlaying) {
                g_lastLocalTick = nowTick;
            }

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
        if (g_hSerial != INVALID_HANDLE_VALUE) {
            CloseHandle(g_hSerial);
            g_hSerial = INVALID_HANDLE_VALUE;
        }
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
        PostMessageW(hExisting, WM_CLOSE, 0, 0);
        Sleep(50);
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
