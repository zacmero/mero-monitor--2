"""Resize MPRIS artwork on the PC; publish only content-addressed bounded JPEG covers."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import io
from pathlib import Path
import re
import time
from urllib.parse import unquote, urlsplit
from urllib.request import Request, urlopen

SIDE=272
MAX_ENCODED=60000


def jpeg(raw):
    from PIL import Image, ImageOps
    with Image.open(io.BytesIO(raw)) as original:
        if original.width*original.height>16000000:
            raise ValueError("artwork image too large")
        image=ImageOps.fit(original.convert("RGB"),(SIDE,SIDE),method=Image.Resampling.LANCZOS)
    for quality in (92,85,75,65):
        output=io.BytesIO();image.save(output,format="JPEG",quality=quality,subsampling=0)
        if len(output.getvalue())<=MAX_ENCODED:return output.getvalue()
    raise ValueError("encoded artwork too large")


def load_art(state):
    from PIL import Image
    url=urlsplit(state.get("art_url",""))
    hosts={"lh3.googleusercontent.com","yt3.ggpht.com","i.ytimg.com","lh3.ggpht.com"}
    urls=[]
    if url.scheme=="https" and url.hostname in hosts:urls.append(url.geturl())
    if re.fullmatch(r"[A-Za-z0-9_-]{11}",state["track"]):
        urls.extend(f'https://i.ytimg.com/vi/{state["track"]}/{size}.jpg' for size in ("maxresdefault","hqdefault"))
    # Firefox's MPRIS export may be only 60px: prefer a real high-resolution
    # source before falling back to that bounded, purpose-specific local file.
    for target in urls:
        try:
            with urlopen(Request(target,headers={"User-Agent":"Mero-Companion/1.0"}),timeout=3) as response:
                raw=response.read(4000001)
            if len(raw)>4000000:continue
            with Image.open(io.BytesIO(raw)) as image:
                if min(image.size)<SIDE:continue
            return jpeg(raw)
        except (OSError,ValueError):continue
    if url.scheme=="file" and url.hostname in (None,""):
        path=Path(unquote(url.path)).resolve()
        firefox=(Path.home()/".mozilla/firefox").resolve()
        try:
            if path.is_relative_to(firefox) and path.parent.name=="firefox-mpris" and path.stat().st_size<=4000000:
                return jpeg(path.read_bytes())
        except (OSError,ValueError):pass
    raise ValueError("no artwork available")


class Artwork:
    def __init__(self,share):
        self.share=Path(share);self.pool=ThreadPoolExecutor(max_workers=1)
        self.future=None;self.pending=None;self.current=None;self.sha="";self.retry=0
        self.retained=sorted((p for p in self.share.glob("art-*") if re.fullmatch(r"art-[a-f0-9]{64}\.(?:jpg|565)",p.name)),key=lambda p:p.stat().st_mtime)
        for old in self.retained[:-4]:old.unlink(missing_ok=True)
        self.retained=self.retained[-4:]

    def update(self,state):
        identity=(state["track"],state.get("art_url","")) if state else None
        # An export can be replaced at the same path while Firefox plays a new song.
        if identity and identity[1].startswith("file:"):
            try:
                stat=Path(unquote(urlsplit(identity[1]).path)).stat()
                identity+=(stat.st_mtime_ns,stat.st_size)
            except OSError:pass
        if self.future and self.future.done():
            try:
                pixels=self.future.result();sha=hashlib.sha256(pixels).hexdigest()
                path=self.share/f"art-{sha}.jpg";path.parent.mkdir(parents=True,exist_ok=True)
                if not path.exists():
                    temp=path.with_suffix(".tmp");temp.write_bytes(pixels);temp.replace(path)
                self.current,self.sha=self.pending,sha
                if path in self.retained:self.retained.remove(path)
                self.retained.append(path)
                # Owned thumbnail caches only; preserve source artwork and user files.
                for old in self.retained[:-4]:old.unlink(missing_ok=True)
                self.retained=self.retained[-4:]
            except (ImportError,OSError,ValueError):
                self.retry=time.monotonic()+30
            self.future=None
        if identity!=self.pending:self.retry=0
        if identity!=self.current and not self.future and time.monotonic()>=self.retry and state:
            self.pending=identity;self.future=self.pool.submit(load_art,dict(state))
        return self.sha if identity==self.current else ""

    def close(self):
        self.pool.shutdown(wait=True)
