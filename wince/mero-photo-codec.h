/* Decode only the bounded PC thumbnail, never an original photo. */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_JPEG
#define STBI_SUPPORT_ZLIB
#define STBI_MAX_DIMENSIONS 272
#include "gallery/stb_image.h"
static int MeroPhotoDecode(const unsigned char *data,unsigned int n,unsigned short *out) {
    int w,h,channels,i;unsigned char *rgb;
    if(n==65280){memcpy(out,data,n);return 1;}
    if(n<4||n>=65280||data[0]!=255||data[1]!=216||
       !stbi_info_from_memory(data,n,&w,&h,&channels)||w!=240||h!=136)return 0;
    rgb=stbi_load_from_memory(data,n,&w,&h,&channels,3);if(!rgb)return 0;
    for(i=0;i<240*136;i++)out[i]=((rgb[i*3]>>3)<<11)|((rgb[i*3+1]>>2)<<5)|(rgb[i*3+2]>>3);
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
