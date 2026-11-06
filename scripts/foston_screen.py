#!/usr/bin/env python3
"""Decode the read-only CE screenshot helper's bounded RGB565 run stream."""
import struct
import sys
from pathlib import Path


def decode(data):
    if data[:8]!=b"MERO32R1" or len(data)<12:
        raise ValueError("invalid capture header")
    w,h=struct.unpack_from("<HH",data,8)
    if (w,h)!=(480,272):raise ValueError("invalid capture size")
    output=bytearray();offset=12
    for _ in range(h):
        count=0
        while count<w:
            if offset+4>len(data):raise ValueError("truncated capture")
            n,c=struct.unpack_from("<HH",data,offset);offset+=4
            if not n or count+n>w:raise ValueError("invalid capture run")
            pixel=bytes((((c>>11)&31)*255//31,((c>>5)&63)*255//63,(c&31)*255//31))
            output.extend(pixel*n);count+=n
    if offset!=len(data):raise ValueError("trailing capture data")
    return w,h,bytes(output)


if __name__=="__main__":
    from PIL import Image
    w,h,rgb=decode(Path(sys.argv[1]).read_bytes())
    Image.frombytes("RGB",(w,h),rgb).save(sys.argv[2])
