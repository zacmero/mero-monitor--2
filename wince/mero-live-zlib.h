/* Bounded inflation after envelope HMAC verification; existing stb decoder. */
static int MeroLiveDecode(const char *data,const char *codec,char *out,unsigned int cap){
    char packed[4096];int n,length;unsigned int a=1,b=0,i,check;
    if(cap<2||cap>4096)return -1;
    if(!codec[0])return MeroBase64(data,out,cap);
    if(strcmp(codec,"zlib1"))return -1;
    n=MeroBase64Bytes(data,packed,sizeof(packed),1);
    if(n<6)return -1;
    length=stbi_zlib_decode_buffer(out,cap-1,packed,n);
    if(length<0||memchr(out,0,length))return -1;
    for(i=0;i<(unsigned)length;i++){a=(a+(unsigned char)out[i])%65521;b=(b+a)%65521;}
    check=((unsigned int)(unsigned char)packed[n-4]<<24)|((unsigned int)(unsigned char)packed[n-3]<<16)|((unsigned int)(unsigned char)packed[n-2]<<8)|(unsigned char)packed[n-1];
    if(((b<<16)|a)!=check)return -1;
    out[length]=0;return length;
}
