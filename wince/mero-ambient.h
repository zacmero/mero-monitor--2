/* Single verified slideshow frame in RAM. Never a photo cache on SD. */
#ifndef MERO_AMBIENT_H
#define MERO_AMBIENT_H
#define MERO_AMBIENT_WIDTH 480
#define MERO_AMBIENT_HEIGHT 272
#define MERO_AMBIENT_BYTES (480*272*2)
typedef struct {DWORD demandTick; char sha[65]; unsigned short pixels[480*272];} MeroAmbient;
static HANDLE mero_ambient_map;
static MeroAmbient *mero_ambient;
static int MeroAmbientOpen(void) {
    if(mero_ambient)return 1;
    mero_ambient_map=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(MeroAmbient),L"MERO_AMBIENT565_V2");
    if(mero_ambient_map)mero_ambient=(MeroAmbient*)MapViewOfFile(mero_ambient_map,FILE_MAP_ALL_ACCESS,0,0,sizeof(MeroAmbient));
    return mero_ambient!=NULL;
}
static void MeroAmbientClose(void) {
    if(mero_ambient)UnmapViewOfFile(mero_ambient);
    if(mero_ambient_map)CloseHandle(mero_ambient_map);
    mero_ambient=NULL;mero_ambient_map=NULL;
}
#endif
