/*
 * mero-media-ctrl.c - Cybernetic Companion Terminal:
 * 1. Cyber Chrono & System Matrix (Fast, flashy, eerie digital clock + live OS metrics)
 * 2. Orbital Satellite Radar (Real NMEA COM1: @ 9600 baud + polar radar sweep)
 * 3. Frequency Matrix & Oscilloscope (FM transmitter telemetry + waveform synth)
 * 4. Atmospheric Telemetry Matrix (Farroupilha, RS weather matrix)
 *
 * Target: Foston FS-460BT (PE32 ARMv4 Windows CE 5.0 Core)
 * Screen: 480x272 16bpp TFT LCD
 * Aesthetic: Obsidian black (#000000), phosphor green (#00FF80), neon cyan (#00F0FF), amber (#FFB400)
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <mmsystem.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define DECK_VERSION        L"3.0.0"
#define TIMER_ID_FRAME      1    /* 50ms = 20 FPS for silky radar & waveform animations */
#define TIMER_ID_SLOW       2    /* 1000ms for slow weather & system updates */

#define WEATHER_FILE_SDMMC  L"\\SDMMC\\MERO\\weather.txt"
#define WEATHER_FILE_FLASH  L"\\ResidentFlash\\MERO\\weather.txt"
#define PATH_SHELL          L"\\SDMMC\\MERO\\mero-shell.exe"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum {
    VIEW_CLOCK   = 0,  /* Cyber Chrono & System Matrix */
    VIEW_GPS     = 1,  /* Satellite Radar & Orbital HUD */
    VIEW_FM      = 2,  /* Frequency Matrix & Oscilloscope */
    VIEW_WEATHER = 3,  /* Atmospheric Matrix */
    VIEW_COUNT   = 4
} DeckView;

static HINSTANCE      g_hInstance = NULL;
static HWND           g_hWnd = NULL;
static const int      g_screenW = 480;
static const int      g_screenH = 272;
static DeckView       g_currentView = VIEW_CLOCK;

/* Minimalist Red [X] Exit Button Rect */
static const RECT     g_rcBtnExit = { 448, 4, 474, 22 };

/* Animation & Glitch Counters */
static int            g_frameTick = 0;
static int            g_radarAngle = 0;
static float          g_oscPhase = 0.0f;
static unsigned int   g_entropySeed = 0x8A4F;

/* --- Live System Metrics --- */
static DWORD          g_memTotalMB = 53;
static DWORD          g_memAvailMB = 38;
static DWORD          g_memLoad = 28;
static BOOL           g_isAC = TRUE;
static int            g_batteryPercent = 100;
static DWORD          g_uptimeSec = 0;

/* --- GPS / Satellite State --- */
typedef struct {
    int  prn;
    int  elev;
    int  azim;
    int  snr;
    BOOL active;
} GpsSat;

#define MAX_SATS 16
static GpsSat         g_sats[MAX_SATS];
static int            g_satCount = 0;
static HANDLE         g_hGps = INVALID_HANDLE_VALUE;
static char           g_gpsRxBuf[512] = "";
static int            g_gpsRxLen = 0;
static BOOL           g_gpsHasFix = FALSE;
static WCHAR          g_gpsLat[32] = L"29 31.4820 S";
static WCHAR          g_gpsLon[32] = L"51 20.1540 W";
static float          g_gpsSpeedKmh = 0.0f;
static float          g_gpsAltitudeM = 742.0f;
static float          g_gpsHeadingDeg = 184.0f;
static DWORD          g_lastGpsSentenceTick = 0;

/* --- Weather Telemetry State --- */
static WCHAR          g_wxLocation[64]  = L"FARROUPILHA, RS [BR]";
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

/* --- SYSTEM TELEMETRY HARVESTING --- */
static void UpdateSystemMetrics(void)
{
    MEMORYSTATUS ms;
    memset(&ms, 0, sizeof(ms));
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    g_memTotalMB = (DWORD)(ms.dwTotalPhys / (1024 * 1024));
    g_memAvailMB = (DWORD)(ms.dwAvailPhys / (1024 * 1024));
    g_memLoad    = ms.dwMemoryLoad;

    SYSTEM_POWER_STATUS_EX2 sps;
    memset(&sps, 0, sizeof(sps));
    if (GetSystemPowerStatusEx2(&sps, sizeof(sps), FALSE)) {
        g_isAC = (sps.ACLineStatus == 1);
        if (sps.BatteryLifePercent <= 100) g_batteryPercent = sps.BatteryLifePercent;
        else g_batteryPercent = -1;
    }

    g_uptimeSec = GetTickCount() / 1000;
}

/* --- GPS SERIAL UART (COM1: @ 9600 baud NMEA-0183) --- */
static void InitGpsPort(void)
{
    if (g_hGps != INVALID_HANDLE_VALUE) return;

    /* Windows CE internal GPS is hardwired to COM1: @ 9600 baud */
    g_hGps = CreateFileW(L"COM1:", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (g_hGps != INVALID_HANDLE_VALUE) {
        DCB dcb;
        memset(&dcb, 0, sizeof(dcb));
        dcb.DCBlength = sizeof(dcb);
        if (GetCommState(g_hGps, &dcb)) {
            dcb.BaudRate = CBR_9600;
            dcb.ByteSize = 8;
            dcb.Parity   = NOPARITY;
            dcb.StopBits = ONESTOPBIT;
            dcb.fBinary  = TRUE;
            SetCommState(g_hGps, &dcb);
        }
        COMMTIMEOUTS ct;
        ct.ReadIntervalTimeout = MAXDWORD;
        ct.ReadTotalTimeoutMultiplier = 0;
        ct.ReadTotalTimeoutConstant = 0;
        ct.WriteTotalTimeoutMultiplier = 0;
        ct.WriteTotalTimeoutConstant = 0;
        SetCommTimeouts(g_hGps, &ct);
    }
}

/* Parse a single field from comma-separated NMEA sentence */
static const char *GetNmeaField(const char *str, int fieldIndex, char *outBuf, int maxLen)
{
    int i = 0;
    outBuf[0] = '\0';
    while (str && *str && i < fieldIndex) {
        if (*str == ',') i++;
        str++;
    }
    if (!str || !*str || *str == ',' || *str == '*') return "";

    int len = 0;
    while (*str && *str != ',' && *str != '*' && *str != '\r' && *str != '\n' && len < maxLen - 1) {
        outBuf[len++] = *str++;
    }
    outBuf[len] = '\0';
    return outBuf;
}

static void ProcessNmeaSentence(const char *nmea)
{
    char f[32];
    if (strncmp(nmea, "$GPRMC", 6) == 0) {
        /* $GPRMC,time,status,lat,latHem,lon,lonHem,speedKnots,heading,date,... */
        GetNmeaField(nmea, 2, f, sizeof(f));
        g_gpsHasFix = (f[0] == 'A');

        /* Sync System Clock with atomic GPS time */
        if (g_gpsHasFix) {
            char timeVal[16], dateVal[16];
            GetNmeaField(nmea, 1, timeVal, sizeof(timeVal));
            GetNmeaField(nmea, 9, dateVal, sizeof(dateVal));
            if (strlen(timeVal) >= 6 && strlen(dateVal) == 6) {
                int hh = (timeVal[0] - '0') * 10 + (timeVal[1] - '0');
                int mm = (timeVal[2] - '0') * 10 + (timeVal[3] - '0');
                int ss = (timeVal[4] - '0') * 10 + (timeVal[5] - '0');
                int d  = (dateVal[0] - '0') * 10 + (dateVal[1] - '0');
                int m  = (dateVal[2] - '0') * 10 + (dateVal[3] - '0');
                int y  = 2000 + (dateVal[4] - '0') * 10 + (dateVal[5] - '0');
                if (d >= 1 && d <= 31 && m >= 1 && m <= 12 && hh >= 0 && hh < 24 && y >= 2024 && y <= 2040) {
                    SYSTEMTIME stUtc;
                    memset(&stUtc, 0, sizeof(stUtc));
                    stUtc.wYear = (WORD)y;
                    stUtc.wMonth = (WORD)m;
                    stUtc.wDay = (WORD)d;
                    stUtc.wHour = (WORD)hh;
                    stUtc.wMinute = (WORD)mm;
                    stUtc.wSecond = (WORD)ss;
                    SetSystemTime(&stUtc);
                }
            }
        }

        char latVal[24], latHem[4], lonVal[24], lonHem[4];
        GetNmeaField(nmea, 3, latVal, sizeof(latVal));
        GetNmeaField(nmea, 4, latHem, sizeof(latHem));
        GetNmeaField(nmea, 5, lonVal, sizeof(lonVal));
        GetNmeaField(nmea, 6, lonHem, sizeof(lonHem));

        if (latVal[0] && lonVal[0]) {
            wsprintfW(g_gpsLat, L"%hs %hs", latVal, latHem);
            wsprintfW(g_gpsLon, L"%hs %hs", lonVal, lonHem);
        }

        GetNmeaField(nmea, 7, f, sizeof(f));
        if (f[0]) g_gpsSpeedKmh = (float)(atof(f) * 1.852);

        GetNmeaField(nmea, 8, f, sizeof(f));
        if (f[0]) g_gpsHeadingDeg = (float)atof(f);

        g_lastGpsSentenceTick = GetTickCount();

    } else if (strncmp(nmea, "$GPGGA", 6) == 0) {
        /* $GPGGA,time,lat,latHem,lon,lonHem,quality,sats,hdop,alt,altUnit,... */
        GetNmeaField(nmea, 6, f, sizeof(f));
        if (f[0] && f[0] != '0') g_gpsHasFix = TRUE;

        GetNmeaField(nmea, 9, f, sizeof(f));
        if (f[0]) g_gpsAltitudeM = (float)atof(f);

        g_lastGpsSentenceTick = GetTickCount();

    } else if (strncmp(nmea, "$GPGSV", 6) == 0) {
        /* $GPGSV,totalMsgs,msgNum,totalSats, sat1_prn,elev,azim,snr, ... */
        GetNmeaField(nmea, 3, f, sizeof(f));
        int totalSats = atoi(f);
        if (totalSats > 0) g_satCount = (totalSats > MAX_SATS) ? MAX_SATS : totalSats;

        int msgNum = atoi(GetNmeaField(nmea, 2, f, sizeof(f)));
        int baseIdx = (msgNum - 1) * 4;

        int s;
        for (s = 0; s < 4; s++) {
            int idx = baseIdx + s;
            if (idx >= MAX_SATS) break;
            int fPrn  = 4 + s * 4;
            int fElev = 5 + s * 4;
            int fAzim = 6 + s * 4;
            int fSnr  = 7 + s * 4;

            GetNmeaField(nmea, fPrn, f, sizeof(f));
            if (f[0]) g_sats[idx].prn = atoi(f);
            GetNmeaField(nmea, fElev, f, sizeof(f));
            if (f[0]) g_sats[idx].elev = atoi(f);
            GetNmeaField(nmea, fAzim, f, sizeof(f));
            if (f[0]) g_sats[idx].azim = atoi(f);
            GetNmeaField(nmea, fSnr, f, sizeof(f));
            if (f[0]) g_sats[idx].snr = atoi(f);
            g_sats[idx].active = (g_sats[idx].snr > 0);
        }
        g_lastGpsSentenceTick = GetTickCount();
    }
}

static void PollGpsReceiver(void)
{
    if (g_hGps == INVALID_HANDLE_VALUE) {
        InitGpsPort();
        if (g_hGps == INVALID_HANDLE_VALUE) return;
    }

    char chunk[128];
    DWORD bytesRead = 0;
    int loop = 4;

    while (loop-- > 0 && ReadFile(g_hGps, chunk, sizeof(chunk) - 1, &bytesRead, NULL) && bytesRead > 0) {
        DWORD i;
        for (i = 0; i < bytesRead; i++) {
            char c = chunk[i];
            if (c == '\r') continue;
            if (c == '\n') {
                g_gpsRxBuf[g_gpsRxLen] = '\0';
                if (g_gpsRxLen > 5 && g_gpsRxBuf[0] == '$') {
                    ProcessNmeaSentence(g_gpsRxBuf);
                }
                g_gpsRxLen = 0;
            } else {
                if (g_gpsRxLen < (int)sizeof(g_gpsRxBuf) - 2) {
                    g_gpsRxBuf[g_gpsRxLen++] = c;
                }
            }
        }
    }

    /* If indoor or no satellite lock, synthesize plausible orbital targets so radar is always alive! */
    DWORD now = GetTickCount();
    if (g_lastGpsSentenceTick == 0 || (now - g_lastGpsSentenceTick) > 3000) {
        g_satCount = 7;
        int prns[7]  = { 3, 11, 14, 19, 22, 28, 31 };
        int elevs[7] = { 65, 42, 28, 78, 15, 52, 36 };
        int azims[7] = { 45, 120, 210, 315, 80, 160, 275 };
        int snrs[7]  = { 42, 38, 25, 48, 18, 35, 29 };
        int i;
        for (i = 0; i < 7; i++) {
            g_sats[i].prn  = prns[i];
            g_sats[i].elev = elevs[i];
            g_sats[i].azim = (azims[i] + (g_frameTick / 4)) % 360;
            g_sats[i].snr  = snrs[i];
            g_sats[i].active = TRUE;
        }
    }
}

/* --- WEATHER STORAGE POLLING --- */
static void PollWeatherFile(void)
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

/* --- FM TRANSMITTER & AUDIO SYNTHESIZER SUBSYSTEM --- */
#define FM_CFG_SDMMC        L"\\SDMMC\\MERO\\fm.cfg"
#define FM_CFG_FLASH        L"\\ResidentFlash\\MERO\\fm.cfg"

static int   g_fmFreqKHz     = 107900;  /* 107.9 MHz default (in kHz) */
static BOOL  g_fmTxActive    = TRUE;    /* FM broadcast active */
static BOOL  g_fmSpkActive   = TRUE;    /* Local speaker active */

typedef enum {
    FM_AUDIO_MUTE  = 0,
    FM_AUDIO_TONE  = 1,   /* 1 kHz synth tone */
    FM_AUDIO_NOISE = 2,   /* White noise */
    FM_AUDIO_COUNT = 3
} FmAudioMode;

static FmAudioMode g_fmAudioMode = FM_AUDIO_TONE;

/* Audio synth constants and buffers */
#define AUDIO_SAMPLE_RATE  22050
#define AUDIO_BUF_SAMPLES  2048
#define AUDIO_NUM_BUFFERS  2

static HWAVEOUT   g_hWaveOut = NULL;
static WAVEHDR    g_waveHdr[AUDIO_NUM_BUFFERS];
static short      g_pcmBuf[AUDIO_NUM_BUFFERS][AUDIO_BUF_SAMPLES];
static float      g_tonePhase = 0.0f;

/* FM interactive touch button areas */
static const RECT g_rcFmDown10  = { 156, 30, 202, 58 }; /* [ -1.0 ] */
static const RECT g_rcFmDown1   = { 206, 30, 252, 58 }; /* [ -0.1 ] */
static const RECT g_rcFmUp1     = { 256, 30, 302, 58 }; /* [ +0.1 ] */
static const RECT g_rcFmUp10    = { 306, 30, 352, 58 }; /* [ +1.0 ] */
static const RECT g_rcFmTx      = { 358, 30, 466, 58 }; /* [ TX: ACTIVE / OFF ] */

static const RECT g_rcAudioBtn  = { 14,  152, 170, 176 }; /* [ AUDIO: TONE / NOISE / MUTE ] */
static const RECT g_rcSpkBtn    = { 176, 152, 310, 176 }; /* [ SPEAKER: ON / MUTE ] */
static const RECT g_rcPresetBtn = { 316, 152, 466, 176 }; /* [ PRESET >> ] */

/* Apply hardware FM transmitter frequency and power state */
static void ApplyFmHardware(int freqKHz, BOOL enable)
{
    int i;
    /* 1. Try WinCE Stream device drivers */
    const WCHAR *devNames[] = { L"FMC1:", L"FMC0:", L"FMT1:", L"FMT0:", L"RAD1:", L"FMC:", NULL };
    for (i = 0; devNames[i]; i++) {
        HANDLE hDev = CreateFileW(devNames[i], GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hDev != INVALID_HANDLE_VALUE) {
            DWORD bytesRet = 0;
            DWORD freq100k = (DWORD)(freqKHz / 100);
            DWORD inBuf[4];
            inBuf[0] = enable ? 1 : 0;
            inBuf[1] = (DWORD)freqKHz;
            inBuf[2] = freq100k;
            inBuf[3] = 0;
            
            DeviceIoControl(hDev, 0x00220004, inBuf, sizeof(inBuf), NULL, 0, &bytesRet, NULL);
            DeviceIoControl(hDev, 0x00220008, &freq100k, sizeof(DWORD), NULL, 0, &bytesRet, NULL);
            DeviceIoControl(hDev, 0x0022000C, &inBuf[0], sizeof(DWORD), NULL, 0, &bytesRet, NULL);
            DeviceIoControl(hDev, 0x800, &freq100k, sizeof(DWORD), NULL, 0, &bytesRet, NULL);
            DeviceIoControl(hDev, 0x801, &inBuf[0], sizeof(DWORD), NULL, 0, &bytesRet, NULL);
            CloseHandle(hDev);
        }
    }

    /* 2. Try dynamic loading of vendor DLLs (fmc.dll / fmtx.dll) */
    HMODULE hFmc = LoadLibraryW(L"fmc.dll");
    if (!hFmc) hFmc = LoadLibraryW(L"fmtx.dll");
    if (hFmc) {
        typedef BOOL (WINAPI *PFN_SETFREQ)(DWORD freq);
        typedef BOOL (WINAPI *PFN_ENABLE)(BOOL en);

        PFN_SETFREQ pfnSetFreq = (PFN_SETFREQ)GetProcAddress(hFmc, L"FM_SetFreq");
        if (!pfnSetFreq) pfnSetFreq = (PFN_SETFREQ)GetProcAddress(hFmc, L"SetFrequency");
        if (!pfnSetFreq) pfnSetFreq = (PFN_SETFREQ)GetProcAddress(hFmc, L"SetFreq");
        if (!pfnSetFreq) pfnSetFreq = (PFN_SETFREQ)GetProcAddress(hFmc, L"FMC_SetFreq");

        if (pfnSetFreq) {
            pfnSetFreq((DWORD)(freqKHz / 100));
            pfnSetFreq((DWORD)freqKHz);
        }

        PFN_ENABLE pfnEn = (PFN_ENABLE)GetProcAddress(hFmc, L"FM_Enable");
        if (!pfnEn) pfnEn = (PFN_ENABLE)GetProcAddress(hFmc, L"FMC_Enable");
        if (!pfnEn) pfnEn = (PFN_ENABLE)GetProcAddress(hFmc, L"FM_Power");
        if (pfnEn) {
            pfnEn(enable);
        }

        FreeLibrary(hFmc);
    }

    /* 3. Update OEM registry configurations used by vendor shell & GPS firmware */
    HKEY hKey;
    const WCHAR *regKeys[] = {
        L"Software\\Mesada\\FM",
        L"Software\\Foston\\FM",
        L"Software\\Apical\\FM",
        L"Drivers\\BuiltIn\\fmc",
        L"Software\\Mero\\FM",
        NULL
    };
    for (i = 0; regKeys[i]; i++) {
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, regKeys[i], 0, NULL, 0, 0, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            DWORD dwFreq100k = (DWORD)(freqKHz / 100);
            DWORD dwFreqKHz  = (DWORD)freqKHz;
            DWORD dwEn       = enable ? 1 : 0;
            RegSetValueExW(hKey, L"Frequency", 0, REG_DWORD, (const BYTE*)&dwFreq100k, sizeof(DWORD));
            RegSetValueExW(hKey, L"FreqKHz", 0, REG_DWORD, (const BYTE*)&dwFreqKHz, sizeof(DWORD));
            RegSetValueExW(hKey, L"Enable", 0, REG_DWORD, (const BYTE*)&dwEn, sizeof(DWORD));
            RegSetValueExW(hKey, L"Power", 0, REG_DWORD, (const BYTE*)&dwEn, sizeof(DWORD));
            RegFlushKey(hKey);
            RegCloseKey(hKey);
        }
    }
}

/* Audio synthesizer initialization */
static void InitAudioSynth(void)
{
    if (g_hWaveOut) return;

    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = AUDIO_SAMPLE_RATE;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    if (waveOutOpen(&g_hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
        int b;
        for (b = 0; b < AUDIO_NUM_BUFFERS; b++) {
            memset(g_pcmBuf[b], 0, sizeof(g_pcmBuf[b]));
            memset(&g_waveHdr[b], 0, sizeof(WAVEHDR));
            g_waveHdr[b].lpData = (LPSTR)g_pcmBuf[b];
            g_waveHdr[b].dwBufferLength = sizeof(g_pcmBuf[b]);
            waveOutPrepareHeader(g_hWaveOut, &g_waveHdr[b], sizeof(WAVEHDR));
            g_waveHdr[b].dwFlags |= WHDR_DONE;
        }
        DWORD vol = g_fmSpkActive ? 0xC000C000 : 0x00000000;
        waveOutSetVolume(g_hWaveOut, vol);
    }
}

/* Synthesize next chunk of audio and submit to waveOut */
static void FeedAudioSynth(void)
{
    if (!g_hWaveOut) {
        InitAudioSynth();
        if (!g_hWaveOut) return;
    }

    int b;
    for (b = 0; b < AUDIO_NUM_BUFFERS; b++) {
        if (g_waveHdr[b].dwFlags & WHDR_DONE) {
            waveOutUnprepareHeader(g_hWaveOut, &g_waveHdr[b], sizeof(WAVEHDR));

            short *pcm = g_pcmBuf[b];
            int i;
            if (g_fmAudioMode == FM_AUDIO_TONE) {
                float step = (float)(2.0 * M_PI * 1000.0 / (double)AUDIO_SAMPLE_RATE);
                for (i = 0; i < AUDIO_BUF_SAMPLES; i++) {
                    pcm[i] = (short)(sinf(g_tonePhase) * 14000.0f);
                    g_tonePhase += step;
                    if (g_tonePhase > (float)(2.0 * M_PI)) g_tonePhase -= (float)(2.0 * M_PI);
                }
            } else if (g_fmAudioMode == FM_AUDIO_NOISE) {
                for (i = 0; i < AUDIO_BUF_SAMPLES; i++) {
                    pcm[i] = (short)(((rand() % 32768) - 16384) * 0.40f);
                }
            } else {
                memset(pcm, 0, sizeof(g_pcmBuf[b]));
            }

            g_waveHdr[b].lpData = (LPSTR)pcm;
            g_waveHdr[b].dwBufferLength = sizeof(g_pcmBuf[b]);
            g_waveHdr[b].dwFlags = 0;
            waveOutPrepareHeader(g_hWaveOut, &g_waveHdr[b], sizeof(WAVEHDR));
            waveOutWrite(g_hWaveOut, &g_waveHdr[b], sizeof(WAVEHDR));
        }
    }
}

static void CloseAudioSynth(void)
{
    if (g_hWaveOut) {
        waveOutReset(g_hWaveOut);
        int b;
        for (b = 0; b < AUDIO_NUM_BUFFERS; b++) {
            waveOutUnprepareHeader(g_hWaveOut, &g_waveHdr[b], sizeof(WAVEHDR));
        }
        waveOutClose(g_hWaveOut);
        g_hWaveOut = NULL;
    }
}

/* Load persisted FM configuration */
static void LoadFmConfig(void)
{
    HANDLE hFile;
    DWORD bytesRead;
    char buffer[256];

    g_fmFreqKHz = 107900;
    g_fmTxActive = TRUE;
    g_fmAudioMode = FM_AUDIO_TONE;
    g_fmSpkActive = TRUE;

    hFile = CreateFileW(FM_CFG_SDMMC, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        hFile = CreateFileW(FM_CFG_FLASH, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    }
    if (hFile != INVALID_HANDLE_VALUE) {
        memset(buffer, 0, sizeof(buffer));
        if (ReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            int freq = 0, tx = 0, audio = 0, spk = 1;
            char *line = strtok(buffer, "\r\n");
            while (line) {
                if (sscanf(line, "freq=%d", &freq) == 1) {
                    if (freq >= 76000 && freq <= 108000) g_fmFreqKHz = freq;
                } else if (sscanf(line, "tx=%d", &tx) == 1) {
                    g_fmTxActive = (tx != 0);
                } else if (sscanf(line, "audio=%d", &audio) == 1) {
                    if (audio >= 0 && audio < FM_AUDIO_COUNT) g_fmAudioMode = (FmAudioMode)audio;
                } else if (sscanf(line, "speaker=%d", &spk) == 1) {
                    g_fmSpkActive = (spk != 0);
                }
                line = strtok(NULL, "\r\n");
            }
        }
        CloseHandle(hFile);
    } else {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Mero\\FM", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD dwVal = 0, dwLen = sizeof(dwVal);
            if (RegQueryValueExW(hKey, L"FreqKHz", NULL, NULL, (LPBYTE)&dwVal, &dwLen) == ERROR_SUCCESS && dwVal >= 76000 && dwVal <= 108000) {
                g_fmFreqKHz = (int)dwVal;
            }
            dwLen = sizeof(dwVal);
            if (RegQueryValueExW(hKey, L"Enable", NULL, NULL, (LPBYTE)&dwVal, &dwLen) == ERROR_SUCCESS) {
                g_fmTxActive = (dwVal != 0);
            }
            RegCloseKey(hKey);
        }
    }
    ApplyFmHardware(g_fmFreqKHz, g_fmTxActive);
}

/* Save FM configuration */
static void SaveFmConfig(void)
{
    HANDLE hFile;
    DWORD bytesWritten;
    char buffer[128];
    int len = sprintf(buffer, "freq=%d\r\ntx=%d\r\naudio=%d\r\nspeaker=%d\r\n",
                      g_fmFreqKHz, g_fmTxActive ? 1 : 0, (int)g_fmAudioMode, g_fmSpkActive ? 1 : 0);

    CreateDirectoryW(L"\\SDMMC\\MERO", NULL);
    CreateDirectoryW(L"\\ResidentFlash\\MERO", NULL);

    hFile = CreateFileW(FM_CFG_SDMMC, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, buffer, (DWORD)len, &bytesWritten, NULL);
        FlushFileBuffers(hFile);
        CloseHandle(hFile);
    }
    hFile = CreateFileW(FM_CFG_FLASH, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        WriteFile(hFile, buffer, (DWORD)len, &bytesWritten, NULL);
        FlushFileBuffers(hFile);
        CloseHandle(hFile);
    }
    ApplyFmHardware(g_fmFreqKHz, g_fmTxActive);
}

/* Suppress Winsock and repllog desktop communications error dialogs */
static void DismissWinsockDialogs(void)
{
    HWND hWnd = FindWindowW(NULL, L"Cannot start communications with the desktop computer.");
    if (hWnd) PostMessageW(hWnd, WM_CLOSE, 0, 0);
    HWND hWnd2 = FindWindowW(NULL, L"Communications Error");
    if (hWnd2) PostMessageW(hWnd2, WM_CLOSE, 0, 0);
    HWND hWndRepl = FindWindowW(NULL, L"Repllog");
    if (hWndRepl) PostMessageW(hWndRepl, WM_CLOSE, 0, 0);

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(hSnap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"repllog.exe") == 0 ||
                    _wcsicmp(pe.szExeFile, L"rnaapp.exe") == 0) {
                    HANDLE hProc = OpenProcess(0x0001, FALSE, pe.th32ProcessID);
                    if (hProc) {
                        TerminateProcess(hProc, 0);
                        CloseHandle(hProc);
                    }
                }
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
}

/* ====================================================================
 * VIEW 1: CYBER CHRONO & SYSTEM MATRIX
 * ==================================================================== */
static void PaintClockView(HDC hdc)
{
    WCHAR buf[128];
    SYSTEMTIME st;
    GetLocalTime(&st);

    /* --- GIANT DIGITAL CLOCK --- */
    /* Time HH:MM:SS */
    wsprintfW(buf, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);

    /* Cyber Bordered Digital Frame */
    RECT timeBox = { 12, 34, 250, 96 };
    HBRUSH hBoxBr = CreateSolidBrush(RGB(6, 20, 16));
    HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hBoxBr);
    HPEN hBoxPen = CreatePen(PS_SOLID, 1, RGB(0, 255, 160));
    HPEN hOldP = (HPEN)SelectObject(hdc, hBoxPen);
    RoundRect(hdc, timeBox.left, timeBox.top, timeBox.right, timeBox.bottom, 6, 6);
    SelectObject(hdc, hOldP);
    SelectObject(hdc, hOldBr);
    DeleteObject(hBoxPen);
    DeleteObject(hBoxBr);

    /* Glowing Digital Time Display — large font via CreateFontIndirectW (only API in WinCE coredll) */
    LOGFONT lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -44;
    lf.lfWeight = FW_BOLD;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_DONTCARE;
    HFONT hClockFont = CreateFontIndirectW(&lf);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hClockFont ? hClockFont : GetStockObject(SYSTEM_FIXED_FONT));
    SetTextColor(hdc, RGB(0, 255, 200));
    RECT timeTextRc = { 16, 38, 248, 94 };
    DrawTextW(hdc, buf, -1, &timeTextRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hOldFont);
    if (hClockFont) DeleteObject(hClockFont);


    /* Date and Day of Week */
    static const WCHAR *days[] = { L"SUNDAY", L"MONDAY", L"TUESDAY", L"WEDNESDAY", L"THURSDAY", L"FRIDAY", L"SATURDAY" };
    wsprintfW(buf, L"%04d-%02d-%02d // %ls", st.wYear, st.wMonth, st.wDay, days[st.wDayOfWeek % 7]);
    SetTextColor(hdc, RGB(255, 215, 0));
    ExtTextOutW(hdc, 16, 104, 0, NULL, buf, lstrlenW(buf), NULL);

    /* --- SYSTEM TELEMETRY MATRIX (Left Lower) --- */
    int y = 126;
    SetTextColor(hdc, RGB(0, 220, 150));
    ExtTextOutW(hdc, 16, y, 0, NULL, L"SYSTEM HARDWARE TELEMETRY", 25, NULL);

    y += 18;
    int uph = (int)(g_uptimeSec / 3600);
    int upm = (int)((g_uptimeSec % 3600) / 60);
    int ups = (int)(g_uptimeSec % 60);
    wsprintfW(buf, L"UPTIME: %02dh %02dm %02ds", uph, upm, ups);
    SetTextColor(hdc, RGB(160, 240, 220));
    ExtTextOutW(hdc, 16, y, 0, NULL, buf, lstrlenW(buf), NULL);

    y += 18;
    if (g_isAC) {
        wsprintfW(buf, L"POWER:  AC ONLINE [CHARGED]");
        SetTextColor(hdc, RGB(0, 255, 128));
    } else {
        wsprintfW(buf, L"POWER:  BATTERY %d%%", g_batteryPercent);
        SetTextColor(hdc, RGB(255, 180, 40));
    }
    ExtTextOutW(hdc, 16, y, 0, NULL, buf, lstrlenW(buf), NULL);

    y += 18;
    wsprintfW(buf, L"MEMORY: %lu MB / %lu MB [%lu%% USED]", g_memTotalMB - g_memAvailMB, g_memTotalMB, g_memLoad);
    SetTextColor(hdc, RGB(160, 240, 220));
    ExtTextOutW(hdc, 16, y, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Memory Segmented Bar */
    y += 16;
    int barW = 220, barH = 6;
    RECT memRc = { 16, y, 16 + barW, y + barH };
    HBRUSH hTrk = CreateSolidBrush(RGB(15, 25, 22));
    FillRect(hdc, &memRc, hTrk);
    DeleteObject(hTrk);

    int fillW = (int)((g_memLoad * barW) / 100);
    if (fillW > barW) fillW = barW;
    if (fillW > 0) {
        RECT fRc = { 16, y, 16 + fillW, y + barH };
        HBRUSH hFill = CreateSolidBrush(RGB(0, 240, 255));
        FillRect(hdc, &fRc, hFill);
        DeleteObject(hFill);
    }

    y += 14;
    SetTextColor(hdc, RGB(0, 140, 110));
    ExtTextOutW(hdc, 16, y, 0, NULL, L"CPU: MT3351 ARM926EJ-S @ 468 MHz", 32, NULL);

    /* --- RIGHT SECTION: EERIE GLITCH & TELEMETRY STREAM --- */
    RECT glitchBox = { 260, 34, 468, 238 };
    HBRUSH hGlitchBr = CreateSolidBrush(RGB(4, 12, 10));
    HBRUSH hOldGBr = (HBRUSH)SelectObject(hdc, hGlitchBr);
    HPEN hGlitchPen = CreatePen(PS_SOLID, 1, RGB(0, 160, 120));
    HPEN hOldGP = (HPEN)SelectObject(hdc, hGlitchPen);
    RoundRect(hdc, glitchBox.left, glitchBox.top, glitchBox.right, glitchBox.bottom, 4, 4);
    SelectObject(hdc, hOldGP);
    SelectObject(hdc, hOldGBr);
    DeleteObject(hGlitchPen);
    DeleteObject(hGlitchBr);

    SetTextColor(hdc, RGB(0, 255, 180));
    ExtTextOutW(hdc, 270, 42, 0, NULL, L"NEO-TOKYO CHRONO VECTOR", 23, NULL);

    /* Cyber Divider */
    HPEN hDivP = CreatePen(PS_SOLID, 1, RGB(16, 40, 32));
    HPEN hOldDP = (HPEN)SelectObject(hdc, hDivP);
    MoveToEx(hdc, 270, 58, NULL); LineTo(hdc, 458, 58);
    SelectObject(hdc, hOldDP);
    DeleteObject(hDivP);

    /* Dynamic Pseudo-Random Glitch Telemetry Stream */
    g_entropySeed = (g_entropySeed * 1103515245 + 12345);
    int gy = 66;

    wsprintfW(buf, L"> CHRONO REF: RTC.STANDALONE");
    SetTextColor(hdc, RGB(100, 180, 160));
    ExtTextOutW(hdc, 270, gy, 0, NULL, buf, lstrlenW(buf), NULL); gy += 16;

    wsprintfW(buf, L"> ADDR 0x%04X: 4F 12 C8 E0", (g_entropySeed & 0x7FFF));
    SetTextColor(hdc, RGB(0, 240, 255));
    ExtTextOutW(hdc, 270, gy, 0, NULL, buf, lstrlenW(buf), NULL); gy += 16;

    wsprintfW(buf, L"> ENTROPY: 0x%02X [OK]", (g_entropySeed >> 8) & 0xFF);
    SetTextColor(hdc, RGB(0, 255, 160));
    ExtTextOutW(hdc, 270, gy, 0, NULL, buf, lstrlenW(buf), NULL); gy += 16;

    wsprintfW(buf, L"> UART1.GPS: %ls", (g_hGps != INVALID_HANDLE_VALUE) ? L"COM1 ONLINE" : L"SEARCHING");
    SetTextColor(hdc, (g_hGps != INVALID_HANDLE_VALUE) ? RGB(0, 255, 128) : RGB(255, 180, 40));
    ExtTextOutW(hdc, 270, gy, 0, NULL, buf, lstrlenW(buf), NULL); gy += 16;

    wsprintfW(buf, L"> FMC.RADIO: 107.9 MHz [STBY]");
    SetTextColor(hdc, RGB(255, 215, 0));
    ExtTextOutW(hdc, 270, gy, 0, NULL, buf, lstrlenW(buf), NULL); gy += 16;

    wsprintfW(buf, L"> STORAGE: FAT32 DIRECT LINK");
    SetTextColor(hdc, RGB(100, 180, 160));
    ExtTextOutW(hdc, 270, gy, 0, NULL, buf, lstrlenW(buf), NULL); gy += 16;

    /* Subtle Flashing Terminal Prompt */
    if ((g_frameTick / 10) % 2 == 0) {
        wsprintfW(buf, L"> SYS.INTEGRITY: 100%% _");
        SetTextColor(hdc, RGB(0, 255, 200));
    } else {
        wsprintfW(buf, L"> SYS.INTEGRITY: 100%%");
        SetTextColor(hdc, RGB(0, 200, 160));
    }
    ExtTextOutW(hdc, 270, gy, 0, NULL, buf, lstrlenW(buf), NULL); gy += 18;

    /* Miniature ASCII Cyber Wave */
    SetTextColor(hdc, RGB(0, 140, 100));
    ExtTextOutW(hdc, 270, gy, 0, NULL, L"[~--~--~--~--~--~--~--~]", 24, NULL);
}

/* ====================================================================
 * VIEW 2: SATELLITE RADAR & ORBITAL HUD (COM1: @ 9600 baud)
 * ==================================================================== */
static void PaintGpsRadarView(HDC hdc)
{
    WCHAR buf[128];
    int cx = 100, cy = 136, rMax = 82;

    /* --- POLAR RADAR CIRCULAR GRID --- */
    HPEN hGridPen = CreatePen(PS_SOLID, 1, RGB(10, 45, 30));
    HPEN hOldP = (HPEN)SelectObject(hdc, hGridPen);

    /* Concentric elevation rings: 30°, 60°, 90° */
    Ellipse(hdc, cx - rMax, cy - rMax, cx + rMax, cy + rMax);
    Ellipse(hdc, cx - (rMax * 2 / 3), cy - (rMax * 2 / 3), cx + (rMax * 2 / 3), cy + (rMax * 2 / 3));
    Ellipse(hdc, cx - (rMax / 3), cy - (rMax / 3), cx + (rMax / 3), cy + (rMax / 3));

    /* Crosshairs */
    MoveToEx(hdc, cx - rMax - 6, cy, NULL); LineTo(hdc, cx + rMax + 6, cy);
    MoveToEx(hdc, cx, cy - rMax - 6, NULL); LineTo(hdc, cx, cy + rMax + 6);
    SelectObject(hdc, hOldP);
    DeleteObject(hGridPen);

    /* Cardinal Labels */
    SetTextColor(hdc, RGB(0, 255, 160));
    ExtTextOutW(hdc, cx - 4, cy - rMax - 16, 0, NULL, L"N", 1, NULL);
    ExtTextOutW(hdc, cx - 4, cy + rMax + 2, 0, NULL, L"S", 1, NULL);
    ExtTextOutW(hdc, cx + rMax + 8, cy - 6, 0, NULL, L"E", 1, NULL);
    ExtTextOutW(hdc, cx - rMax - 16, cy - 6, 0, NULL, L"W", 1, NULL);

    /* --- ROTATING RADAR SWEEP LINE --- */
    float rad = (float)(g_radarAngle * M_PI / 180.0);
    int sx = cx + (int)(rMax * cos(rad));
    int sy = cy + (int)(rMax * sin(rad));

    HPEN hSweep = CreatePen(PS_SOLID, 2, RGB(0, 255, 128));
    hOldP = (HPEN)SelectObject(hdc, hSweep);
    MoveToEx(hdc, cx, cy, NULL);
    LineTo(hdc, sx, sy);
    SelectObject(hdc, hOldP);
    DeleteObject(hSweep);

    /* Center blip */
    HBRUSH hCtr = CreateSolidBrush(RGB(0, 255, 200));
    HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hCtr);
    Ellipse(hdc, cx - 3, cy - 3, cx + 4, cy + 4);
    SelectObject(hdc, hOldBr);
    DeleteObject(hCtr);

    /* --- SATELLITE CONTACTS --- */
    int i;
    for (i = 0; i < g_satCount && i < MAX_SATS; i++) {
        if (!g_sats[i].active && g_sats[i].snr <= 0) continue;

        /* Polar projection: distance r proportional to (90 - elevation) */
        float satR = (float)(rMax * (90 - g_sats[i].elev) / 90.0);
        float satA = (float)((g_sats[i].azim - 90) * M_PI / 180.0);
        int satX = cx + (int)(satR * cos(satA));
        int satY = cy + (int)(satR * sin(satA));

        /* Blip color based on SNR */
        COLORREF satCol = (g_sats[i].snr >= 35) ? RGB(0, 255, 128) :
                          (g_sats[i].snr >= 20) ? RGB(255, 215, 0) : RGB(255, 80, 80);

        HBRUSH hSatBr = CreateSolidBrush(satCol);
        hOldBr = (HBRUSH)SelectObject(hdc, hSatBr);
        Ellipse(hdc, satX - 3, satY - 3, satX + 4, satY + 4);
        SelectObject(hdc, hOldBr);
        DeleteObject(hSatBr);

        /* Satellite PRN Tag */
        wsprintfW(buf, L"%02d", g_sats[i].prn);
        SetTextColor(hdc, RGB(160, 240, 220));
        ExtTextOutW(hdc, satX + 5, satY - 6, 0, NULL, buf, lstrlenW(buf), NULL);
    }

    /* --- RIGHT SECTION: ORBITAL TELEMETRY & COORDINATES --- */
    int rightX = 210;

    /* Fix Status Badge */
    RECT fixRc = { rightX, 32, rightX + 130, 50 };
    COLORREF fBorder = g_gpsHasFix ? RGB(0, 255, 128) : RGB(255, 180, 40);
    COLORREF fBg     = g_gpsHasFix ? RGB(0, 32, 16)   : RGB(32, 22, 0);

    HBRUSH hFBr = CreateSolidBrush(fBg);
    hOldBr = (HBRUSH)SelectObject(hdc, hFBr);
    HPEN hFP = CreatePen(PS_SOLID, 1, fBorder);
    hOldP = (HPEN)SelectObject(hdc, hFP);
    RoundRect(hdc, fixRc.left, fixRc.top, fixRc.right, fixRc.bottom, 4, 4);
    SelectObject(hdc, hOldP);
    SelectObject(hdc, hOldBr);
    DeleteObject(hFP);
    DeleteObject(hFBr);

    SetTextColor(hdc, fBorder);
    DrawTextW(hdc, g_gpsHasFix ? L"3D FIX [TRACKING]" : L"ACQUIRING FIX...", -1, &fixRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    /* Satellite Count Badge */
    wsprintfW(buf, L"SATS: %d / %d", g_satCount, MAX_SATS);
    SetTextColor(hdc, RGB(0, 240, 255));
    ExtTextOutW(hdc, rightX + 140, 34, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Coordinates */
    int y = 58;
    SetTextColor(hdc, RGB(80, 160, 140));
    ExtTextOutW(hdc, rightX, y, 0, NULL, L"POSITION:", 9, NULL);
    SetTextColor(hdc, RGB(0, 255, 200));
    wsprintfW(buf, L"LAT: %ls", g_gpsLat);
    ExtTextOutW(hdc, rightX + 60, y, 0, NULL, buf, lstrlenW(buf), NULL);
    y += 18;
    wsprintfW(buf, L"LON: %ls", g_gpsLon);
    ExtTextOutW(hdc, rightX + 60, y, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Speed & Altitude */
    y += 20;
    SetTextColor(hdc, RGB(80, 160, 140));
    ExtTextOutW(hdc, rightX, y, 0, NULL, L"SPEED:", 6, NULL);
    wsprintfW(buf, L"%.1f KM/H", g_gpsSpeedKmh);
    SetTextColor(hdc, RGB(255, 215, 0));
    ExtTextOutW(hdc, rightX + 60, y, 0, NULL, buf, lstrlenW(buf), NULL);

    ExtTextOutW(hdc, rightX + 150, y, 0, NULL, L"ALT:", 4, NULL);
    wsprintfW(buf, L"%.1f M", g_gpsAltitudeM);
    SetTextColor(hdc, RGB(0, 240, 255));
    ExtTextOutW(hdc, rightX + 190, y, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Heading */
    y += 18;
    SetTextColor(hdc, RGB(80, 160, 140));
    ExtTextOutW(hdc, rightX, y, 0, NULL, L"HEADING:", 8, NULL);
    wsprintfW(buf, L"%.1f DEG", g_gpsHeadingDeg);
    SetTextColor(hdc, RGB(160, 240, 220));
    ExtTextOutW(hdc, rightX + 60, y, 0, NULL, buf, lstrlenW(buf), NULL);

    /* --- SATELLITE SNR SIGNAL BAR GRAPH --- */
    y += 24;
    SetTextColor(hdc, RGB(0, 220, 150));
    ExtTextOutW(hdc, rightX, y, 0, NULL, L"SNR SPECTRUM [dBHz]:", 20, NULL);
    y += 16;

    int barW = 16, barMaxH = 40, gap = 4;
    int b;
    for (b = 0; b < 10 && b < g_satCount; b++) {
        int bx = rightX + b * (barW + gap);
        int snrVal = g_sats[b].snr;
        if (snrVal > 50) snrVal = 50;
        int bh = (snrVal * barMaxH) / 50;
        if (bh < 2) bh = 2;

        /* Background track */
        RECT trk = { bx, y, bx + barW, y + barMaxH };
        HBRUSH hTBr = CreateSolidBrush(RGB(15, 25, 22));
        FillRect(hdc, &trk, hTBr);
        DeleteObject(hTBr);

        /* Active bar */
        RECT bRc = { bx, y + barMaxH - bh, bx + barW, y + barMaxH };
        COLORREF bCol = (g_sats[b].snr >= 35) ? RGB(0, 255, 128) :
                        (g_sats[b].snr >= 20) ? RGB(255, 215, 0) : RGB(255, 80, 80);
        HBRUSH hBBr = CreateSolidBrush(bCol);
        FillRect(hdc, &bRc, hBBr);
        DeleteObject(hBBr);

        /* PRN label below */
        wsprintfW(buf, L"%02d", g_sats[b].prn);
        SetTextColor(hdc, RGB(100, 160, 140));
        ExtTextOutW(hdc, bx, y + barMaxH + 2, 0, NULL, buf, lstrlenW(buf), NULL);
    }
}

/* ====================================================================
 * VIEW 3: FREQUENCY MATRIX & OSCILLOSCOPE (FM Transmitter)
 * ==================================================================== */
static void PaintFmView(HDC hdc)
{
    WCHAR buf[128];
    float fFreq = (float)g_fmFreqKHz / 1000.0f;

    /* --- TOP BAR: CARRIER FREQUENCY & TUNING CONTROLS --- */
    /* 1. Large Frequency Readout Box */
    RECT rcFreqBox = { 14, 28, 150, 58 };
    HBRUSH hFreqBr = CreateSolidBrush(RGB(6, 20, 16));
    HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hFreqBr);
    HPEN hFreqPen = CreatePen(PS_SOLID, 1, RGB(0, 240, 255));
    HPEN hOldP = (HPEN)SelectObject(hdc, hFreqPen);
    RoundRect(hdc, rcFreqBox.left, rcFreqBox.top, rcFreqBox.right, rcFreqBox.bottom, 4, 4);
    SelectObject(hdc, hOldP);
    SelectObject(hdc, hOldBr);
    DeleteObject(hFreqPen);
    DeleteObject(hFreqBr);

    /* Large Bold Frequency Text */
    wsprintfW(buf, L"%.1f MHz", fFreq);
    LOGFONT lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -22;
    lf.lfWeight = FW_BOLD;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_DONTCARE;
    HFONT hFmFont = CreateFontIndirectW(&lf);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFmFont ? hFmFont : GetStockObject(SYSTEM_FIXED_FONT));
    SetTextColor(hdc, RGB(0, 255, 200));
    DrawTextW(hdc, buf, -1, &rcFreqBox, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hOldFont);
    if (hFmFont) DeleteObject(hFmFont);

    /* 2. Frequency Tuning Buttons */
    const RECT *btnTuneRcs[] = { &g_rcFmDown10, &g_rcFmDown1, &g_rcFmUp1, &g_rcFmUp10 };
    const WCHAR *btnTuneTxt[] = { L"-1.0", L"-0.1", L"+0.1", L"+1.0" };
    int i;
    for (i = 0; i < 4; i++) {
        const RECT *rcB = btnTuneRcs[i];
        HBRUSH hBBr = CreateSolidBrush(RGB(10, 26, 22));
        hOldBr = (HBRUSH)SelectObject(hdc, hBBr);
        HPEN hBPen = CreatePen(PS_SOLID, 1, RGB(0, 200, 180));
        hOldP = (HPEN)SelectObject(hdc, hBPen);
        RoundRect(hdc, rcB->left, rcB->top, rcB->right, rcB->bottom, 3, 3);
        SelectObject(hdc, hOldP);
        SelectObject(hdc, hOldBr);
        DeleteObject(hBPen);
        DeleteObject(hBBr);

        SetTextColor(hdc, RGB(0, 255, 220));
        DrawTextW(hdc, btnTuneTxt[i], -1, (LPRECT)rcB, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* 3. FM Transmitter Power Toggle Button */
    {
        COLORREF txBorder = g_fmTxActive ? RGB(0, 255, 128) : RGB(255, 140, 40);
        COLORREF txBg     = g_fmTxActive ? RGB(0, 34, 16)   : RGB(34, 20, 0);
        HBRUSH hTxBr = CreateSolidBrush(txBg);
        hOldBr = (HBRUSH)SelectObject(hdc, hTxBr);
        HPEN hTxPen = CreatePen(PS_SOLID, 1, txBorder);
        hOldP = (HPEN)SelectObject(hdc, hTxPen);
        RoundRect(hdc, g_rcFmTx.left, g_rcFmTx.top, g_rcFmTx.right, g_rcFmTx.bottom, 3, 3);
        SelectObject(hdc, hOldP);
        SelectObject(hdc, hOldBr);
        DeleteObject(hTxPen);
        DeleteObject(hTxBr);

        SetTextColor(hdc, txBorder);
        DrawTextW(hdc, g_fmTxActive ? L"[ FM TX: ON ]" : L"[ FM TX: OFF ]", -1, (LPRECT)&g_rcFmTx, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* --- OSCILLOSCOPE FRAME (x=14, y=62, w=452, h=86) --- */
    RECT oscBox = { 14, 62, 466, 148 };
    HBRUSH hOscBr = CreateSolidBrush(RGB(4, 16, 12));
    hOldBr = (HBRUSH)SelectObject(hdc, hOscBr);
    HPEN hOscPen = CreatePen(PS_SOLID, 1, RGB(0, 160, 110));
    hOldP = (HPEN)SelectObject(hdc, hOscPen);
    RoundRect(hdc, oscBox.left, oscBox.top, oscBox.right, oscBox.bottom, 4, 4);
    SelectObject(hdc, hOldP);
    SelectObject(hdc, hOldBr);
    DeleteObject(hOscPen);
    DeleteObject(hOscBr);

    /* Oscilloscope Sub-header */
    SetTextColor(hdc, RGB(0, 200, 160));
    if (g_fmAudioMode == FM_AUDIO_TONE) {
        wsprintfW(buf, L"OSC: 1 kHz TEST TONE // CARRIER %.1f MHz %ls", fFreq, g_fmTxActive ? L"[BROADCASTING]" : L"[STANDBY]");
    } else if (g_fmAudioMode == FM_AUDIO_NOISE) {
        wsprintfW(buf, L"OSC: WHITE NOISE GENERATOR // CARRIER %.1f MHz %ls", fFreq, g_fmTxActive ? L"[BROADCASTING]" : L"[STANDBY]");
    } else {
        wsprintfW(buf, L"OSC: SILENT CARRIER // %.1f MHz %ls", fFreq, g_fmTxActive ? L"[CARRIER ACTIVE]" : L"[STANDBY]");
    }
    ExtTextOutW(hdc, 22, 66, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Grid lines */
    int midY = 107;
    HPEN hGridP = CreatePen(PS_SOLID, 1, RGB(10, 36, 26));
    hOldP = (HPEN)SelectObject(hdc, hGridP);
    MoveToEx(hdc, 14, midY, NULL); LineTo(hdc, 466, midY);
    int gx;
    for (gx = 50; gx < 450; gx += 45) {
        MoveToEx(hdc, 14 + gx, 78, NULL); LineTo(hdc, 14 + gx, 146);
    }
    SelectObject(hdc, hOldP);
    DeleteObject(hGridP);

    /* Dynamic Waveform Rendering */
    COLORREF waveCol = (g_fmAudioMode == FM_AUDIO_TONE)  ? RGB(0, 255, 220) :
                       (g_fmAudioMode == FM_AUDIO_NOISE) ? RGB(255, 220, 80) : RGB(0, 180, 140);
    HPEN hWaveP = CreatePen(PS_SOLID, (g_fmAudioMode == FM_AUDIO_NOISE) ? 1 : 2, waveCol);
    hOldP = (HPEN)SelectObject(hdc, hWaveP);

    int x;
    int oscW = 448;
    for (x = 0; x < oscW; x++) {
        int wy = midY;
        if (g_fmAudioMode == FM_AUDIO_TONE) {
            float t = (float)(x * 0.08f + g_oscPhase * 2.0f);
            float w = sinf(t) * 30.0f + sinf(t * 2.0f + 0.4f) * 6.0f;
            wy = midY + (int)w;
        } else if (g_fmAudioMode == FM_AUDIO_NOISE) {
            int rnd = ((x * 1103515245 + g_frameTick * 12345) >> 16) & 0x3F;
            wy = midY + (rnd - 32);
        } else {
            if (g_fmTxActive) {
                wy = midY + (int)(sinf(x * 0.06f + g_oscPhase) * 4.0f);
            } else {
                wy = midY;
            }
        }
        if (wy < 80) wy = 80;
        if (wy > 144) wy = 144;

        if (x == 0) MoveToEx(hdc, 16 + x, wy, NULL);
        else LineTo(hdc, 16 + x, wy);
    }
    SelectObject(hdc, hOldP);
    DeleteObject(hWaveP);

    /* --- AUDIO & OUTPUT CONTROLS BAR (y=152..176) --- */
    /* 1. Audio Mode Toggle Button */
    {
        const WCHAR *audioLabels[] = { L"AUDIO: MUTE", L"AUDIO: 1kHz TONE", L"AUDIO: WHITE NOISE" };
        COLORREF aBorder = (g_fmAudioMode != FM_AUDIO_MUTE) ? RGB(0, 255, 180) : RGB(140, 140, 140);
        HBRUSH hABr = CreateSolidBrush(RGB(8, 24, 20));
        hOldBr = (HBRUSH)SelectObject(hdc, hABr);
        HPEN hAPen = CreatePen(PS_SOLID, 1, aBorder);
        hOldP = (HPEN)SelectObject(hdc, hAPen);
        RoundRect(hdc, g_rcAudioBtn.left, g_rcAudioBtn.top, g_rcAudioBtn.right, g_rcAudioBtn.bottom, 3, 3);
        SelectObject(hdc, hOldP);
        SelectObject(hdc, hOldBr);
        DeleteObject(hAPen);
        DeleteObject(hABr);

        SetTextColor(hdc, (g_fmAudioMode != FM_AUDIO_MUTE) ? RGB(0, 255, 200) : RGB(160, 160, 160));
        DrawTextW(hdc, audioLabels[g_fmAudioMode % FM_AUDIO_COUNT], -1, (LPRECT)&g_rcAudioBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* 2. Speaker Monitor Toggle Button */
    {
        COLORREF sBorder = g_fmSpkActive ? RGB(0, 240, 255) : RGB(255, 100, 100);
        HBRUSH hSBr = CreateSolidBrush(RGB(6, 20, 24));
        hOldBr = (HBRUSH)SelectObject(hdc, hSBr);
        HPEN hSPen = CreatePen(PS_SOLID, 1, sBorder);
        hOldP = (HPEN)SelectObject(hdc, hSPen);
        RoundRect(hdc, g_rcSpkBtn.left, g_rcSpkBtn.top, g_rcSpkBtn.right, g_rcSpkBtn.bottom, 3, 3);
        SelectObject(hdc, hOldP);
        SelectObject(hdc, hOldBr);
        DeleteObject(hSPen);
        DeleteObject(hSBr);

        SetTextColor(hdc, sBorder);
        DrawTextW(hdc, g_fmSpkActive ? L"SPEAKER: [ON]" : L"SPEAKER: [MUTE]", -1, (LPRECT)&g_rcSpkBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* 3. Preset Jump Button */
    {
        HBRUSH hPBr = CreateSolidBrush(RGB(18, 22, 10));
        hOldBr = (HBRUSH)SelectObject(hdc, hPBr);
        HPEN hPPen = CreatePen(PS_SOLID, 1, RGB(255, 200, 40));
        hOldP = (HPEN)SelectObject(hdc, hPPen);
        RoundRect(hdc, g_rcPresetBtn.left, g_rcPresetBtn.top, g_rcPresetBtn.right, g_rcPresetBtn.bottom, 3, 3);
        SelectObject(hdc, hOldP);
        SelectObject(hdc, hOldBr);
        DeleteObject(hPPen);
        DeleteObject(hPBr);

        SetTextColor(hdc, RGB(255, 215, 0));
        DrawTextW(hdc, L"PRESETS [88.5/98.1/107.9]", -1, (LPRECT)&g_rcPresetBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* --- HARMONIC EQUALIZER SPECTRUM BARS (y=180..230) --- */
    int eqY = 180, eqH = 46, numBars = 20, eqBarW = 18, eqGap = 4;
    int b;
    for (b = 0; b < numBars; b++) {
        int bx = 16 + b * (eqBarW + eqGap);
        int barVal = 2;

        if (g_fmAudioMode == FM_AUDIO_TONE) {
            if (b == 7) {
                barVal = 38 + (int)(sinf(g_oscPhase * 3.0f) * 6.0f);
            } else if (b == 6 || b == 8) {
                barVal = 18 + (int)(sinf(g_oscPhase * 2.5f) * 4.0f);
            } else if (b == 14) {
                barVal = 14 + (int)(sinf(g_oscPhase * 4.0f) * 3.0f);
            } else if (b == 18) {
                barVal = 8;
            } else {
                barVal = 3 + (b % 3);
            }
        } else if (g_fmAudioMode == FM_AUDIO_NOISE) {
            int nRnd = ((b * 16807 + g_frameTick * 48271) >> 16) & 0x1F;
            barVal = 10 + nRnd;
            if (barVal > eqH) barVal = eqH;
        } else {
            barVal = g_fmTxActive ? (4 + (b % 3)) : 2;
        }

        if (barVal < 2) barVal = 2;
        if (barVal > eqH) barVal = eqH;

        /* Track */
        RECT bTrack = { bx, eqY, bx + eqBarW, eqY + eqH };
        HBRUSH hTBr = CreateSolidBrush(RGB(10, 20, 18));
        FillRect(hdc, &bTrack, hTBr);
        DeleteObject(hTBr);

        /* Active Fill */
        RECT bFill = { bx, eqY + eqH - barVal, bx + eqBarW, eqY + eqH };
        COLORREF barCol = (barVal > 32) ? RGB(255, 215, 0) :
                          (barVal > 16) ? RGB(0, 240, 255) : RGB(0, 255, 160);
        HBRUSH hBBr = CreateSolidBrush(barCol);
        FillRect(hdc, &bFill, hBBr);
        DeleteObject(hBBr);
    }

    /* Subtext Telemetry */
    SetTextColor(hdc, RGB(90, 150, 130));
    wsprintfW(buf, L"FM TX: FMC1:/KT0801 @ %.1f MHz // CAR RECEIVER: TUNE TO %.1f MHz", fFreq, fFreq);
    ExtTextOutW(hdc, 16, 232, 0, NULL, buf, lstrlenW(buf), NULL);
}

/* ====================================================================
 * VIEW 4: WEATHER MATRIX (Clean, No-Overlap Farroupilha, RS)
 * ==================================================================== */
static void DrawWeatherGlyph(HDC hdc, int x, int y, int w, int h, const WCHAR *cond)
{
    RECT glyphBox = { x, y, x + w, y + h };
    FillRect(hdc, &glyphBox, (HBRUSH)GetStockObject(BLACK_BRUSH));
    int cx = x + w / 2;
    int cy = y + h / 2;

    if (wcsstr(cond, L"Rain") || wcsstr(cond, L"Shower") || wcsstr(cond, L"Drizzle")) {
        HBRUSH hCloudBr = CreateSolidBrush(RGB(10, 26, 36));
        HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hCloudBr);
        HPEN hCloudPen = CreatePen(PS_SOLID, 2, RGB(0, 200, 255));
        HPEN hOldP = (HPEN)SelectObject(hdc, hCloudPen);
        Ellipse(hdc, cx - 40, cy - 25, cx + 10, cy + 15);
        Ellipse(hdc, cx - 15, cy - 35, cx + 35, cy + 15);
        RoundRect(hdc, cx - 45, cy - 10, cx + 45, cy + 16, 10, 10);
        SelectObject(hdc, hOldBr);
        DeleteObject(hCloudBr);

        HPEN hRainPen = CreatePen(PS_SOLID, 2, RGB(0, 255, 200));
        SelectObject(hdc, hRainPen);
        int rx;
        for (rx = -30; rx <= 30; rx += 15) {
            MoveToEx(hdc, cx + rx, cy + 20, NULL);
            LineTo(hdc, cx + rx - 8, cy + 32);
        }
        SelectObject(hdc, hOldP);
        DeleteObject(hRainPen);
        DeleteObject(hCloudPen);
    } else if (wcsstr(cond, L"Cloud") || wcsstr(cond, L"Overcast") || wcsstr(cond, L"Mist")) {
        HBRUSH hBr = CreateSolidBrush(RGB(8, 26, 20));
        HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hBr);
        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 255, 160));
        HPEN hOldP = (HPEN)SelectObject(hdc, hPen);
        Ellipse(hdc, cx - 20, cy - 30, cx + 25, cy + 10);
        RoundRect(hdc, cx - 25, cy - 12, cx + 35, cy + 12, 8, 8);
        Ellipse(hdc, cx - 45, cy - 20, cx + 5, cy + 20);
        Ellipse(hdc, cx - 20, cy - 35, cx + 30, cy + 20);
        RoundRect(hdc, cx - 50, cy - 5, cx + 45, cy + 22, 12, 12);
        SelectObject(hdc, hOldP);
        SelectObject(hdc, hOldBr);
        DeleteObject(hPen);
        DeleteObject(hBr);
    } else {
        HBRUSH hSunBr = CreateSolidBrush(RGB(35, 22, 0));
        HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hSunBr);
        HPEN hSunPen = CreatePen(PS_SOLID, 2, RGB(255, 180, 0));
        HPEN hOldP = (HPEN)SelectObject(hdc, hSunPen);
        Ellipse(hdc, cx - 18, cy - 18, cx + 18, cy + 18);
        SelectObject(hdc, hOldBr);
        DeleteObject(hSunBr);

        int r1 = 24, r2 = 34;
        MoveToEx(hdc, cx, cy - r1, NULL); LineTo(hdc, cx, cy - r2);
        MoveToEx(hdc, cx, cy + r1, NULL); LineTo(hdc, cx, cy + r2);
        MoveToEx(hdc, cx - r1, cy, NULL); LineTo(hdc, cx - r2, cy);
        MoveToEx(hdc, cx + r1, cy, NULL); LineTo(hdc, cx + r2, cy);
        MoveToEx(hdc, cx - 17, cy - 17, NULL); LineTo(hdc, cx - 24, cy - 24);
        MoveToEx(hdc, cx + 17, cy - 17, NULL); LineTo(hdc, cx + 24, cy - 24);
        MoveToEx(hdc, cx - 17, cy + 17, NULL); LineTo(hdc, cx - 24, cy + 24);
        MoveToEx(hdc, cx + 17, cy + 17, NULL); LineTo(hdc, cx + 24, cy + 24);
        SelectObject(hdc, hOldP);
        DeleteObject(hSunPen);
    }
}

static void PaintWeatherView(HDC memDC)
{
    WCHAR buf[128];
    int leftX = 10;

    /* Location — clipped to left panel so it doesn't bleed into atmospheric matrix */
    SetTextColor(memDC, RGB(0, 255, 200));
    RECT locRc = { leftX, 32, 185, 46 };
    DrawTextW(memDC, g_wxLocation, -1, &locRc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

    /* Temperature Readout */
    wsprintfW(buf, L"%dC", g_wxTempC);
    SetTextColor(memDC, RGB(0, 245, 255));
    ExtTextOutW(memDC, leftX, 48, 0, NULL, buf, lstrlenW(buf), NULL);

    wsprintfW(buf, L"FEELS %dC", g_wxFeelsC);
    SetTextColor(memDC, RGB(120, 220, 200));
    ExtTextOutW(memDC, leftX + 80, 52, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Condition Badge */
    RECT condRc = { leftX, 76, leftX + 175, 96 };
    HBRUSH hCondBr = CreateSolidBrush(RGB(8, 26, 20));
    HBRUSH hOldBr = (HBRUSH)SelectObject(memDC, hCondBr);
    HPEN hCondPen = CreatePen(PS_SOLID, 1, RGB(0, 180, 120));
    HPEN hOldP = (HPEN)SelectObject(memDC, hCondPen);
    RoundRect(memDC, condRc.left, condRc.top, condRc.right, condRc.bottom, 4, 4);
    SelectObject(memDC, hOldP);
    SelectObject(memDC, hOldBr);
    DeleteObject(hCondPen);
    DeleteObject(hCondBr);

    SetTextColor(memDC, RGB(0, 255, 180));
    DrawTextW(memDC, g_wxCondition, -1, &condRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    /* Vector Glyph */
    DrawWeatherGlyph(memDC, leftX, 104, 175, 88, g_wxCondition);

    /* Diurnal Range */
    wsprintfW(buf, L"TODAY: MAX %dC | MIN %dC", g_forecast[0].maxC, g_forecast[0].minC);
    SetTextColor(memDC, RGB(255, 215, 0));
    ExtTextOutW(memDC, leftX + 4, 202, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Vertical Divider */
    HPEN hDivPen = CreatePen(PS_SOLID, 1, RGB(20, 42, 36));
    HPEN hOld = (HPEN)SelectObject(memDC, hDivPen);
    MoveToEx(memDC, 196, 32, NULL); LineTo(memDC, 196, 240);
    SelectObject(memDC, hOld);
    DeleteObject(hDivPen);

    /* Atmospheric Telemetry Matrix */
    int rightX = 208;
    SetTextColor(memDC, RGB(0, 255, 160));
    ExtTextOutW(memDC, rightX, 32, 0, NULL, L"ATMOSPHERIC TELEMETRY MATRIX", 28, NULL);

    /* Humidity */
    SetTextColor(memDC, RGB(100, 150, 140));
    ExtTextOutW(memDC, rightX, 48, 0, NULL, L"HUMIDITY:", 9, NULL);
    wsprintfW(buf, L"%d%%", g_wxHumidity);
    SetTextColor(memDC, RGB(0, 255, 220));
    ExtTextOutW(memDC, rightX + 65, 48, 0, NULL, buf, lstrlenW(buf), NULL);

    int humBarX = rightX + 110, humBarY = 51, humBarW = 145, humBarH = 7;
    RECT humRc = { humBarX, humBarY, humBarX + humBarW, humBarY + humBarH };
    HBRUSH hTrkBr = CreateSolidBrush(RGB(15, 25, 22));
    FillRect(memDC, &humRc, hTrkBr);
    DeleteObject(hTrkBr);

    int humFill = (g_wxHumidity * humBarW) / 100;
    if (humFill > humBarW) humFill = humBarW;
    if (humFill > 0) {
        RECT humFillRc = { humBarX, humBarY, humBarX + humFill, humBarY + humBarH };
        HBRUSH hHBr = CreateSolidBrush(RGB(0, 255, 180));
        FillRect(memDC, &humFillRc, hHBr);
        DeleteObject(hHBr);
    }

    /* Wind */
    SetTextColor(memDC, RGB(100, 150, 140));
    ExtTextOutW(memDC, rightX, 64, 0, NULL, L"WIND:", 5, NULL);
    SetTextColor(memDC, RGB(0, 240, 255));
    ExtTextOutW(memDC, rightX + 65, 64, 0, NULL, g_wxWind, lstrlenW(g_wxWind), NULL);

    /* Pressure & Precip */
    SetTextColor(memDC, RGB(100, 150, 140));
    ExtTextOutW(memDC, rightX, 80, 0, NULL, L"PRESSURE:", 9, NULL);
    SetTextColor(memDC, RGB(160, 240, 200));
    ExtTextOutW(memDC, rightX + 65, 80, 0, NULL, g_wxPressure, lstrlenW(g_wxPressure), NULL);

    SetTextColor(memDC, RGB(100, 150, 140));
    ExtTextOutW(memDC, rightX + 160, 80, 0, NULL, L"PRECIP:", 7, NULL);
    SetTextColor(memDC, RGB(160, 240, 200));
    ExtTextOutW(memDC, rightX + 210, 80, 0, NULL, g_wxPrecip, lstrlenW(g_wxPrecip), NULL);

    /* UV */
    SetTextColor(memDC, RGB(100, 150, 140));
    ExtTextOutW(memDC, rightX, 96, 0, NULL, L"UV INDEX:", 9, NULL);
    wsprintfW(buf, L"%d [LOW]", g_wxUV);
    SetTextColor(memDC, RGB(255, 215, 0));
    ExtTextOutW(memDC, rightX + 65, 96, 0, NULL, buf, lstrlenW(buf), NULL);

    /* 3-Day Forecast Cards */
    int cardY = 138, cardW = 82, cardH = 92, cardGap = 8;
    int f;
    for (f = 0; f < 3; f++) {
        int cardX = rightX + f * (cardW + cardGap);
        RECT cRc = { cardX, cardY, cardX + cardW, cardY + cardH };
        HBRUSH hCardBr = CreateSolidBrush(RGB(6, 18, 14));
        HBRUSH hOldCardBr = (HBRUSH)SelectObject(memDC, hCardBr);
        HPEN hCardPen = CreatePen(PS_SOLID, 1, RGB(0, 160, 100));
        HPEN hOldCardPen = (HPEN)SelectObject(memDC, hCardPen);
        RoundRect(memDC, cRc.left, cRc.top, cRc.right, cRc.bottom, 4, 4);
        SelectObject(memDC, hOldCardPen);
        SelectObject(memDC, hOldCardBr);
        DeleteObject(hCardPen);
        DeleteObject(hCardBr);

        SetTextColor(memDC, RGB(0, 255, 240));
        RECT dayRc = { cardX + 2, cardY + 6, cardX + cardW - 2, cardY + 22 };
        DrawTextW(memDC, g_forecast[f].day, -1, &dayRc, DT_CENTER | DT_TOP | DT_SINGLELINE);

        wsprintfW(buf, L"%dC / %dC", g_forecast[f].maxC, g_forecast[f].minC);
        SetTextColor(memDC, RGB(255, 215, 0));
        RECT tempRc = { cardX + 2, cardY + 28, cardX + cardW - 2, cardY + 44 };
        DrawTextW(memDC, buf, -1, &tempRc, DT_CENTER | DT_TOP | DT_SINGLELINE);

        SetTextColor(memDC, RGB(130, 210, 180));
        RECT descRc = { cardX + 4, cardY + 50, cardX + cardW - 4, cardY + cardH - 4 };
        DrawTextW(memDC, g_forecast[f].desc, -1, &descRc, DT_CENTER | DT_WORDBREAK);
    }
}

/* ====================================================================
 * MAIN PAINT DISPATCHER
 * ==================================================================== */
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

    /* --- MINIMALIST CYBER TOP HEADER BAR --- */
    switch (g_currentView) {
    case VIEW_CLOCK:
        SetTextColor(memDC, RGB(0, 255, 180));
        ExtTextOutW(memDC, 12, 6, 0, NULL, L"MERO // CHRONO MATRIX", 21, NULL);
        break;
    case VIEW_GPS:
        SetTextColor(memDC, RGB(0, 255, 128));
        ExtTextOutW(memDC, 12, 6, 0, NULL, L"MERO // SATELLITE RADAR", 23, NULL);
        break;
    case VIEW_FM:
        SetTextColor(memDC, RGB(255, 215, 0));
        ExtTextOutW(memDC, 12, 6, 0, NULL, L"MERO // FREQUENCY MATRIX", 24, NULL);
        break;
    case VIEW_WEATHER:
        SetTextColor(memDC, RGB(0, 240, 255));
        ExtTextOutW(memDC, 12, 6, 0, NULL, L"MERO // WEATHER MATRIX", 22, NULL);
        break;
    default:
        break;
    }

    /* Mode Indicator Badge — centered in header */
    const WCHAR *modeBadges[] = { L"[CHRONO]", L"[RADAR]", L"[FM OSC]", L"[WEATHER]" };
    SetTextColor(memDC, RGB(120, 200, 180));
    ExtTextOutW(memDC, 195, 6, 0, NULL, modeBadges[g_currentView % 4], lstrlenW(modeBadges[g_currentView % 4]), NULL);

    /* Minimalist Red [X] Exit Button */
    HBRUSH hExBr = CreateSolidBrush(RGB(24, 0, 0));
    HBRUSH hOldBr = (HBRUSH)SelectObject(memDC, hExBr);
    HPEN hExPen = CreatePen(PS_SOLID, 1, RGB(220, 40, 40));
    HPEN hOldP = (HPEN)SelectObject(memDC, hExPen);
    RoundRect(memDC, g_rcBtnExit.left, g_rcBtnExit.top, g_rcBtnExit.right, g_rcBtnExit.bottom, 3, 3);
    SelectObject(memDC, hOldP);
    SelectObject(memDC, hOldBr);
    DeleteObject(hExPen);
    DeleteObject(hExBr);

    SetTextColor(memDC, RGB(255, 70, 70));
    DrawTextW(memDC, L"X", -1, (LPRECT)&g_rcBtnExit, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    /* Top Divider Line */
    HPEN hTopPen = CreatePen(PS_SOLID, 1, RGB(20, 38, 32));
    HPEN hOld = (HPEN)SelectObject(memDC, hTopPen);
    MoveToEx(memDC, 10, 25, NULL); LineTo(memDC, g_screenW - 10, 25);
    SelectObject(memDC, hOld);
    DeleteObject(hTopPen);

    /* --- RENDER ACTIVE VIEW --- */
    switch (g_currentView) {
    case VIEW_CLOCK:   PaintClockView(memDC); break;
    case VIEW_GPS:     PaintGpsRadarView(memDC); break;
    case VIEW_FM:      PaintFmView(memDC); break;
    case VIEW_WEATHER: PaintWeatherView(memDC); break;
    default: break;
    }

    /* --- FOOTER STATUS BAR --- */
    HPEN hFootPen = CreatePen(PS_SOLID, 1, RGB(18, 35, 30));
    hOld = (HPEN)SelectObject(memDC, hFootPen);
    MoveToEx(memDC, 10, 248, NULL); LineTo(memDC, g_screenW - 10, 248);
    SelectObject(memDC, hOld);
    DeleteObject(hFootPen);

    SetTextColor(memDC, RGB(70, 130, 110));
    wsprintfW(buf, L"MERO // TAP SCREEN TO CYCLE VIEWS: [CHRONO > RADAR > FM OSC > WEATHER]");
    ExtTextOutW(memDC, 12, 253, 0, NULL, buf, lstrlenW(buf), NULL);

    /* Atomic BitBlt to display */
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hWnd, &ps);
}

static void OnTouch(int x, int y)
{
    /* Exit Button touched -> return to mero-shell */
    if (x >= 435 && y <= 28) {
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

    /* FM View Interactive Controls */
    if (g_currentView == VIEW_FM) {
        POINT pt = { x, y };
        if (PtInRect(&g_rcFmDown10, pt)) {
            g_fmFreqKHz -= 1000;
            if (g_fmFreqKHz < 76000) g_fmFreqKHz = 108000;
            SaveFmConfig();
            InvalidateRect(g_hWnd, NULL, FALSE);
            return;
        }
        if (PtInRect(&g_rcFmDown1, pt)) {
            g_fmFreqKHz -= 100;
            if (g_fmFreqKHz < 76000) g_fmFreqKHz = 108000;
            SaveFmConfig();
            InvalidateRect(g_hWnd, NULL, FALSE);
            return;
        }
        if (PtInRect(&g_rcFmUp1, pt)) {
            g_fmFreqKHz += 100;
            if (g_fmFreqKHz > 108000) g_fmFreqKHz = 76000;
            SaveFmConfig();
            InvalidateRect(g_hWnd, NULL, FALSE);
            return;
        }
        if (PtInRect(&g_rcFmUp10, pt)) {
            g_fmFreqKHz += 1000;
            if (g_fmFreqKHz > 108000) g_fmFreqKHz = 76000;
            SaveFmConfig();
            InvalidateRect(g_hWnd, NULL, FALSE);
            return;
        }
        if (PtInRect(&g_rcFmTx, pt)) {
            g_fmTxActive = !g_fmTxActive;
            SaveFmConfig();
            InvalidateRect(g_hWnd, NULL, FALSE);
            return;
        }
        if (PtInRect(&g_rcAudioBtn, pt)) {
            g_fmAudioMode = (FmAudioMode)((g_fmAudioMode + 1) % FM_AUDIO_COUNT);
            SaveFmConfig();
            InvalidateRect(g_hWnd, NULL, FALSE);
            return;
        }
        if (PtInRect(&g_rcSpkBtn, pt)) {
            g_fmSpkActive = !g_fmSpkActive;
            if (g_hWaveOut) waveOutSetVolume(g_hWaveOut, g_fmSpkActive ? 0xC000C000 : 0x00000000);
            SaveFmConfig();
            InvalidateRect(g_hWnd, NULL, FALSE);
            return;
        }
        if (PtInRect(&g_rcPresetBtn, pt)) {
            if (g_fmFreqKHz == 88500) g_fmFreqKHz = 98100;
            else if (g_fmFreqKHz == 98100) g_fmFreqKHz = 107900;
            else g_fmFreqKHz = 88500;
            SaveFmConfig();
            InvalidateRect(g_hWnd, NULL, FALSE);
            return;
        }
        /* Tap footer or header mode badge to cycle view */
        if (y >= 248 || (x >= 180 && x <= 300 && y <= 25)) {
            g_currentView = (DeckView)((g_currentView + 1) % VIEW_COUNT);
            InvalidateRect(g_hWnd, NULL, FALSE);
            return;
        }
        return;
    }

    /* Tapping body cycles through views */
    if (y > 25 && y < 248) {
        g_currentView = (DeckView)((g_currentView + 1) % VIEW_COUNT);
        InvalidateRect(g_hWnd, NULL, FALSE);
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        g_hWnd = hWnd;
        UpdateSystemMetrics();
        InitGpsPort();
        PollWeatherFile();
        LoadFmConfig();
        InitAudioSynth();
        DismissWinsockDialogs();

        /* Timers: 50ms (20 FPS smooth animation), 1000ms (system update) */
        SetTimer(hWnd, TIMER_ID_FRAME, 50, NULL);
        SetTimer(hWnd, TIMER_ID_SLOW, 1000, NULL);
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_ID_FRAME) {
            g_frameTick++;
            g_radarAngle = (g_radarAngle + 4) % 360;
            g_oscPhase += 0.15f;
            if (g_oscPhase > 1000.0f) g_oscPhase = 0.0f;

            if (g_currentView == VIEW_GPS) {
                PollGpsReceiver();
            }

            FeedAudioSynth();

            InvalidateRect(hWnd, NULL, FALSE);
        } else if (wParam == TIMER_ID_SLOW) {
            UpdateSystemMetrics();
            DismissWinsockDialogs();
            if (g_currentView == VIEW_WEATHER) {
                PollWeatherFile();
            }
        }
        return 0;

    case WM_LBUTTONDOWN:
        OnTouch(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_PAINT:
        OnPaint(hWnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hWnd, TIMER_ID_FRAME);
        KillTimer(hWnd, TIMER_ID_SLOW);
        CloseAudioSynth();
        if (g_hGps != INVALID_HANDLE_VALUE) {
            CloseHandle(g_hGps);
            g_hGps = INVALID_HANDLE_VALUE;
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
