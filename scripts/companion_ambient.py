"""PC folder slideshow. Only bounded, resized frames leave the PC; never originals."""
from array import array
from concurrent.futures import ThreadPoolExecutor
import hashlib
import io
import json
from pathlib import Path
import re
import sys
import time

from companion_live import atomic

WIDTH, HEIGHT = 240, 136
FRAME_BYTES = WIDTH * HEIGHT * 2
EXTENSIONS = {".jpg", ".jpeg", ".png", ".webp", ".bmp"}


def clean(value, limit=70):
    return re.sub(r"[\r\n|=]", " ", str(value))[:limit]


def frame_image(path, portrait=False, native=False):
    from PIL import Image, ImageOps
    with Image.open(path) as original:
        if original.width * original.height > 24000000:
            raise ValueError("image exceeds 24 megapixels")
        original = ImageOps.exif_transpose(original).convert("RGB")
        width,height = (480,272) if native else (WIDTH,HEIGHT)
        size = (height, width) if portrait else (width, height)
        image = ImageOps.fit(original, size, method=Image.Resampling.LANCZOS)
        return image.transpose(Image.Transpose.ROTATE_90) if portrait else image


def render(path):
    image = frame_image(path)
    pixels = array("H", ((r >> 3) << 11 | (g >> 2) << 5 | b >> 3 for r, g, b in image.getdata()))
    if sys.byteorder != "little":
        pixels.byteswap()
    return pixels.tobytes()


def render_jpeg(path, portrait=False):
    image=frame_image(path,portrait,native=True)
    # Keep the proven transfer ceiling even for detailed/noisy native frames.
    for quality in (92,88,84,78,70,60,45,30):
        output=io.BytesIO()
        image.save(output, format="JPEG", quality=quality, optimize=True, subsampling=0 if quality>=84 else 2)
        data=output.getvalue()
        if len(data)<65280:return data
    raise ValueError("compressed frame too large")


class Ambient:
    def __init__(self, share, config=None, compressed=False):
        self.compressed=compressed
        self.portrait=False
        self.share = Path(share)
        self.config = Path(config or Path.home()/".config/mero/ambient.json")
        if not self.config.exists():
            pictures = Path.home()/"Pictures"
            folders = [pictures] + sorted(p for p in pictures.iterdir() if p.is_dir() and not p.is_symlink() and not p.name.startswith(".")) if pictures.is_dir() else []
            atomic(self.config, json.dumps({"folders": [{"name": p.name, "path": str(p)} for p in folders[:8]], "interval": 20, "selected": 0}, indent=2).encode())
        data = json.loads(self.config.read_text())
        self.folders = [(clean(f["name"], 30), Path(f["path"]).expanduser().resolve()) for f in data["folders"][:8]]
        self.portrait=bool(data.get("portrait",False))
        self.selected = max(0, min(int(data.get("selected", 0)), len(self.folders)-1))
        self.interval = max(15, min(600, int(data.get("interval", 20))))
        self.token = hashlib.sha256((json.dumps([(n, str(p)) for n,p in self.folders])+str(time.time_ns())).encode()).hexdigest()[:16]
        self.pool = ThreadPoolExecutor(max_workers=1)
        self.future = None
        self.revision = 0
        self.generation = 0
        self.awaiting_display = False
        self.pending_revision = 0
        self.active_until = 0
        self.next_frame = 0
        self.paused = False
        self.index = -1
        self.sha = self.title = ""
        self.status = "Open Ambient Home"
        self.files = []
        self.retained = sorted((p for p in self.share.glob("ambient-*.565") if re.fullmatch(r"ambient-[a-f0-9]{64}\.565", p.name)), key=lambda p:p.stat().st_mtime)
        self._prune()

    def _prune(self):
        for path in self.retained[:-2]:
            path.unlink(missing_ok=True)
        self.retained = self.retained[-2:]

    def action(self, event):
        action = event["action"]
        if event["track"] != self.token:
            return "folder_changed_press_again"
        if action not in {"ambient-ping", "ambient-shown", "ambient-next", "ambient-prev", "ambient-pause", "ambient-folder", "ambient-rotate"}:
            return "control_unavailable"
        self.active_until = time.monotonic()+60
        if action == "ambient-shown" and int(event["value"]) == self.generation and self.awaiting_display:
            self.awaiting_display = False
            self.next_frame = time.monotonic()+self.interval
        if action == "ambient-rotate":
            self.portrait=not self.portrait
            self.index-=1
            data=json.loads(self.config.read_text());data["portrait"]=self.portrait
            atomic(self.config,json.dumps(data,indent=2).encode())
            self.revision+=1;self.awaiting_display=False;self.next_frame=0;self.sha=""
        if action == "ambient-folder":
            selected = int(event["value"])
            if not 0 <= selected < len(self.folders):
                return "control_unavailable"
            self.selected = selected
            data = json.loads(self.config.read_text())
            data["selected"] = selected
            atomic(self.config, json.dumps(data, indent=2).encode())
            self.index = -1
            self.sha = self.title = ""
            self.revision += 1
            self.awaiting_display = False
            self.status = "Loading folder"
            self.next_frame = 0
        elif action in {"ambient-next", "ambient-prev"}:
            if action == "ambient-prev":
                self.index -= 2
            self.revision += 1
            self.awaiting_display = False
            self.next_frame = 0
        elif action == "ambient-pause":
            self.paused = not self.paused
        return "ok"

    def update(self):
        now = time.monotonic()
        if self.future and self.future.done():
            try:
                pixels = self.future.result()
                if self.pending_revision == self.revision:
                    sha = hashlib.sha256(pixels).hexdigest()
                    path = self.share/f"ambient-{sha}.565"
                    atomic(path, pixels)
                    self.sha = sha
                    self.generation += 1
                    self.awaiting_display = True
                    self.title = clean(self.pending_path.stem)
                    self.status = "Paused" if self.paused else "Slideshow"
                    if path in self.retained:
                        self.retained.remove(path)
                    self.retained.append(path)
                    self._prune()
                    self.next_frame = now+self.interval
            except Exception:
                self.status = "Image unavailable; skipping"
                self.next_frame = now+3
            self.future = None
        if self.folders and now < self.active_until and not self.future and not self.awaiting_display and now >= self.next_frame and (not self.paused or not self.sha or not self.next_frame):
            try:
                folder = self.folders[self.selected][1]
                self.files = sorted(p for p in folder.iterdir() if p.is_file() and not p.is_symlink() and p.suffix.lower() in EXTENSIONS)[:4096]
                if not self.files:
                    self.sha = self.title = ""
                    self.status = "No images in this folder"
                    self.next_frame = now+15
                else:
                    self.index = (self.index+1) % len(self.files)
                    self.pending_path = self.files[self.index]
                    if self.pending_path.stat().st_size > 32000000:
                        raise ValueError("image exceeds 32 MB")
                    self.pending_revision = self.revision
                    self.future = self.pool.submit(render_jpeg,self.pending_path,self.portrait) if self.compressed else self.pool.submit(render,self.pending_path)
            except (OSError, ValueError):
                self.status = "Folder or image unavailable"
                self.next_frame = now+5
        fields = dict(portrait=int(self.portrait), token=self.token, folder=self.selected, name=self.folders[self.selected][0] if self.folders else "Configure PC folders", title=self.title, sha=self.sha, status=self.status, paused=int(self.paused), count=len(self.files), generation=self.generation, clock=time.strftime("%H:%M"))
        fields.update({f"f{i}": name for i,(name,path) in enumerate(self.folders)})
        return "MERO:AMB:"+"|".join(f"{key}={clean(value)}" for key,value in fields.items())

    def close(self):
        self.pool.shutdown(wait=True)
