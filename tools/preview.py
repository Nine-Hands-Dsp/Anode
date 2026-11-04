# apercu rapide (hors plugin) : fond + commandes rendues, compare au visuel de reference
import os, sys, math, numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import controls_art as ca, make_background as mb
ROOT = ca.ROOT
bg = Image.open(os.path.join(ROOT, "background.jpg")).convert("RGBA")
S = 2
def paste(img, cx, cy):
    im = Image.fromarray(img, "RGBA"); bg.alpha_composite(im, (int(round(cx * S - im.width / 2)), int(round(cy * S - im.height / 2))))
k = ca.knob(0.0)
for (x, y) in mb.KNOBS:
    paste(k, x, y)
    d = ImageDraw.Draw(bg)
    d.line([(x * S, (y - 16.2) * S), (x * S, (y - 25.6) * S)], fill=(238, 238, 238, 255), width=int(2.6 * S))
paste(ca.selector(-13.5), *mb.SEL)
t = ca.toggle(1)
for (x, y) in mb.TOG: paste(t, x, y)
lab = Image.open(os.path.join(ROOT, "label_EL34.png")); bg.alpha_composite(lab, (320 * S, 80 * S))
bg.convert("RGB").save("/home/claude/dev/art/preview.png")
ref = Image.open(os.path.join(ROOT, "design/reference.png")).convert("RGB").resize(bg.size, Image.LANCZOS)
both = Image.new("RGB", (bg.width, bg.height * 2)); both.paste(bg.convert("RGB"), (0, 0)); both.paste(ref, (0, bg.height))
both.crop((280, 380, 1300, 812 + 812)).save("/home/claude/dev/art/compare.png")
