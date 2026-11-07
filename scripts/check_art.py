#!/usr/bin/env python3
import hashlib
import io
from pathlib import Path
import tempfile
from unittest.mock import patch
from PIL import Image
from companion_art import jpeg,Artwork,SIDE,load_art

def png(color):
    output=io.BytesIO();Image.new("RGB",(160,90),color).save(output,format="PNG");return output.getvalue()

red=png("red");pixels=jpeg(red)
assert len(pixels)<=60000
with Image.open(io.BytesIO(pixels)) as image:assert image.size==(SIDE,SIDE)
with tempfile.TemporaryDirectory() as directory:
    home=Path(directory);source=home/".mozilla/firefox/firefox-mpris/current.png";source.parent.mkdir(parents=True)
    source.write_bytes(red);share=home/"share";share.mkdir();(share/"user-image.png").write_bytes(red)
    with patch.object(Path,"home",return_value=home), patch("companion_art.urlopen",side_effect=OSError("offline")):
        art=Artwork(share)
        state=dict(track="abcdefghijk",art_url=source.as_uri())
        assert art.update(state)=="";art.future.result(timeout=3)
        sha=art.update(state);assert sha==hashlib.sha256(pixels).hexdigest()
        assert (share/f"art-{sha}.jpg").read_bytes()==pixels
        # Same export path replaced for another track: never return the old cover.
        source.write_bytes(png("blue"));state["track"]="lmnopqrstuv"
        assert art.update(state)=="";art.future.result(timeout=3)
        nextsha=art.update(state);assert nextsha!=sha
        assert (share/f"art-{nextsha}.jpg").read_bytes()==jpeg(png("blue"))
        assert art.update(None)=="" and (share/"user-image.png").read_bytes()==red
        art.close()
print("PASS: bounded native-width JPEG cover conversion, source preservation, reused Firefox path and track-bound cache")

# Prefer a real high-resolution remote source over a tiny Firefox export.
with patch("companion_art.urlopen",return_value=io.BytesIO(png("blue"))):
    # Reject undersized network thumbnails; source selection must not accept them.
    try:load_art(dict(track="abcdefghijk",art_url=""))
    except ValueError:pass
    else:raise AssertionError("undersized remote accepted")
output=io.BytesIO();Image.new("RGB",(512,512),"blue").save(output,format="PNG")
with patch("companion_art.urlopen",return_value=io.BytesIO(output.getvalue())) as request:
    cover=load_art(dict(track="abcdefghijk",art_url="file:///nonexistent"))
    assert request.call_args[0][0].full_url.endswith("/maxresdefault.jpg")
    with Image.open(io.BytesIO(cover)) as image:assert image.size==(272,272)

# Exercise the actual native bounded JPEG decoder, including wrong dimensions.
import ctypes, subprocess
with tempfile.TemporaryDirectory() as directory:
    root=Path(__file__).resolve().parents[1];directory=Path(directory)
    (directory/"decode.c").write_text('#include <string.h>\n#include "mero-photo-codec.h"\nint decode(const unsigned char*d,unsigned n,unsigned short*out){return MeroArtDecode(d,n,out);}')
    subprocess.run(["cc","-shared","-fPIC","-O2","-I",str(root/"wince"),str(directory/"decode.c"),"-o",str(directory/"decode.so"),"-lm"],check=True)
    decoder=ctypes.CDLL(str(directory/"decode.so")).decode
    decoder.argtypes=[ctypes.c_char_p,ctypes.c_uint,ctypes.POINTER(ctypes.c_ushort)]
    output=(ctypes.c_ushort*(272*272))()
    assert decoder(pixels,len(pixels),output)==1 and output[0]==0xf800
    assert decoder(pixels,20,output)==0
    invalid=io.BytesIO();Image.new("RGB",(273,272),"red").save(invalid,format="JPEG")
    raw=invalid.getvalue();assert decoder(raw,len(raw),output)==0
    assert decoder(pixels,60001,output)==0
print("PASS: high-resolution source preference, undersized rejection and actual native JPEG dimensions/bounds")
