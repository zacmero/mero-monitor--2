#!/usr/bin/env python3
import hashlib
import io
from pathlib import Path
import tempfile
from unittest.mock import patch
from PIL import Image
from companion_art import rgb565,Artwork

def png(color):
    output=io.BytesIO();Image.new("RGB",(160,90),color).save(output,format="PNG");return output.getvalue()

red=png("red");pixels=rgb565(red)
assert len(pixels)==112*112*2 and pixels==b"\0\xf8"*(112*112)
with tempfile.TemporaryDirectory() as directory:
    home=Path(directory);source=home/".mozilla/firefox/firefox-mpris/current.png";source.parent.mkdir(parents=True)
    source.write_bytes(red);share=home/"share";share.mkdir();(share/"user-image.png").write_bytes(red)
    with patch.object(Path,"home",return_value=home):
        art=Artwork(share)
        state=dict(track="abcdefghijk",art_url=source.as_uri())
        assert art.update(state)=="";art.future.result(timeout=3)
        sha=art.update(state);assert sha==hashlib.sha256(pixels).hexdigest()
        assert (share/f"art-{sha}.565").read_bytes()==pixels
        # Same export path replaced for another track: never return the old cover.
        source.write_bytes(png("blue"));state["track"]="lmnopqrstuv"
        assert art.update(state)=="";art.future.result(timeout=3)
        nextsha=art.update(state);assert nextsha!=sha
        assert (share/f"art-{nextsha}.565").read_bytes()==b"\x1f\0"*(112*112)
        assert art.update(None)=="" and (share/"user-image.png").read_bytes()==red
        art.close()
print("PASS: bounded RGB565 cover conversion, source preservation, reused Firefox path and track-bound cache")
