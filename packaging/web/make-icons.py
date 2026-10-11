"""Regenerates the installable-app icons from a square source logo with transparency.
usage: python packaging/web/make-icons.py <logo.png>"""
import sys
from pathlib import Path
from PIL import Image

here = Path(__file__).parent
logo = Image.open(sys.argv[1]).convert("RGBA")
box = logo.getchannel("A").getbbox()
logo = logo.crop(box)
side = max(logo.size)
square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
square.paste(logo, ((side - logo.width) // 2, (side - logo.height) // 2))
for size in (192, 512):
    square.resize((size, size), Image.LANCZOS).save(here / f"icon-{size}.png", optimize=True)
# Maskable: Android crops to a circle or squircle, so the logo sits inside the safe 60% on a solid tile.
tile = Image.new("RGBA", (512, 512), (3, 8, 6, 255))
inner = square.resize((308, 308), Image.LANCZOS)
tile.paste(inner, (102, 102), inner)
tile.save(here / "icon-maskable-512.png", optimize=True)
# Android launcher icons use the same tile.
for name, size in (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)):
    folder = here.parent.parent / "apps" / "client" / "android" / "res" / f"mipmap-{name}"
    folder.mkdir(parents=True, exist_ok=True)
    tile.resize((size, size), Image.LANCZOS).save(folder / "ic_launcher.png", optimize=True)
print("icons written to", here)
