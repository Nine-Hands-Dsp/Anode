# ANODE v2 : exporte les calques du plugin (repere 2737 x 1800 px = 2x des unites du plugin, 1368,5 x 900 unites)
# et ecrit Layout.h. Fond fixe (lampes froides, cadrans et Power eteints, corps des potards, ombres) ;
# par-dessus, dans le plugin : lampes chaudes, lueur au jeu, plaques rouges, Power allume, cadrans eclaires,
# aiguilles, traits des potards, inverseurs, textes / points / logos vectoriels.
import math, os, shutil, sys
import numpy as np, cv2
from PIL import Image
import make_design as md
import make_design2 as m2
import make_final as mf
import fix_base

OUT = sys.argv[1] if len(sys.argv) > 1 else md.REPO
T, s = mf.T, mf.s
U = 0.5                                                     # unites par pixel

def save_rgba(arr, name):
    Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGBA").save(os.path.join(OUT, name), optimize=True)

def sprite_on_canvas(spr, x, y):
    """ pose un sprite RGBA (non premultiplie) sur une toile transparente pleine taille, renvoie la toile """
    c = np.zeros((mf.OUT_H, mf.OUT_W, 4))
    h, w = spr.shape[:2]; x0, y0 = int(round(x)), int(round(y))
    c[y0:y0 + h, x0:x0 + w] = spr
    return c

def crop_box(alpha, pad=2):
    ys, xs = np.where(alpha > 0.5)
    return max(0, xs.min() - pad), max(0, ys.min() - pad), min(mf.OUT_W, xs.max() + 1 + pad), min(mf.OUT_H, ys.max() + 1 + pad)

L = {}                                                      # valeurs ecrites dans Layout.h (unites)

# ------------------------------------------------------------------ base (facade + lampes)
img = np.zeros((mf.OUT_H, mf.OUT_W, 4)); img[..., 3] = 255
img[..., :3] = mf.AMP[..., :3] * (mf.AMP[..., 3:4] / 255)
img = mf.full_panel(img)
img = fix_base.fix(img)                                     # barre chromee du pied de cage : droite et propre
hot = img.copy()
cold, warm = mf.tube_layers(img)
img[..., :3] = cold

# lampes chaudes au repos (lueur de la maquette validee) : calque opaque, zone des lampes
hot = mf.bloom(hot, warm, 0.4)
zx0, zy0 = T(300, 430); zx1, zy1 = T(1960, 1205)
ZR = tuple(int(round(v)) for v in (zx0, zy0, zx1, zy1))
# bords du calque fondus sur le fond froid (24 px) : aucune marche visible sur la barre chromee ni le chassis
_h, _w = ZR[3] - ZR[1], ZR[2] - ZR[0]
_d = np.minimum(np.minimum(np.arange(_w)[None, :], (_w - 1 - np.arange(_w))[None, :]),
                np.minimum(np.arange(_h)[:, None], (_h - 1 - np.arange(_h))[:, None]))
_fe = (0.5 - 0.5 * np.cos(np.pi * np.clip(_d / 24.0, 0, 1)))[..., None]
hot[ZR[1]:ZR[3], ZR[0]:ZR[2], :3] = img[ZR[1]:ZR[3], ZR[0]:ZR[2], :3] * (1 - _fe) + hot[ZR[1]:ZR[3], ZR[0]:ZR[2], :3] * _fe
Image.fromarray(np.clip(hot[ZR[1]:ZR[3], ZR[0]:ZR[2], :3], 0, 255).astype(np.uint8)).save(os.path.join(OUT, "tubes_hot.jpg"), quality=92, subsampling=0)
# lueur supplementaire quand on joue (additive) : couleur / alpha
G = (hot[..., :3] * warm[..., None] * 0.55 + cv2.GaussianBlur(hot[..., :3] * warm[..., None], (0, 0), s(28)) * 0.9 \
    + cv2.GaussianBlur(hot[..., :3] * warm[..., None], (0, 0), s(6)) * 0.35) * 0.65          # lueur au jeu (reduite de 35 %)
G = G[ZR[1]:ZR[3], ZR[0]:ZR[2]]
a = np.clip(G.max(2) / 255.0, 0, 1)
col = np.where(a[..., None] > 1e-3, G / np.maximum(a, 1e-3)[..., None], 0)
save_rgba(np.dstack([col, a * _fe[..., 0] * 255]), "tubes_bloom.png")
# plaques qui rougissent : structures grises (plaques, ecrans) a l'interieur des lampes
rgb = hot[ZR[1]:ZR[3], ZR[0]:ZR[2], :3]
lum = rgb.mean(2); sat = rgb.max(2) - rgb.min(2)
inner = np.zeros(lum.shape)
for bx0, by0, bx1, by1 in md.TUBE_BOX:
    w_ = bx1 - bx0
    (a0, b0), (a1, b1) = T(bx0 + 0.22 * w_, by0 + 0.18 * (by1 - by0)), T(bx1 - 0.22 * w_, by1 - 0.12 * (by1 - by0))
    inner[int(b0) - ZR[1]:int(b1) - ZR[1], int(a0) - ZR[0]:int(a1) - ZR[0]] = 1
inner = cv2.GaussianBlur(inner, (0, 0), s(8))
plate = np.clip((lum - 38) / 40, 0, 1) * np.clip((150 - lum) / 50, 0, 1) * np.clip((60 - sat) / 30, 0, 1) * inner
plate = cv2.GaussianBlur(plate, (0, 0), s(1.2))
redcol = np.dstack([np.full(lum.shape, 255.0), 70 + 0.25 * lum, np.full(lum.shape, 22.0)])
save_rgba(np.dstack([redcol, np.clip(plate * 0.47, 0, 1) * _fe[..., 0] * 255]), "tubes_red.png")
L["tubeZone"] = [v * U for v in (ZR[0], ZR[1], ZR[2] - ZR[0], ZR[3] - ZR[1])]

# ------------------------------------------------------------------ Power (eteint dans le fond, allume en calque)
spr_off, c = mf.power_sprite(False); px, py = T(*m2.PWR)
mf.drop_shadow(img, spr_off, px - c[0], py - c[1], s(4), s(6), s(6), 0.65)
md.over(img, spr_off, px - c[0], py - c[1])
spr_on, c = mf.power_sprite(True)
cv = sprite_on_canvas(spr_on, px - c[0], py - c[1]); b = crop_box(cv[..., 3])
save_rgba(cv[b[1]:b[3], b[0]:b[2]], "power_on.png")
L["powerRect"] = [v * U for v in (b[0], b[1], b[2] - b[0], b[3] - b[1])]
L["powerC"] = [px * U, py * U]; L["powerR"] = s(m2.PWR_OUT_R) * U

# ------------------------------------------------------------------ cadrans (eteints dans le fond ; eclaires, mecanisme en calques)
k = s(m2.VU_OUT_R) / m2.VU_IMG_R
mf.vu_sprite(0.0, -60.0)                                    # charge les sources
src = mf._vu_src
dial_off = m2.scaled(src["off"], k); dial_on = m2.scaled(src["on"], k)
mech = cv2.resize(src["mech"], (dial_off.shape[1], dial_off.shape[0]), interpolation=cv2.INTER_AREA)
vu_rects = []
for i, (vx, vy) in enumerate(m2.VU):
    x, y = T(vx, vy); r = s(m2.VU_OUT_R)
    ox, oy = x - m2.VU_IMG_C[0] * k, y - m2.VU_IMG_C[1] * k
    mf.drop_shadow(img, dial_off, ox, oy, 0, s(1.5), s(3.5), 0.85)
    mf.drop_shadow(img, dial_off, ox, oy, r * 0.07, r * 0.11, r * 0.09, 0.5)
    md.over(img, dial_off, ox, oy)
    vu_rects.append([ox * U, oy * U, dial_off.shape[1] * U, dial_off.shape[0] * U])
save_rgba(dial_on, "vu_on.png")
mo = dial_off.copy(); mo[..., 3] = mo[..., 3] * mech; save_rgba(mo, "vu_mech_off.png")
mn = dial_on.copy();  mn[..., 3] = mn[..., 3] * mech; save_rgba(mn, "vu_mech_on.png")
L["vuRect"] = vu_rects
L["vuPivot"] = [m2.VU_PIVOT[0] * k * U, m2.VU_PIVOT[1] * k * U]          # dans le rectangle du cadran
L["vuNeedleLen"] = m2.VU_NEEDLE * k * U; L["vuNeedleW"] = s(2.6) * U
L["vuTicks"] = m2.VU_TICKS; L["vuRest"] = m2.VU_REST

# ------------------------------------------------------------------ corps des potards et du selecteur (fixes)
parts = md.knob_parts()
body, (pcx, pcy, pr), seg, lw = parts
def body_only(radius):
    sp, cc = md.knob_sprite(radius, 0.0, parts, line_w=0.0001)
    return sp, cc
for nm, (kx, ky) in md.KNOBS.items():
    sp, cc = body_only(s(md.KNOB_R)); x, y = T(kx, ky)
    md.over(img, sp, x - cc, y - cc)
sp, cc = body_only(s(md.SEL_R)); x, y = T(*md.SEL); md.over(img, sp, x - cc, y - cc)
kk = s(md.KNOB_R) / pr
L["knobC"] = [[T(*md.KNOBS[n])[0] * U, T(*md.KNOBS[n])[1] * U] for n in ("Depth", "Contour", "Volume")]
L["knobR"] = s(md.KNOB_R) * U
L["knobLine"] = [-seg[1] * kk * U, -seg[0] * kk * U, max(1.5, lw * kk) * U]   # du centre : debut, fin, epaisseur
ks = s(md.SEL_R) / pr
L["selC"] = [T(*md.SEL)[0] * U, T(*md.SEL)[1] * U]; L["selR"] = s(md.SEL_R) * U
L["selLine"] = [-seg[1] * ks * U, -seg[0] * ks * U, max(1.5, lw * kk) * U]
L["selAngle"] = md.SEL_ANG

# ------------------------------------------------------------------ fond
Image.fromarray(np.clip(img[..., :3], 0, 255).astype(np.uint8)).save(os.path.join(OUT, "background.jpg"), quality=92, subsampling=0)
L["W"], L["H"] = mf.OUT_W * U, mf.OUT_H * U
# zones transparentes (pas d'ampli) : peintes en noir par le fond lui-meme

# ------------------------------------------------------------------ inverseurs (kit photo NINE HANDS | DSP)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import toggle_photo
R_u = s(m2.TOG_R) * U
fr = toggle_photo.frames(R_u, 76.0, 2)                     # cadre assez grand : levier haut / bas jamais coupe
Image.fromarray(np.concatenate(fr, 0), "RGBA").save(os.path.join(OUT, "toggle_strip.png"), optimize=True)
L["togC"] = [[T(x, m2.TOG_Y)[0] * U, T(x, m2.TOG_Y)[1] * U] for x, _, _ in m2.TOGGLES]
L["togSize"] = 76.0; L["togHitR"] = R_u * 1.25

# ------------------------------------------------------------------ textes, points, graduations (vectoriels dans le plugin)
from PIL import ImageDraw, ImageFont
def cap_bottom(y_center, size):
    f = ImageFont.truetype(mf.FONT, int(round(size)))
    bb = ImageDraw.Draw(Image.new("L", (8, 8))).textbbox((0, 0), "H", font=f)
    return y_center + (bb[3] - bb[1]) / 2
texts = []
for nm in ("Depth", "Contour", "Volume"):
    x, y = T(md.KNOBS[nm][0], m2.LABEL_Y); texts.append((nm.upper(), x, y, s(29), s(2.5)))
x, y = T(md.SEL[0], 1258); texts.append(("TUBE", x, y, s(29), s(2.5)))
for nm, ang in zip(md.TUBE_NAMES, md.SEL_ANG):
    t = math.radians(ang); rr = md.SEL_R + 46
    x, y = T(md.SEL[0] + math.sin(t) * rr, md.SEL[1] - math.cos(t) * rr); texts.append((nm, x, y, s(24), s(1.0)))
for tx, top, bottom in m2.TOGGLES:
    x, y = T(tx, m2.TOG_Y - m2.TOG_LAB_UP); texts.append((top, x, y, s(22), s(1.5)))
    x, y = T(tx, m2.TOG_Y + m2.TOG_LAB_DN); texts.append((bottom, x, y, s(22), s(1.5)))
L["texts"] = [(t, x * U, y * U, sz * U, tr * U) for t, x, y, sz, tr in texts]
L["dotRing"] = (md.KNOB_R + 12) * mf.SC * U; L["dotR"] = s(2.9) * U
L["selTick"] = [(md.SEL_R + 10) * mf.SC * U, (md.SEL_R + 24) * mf.SC * U, s(3) * U]
x, y = T(m2.PWR[0], 1362); L["powerSym"] = [x * U, y * U, s(11) * U, s(15) * U, s(2) * U, s(3) * U]
# logos (rectangles en unites)
pent_bottom = cap_bottom(T(0, m2.TOG_Y + m2.TOG_LAB_DN)[1], s(22))
lwp = s(340) * 0.7; kk_ = lwp / 2500.0
fy = int(mf.OUT_H - 25); xs = np.where(mf.AMP[fy, :mf.OUT_W // 2, 3] > 128)[0]; foot = (xs.min() + xs.max()) / 2
L["nineHands"] = [(foot - lwp / 2) * U, (pent_bottom - (575 - 387) * kk_) * U, lwp * U, 226 * kk_ * U]   # viewBox 250 387 2500 226
vb = (110.00, 33.90, 413.00, 120.20); ka = s(52.0) * 0.7 / 91.0
L["anode"] = [(T(1960, 0)[0] - vb[2] * ka / 2) * U, (pent_bottom - (139.5 - vb[1]) * ka) * U, vb[2] * ka * U, vb[3] * ka * U]
# barre de presets (haut, au centre, sur le haut des transformateurs) et ligne d'etat
# barre de presets / ligne d'etat : au-dessus des deux VU-metres, centree sur la paire, a egale distance
# de l'arete haute du chassis (y 505) et du haut des lunettes des VU (y 587,5, mesure sur le fond) ; 285 x 30 (-25 %)
_bw, _bh = 285.0, 30.0
_cx = 0.5 * ((vu_rects[0][0] + vu_rects[0][2] / 2) + (vu_rects[1][0] + vu_rects[1][2] / 2))
_top, _vu = 1010.0 * U, 1175.0 * U
L["preset"] = [_cx - _bw / 2, _top + (_vu - _top - _bh) / 2, _bw, _bh]

# masque de l'ampli (couverture de la notice)
Image.fromarray(np.clip(mf.AMP[..., 3], 0, 255).astype(np.uint8), "L").save(os.path.join(md.REPO, "design", "v2", "amp_alpha.png"), optimize=True)

# ------------------------------------------------------------------ Layout.h
def f(v): return f"{v:.2f}f"
def arr(a): return "{ " + ", ".join(f(v) for v in a) + " }"
o = ["#pragma once", "// ANODE v2 - genere par tools/v2/make_layers.py : repere 1368,5 x 900 unites (images 2x = 2737 x 1800 px)",
     "namespace layout", "{", f"    constexpr float W = {f(L['W'])}, H = {f(L['H'])};"]
o.append(f"    constexpr float tubeZone[4] = {arr(L['tubeZone'])};")
o.append(f"    constexpr float powerRect[4] = {arr(L['powerRect'])}, powerC[2] = {arr(L['powerC'])}, powerR = {f(L['powerR'])};")
o.append(f"    constexpr float powerSym[6] = {arr(L['powerSym'])};      // x, y, rayon, haut du trait, bas du trait, epaisseur")
o.append(f"    constexpr float vuRect[2][4] = {{ {arr(L['vuRect'][0])}, {arr(L['vuRect'][1])} }};")
o.append(f"    constexpr float vuPivot[2] = {arr(L['vuPivot'])};                 // dans le rectangle du cadran")
o.append(f"    constexpr float vuNeedleLen = {f(L['vuNeedleLen'])}, vuNeedleW = {f(L['vuNeedleW'])}, vuRest = {f(L['vuRest'])};")
o.append(f"    constexpr int   vuNumTicks = {len(L['vuTicks'])};")
o.append(f"    constexpr float vuTickDb[vuNumTicks] = {arr([t[0] for t in L['vuTicks']])};")
o.append(f"    constexpr float vuTickDeg[vuNumTicks] = {arr([t[1] for t in L['vuTicks']])};")
o.append(f"    constexpr float knobC[3][2] = {{ {', '.join(arr(c) for c in L['knobC'])} }};")
o.append(f"    constexpr float knobR = {f(L['knobR'])}, knobLine[3] = {arr(L['knobLine'])};   // trait : debut, fin (du centre), epaisseur")
o.append(f"    constexpr float knobSpan = 135.0f;")
o.append(f"    constexpr float dotRing = {f(L['dotRing'])}, dotR = {f(L['dotR'])};")
o.append(f"    constexpr float selC[2] = {arr(L['selC'])}, selR = {f(L['selR'])}, selLine[3] = {arr(L['selLine'])};")
o.append(f"    constexpr float selAngle[4] = {arr(L['selAngle'])};")
o.append(f"    constexpr float selTick[3] = {arr(L['selTick'])};")
o.append(f"    constexpr float togC[2][2] = {{ {', '.join(arr(c) for c in L['togC'])} }};")
o.append(f"    constexpr float togSize = {f(L['togSize'])}, togHitR = {f(L['togHitR'])};")
o.append(f"    constexpr float nineHands[4] = {arr(L['nineHands'])}, anode[4] = {arr(L['anode'])};")
o.append(f"    constexpr float preset[4] = {arr(L['preset'])};")
o.append("    struct Label { const char* text; float x, y, size, tracking; };   // centre des capitales, taille (em), espacement")
o.append(f"    constexpr int numLabels = {len(L['texts'])};")
o.append("    constexpr Label labels[numLabels] = {")
for t, x, y, sz, tr in L["texts"]:
    o.append(f'        {{ "{t}", {f(x)}, {f(y)}, {f(sz)}, {f(tr)} }},')
o.append("    };")
o.append("}")
open(os.path.join(OUT, "Layout.h"), "w").write("\n".join(o) + "\n")
print("\n".join(o))
