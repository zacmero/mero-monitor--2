/*
 * mero-gallery.c - Native Cybernetic Image Visualizer & Atmosphere Frame
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

#define GALLERY_VERSION        L"0.1.0"
#define TIMER_ID_SLIDESHOW     1
#define TIMER_ID_OSD_HIDE      2

#define MAX_IMAGES             256
#define MAX_FOLDERS            16
#define MAX_PATH_LEN           160

typedef struct {
    WCHAR path[MAX_PATH_LEN];
    WCHAR name[64];
    int   imageCount;
} ImageFolder;

typedef struct {
    WCHAR fileName[64];
    WCHAR fullPath[MAX_PATH_LEN];
} ImageFile;

typedef enum {
    FIT_CONTAIN = 0,   /* Letterbox: entire image visible, black bars if needed */
    FIT_COVER   = 1    /* Fill: scales image to completely cover 480x272 without black bars */
} FitMode;

typedef enum {
    ORIENT_0   = 0,  /* Landscape normal (0°) */
    ORIENT_90  = 1,  /* Portrait clockwise (90°) */
    ORIENT_180 = 2,  /* Landscape inverted (180°) */
    ORIENT_270 = 3   /* Portrait counter-clockwise (270°) */
} OrientMode;

static HINSTANCE    g_hInstance = NULL;
static HWND         g_hWnd = NULL;
static int          g_screenW = 480;
static int          g_screenH = 272;

static HBITMAP      g_hDIBSection = NULL;
static DWORD       *g_pDIBBits = NULL;
static HDC          g_memDC = NULL;
static HBITMAP      g_hOldBmp = NULL;

static ImageFolder  g_folders[MAX_FOLDERS];
static int          g_folderCount = 0;
static int          g_currentFolderIdx = 0;

static ImageFile    g_files[MAX_IMAGES];
static int          g_fileCount = 0;
static int          g_currentFileIdx = 0;

static int          g_imgOriginalW = 0;
static int          g_imgOriginalH = 0;
static BOOL         g_hasImage = FALSE;

static BOOL         g_isPlaying = TRUE;
static int          g_intervalSec = 3;
static const int    g_intervalOptions[] = { 1, 2, 3, 5, 10, 30 };
static const int    g_intervalOptionCount = 6;
static int          g_intervalOptIdx = 2; /* Default 3s */

static FitMode      g_fitMode = FIT_COVER;
static OrientMode   g_orientMode = ORIENT_270;
static BOOL         g_osdVisible = TRUE;
static BOOL         g_folderDialogOpen = FALSE;

/* UI Button rects */
static RECT         g_rcBtnPrev;
static RECT         g_rcBtnPlay;
static RECT         g_rcBtnNext;
static RECT         g_rcBtnInterval;
static RECT         g_rcBtnFit;
static RECT         g_rcBtnOrient;
static RECT         g_rcBtnFolder;
static RECT         g_rcBtnExit;

/* Display Calibration & Color Tuning */
static unsigned char g_colorLut[256];
static int           g_brightness = 0;    /* -50 to +50 */
static int           g_contrast   = 125;  /* 50 to 200 (125 = 1.25x) */
static int           g_saturation = 130;  /* 50 to 200 (130 = 1.30x) */
static int           g_gamma      = 115;  /* 60 to 180 (115 = 1.15) */
static int           g_satScale   = 332;  /* (g_saturation * 256) / 100 */

static void BuildColorLut(void)
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
        g_colorLut[i] = (unsigned char)res;
    }

    g_satScale = (g_saturation * 256) / 100;
}

static void LoadDisplayConfig(void)
{
    HANDLE hFile;
    DWORD bytesRead;
    char buf[512];

    g_brightness = 0;
    g_contrast   = 125;
    g_saturation = 130;
    g_gamma      = 115;

    hFile = CreateFileW(L"\\SDMMC\\MERO\\display.cfg", GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        hFile = CreateFileW(L"\\ResidentFlash\\MERO\\display.cfg", GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    if (hFile != INVALID_HANDLE_VALUE) {
        memset(buf, 0, sizeof(buf));
        if (ReadFile(hFile, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
            char *line = strtok(buf, "\r\n");
            while (line) {
                int val = 0;
                if (sscanf(line, "brightness=%d", &val) == 1) g_brightness = val;
                else if (sscanf(line, "contrast=%d", &val) == 1) g_contrast = val;
                else if (sscanf(line, "saturation=%d", &val) == 1) g_saturation = val;
                else if (sscanf(line, "gamma=%d", &val) == 1) g_gamma = val;
                line = strtok(NULL, "\r\n");
            }
        }
        CloseHandle(hFile);
    }

    BuildColorLut();
}

static void SaveGalleryConfig(void)
{
    HANDLE hFile;
    DWORD written;
    char buf[128];

    sprintf(buf, "fit=%d\r\norient=%d\r\ninterval=%d\r\n",
            (int)g_fitMode, (int)g_orientMode, g_intervalSec);

    CreateDirectoryW(L"\\SDMMC\\MERO", NULL);
    hFile = CreateFileW(L"\\SDMMC\\MERO\\gallery.cfg", GENERIC_WRITE, FILE_SHARE_READ, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, buf, (DWORD)strlen(buf), &written, NULL);
        FlushFileBuffers(hFile);
        CloseHandle(hFile);
    }
}

static void LoadGalleryConfig(void)
{
    HANDLE hFile;
    DWORD bytesRead;
    char buf[256];

    /* Default requested: Fill screen at 270° orientation */
    g_fitMode = FIT_COVER;
    g_orientMode = ORIENT_270;

    hFile = CreateFileW(L"\\SDMMC\\MERO\\gallery.cfg", GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        hFile = CreateFileW(L"\\ResidentFlash\\MERO\\gallery.cfg", GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    if (hFile != INVALID_HANDLE_VALUE) {
        memset(buf, 0, sizeof(buf));
        if (ReadFile(hFile, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
            char *line = strtok(buf, "\r\n");
            while (line) {
                int val = 0;
                if (sscanf(line, "fit=%d", &val) == 1) {
                    if (val == FIT_CONTAIN || val == FIT_COVER) g_fitMode = (FitMode)val;
                } else if (sscanf(line, "orient=%d", &val) == 1) {
                    if (val >= 0 && val < 4) g_orientMode = (OrientMode)val;
                } else if (sscanf(line, "interval=%d", &val) == 1) {
                    if (val >= 1 && val <= 60) g_intervalSec = val;
                }
                line = strtok(NULL, "\r\n");
            }
        }
        CloseHandle(hFile);
    }
}

/* Forward declarations */
static void ScanActiveFolder(void);
static void LoadCurrentImage(void);
static void DiscoverFolders(void);
static void ResetOsdTimer(void);

static BOOL HasImageExtension(const WCHAR *name)
{
    /* Ignore system stream/media assets: album cover art and webcam frame */
    if (_wcsnicmp(name, L"cover.", 6) == 0 ||
        _wcsnicmp(name, L"cam.", 4) == 0) {
        return FALSE;
    }

    const WCHAR *ext = wcsrchr(name, L'.');
    if (!ext) return FALSE;
    if (_wcsicmp(ext, L".bmp") == 0 ||
        _wcsicmp(ext, L".jpg") == 0 ||
        _wcsicmp(ext, L".jpeg") == 0 ||
        _wcsicmp(ext, L".png") == 0) {
        return TRUE;
    }
    return FALSE;
}

static int CountImagesInDirectory(const WCHAR *dirPath)
{
    WIN32_FIND_DATAW wfd;
    HANDLE hFind;
    WCHAR pattern[MAX_PATH_LEN];
    int count = 0;

    wsprintfW(pattern, L"%s\\*.*", dirPath);
    hFind = FindFirstFileW(pattern, &wfd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                if (HasImageExtension(wfd.cFileName)) {
                    count++;
                }
            }
        } while (FindNextFileW(hFind, &wfd));
        FindClose(hFind);
    }
    return count;
}

static void AddFolderIfValid(const WCHAR *path, const WCHAR *displayName)
{
    DWORD attr = GetFileAttributesW(path);
    if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        int count = CountImagesInDirectory(path);
        if (g_folderCount < MAX_FOLDERS) {
            lstrcpynW(g_folders[g_folderCount].path, path, MAX_PATH_LEN);
            lstrcpynW(g_folders[g_folderCount].name, displayName, 64);
            g_folders[g_folderCount].imageCount = count;
            g_folderCount++;
        }
    }
}

/* Discover image directories across SDMMC and internal flash */
static void DiscoverFolders(void)
{
    WIN32_FIND_DATAW wfd;
    HANDLE hFind;
    WCHAR subPath[MAX_PATH_LEN];

    g_folderCount = 0;

    /* 1. Standard Presets */
    AddFolderIfValid(L"\\SDMMC\\Stream", L"SDMMC: Host Stream");
    AddFolderIfValid(L"\\ResidentFlash\\Stream", L"Flash: Host Stream");
    AddFolderIfValid(L"\\SDMMC\\Pictures", L"SDMMC: Pictures");
    AddFolderIfValid(L"\\ResidentFlash\\Pictures", L"Flash: Pictures");
    AddFolderIfValid(L"\\SDMMC\\Gallery", L"SDMMC: Gallery");
    AddFolderIfValid(L"\\SDMMC\\Suzy", L"SDMMC: Suzy");
    AddFolderIfValid(L"\\SDMMC\\Wallpapers", L"SDMMC: Wallpapers");
    AddFolderIfValid(L"\\SDMMC", L"SDMMC: Root (\\)");
    AddFolderIfValid(L"\\ResidentFlash\\MERO", L"Flash: MERO");

    /* 2. Dynamically discover any other subdirectories on SDMMC */
    hFind = FindFirstFileW(L"\\SDMMC\\*.*", &wfd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                wcscmp(wfd.cFileName, L".") != 0 &&
                wcscmp(wfd.cFileName, L"..") != 0 &&
                _wcsicmp(wfd.cFileName, L"Stream") != 0 &&
                _wcsicmp(wfd.cFileName, L"Pictures") != 0 &&
                _wcsicmp(wfd.cFileName, L"Gallery") != 0 &&
                _wcsicmp(wfd.cFileName, L"Suzy") != 0 &&
                _wcsicmp(wfd.cFileName, L"Wallpapers") != 0) {
                
                wsprintfW(subPath, L"\\SDMMC\\%s", wfd.cFileName);
                int cnt = CountImagesInDirectory(subPath);
                if (cnt > 0 && g_folderCount < MAX_FOLDERS) {
                    lstrcpynW(g_folders[g_folderCount].path, subPath, MAX_PATH_LEN);
                    wsprintfW(g_folders[g_folderCount].name, L"SD: %s", wfd.cFileName);
                    g_folders[g_folderCount].imageCount = cnt;
                    g_folderCount++;
                }
            }
        } while (FindNextFileW(hFind, &wfd));
        FindClose(hFind);
    }

    /* If no folders found, ensure at least \SDMMC\Pictures placeholder exists */
    if (g_folderCount == 0) {
        CreateDirectoryW(L"\\SDMMC\\Pictures", NULL);
        lstrcpyW(g_folders[0].path, L"\\SDMMC\\Pictures");
        lstrcpyW(g_folders[0].name, L"SDMMC: Pictures");
        g_folders[0].imageCount = 0;
        g_folderCount = 1;
    }

    /* Set first folder with images as active, or index 0 */
    g_currentFolderIdx = 0;
    int i;
    for (i = 0; i < g_folderCount; i++) {
        if (g_folders[i].imageCount > 0) {
            g_currentFolderIdx = i;
            break;
        }
    }
}

/* Scan all image files in active folder */
static void ScanActiveFolder(void)
{
    WIN32_FIND_DATAW wfd;
    HANDLE hFind;
    WCHAR pattern[MAX_PATH_LEN];

    g_fileCount = 0;
    g_currentFileIdx = 0;
    g_hasImage = FALSE;

    if (g_folderCount == 0) return;

    wsprintfW(pattern, L"%s\\*.*", g_folders[g_currentFolderIdx].path);
    hFind = FindFirstFileW(pattern, &wfd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                if (HasImageExtension(wfd.cFileName)) {
                    if (g_fileCount < MAX_IMAGES) {
                        lstrcpynW(g_files[g_fileCount].fileName, wfd.cFileName, 64);
                        wsprintfW(g_files[g_fileCount].fullPath, L"%s\\%s",
                                  g_folders[g_currentFolderIdx].path, wfd.cFileName);
                        g_fileCount++;
                    }
                }
            }
        } while (FindNextFileW(hFind, &wfd));
        FindClose(hFind);
    }

    g_folders[g_currentFolderIdx].imageCount = g_fileCount;

    if (g_fileCount > 0) {
        LoadCurrentImage();
    } else {
        /* Clear display to black */
        if (g_pDIBBits) {
            memset(g_pDIBBits, 0, g_screenW * g_screenH * sizeof(DWORD));
        }
        if (g_hWnd) {
            InvalidateRect(g_hWnd, NULL, FALSE);
        }
    }
}

/* Decode image and render to 480x272 DIB buffer */
static void LoadCurrentImage(void)
{
    HANDLE hFile;
    DWORD fileSize;
    DWORD bytesRead;
    unsigned char *fileBuf = NULL;
    int srcW = 0, srcH = 0, channels = 0;
    unsigned char *pixels = NULL;

    if (g_fileCount == 0 || !g_pDIBBits) {
        g_hasImage = FALSE;
        return;
    }

    hFile = CreateFileW(g_files[g_currentFileIdx].fullPath, GENERIC_READ, FILE_SHARE_READ,
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        g_hasImage = FALSE;
        return;
    }

    fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0 || fileSize > (16 * 1024 * 1024)) {
        CloseHandle(hFile);
        g_hasImage = FALSE;
        return;
    }

    fileBuf = (unsigned char*)malloc(fileSize);
    if (!fileBuf) {
        CloseHandle(hFile);
        g_hasImage = FALSE;
        return;
    }

    if (!ReadFile(hFile, fileBuf, fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
        free(fileBuf);
        CloseHandle(hFile);
        g_hasImage = FALSE;
        return;
    }
    CloseHandle(hFile);

    /* Decode JPEG/PNG/BMP directly into uncompressed 32bpp RGBA memory */
    pixels = stbi_load_from_memory(fileBuf, (int)fileSize, &srcW, &srcH, &channels, 4);
    free(fileBuf);

    if (!pixels || srcW <= 0 || srcH <= 0) {
        if (pixels) stbi_image_free(pixels);
        g_hasImage = FALSE;
        return;
    }

    g_imgOriginalW = srcW;
    g_imgOriginalH = srcH;

    /* Clear display buffer with solid black */
    memset(g_pDIBBits, 0, g_screenW * g_screenH * sizeof(DWORD));

    /* Effective source dimensions taking orientation into account */
    int effW = (g_orientMode == ORIENT_90 || g_orientMode == ORIENT_270) ? srcH : srcW;
    int effH = (g_orientMode == ORIENT_90 || g_orientMode == ORIENT_270) ? srcW : srcH;

    int dstX = 0, dstY = 0, dstW = 0, dstH = 0;
    float cropEffX = 0.0f, cropEffY = 0.0f;
    float cropEffW = (float)effW, cropEffH = (float)effH;

    if (g_fitMode == FIT_COVER) {
        /* FILL: Cover entire 480x272 screen completely with zero black bars */
        float sW = (float)g_screenW / (float)effW;
        float sH = (float)g_screenH / (float)effH;
        float s = (sH > sW) ? sH : sW; /* s = max(sW, sH) */

        dstW = g_screenW;
        dstH = g_screenH;
        dstX = 0;
        dstY = 0;

        cropEffW = (float)g_screenW / s;
        cropEffH = (float)g_screenH / s;
        cropEffX = ((float)effW - cropEffW) / 2.0f;
        cropEffY = ((float)effH - cropEffH) / 2.0f;
    } else {
        /* FIT: Contain within 480x272, preserving aspect ratio with letterbox */
        float sW = (float)g_screenW / (float)effW;
        float sH = (float)g_screenH / (float)effH;
        float s = (sH < sW) ? sH : sW; /* s = min(sW, sH) */

        dstW = (int)((float)effW * s);
        dstH = (int)((float)effH * s);
        if (dstW > g_screenW) dstW = g_screenW;
        if (dstH > g_screenH) dstH = g_screenH;

        dstX = (g_screenW - dstW) / 2;
        dstY = (g_screenH - dstH) / 2;

        cropEffX = 0.0f;
        cropEffY = 0.0f;
        cropEffW = (float)effW;
        cropEffH = (float)effH;
    }

    /* High performance software scaler into 32bpp DIB buffer */
    int dy, dx;
    for (dy = 0; dy < dstH; dy++) {
        float v = ((float)dy / (float)dstH) * cropEffH + cropEffY;
        int screenY = dstY + dy;
        if (screenY < 0 || screenY >= g_screenH) continue;
        DWORD *dstRow = &g_pDIBBits[screenY * g_screenW];

        for (dx = 0; dx < dstW; dx++) {
            float u = ((float)dx / (float)dstW) * cropEffW + cropEffX;
            int screenX = dstX + dx;
            if (screenX < 0 || screenX >= g_screenW) continue;

            int sx, sy;
            switch (g_orientMode) {
            case ORIENT_90:  /* 90° clockwise */
                sx = (int)v;
                sy = srcH - 1 - (int)u;
                break;
            case ORIENT_180: /* 180° upside down */
                sx = srcW - 1 - (int)u;
                sy = srcH - 1 - (int)v;
                break;
            case ORIENT_270: /* 270° counter-clockwise */
                sx = srcW - 1 - (int)v;
                sy = (int)u;
                break;
            case ORIENT_0:   /* 0° normal */
            default:
                sx = (int)u;
                sy = (int)v;
                break;
            }

            if (sx < 0) sx = 0;
            else if (sx >= srcW) sx = srcW - 1;
            if (sy < 0) sy = 0;
            else if (sy >= srcH) sy = srcH - 1;

            unsigned char *p = &pixels[(sy * srcW + sx) * 4];
            int r = g_colorLut[p[0]];
            int g = g_colorLut[p[1]];
            int b = g_colorLut[p[2]];

            if (g_satScale != 256) {
                int y = (77 * r + 150 * g + 29 * b) >> 8;
                r = y + (((r - y) * g_satScale) >> 8);
                g = y + (((g - y) * g_satScale) >> 8);
                b = y + (((b - y) * g_satScale) >> 8);
                if (r < 0) r = 0; else if (r > 255) r = 255;
                if (g < 0) g = 0; else if (g > 255) g = 255;
                if (b < 0) b = 0; else if (b > 255) b = 255;
            }

            /* Pack calibrated RGB into Win32 DIB format: 0x00RRGGBB */
            dstRow[screenX] = ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
        }
    }

    stbi_image_free(pixels);
    g_hasImage = TRUE;

    if (g_hWnd) {
        InvalidateRect(g_hWnd, NULL, FALSE);
    }
}

static void NextImage(void)
{
    if (g_fileCount <= 1) return;
    g_currentFileIdx = (g_currentFileIdx + 1) % g_fileCount;
    LoadCurrentImage();
    ResetOsdTimer();
}

static void PrevImage(void)
{
    if (g_fileCount <= 1) return;
    g_currentFileIdx = (g_currentFileIdx - 1 + g_fileCount) % g_fileCount;
    LoadCurrentImage();
    ResetOsdTimer();
}

static void TogglePlayPause(void)
{
    g_isPlaying = !g_isPlaying;
    if (g_isPlaying) {
        SetTimer(g_hWnd, TIMER_ID_SLIDESHOW, g_intervalSec * 1000, NULL);
    } else {
        KillTimer(g_hWnd, TIMER_ID_SLIDESHOW);
    }
    ResetOsdTimer();
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void CycleInterval(void)
{
    g_intervalOptIdx = (g_intervalOptIdx + 1) % g_intervalOptionCount;
    g_intervalSec = g_intervalOptions[g_intervalOptIdx];
    if (g_isPlaying) {
        SetTimer(g_hWnd, TIMER_ID_SLIDESHOW, g_intervalSec * 1000, NULL);
    }
    ResetOsdTimer();
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void CycleFitMode(void)
{
    g_fitMode = (g_fitMode == FIT_CONTAIN) ? FIT_COVER : FIT_CONTAIN;
    SaveGalleryConfig();
    LoadCurrentImage();
    ResetOsdTimer();
}

static void CycleOrientMode(void)
{
    g_orientMode = (OrientMode)((g_orientMode + 1) % 4);
    SaveGalleryConfig();
    LoadCurrentImage();
    ResetOsdTimer();
}

static void ResetOsdTimer(void)
{
    g_osdVisible = TRUE;
    SetTimer(g_hWnd, TIMER_ID_OSD_HIDE, 5000, NULL);
    InvalidateRect(g_hWnd, NULL, FALSE);
}

static void ExitGallery(void)
{
    KillTimer(g_hWnd, TIMER_ID_SLIDESHOW);
    KillTimer(g_hWnd, TIMER_ID_OSD_HIDE);

    /* Relaunch Mero Shell before closing (try SDMMC first, then ResidentFlash) */
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessW(L"\\SDMMC\\MERO\\mero-shell.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)) {
        CreateProcessW(L"\\ResidentFlash\\MERO\\mero-shell.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi);
    }
    if (pi.hProcess) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    DestroyWindow(g_hWnd);
}

/* Paint procedure: Double buffered GDI rendering */
static void OnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc;
    HDC backDC;
    HBITMAP backBmp, oldBackBmp;
    RECT rc;
    WCHAR buf[128];

    hdc = BeginPaint(hWnd, &ps);
    GetClientRect(hWnd, &rc);

    backDC = CreateCompatibleDC(hdc);
    backBmp = CreateCompatibleBitmap(hdc, g_screenW, g_screenH);
    oldBackBmp = (HBITMAP)SelectObject(backDC, backBmp);

    /* 1. Copy active image DIB onto backbuffer */
    if (g_memDC && g_hDIBSection) {
        BitBlt(backDC, 0, 0, g_screenW, g_screenH, g_memDC, 0, 0, SRCCOPY);
    } else {
        FillRect(backDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }

    SetBkMode(backDC, TRANSPARENT);

    /* 2. Empty state rendering */
    if (!g_hasImage) {
        SetTextColor(backDC, RGB(255, 80, 80));
        wsprintfW(buf, L"NO IMAGES IN %s", g_folders[g_currentFolderIdx].name);
        ExtTextOutW(backDC, 20, 80, 0, NULL, buf, lstrlenW(buf), NULL);

        SetTextColor(backDC, RGB(180, 200, 190));
        wsprintfW(buf, L"Drop .BMP, .JPG, or .PNG files into folder.");
        ExtTextOutW(backDC, 20, 110, 0, NULL, buf, lstrlenW(buf), NULL);

        SetTextColor(backDC, RGB(0, 220, 255));
        wsprintfW(buf, L"Tap [ FOLDER ] below to choose another directory.");
        ExtTextOutW(backDC, 20, 135, 0, NULL, buf, lstrlenW(buf), NULL);
    }

    /* 3. Cybernetic OSD (On-Screen Display HUD) */
    if (g_osdVisible && !g_folderDialogOpen) {
        /* TOP HUD BANNER (Dark slate overlay with emerald divider) */
        RECT rcTop = { 0, 0, g_screenW, 26 };
        HBRUSH hBrTop = CreateSolidBrush(RGB(8, 16, 12));
        FillRect(backDC, &rcTop, hBrTop);
        DeleteObject(hBrTop);

        HPEN hPenTop = CreatePen(PS_SOLID, 1, RGB(0, 180, 100));
        HPEN hOldPen = (HPEN)SelectObject(backDC, hPenTop);
        MoveToEx(backDC, 0, 25, NULL);
        LineTo(backDC, g_screenW, 25);
        SelectObject(backDC, hOldPen);
        DeleteObject(hPenTop);

        /* Image Title & Counter */
        SetTextColor(backDC, RGB(0, 255, 128));
        if (g_fileCount > 0) {
            wsprintfW(buf, L"[%d/%d] %s (%dx%d)",
                      g_currentFileIdx + 1, g_fileCount,
                      g_files[g_currentFileIdx].fileName,
                      g_imgOriginalW, g_imgOriginalH);
        } else {
            wsprintfW(buf, L"MERO VISUALIZER // EMPTY DIRECTORY");
        }
        ExtTextOutW(backDC, 10, 5, 0, NULL, buf, lstrlenW(buf), NULL);

        /* Status & Folder badge */
        SetTextColor(backDC, RGB(0, 220, 255));
        {
            const WCHAR *orientLabels[] = { L"0 DEG", L"90 DEG", L"180 DEG", L"270 DEG" };
            wsprintfW(buf, L"%s | %s | %s | %s",
                      g_isPlaying ? L"[PLAY]" : L"[PAUSED]",
                      g_fitMode == FIT_CONTAIN ? L"FIT" : L"FILL",
                      orientLabels[g_orientMode],
                      g_folders[g_currentFolderIdx].name);
        }
        ExtTextOutW(backDC, 230, 5, 0, NULL, buf, lstrlenW(buf), NULL);

        /* BOTTOM HUD DOCK (Dark overlay with 8 Cybernetic Buttons) */
        RECT rcBottom = { 0, 232, g_screenW, g_screenH };
        HBRUSH hBrBottom = CreateSolidBrush(RGB(10, 15, 20));
        FillRect(backDC, &rcBottom, hBrBottom);
        DeleteObject(hBrBottom);

        HPEN hPenBottom = CreatePen(PS_SOLID, 1, RGB(40, 70, 90));
        hOldPen = (HPEN)SelectObject(backDC, hPenBottom);
        MoveToEx(backDC, 0, 232, NULL);
        LineTo(backDC, g_screenW, 232);
        SelectObject(backDC, hOldPen);
        DeleteObject(hPenBottom);

        /* Button renderer helper */
        #define DRAW_BTN(rc, title, col) do { \
            HPEN hp = CreatePen(PS_SOLID, 1, (col)); \
            HPEN hop = (HPEN)SelectObject(backDC, hp); \
            Rectangle(backDC, (rc).left, (rc).top, (rc).right, (rc).bottom); \
            SelectObject(backDC, hop); \
            DeleteObject(hp); \
            SetTextColor(backDC, (col)); \
            ExtTextOutW(backDC, (rc).left + 4, (rc).top + 8, 0, NULL, (title), lstrlenW(title), NULL); \
        } while(0)

        DRAW_BTN(g_rcBtnPrev,     L"< PREV",         RGB(0, 220, 255));
        DRAW_BTN(g_rcBtnPlay,     g_isPlaying ? L"PAUSE" : L"PLAY >", RGB(0, 255, 128));
        DRAW_BTN(g_rcBtnNext,     L"NEXT >",         RGB(0, 220, 255));

        wsprintfW(buf, L"%ds TIME", g_intervalSec);
        DRAW_BTN(g_rcBtnInterval, buf,               RGB(255, 180, 0));

        DRAW_BTN(g_rcBtnFit,      g_fitMode == FIT_CONTAIN ? L"FIT" : L"FILL", RGB(180, 140, 255));
        {
            const WCHAR *orientBtnLabels[] = { L"0 DEG", L"90 DEG", L"180 DEG", L"270 DEG" };
            DRAW_BTN(g_rcBtnOrient, orientBtnLabels[g_orientMode], RGB(0, 255, 200));
        }
        DRAW_BTN(g_rcBtnFolder,   L"FOLDER",         RGB(212, 0, 255));
        DRAW_BTN(g_rcBtnExit,     L"EXIT",           RGB(255, 80, 80));

        #undef DRAW_BTN
    }

    /* 4. Folder Chooser Modal Dialog */
    if (g_folderDialogOpen) {
        /* Dim background */
        RECT rcDialog = { 20, 15, g_screenW - 20, g_screenH - 15 };
        HBRUSH hBrDlg = CreateSolidBrush(RGB(15, 20, 28));
        FillRect(backDC, &rcDialog, hBrDlg);
        DeleteObject(hBrDlg);

        HPEN hPenDlg = CreatePen(PS_SOLID, 2, RGB(0, 255, 128));
        HPEN hOld = (HPEN)SelectObject(backDC, hPenDlg);
        Rectangle(backDC, rcDialog.left, rcDialog.top, rcDialog.right, rcDialog.bottom);
        SelectObject(backDC, hOld);
        DeleteObject(hPenDlg);

        SetTextColor(backDC, RGB(0, 255, 128));
        wsprintfW(buf, L"SELECT ATMOSPHERE DIRECTORY");
        ExtTextOutW(backDC, 35, 25, 0, NULL, buf, lstrlenW(buf), NULL);

        int i;
        int y = 48;
        int maxShown = g_folderCount < 5 ? g_folderCount : 5;
        for (i = 0; i < maxShown; i++) {
            RECT rcItem = { 35, y, g_screenW - 35, y + 30 };
            COLORREF col = (i == g_currentFolderIdx) ? RGB(0, 255, 128) : RGB(140, 180, 220);

            HPEN hp = CreatePen(PS_SOLID, 1, col);
            HPEN hop = (HPEN)SelectObject(backDC, hp);
            Rectangle(backDC, rcItem.left, rcItem.top, rcItem.right, rcItem.bottom);
            SelectObject(backDC, hop);
            DeleteObject(hp);

            SetTextColor(backDC, col);
            wsprintfW(buf, L"[%d] %s (%d images)", i + 1, g_folders[i].name, g_folders[i].imageCount);
            ExtTextOutW(backDC, rcItem.left + 8, rcItem.top + 7, 0, NULL, buf, lstrlenW(buf), NULL);
            y += 34;
        }

        /* Cancel button at bottom */
        RECT rcCancel = { 180, 220, 300, 250 };
        HPEN hp = CreatePen(PS_SOLID, 1, RGB(255, 80, 80));
        HPEN hop = (HPEN)SelectObject(backDC, hp);
        Rectangle(backDC, rcCancel.left, rcCancel.top, rcCancel.right, rcCancel.bottom);
        SelectObject(backDC, hop);
        DeleteObject(hp);
        SetTextColor(backDC, RGB(255, 80, 80));
        ExtTextOutW(backDC, 215, 227, 0, NULL, L"[ CANCEL ]", 10, NULL);
    }

    /* 5. Atomic BitBlt to display */
    BitBlt(hdc, 0, 0, g_screenW, g_screenH, backDC, 0, 0, SRCCOPY);

    SelectObject(backDC, oldBackBmp);
    DeleteObject(backBmp);
    DeleteDC(backDC);

    EndPaint(hWnd, &ps);
}

/* Touch handling */
static void OnTouch(int x, int y)
{
    POINT pt = { x, y };

    /* Handle Folder Chooser interaction */
    if (g_folderDialogOpen) {
        /* Cancel button */
        RECT rcCancel = { 180, 220, 300, 250 };
        if (PtInRect(&rcCancel, pt)) {
            g_folderDialogOpen = FALSE;
            ResetOsdTimer();
            return;
        }

        /* Folder list items */
        int i;
        int itemY = 48;
        int maxShown = g_folderCount < 5 ? g_folderCount : 5;
        for (i = 0; i < maxShown; i++) {
            RECT rcItem = { 35, itemY, g_screenW - 35, itemY + 30 };
            if (PtInRect(&rcItem, pt)) {
                g_currentFolderIdx = i;
                g_folderDialogOpen = FALSE;
                ScanActiveFolder();
                ResetOsdTimer();
                return;
            }
            itemY += 34;
        }
        return;
    }

    /* Handle OSD bottom toolbar interactions */
    if (g_osdVisible) {
        if (PtInRect(&g_rcBtnPrev, pt)) {
            PrevImage();
            return;
        }
        if (PtInRect(&g_rcBtnPlay, pt)) {
            TogglePlayPause();
            return;
        }
        if (PtInRect(&g_rcBtnNext, pt)) {
            NextImage();
            return;
        }
        if (PtInRect(&g_rcBtnInterval, pt)) {
            CycleInterval();
            return;
        }
        if (PtInRect(&g_rcBtnFit, pt)) {
            CycleFitMode();
            return;
        }
        if (PtInRect(&g_rcBtnOrient, pt)) {
            CycleOrientMode();
            return;
        }
        if (PtInRect(&g_rcBtnFolder, pt)) {
            DiscoverFolders();
            g_folderDialogOpen = TRUE;
            InvalidateRect(g_hWnd, NULL, FALSE);
            return;
        }
        if (PtInRect(&g_rcBtnExit, pt)) {
            ExitGallery();
            return;
        }

        /* Tapping top banner or background toggles OSD hide */
        if (y < 232) {
            g_osdVisible = FALSE;
            KillTimer(g_hWnd, TIMER_ID_OSD_HIDE);
            InvalidateRect(g_hWnd, NULL, FALSE);
            return;
        }
    } else {
        /* OSD is hidden: simple gesture zones */
        if (x < 130) {
            /* Left zone: Previous */
            PrevImage();
        } else if (x > 350) {
            /* Right zone: Next */
            NextImage();
        } else {
            /* Center zone: Reveal OSD */
            ResetOsdTimer();
        }
    }
}

/* Initialize button layouts */
static void InitButtonRects(void)
{
    int y0 = 236;
    int h = 32;

    SetRect(&g_rcBtnPrev,     4,   y0, 58,  y0 + h);
    SetRect(&g_rcBtnPlay,     62,  y0, 118, y0 + h);
    SetRect(&g_rcBtnNext,     122, y0, 176, y0 + h);
    SetRect(&g_rcBtnInterval, 180, y0, 234, y0 + h);
    SetRect(&g_rcBtnFit,      238, y0, 292, y0 + h);
    SetRect(&g_rcBtnOrient,   296, y0, 354, y0 + h);
    SetRect(&g_rcBtnFolder,   358, y0, 418, y0 + h);
    SetRect(&g_rcBtnExit,     422, y0, 476, y0 + h);
}

/* Window Procedure */
static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        g_hWnd = hWnd;
        InitButtonRects();

        /* Create 480x272 32bpp DIB Section for direct memory rendering */
        {
            HDC hdc = GetDC(hWnd);
            BITMAPINFO bmi;
            memset(&bmi, 0, sizeof(bmi));
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = g_screenW;
            bmi.bmiHeader.biHeight = -g_screenH; /* Top-down DIB */
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            g_hDIBSection = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&g_pDIBBits, NULL, 0);
            g_memDC = CreateCompatibleDC(hdc);
            g_hOldBmp = (HBITMAP)SelectObject(g_memDC, g_hDIBSection);
            ReleaseDC(hWnd, hdc);
        }

        LoadDisplayConfig();
        LoadGalleryConfig();
        DiscoverFolders();
        ScanActiveFolder();

        /* Start automatic slideshow timer */
        if (g_isPlaying) {
            SetTimer(hWnd, TIMER_ID_SLIDESHOW, g_intervalSec * 1000, NULL);
        }
        ResetOsdTimer();
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_ID_SLIDESHOW) {
            if (g_isPlaying && !g_folderDialogOpen) {
                if (g_fileCount > 0) {
                    int prevIdx = g_currentFileIdx;
                    g_currentFileIdx = (g_currentFileIdx + 1) % g_fileCount;
                    if (g_currentFileIdx == 0 && prevIdx > 0) {
                        /* Looped back to start: re-scan folder to pick up any new files streamed from host */
                        ScanActiveFolder();
                    } else {
                        LoadCurrentImage();
                    }
                } else {
                    /* Empty folder: re-scan periodically in case stream drops new files */
                    ScanActiveFolder();
                }
            }
        } else if (wParam == TIMER_ID_OSD_HIDE) {
            if (!g_folderDialogOpen) {
                g_osdVisible = FALSE;
                KillTimer(hWnd, TIMER_ID_OSD_HIDE);
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
        KillTimer(hWnd, TIMER_ID_SLIDESHOW);
        KillTimer(hWnd, TIMER_ID_OSD_HIDE);
        if (g_memDC && g_hOldBmp) {
            SelectObject(g_memDC, g_hOldBmp);
            DeleteDC(g_memDC);
        }
        if (g_hDIBSection) {
            DeleteObject(g_hDIBSection);
        }
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

/* Entry Point */
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

    /* Single instance check */
    HWND hExisting = FindWindowW(L"MeroGalleryWndClass", L"Mero Gallery");
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
    wc.lpszClassName = L"MeroGalleryWndClass";

    if (!RegisterClassW(&wc)) {
        return 1;
    }

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"MeroGalleryWndClass",
        L"Mero Gallery",
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
