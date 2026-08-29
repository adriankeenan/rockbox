#!/usr/bin/env python3
"""Generate PSP EBOOT.PBP artwork (ICON0.PNG / PIC1.PNG) from the official
Rockbox SVG logos in docs/logo/."""

import sys
from PIL import Image, ImageDraw, ImageFont

OUT_ICON = sys.argv[1]
OUT_PIC1 = sys.argv[2]
LOGO = sys.argv[3]   # pre-rendered rockbox-logo.svg (RGBA)
ICON = sys.argv[4]   # pre-rendered rockbox-icon.svg (RGBA)
CLEF = sys.argv[5]   # pre-rendered rockbox-clef.svg (RGBA)

AMBER = (255, 192, 0)
STEEL = (180, 195, 211)

FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FONT_REG = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"


def vgradient(size, top, bottom):
    w, h = size
    img = Image.new("RGB", (1, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(h - 1, 1)
        d.point((0, y), tuple(round(top[i] + (bottom[i] - top[i]) * t)
                              for i in range(3)))
    return img.resize((w, h), Image.BICUBIC)


def fit(img, box_w, box_h):
    """Scale preserving aspect to fit inside the box."""
    s = min(box_w / img.width, box_h / img.height)
    return img.resize((max(1, round(img.width * s)),
                       max(1, round(img.height * s))), Image.LANCZOS)


# ---------------------------------------------------------------- ICON0
# 144x80 is the XMB game-list icon. Dark tile so the amber logo panel pops
# against whatever wallpaper the user has set.
logo = Image.open(LOGO).convert("RGBA")

icon = vgradient((144, 80), (38, 38, 42), (12, 12, 14)).convert("RGBA")
# Drop the logo's "open source jukebox firmware" strip here -- at 144px wide
# it renders as an illegible smear, and losing it lets the wordmark itself
# sit much larger in the tile.
wordmark = logo.crop((0, 0, logo.width, round(logo.height * 0.868)))
lg = fit(wordmark, 134, 64)
icon.alpha_composite(lg, ((144 - lg.width) // 2, (80 - lg.height) // 2))

# subtle amber hairline border
d = ImageDraw.Draw(icon)
d.rectangle([0, 0, 143, 79], outline=(255, 192, 0, 90))
icon.convert("RGB").save(OUT_ICON)

# ---------------------------------------------------------------- PIC1
# 480x272 wallpaper shown behind the XMB when the entry is highlighted.
W, H = 480, 272
pic = vgradient((W, H), (26, 27, 32), (8, 8, 10)).convert("RGBA")

# Faint oversized treble-clef watermark bled off the bottom-right. The clef
# SVG ships on its own amber tile, so keep only the dark glyph pixels as a
# mask and recolour them -- compositing the raw asset would drop a solid
# amber square onto the gradient.
clef = Image.open(CLEF).convert("RGBA")
glyph_mask = Image.new("L", clef.size, 0)
cp, mp = clef.load(), glyph_mask.load()
for y in range(clef.height):
    for x in range(clef.width):
        r, g, b, a = cp[x, y]
        if a > 128 and r + g + b < 240:
            mp[x, y] = 40
wm = Image.new("RGBA", clef.size, AMBER + (0,))
wm.putalpha(glyph_mask)
wm = fit(wm, 250, 250)
pic.alpha_composite(wm, (W - 128, H - 132))

# Deliberately no wordmark here. ICON0 already carries it, and the XMB draws
# that icon across the middle of this image -- a logo here meant the icon
# landed on top of an identical logo, hiding both. Everything drawn below sits
# in the bottom band, which the icon clears.

d = ImageDraw.Draw(pic)
f_title = ImageFont.truetype(FONT_BOLD, 20)


def centred(text, y, font, fill):
    tw = d.textbbox((0, 0), text, font=font)[2]
    d.text(((W - tw) // 2, y), text, font=font, fill=fill)


rule_y = 182
d.line([(W // 2 - 104, rule_y), (W // 2 + 104, rule_y)], fill=(255, 192, 0, 110))
centred("PlayStation Portable", rule_y + 14, f_title, AMBER)

pic.convert("RGB").save(OUT_PIC1)
print("wrote", OUT_ICON, OUT_PIC1)
