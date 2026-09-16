/*
 * mero-cam.c - DarkHorse Webcam Live Monitor & Control Deck
 * Target: Foston FS-460BT (PE32 ARMv4 Windows CE 5.0, Samsung S3C2440 400 MHz)
 * Screen: 480x272 16bpp TFT LCD
 *
 * Hardened & Crash-Proof:
 * - 16bpp Compatible Backbuffer for all GDI text and HUD drawing (never draws text onto 32bpp DIB)
 * - Zero FlushFileBuffers (FAT32 driver safe)
 * - Integer 16.16 fixed-point scaler (zero software floating-point emulation)
 * - JPEG SOI/EOI (FFD8...FFD9) integrity check before decoding
 * - Direct integer-bounds touch button hit tests
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
#include "stb_image.h"

#define SCREEN_W        480
#define SCREEN_H        272
#define TIMER_POLL      1
#define POLL_MS         250

#define CAM_FILE_SDMMC      L"\\SDMMC\\Stream\\cam.jpg"
#define CAM_FILE_MERO       L"\\SDMMC\\MERO\\cam.jpg"
#define STATUS_FILE_SDMMC   L"\\SDMMC\\Stream\\cam_status.txt"
#define STATUS_FILE_MERO    L"\\SDMMC\\MERO\\cam_status.txt"
#define CMD_FILE_SDMMC      L"\\SDMMC\\Stream\\cam_cmd.txt"
#define CMD_FILE_MERO       L"\\SDMMC\\MERO\\cam_cmd.txt"

static HWND       g_hWnd        = NULL;
static HINSTANCE  g_hInst       = NULL;

/* 32bpp DIB Section for video frame decoding */
static HBITMAP    g_hDIB        = NULL;
static DWORD     *g_pBits       = NULL;
static HDC        g_memDC       = NULL;
static HBITMAP    g_hOldBmp     = NULL;

/* App State */
static BOOL       g_hasFrame    = FALSE;
static BOOL       g_showOsd     = TRUE;
static int        g_cmdSeq      = 0;
static DWORD      g_lastHeartbeat = 0;

/* Status from Host */
static BOOL       g_isRecording = FALSE;
static BOOL       g_isPreview   = FALSE;
static int        g_brightness  = 160;
static int        g_contrast    = 127;
static WCHAR      g_statusMsg[128] = L"Connecting to DarkHorse Cam...";

/* Touch Button Definitions */
typedef struct {
    RECT   rc;
    const WCHAR *label;
    const char  *cmd;
} CamButton;

#define NUM_BUTTONS 8
static CamButton g_buttons[NUM_BUTTONS];

static void InitButtons(void)
{
    int yT = 232;
    int yB = 270;
    g_buttons[0] = (CamButton){ {   4, yT,  58, yB }, L"SNAP",  "SNAP"    };
    g_buttons[1] = (CamButton){ {  62, yT, 116, yB }, L"REC",   "REC"     };
    g_buttons[2] = (CamButton){ { 120, yT, 174, yB }, L"PREV",  "PREVIEW" };
    g_buttons[3] = (CamButton){ { 178, yT, 222, yB }, L"B -",   "BRT_DN"  };
    g_buttons[4] = (CamButton){ { 226, yT, 270, yB }, L"B +",   "BRT_UP"  };
    g_buttons[5] = (CamButton){ { 274, yT, 318, yB }, L"C -",   "CTR_DN"  };
    g_buttons[6] = (CamButton){ { 322, yT, 366, yB }, L"C +",   "CTR_UP"  };
    g_buttons[7] = (CamButton){ { 372, yT, 476, yB }, L"EXIT",  "EXIT"    };
}

/* ------------------------------------------------------------------ */
static void SendCamCommand(const char *cmd)
{
    g_cmdSeq++;
    char payload[128];
    sprintf(payload, "seq=%d\r\ncmd=%s\r\n", g_cmdSeq, cmd);

    CreateDirectoryW(L"\\SDMMC\\Stream", NULL);
    CreateDirectoryW(L"\\SDMMC\\MERO", NULL);

    HANDLE h = CreateFileW(CMD_FILE_SDMMC, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, payload, (DWORD)strlen(payload), &written, NULL);
        CloseHandle(h); /* No FlushFileBuffers on WinCE */
    }

    h = CreateFileW(CMD_FILE_MERO, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, payload, (DWORD)strlen(payload), &written, NULL);
        CloseHandle(h); /* No FlushFileBuffers on WinCE */
    }
}

/* ------------------------------------------------------------------ */
static BOOL InitDIB(HDC hdc)
{
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = SCREEN_W;
    bmi.bmiHeader.biHeight      = -SCREEN_H; /* Top-down DIB */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    g_hDIB = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&g_pBits, NULL, 0);
    if (!g_hDIB) return FALSE;
    g_memDC    = CreateCompatibleDC(hdc);
    g_hOldBmp  = (HBITMAP)SelectObject(g_memDC, g_hDIB);
    return TRUE;
}

/* ------------------------------------------------------------------ */
/* Ultra-fast integer frame renderer (Zero FPU instructions)          */
/* ------------------------------------------------------------------ */
static void RenderFrame(const unsigned char *pixels, int srcW, int srcH)
{
    if (!g_pBits) return;

    if (srcW == SCREEN_W && srcH == SCREEN_H) {
        /* Direct 1:1 pixel copy: 480x272 RGBA -> 32bpp DIB (0.5 ms) */
        int i;
        for (i = 0; i < SCREEN_W * SCREEN_H; i++) {
            const unsigned char *p = &pixels[i * 4];
            g_pBits[i] = ((DWORD)p[0] << 16) | ((DWORD)p[1] << 8) | (DWORD)p[2];
        }
    } else {
        /* Integer fixed-point 16.16 scaler */
        int stepX = (srcW << 16) / SCREEN_W;
        int stepY = (srcH << 16) / SCREEN_H;
        int y, x;
        for (y = 0; y < SCREEN_H; y++) {
            int sy = (y * stepY) >> 16;
            if (sy >= srcH) sy = srcH - 1;
            const unsigned char *srcRow = &pixels[sy * srcW * 4];
            DWORD *dstRow = &g_pBits[y * SCREEN_W];
            for (x = 0; x < SCREEN_W; x++) {
                int sx = (x * stepX) >> 16;
                if (sx >= srcW) sx = srcW - 1;
                const unsigned char *p = &srcRow[sx * 4];
                dstRow[x] = ((DWORD)p[0] << 16) | ((DWORD)p[1] << 8) | (DWORD)p[2];
            }
        }
    }
    g_hasFrame = TRUE;
}

/* ------------------------------------------------------------------ */
static void PollStatusFile(void)
{
    HANDLE hFile = CreateFileW(STATUS_FILE_SDMMC, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        hFile = CreateFileW(STATUS_FILE_MERO, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    if (hFile == INVALID_HANDLE_VALUE) return;

    char buf[512];
    DWORD readBytes = 0;
    if (ReadFile(hFile, buf, sizeof(buf) - 1, &readBytes, NULL) && readBytes > 0) {
        buf[readBytes] = '\0';
        char *line = strtok(buf, "\r\n");
        while (line) {
            if (strncmp(line, "recording=", 10) == 0) {
                g_isRecording = (atoi(line + 10) != 0);
            } else if (strncmp(line, "preview=", 8) == 0) {
                g_isPreview = (atoi(line + 8) != 0);
            } else if (strncmp(line, "brightness=", 11) == 0) {
                g_brightness = atoi(line + 11);
            } else if (strncmp(line, "contrast=", 9) == 0) {
                g_contrast = atoi(line + 9);
            } else if (strncmp(line, "msg=", 4) == 0) {
                if (strlen(line + 4) > 0) {
                    MultiByteToWideChar(CP_UTF8, 0, line + 4, -1, g_statusMsg, 128);
                }
            }
            line = strtok(NULL, "\r\n");
        }
    }
    CloseHandle(hFile);
}

/* ------------------------------------------------------------------ */
/* Draw HUD and touch buttons onto native 16bpp backbuffer DC         */
/* ------------------------------------------------------------------ */
static void DrawHUD(HDC backDC)
{
    if (!g_showOsd) return;

    /* Top HUD banner (Dark Cyberpunk Slate) */
    RECT rcTop = { 0, 0, SCREEN_W, 24 };
    HBRUSH hBrTop = CreateSolidBrush(RGB(10, 15, 20));
    FillRect(backDC, &rcTop, hBrTop);
    DeleteObject(hBrTop);

    HPEN hPenTop = CreatePen(PS_SOLID, 1, RGB(0, 180, 100));
    HPEN hOldPen = (HPEN)SelectObject(backDC, hPenTop);
    MoveToEx(backDC, 0, 24, NULL);
    LineTo(backDC, SCREEN_W, 24);
    SelectObject(backDC, hOldPen);
    DeleteObject(hPenTop);

    SetBkMode(backDC, TRANSPARENT);
    SetTextColor(backDC, g_isRecording ? RGB(255, 60, 60) : RGB(0, 255, 128));

    WCHAR topText[160];
    wsprintfW(topText, L"DARKHORSE OV7660 // %s // B:%d C:%d // %s",
              g_isRecording ? L"REC [ON]" : L"LIVE",
              g_brightness, g_contrast,
              g_statusMsg);

    ExtTextOutW(backDC, 8, 4, 0, NULL, topText, lstrlenW(topText), NULL);

    /* Bottom Touch Dock (Y: 228..272) */
    RECT rcBot = { 0, 228, SCREEN_W, SCREEN_H };
    HBRUSH hBrBot = CreateSolidBrush(RGB(10, 15, 20));
    FillRect(backDC, &rcBot, hBrBot);
    DeleteObject(hBrBot);

    HPEN hPenBot = CreatePen(PS_SOLID, 1, RGB(40, 70, 90));
    hOldPen = (HPEN)SelectObject(backDC, hPenBot);
    MoveToEx(backDC, 0, 228, NULL);
    LineTo(backDC, SCREEN_W, 228);
    SelectObject(backDC, hOldPen);
    DeleteObject(hPenBot);

    int i;
    for (i = 0; i < NUM_BUTTONS; i++) {
        COLORREF colBg  = RGB(25, 35, 45);
        COLORREF colTxt = RGB(220, 220, 220);
        COLORREF colBdr = RGB(50, 80, 100);

        if (strcmp(g_buttons[i].cmd, "REC") == 0 && g_isRecording) {
            colBg  = RGB(180, 20, 20);
            colTxt = RGB(255, 255, 255);
            colBdr = RGB(255, 60, 60);
        } else if (strcmp(g_buttons[i].cmd, "PREVIEW") == 0 && g_isPreview) {
            colTxt = RGB(0, 255, 128);
            colBdr = RGB(0, 200, 100);
        } else if (strcmp(g_buttons[i].cmd, "EXIT") == 0) {
            colTxt = RGB(255, 90, 90);
            colBdr = RGB(160, 40, 40);
        }

        /* Fill button body with dark slate */
        HBRUSH hBtnBr = CreateSolidBrush(colBg);
        FillRect(backDC, &g_buttons[i].rc, hBtnBr);
        DeleteObject(hBtnBr);

        /* Stroke 1-pixel border outline using NULL_BRUSH (leaves background intact) */
        HPEN hPen = CreatePen(PS_SOLID, 1, colBdr);
        HPEN hOldP = (HPEN)SelectObject(backDC, hPen);
        HBRUSH hOldB = (HBRUSH)SelectObject(backDC, GetStockObject(NULL_BRUSH));
        Rectangle(backDC, g_buttons[i].rc.left, g_buttons[i].rc.top,
                  g_buttons[i].rc.right, g_buttons[i].rc.bottom);
        SelectObject(backDC, hOldB);
        SelectObject(backDC, hOldP);
        DeleteObject(hPen);

        SetBkMode(backDC, TRANSPARENT);
        SetTextColor(backDC, colTxt);
        DrawTextW(backDC, g_buttons[i].label, -1, &g_buttons[i].rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

/* ------------------------------------------------------------------ */
/* Poll and load rolling frame from SDMMC                             */
/* ------------------------------------------------------------------ */
static void PollCamFrame(void)
{
    DWORD now = GetTickCount();
    if (now - g_lastHeartbeat > 1500) {
        SendCamCommand("HEARTBEAT");
        g_lastHeartbeat = now;
        PollStatusFile();
    }

    const WCHAR *camPath = CAM_FILE_SDMMC;
    HANDLE hFile = CreateFileW(camPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        camPath = CAM_FILE_MERO;
        hFile = CreateFileW(camPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    if (hFile == INVALID_HANDLE_VALUE) {
        InvalidateRect(g_hWnd, NULL, FALSE);
        return;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize < 500 || fileSize > 512 * 1024) {
        CloseHandle(hFile);
        return;
    }

    unsigned char *buf = (unsigned char*)malloc(fileSize);
    if (!buf) {
        CloseHandle(hFile);
        return;
    }

    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, buf, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    if (!ok || bytesRead < 500) {
        free(buf);
        return;
    }

    /* Verify complete JPEG frame markers: SOI (FFD8) and EOI (FFD9) */
    if (buf[0] != 0xFF || buf[1] != 0xD8 ||
        buf[bytesRead - 2] != 0xFF || buf[bytesRead - 1] != 0xD9) {
        free(buf);
        return;
    }

    /* Decode as 4 channels (RGBA 32bpp) */
    int w = 0, h = 0, ch = 0;
    unsigned char *pixels = stbi_load_from_memory(buf, (int)bytesRead, &w, &h, &ch, 4);
    free(buf);

    if (!pixels || w <= 0 || h <= 0) {
        if (pixels) stbi_image_free(pixels);
        return;
    }

    RenderFrame(pixels, w, h);
    stbi_image_free(pixels);

    InvalidateRect(g_hWnd, NULL, FALSE);
}

/* ------------------------------------------------------------------ */
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        g_hWnd = hWnd;
        InitButtons();

        HDC hdc = GetDC(hWnd);
        InitDIB(hdc);
        ReleaseDC(hWnd, hdc);

        if (g_pBits) memset(g_pBits, 0, SCREEN_W * SCREEN_H * 4);

        /* Signal host bridge to start camera streaming immediately */
        SendCamCommand("START");
        g_lastHeartbeat = GetTickCount();

        SetTimer(hWnd, TIMER_POLL, POLL_MS, NULL);
        return 0;
    }

    case WM_TIMER:
        if (wParam == TIMER_POLL)
            PollCamFrame();
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        /* Standard WinCE double buffer pattern: native 16bpp compatible backbuffer */
        HDC backDC = CreateCompatibleDC(hdc);
        HBITMAP backBmp = CreateCompatibleBitmap(hdc, SCREEN_W, SCREEN_H);
        HBITMAP oldBackBmp = (HBITMAP)SelectObject(backDC, backBmp);

        /* 1. Copy decoded video frame from g_memDC onto backDC */
        if (g_hasFrame && g_memDC && g_hDIB) {
            BitBlt(backDC, 0, 0, SCREEN_W, SCREEN_H, g_memDC, 0, 0, SRCCOPY);
        } else {
            RECT rcFull = { 0, 0, SCREEN_W, SCREEN_H };
            HBRUSH hBlk = (HBRUSH)GetStockObject(BLACK_BRUSH);
            FillRect(backDC, &rcFull, hBlk);
            SetBkMode(backDC, TRANSPARENT);
            SetTextColor(backDC, RGB(0, 220, 100));
            ExtTextOutW(backDC, 20, 110, 0, NULL, L"WAITING FOR DARKHORSE CAM FEED...", 33, NULL);
        }

        /* 2. Compose HUD and buttons onto native backDC */
        DrawHUD(backDC);

        /* 3. Atomic single-blit to screen */
        BitBlt(hdc, 0, 0, SCREEN_W, SCREEN_H, backDC, 0, 0, SRCCOPY);

        SelectObject(backDC, oldBackBmp);
        DeleteObject(backBmp);
        DeleteDC(backDC);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);

        if (g_showOsd && y >= 228) {
            /* Check button click with foolproof integer bounds */
            int i;
            for (i = 0; i < NUM_BUTTONS; i++) {
                if (x >= g_buttons[i].rc.left && x <= g_buttons[i].rc.right &&
                    y >= g_buttons[i].rc.top  && y <= g_buttons[i].rc.bottom) {

                    if (strcmp(g_buttons[i].cmd, "EXIT") == 0) {
                        SendCamCommand("STOP");
                        PostQuitMessage(0);
                        return 0;
                    }
                    SendCamCommand(g_buttons[i].cmd);
                    PollStatusFile();
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
            }
        } else if (y > 28 && y < 228) {
            /* Tap middle video area toggles HUD on/off */
            g_showOsd = !g_showOsd;
            InvalidateRect(hWnd, NULL, FALSE);
        } else if (!g_showOsd) {
            /* Any tap when HUD is hidden brings it back */
            g_showOsd = TRUE;
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE || wParam == 'Q') {
            SendCamCommand("STOP");
            PostQuitMessage(0);
            return 0;
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, TIMER_POLL);
        SendCamCommand("STOP");
        if (g_memDC) {
            SelectObject(g_memDC, g_hOldBmp);
            DeleteDC(g_memDC);
        }
        if (g_hDIB) DeleteObject(g_hDIB);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

/* ------------------------------------------------------------------ */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmd, int nShow)
{
    (void)hPrev; (void)lpCmd; (void)nShow;
    g_hInst = hInst;

    HWND hExisting = FindWindowW(L"MeroCamWnd", NULL);
    if (hExisting) {
        SendMessage(hExisting, WM_CLOSE, 0, 0);
        Sleep(200);
    }

    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"MeroCamWnd";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassW(&wc);

    g_hWnd = CreateWindowW(L"MeroCamWnd", L"DarkHorse Cam",
                           WS_VISIBLE | WS_POPUP,
                           0, 0, SCREEN_W, SCREEN_H,
                           NULL, NULL, hInst, NULL);
    if (!g_hWnd) return 1;

    ShowWindow(g_hWnd, SW_SHOWMAXIMIZED);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
