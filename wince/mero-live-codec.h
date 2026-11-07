#ifndef MERO_LIVE_CODEC_H
#define MERO_LIVE_CODEC_H
/* Strict base64 for authenticated UTF-8 telemetry, never executable input. */
static int MeroBase64Bytes(const char *s, char *out, unsigned int cap,int binary) {
    unsigned int bits = 0, value = 0, n = 0, i, padding = 0;
    for (i = 0; s[i]; i++) {
        unsigned int c = (unsigned char)s[i], v;
        if (c == '=') { padding++; if (padding > 2) return -1; continue; }
        if (padding) return -1;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+') v = 62; else if (c == '/') v = 63; else return -1;
        value = (value << 6) | v; bits += 6;
        if (bits >= 8) { bits -= 8; if (n + 1 >= cap) return -1; out[n++] = (char)(value >> bits); }
    }
    if (i % 4 || (padding == 1 && bits != 2) || (padding == 2 && bits != 4) ||
        (!padding && bits) || (bits && (value & ((1u << bits) - 1)))) return -1;
    if (!binary&&memchr(out, 0, n)) return -1;
    out[n] = 0; return (int)n;
}
static int MeroBase64(const char *s,char *out,unsigned int cap){return MeroBase64Bytes(s,out,cap,0);}
#endif
