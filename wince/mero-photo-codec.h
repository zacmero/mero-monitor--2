/* Decode only the bounded PC frame, never an original photo. */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_JPEG
#define STBI_SUPPORT_ZLIB
#define STBI_MAX_DIMENSIONS 480
#include "gallery/stb_image.h"
static int MeroPhotoDecode(const unsigned char *data,unsigned int n,unsigned short *out) {
    int w,h,channels,i,x,y;unsigned char *rgb;
    /* Legacy raw input remains accepted; output is always a native V2 frame. */
    if(n==65280){for(y=0;y<272;y++)for(x=0;x<480;x++){
        i=((y/2)*240+x/2)*2;out[y*480+x]=data[i]|((unsigned short)data[i+1]<<8);
    }return 1;}
    if(n<4||n>=65280||data[0]!=255||data[1]!=216||
       !stbi_info_from_memory(data,n,&w,&h,&channels)||
       !((w==480&&h==272)||(w==240&&h==136)))return 0;
    rgb=stbi_load_from_memory(data,n,&w,&h,&channels,3);if(!rgb)return 0;
    for(y=0;y<272;y++)for(x=0;x<480;x++){
        i=((y*h/272)*w+x*w/480)*3;
        out[y*480+x]=((rgb[i]>>3)<<11)|((rgb[i+1]>>2)<<5)|(rgb[i+2]>>3);
    }
    stbi_image_free(rgb);return 1;
}

/* Full portrait-width cover; validate dimensions before allocating pixels. */
static int MeroArtDecode(const unsigned char *data,unsigned int n,unsigned short *out) {
    int w,h,channels,i;unsigned char *rgb;
    if(n<4||n>60000||data[0]!=255||data[1]!=216||
       !stbi_info_from_memory(data,n,&w,&h,&channels)||w!=272||h!=272)return 0;
    rgb=stbi_load_from_memory(data,n,&w,&h,&channels,3);if(!rgb)return 0;
    for(i=0;i<272*272;i++)out[i]=((rgb[i*3]>>3)<<11)|((rgb[i*3+1]>>2)<<5)|(rgb[i*3+2]>>3);
    stbi_image_free(rgb);return 1;
}
