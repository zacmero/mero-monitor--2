/* Independent RAM artwork cache; the telemetry bus ABI stays unchanged. */
#ifndef MERO_ART_H
#define MERO_ART_H
#define MERO_ART_SIDE 272
#define MERO_ART_BYTES (MERO_ART_SIDE*MERO_ART_SIDE*2)
typedef struct { char sha[65],track[32]; unsigned short pixels[MERO_ART_SIDE*MERO_ART_SIDE]; } MeroArt;
static HANDLE mero_art_map;
static MeroArt *mero_art;
static int MeroArtOpen(void) {
    if(mero_art)return 1;
    mero_art_map=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(MeroArt),L"MERO_ART565_V2");
    if(mero_art_map)mero_art=(MeroArt*)MapViewOfFile(mero_art_map,FILE_MAP_ALL_ACCESS,0,0,sizeof(MeroArt));
    return mero_art!=NULL;
}
static void MeroArtClose(void) {
    if(mero_art)UnmapViewOfFile(mero_art);
    if(mero_art_map)CloseHandle(mero_art_map);
    mero_art=NULL;mero_art_map=NULL;
}
#endif
