"""Regenerate the Luma icon (optional, requires Pillow; not needed to build)."""
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
scale = 4
size = 256 * scale
im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
d = ImageDraw.Draw(im)
d.rounded_rectangle((8 * scale, 8 * scale, 248 * scale, 248 * scale), radius=52 * scale, fill="#1e1d1a")
d.ellipse((46 * scale, 46 * scale, 210 * scale, 210 * scale), fill="#f7f4ec")
d.ellipse((52 * scale, 52 * scale, 204 * scale, 204 * scale), outline="#d6d0c2", width=2 * scale)
for x0, y0, x1, y1 in ((128, 55, 128, 64), (128, 192, 128, 201), (55, 128, 64, 128), (192, 128, 201, 128)):
    d.line((x0 * scale, y0 * scale, x1 * scale, y1 * scale), fill="#1e1d1a", width=7 * scale)
d.line((128 * scale, 128 * scale, 78 * scale, 99 * scale), fill="#1e1d1a", width=10 * scale)
d.line((128 * scale, 128 * scale, 192 * scale, 91 * scale), fill="#1e1d1a", width=6 * scale)
d.line((128 * scale, 128 * scale, 169 * scale, 169 * scale), fill="#a63d2a", width=4 * scale)
d.ellipse((120 * scale, 120 * scale, 136 * scale, 136 * scale), fill="#a63d2a")
im = im.resize((256, 256), Image.LANCZOS)
im.save(root / "resources" / "app.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
