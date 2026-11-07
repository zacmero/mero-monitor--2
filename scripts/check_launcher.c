/* Actual shell preference functions, compiled against a small CE file shim. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
typedef wchar_t WCHAR;typedef unsigned int DWORD;typedef int BOOL,HANDLE,HKEY;
#include "shell_apps.inc"
typedef unsigned char *LPBYTE;
#define TRUE 1
#define FALSE 0
#define CFG_DIR L"\\SDMMC\\MERO"
#define CFG_FILE L"\\SDMMC\\MERO\\shell.cfg"
#define INVALID_HANDLE_VALUE (-1)
#define INVALID_FILE_ATTRIBUTES (-1)
#define GENERIC_READ 1
#define GENERIC_WRITE 2
#define FILE_SHARE_READ 1
#define OPEN_EXISTING 1
#define CREATE_ALWAYS 2
#define FILE_ATTRIBUTE_NORMAL 0
#define HKEY_LOCAL_MACHINE 1
#define KEY_READ 1
#define ERROR_SUCCESS 0
static int g_pref,g_countdownSeconds,g_countdownActive,g_returningHome;
static int g_sdPrefsLoaded;
static WCHAR g_statusMsg[160];
static struct {const WCHAR *name;} g_apps[AUTOLAUNCH_COUNT];
static char config[256],stage[256],backup[256];
static int configExists,legacy=1,registryReads,failWrite,failInstall,failArchive;
static HANDLE CreateFileW(const WCHAR *path,int access,int share,void *security,int mode,int attr,void *template) {
    (void)access;(void)share;(void)security;(void)mode;(void)attr;(void)template;
    if(!wcscmp(path,CFG_FILE))return configExists?1:-1;
    if(!wcscmp(path,L"\\SDMMC\\MERO\\shell.cfg.next"))return 2;
    return -1;
}
static int ReadFile(int handle,char *out,int cap,DWORD *n,void *overlap){(void)handle;(void)overlap;*n=strlen(config);assert(*n<(DWORD)cap);memcpy(out,config,*n);return 1;}
static int WriteFile(int h,const char *p,int n,DWORD *done,void *o){(void)o;assert(h==2);if(failWrite)return 0;memcpy(stage,p,n);stage[n]=0;*done=n;return 1;}
static int FlushFileBuffers(int h){(void)h;return 1;}
static void CloseHandle(int h){(void)h;}
static void CreateDirectoryW(const WCHAR *p,void *s){(void)p;(void)s;}
static int GetFileAttributesW(const WCHAR *p){assert(!wcscmp(p,CFG_FILE));return configExists?0:-1;}
static DWORD GetTickCount(void){return 123;}
static void wsprintfW(WCHAR *out,const WCHAR *format,...){(void)format;wcscpy(out,L"status");}
static int MoveFileW(const WCHAR *from,const WCHAR *to){
    if(!wcscmp(from,CFG_FILE)){if(failArchive)return 0;strcpy(backup,config);configExists=0;return 1;}
    if(!wcscmp(to,CFG_FILE)){
        if(!wcscmp(from,L"\\SDMMC\\MERO\\shell.cfg.next")){if(failInstall)return 0;strcpy(config,stage);}
        else strcpy(config,backup);
        configExists=1;return 1;
    }return 0;
}
static int RegOpenKeyExW(int root,const WCHAR *path,int a,int b,HKEY *key){(void)root;(void)path;(void)a;(void)b;registryReads++;*key=1;return 0;}
static int RegQueryValueExW(int key,const WCHAR *name,void *a,void *b,LPBYTE p,DWORD *n){(void)key;(void)name;(void)a;(void)b;*(DWORD*)p=legacy;*n=4;return 0;}
static void RegCloseKey(int key){(void)key;}
#include "shell_prefs.inc"
int main(void){
    configExists=1;strcpy(config,"autolaunch=0\r\ntimeout=3\r\n");LoadConfig();
    assert(g_pref==0&&!g_countdownActive&&registryReads==0);
    strcpy(config,"autolaunch=8\r\n");LoadConfig();assert(g_pref==8&&g_countdownActive);
    g_returningHome=1;LoadConfig();assert(g_pref==8&&!g_countdownActive);
    g_returningHome=0;strcpy(config,"autolaunch=999\r\n");LoadConfig();assert(g_pref==0&&registryReads==0);
    configExists=0;LoadConfig();assert(g_pref==1&&registryReads==1);
    configExists=1;strcpy(config,"autolaunch=0\r\n");g_pref=7;failWrite=1;
    assert(!SaveConfig()&&configExists&&!strcmp(config,"autolaunch=0\r\n"));
    failWrite=0;failArchive=1;assert(!SaveConfig()&&configExists);
    failArchive=0;failInstall=1;assert(!SaveConfig()&&configExists&&!strcmp(config,"autolaunch=0\r\n"));
    failInstall=0;assert(SaveConfig()&&strstr(config,"autolaunch=7")&&strstr(backup,"autolaunch=0"));
    assert(AUTOLAUNCH_MUSIC==8&&AUTOLAUNCH_GALLERY==1&&AUTOLAUNCH_AMBIENT==15);
    g_pref=AUTOLAUNCH_AMBIENT;assert(SaveConfig());g_pref=0;LoadConfig();assert(g_pref==AUTOLAUNCH_AMBIENT&&g_countdownActive);
    snprintf(config,sizeof(config),"autolaunch=%d\r\n",AUTOLAUNCH_COUNT);LoadConfig();assert(g_pref==AUTOLAUNCH_NONE);
    puts("PASS: actual shell default parsing, saved zero, home/autostart, backup and failed-save recovery");return 0;
}
