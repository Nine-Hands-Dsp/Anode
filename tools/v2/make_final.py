# ANODE v2 : rendu final plein cadre 2737 x 1800, chaque element recalcule a sa taille native
# (texture, bouton Power, VU-metres, potards, inverseurs, logos SVG). Seule la photo de l'ampli est agrandie.
# Toutes les mesures de la maquette (pixels de la photo 2261 x 2261) passent par T() / s().
import math, subprocess, sys
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont
import make_design as md
import make_design2 as m2

D = md.D
TMP = __import__("tempfile").gettempdir() + "/"
OUT_W, OUT_H = 2737, 1800
CX0, CY0 = 11.0, 336.0                                   # coin haut-gauche du cadrage plein cadre (photo)
SC = OUT_H / 1474.0                                      # 1474 px de photo -> 1800 px
def T(x, y): return ((x - CX0) * SC, (y - CY0) * SC)
def s(v): return v * SC
FONT = md.FONT

# ------------------------------------------------------------------ photo de l'ampli a l'echelle, cadree
def amp_scaled():
    a = np.asarray(Image.open(D + "amp.png").convert("RGBA")).astype(np.float64)
    pm = a.copy(); pm[..., :3] *= pm[..., 3:4] / 255
    M = np.float32([[SC, 0, -CX0 * SC], [0, SC, -CY0 * SC]])
    big = cv2.warpAffine(pm, M, (OUT_W, OUT_H), flags=cv2.INTER_LANCZOS4, borderValue=0)
    big = np.clip(big, 0, 255)
    al = big[..., 3:4] / 255
    big[..., :3] = np.where(al > 1e-3, big[..., :3] / np.maximum(al, 1e-3), 0)
    return big

AMP = amp_scaled()

# ------------------------------------------------------------------ facade : texture fournie + bevels releves sur la photo
def full_panel(img):
    x0, y0 = map(int, map(round, T(13, 1163))); x1, y1 = map(int, map(round, T(2250, 1757)))
    md.FACE = (x0, y0, x1, y1)
    tex = md.face_texture()
    lum = AMP[..., :3].mean(2)
    r = lambda y: int(round(T(0, y)[1])); c = lambda x: int(round(T(x, 0)[0]))
    face = np.median(lum[r(1250):r(1700), c(1100):c(1150)])
    px = np.median(lum[r(1250):r(1700), x0:x1], axis=0) / face
    py = np.median(lum[y0:y1, c(1000):c(2100)], axis=1) / face
    xs = np.arange(x0, x1) / SC + CX0; ys = np.arange(y0, y1) / SC + CY0   # retour en mesures de la photo
    wx = np.clip(np.maximum((150 - xs) / 20.0, (xs - 2105) / 20.0), 0, 1)
    wy = np.clip(np.maximum((1192 - ys) / 6.0, (ys - 1736) / 6.0), 0, 1)
    px = 1 + (cv2.GaussianBlur(px[None, :], (0, 0), s(1.2))[0] - 1) * wx
    py = 1 + (cv2.GaussianBlur(py[:, None], (0, 0), s(1.0))[:, 0] - 1) * wy
    tex = tex * np.clip(px, 0, 3.2)[None, :, None] * np.clip(py, 0, 3.2)[:, None, None]
    al = AMP[y0:y1, x0:x1, 3:4] / 255.0
    sub = img[y0:y1, x0:x1, :3]
    sub[:] = sub * (1 - al) + tex * al
    return img

# ------------------------------------------------------------------ lampes : froides / chaudes
def tube_layers(img):
    rgb = img[..., :3]
    warm = np.clip((rgb[..., 0] - rgb[..., 2] - 22) / 110.0, 0, 1)
    region = np.zeros(rgb.shape[:2])
    for bx0, by0, bx1, by1 in md.TUBE_BOX:
        (a0, b0), (a1, b1) = T(bx0, by0), T(bx1, by1)
        region[int(b0):int(b1), int(a0):int(a1)] = 1
    region = cv2.GaussianBlur(region, (0, 0), s(6))
    w = (warm * region)[..., None]
    lum = rgb.mean(2, keepdims=True)
    return rgb * (1 - w) + (lum * 0.32 * np.array([1.0, 0.92, 0.85])) * w, w[..., 0]

def bloom(img, warm, k):
    img[..., :3] += cv2.GaussianBlur(img[..., :3] * warm[..., None], (0, 0), s(28)) * 0.55 * k
    return img

# ------------------------------------------------------------------ elements photo, a l'echelle native
def drop_shadow(img, spr, x, y, dx, dy, blur, amount):
    pad = int(abs(dx) + abs(dy) + 4 * blur + 4)
    md.over(img, md.shadow_of(spr, dx, dy, blur, amount, pad), x - pad, y - pad)

def power_sprite(on):
    im = m2.load_px("power_on.png" if on else "power_off.png")
    ys, xs = np.where(im[..., 3] > 128)
    c = ((xs.min() + xs.max()) / 2, (ys.min() + ys.max()) / 2); r = (xs.max() - xs.min()) / 2
    k = s(m2.PWR_OUT_R) / r
    return m2.scaled(im, k), (c[0] * k, c[1] * k)

def needle_layer(size, pivot, length, angle, width):
    S4 = 4
    L = Image.new("L", (size[0] * S4, size[1] * S4), 0); dl = ImageDraw.Draw(L)
    t = math.radians(angle); ux, uy = math.sin(t), -math.cos(t)
    px, py = pivot; tip = (px + ux * length, py + uy * length); nx, ny = -uy, ux
    w0, w1 = width * 1.35, width * 0.55
    dl.polygon([((px + nx * w0 / 2) * S4, (py + ny * w0 / 2) * S4), ((tip[0] + nx * w1 / 2) * S4, (tip[1] + ny * w1 / 2) * S4),
                ((tip[0] - nx * w1 / 2) * S4, (tip[1] - ny * w1 / 2) * S4), ((px - nx * w0 / 2) * S4, (py - ny * w0 / 2) * S4)], fill=255)
    a = np.asarray(L.resize(size, Image.LANCZOS)).astype(np.float64) / 255
    sh = cv2.GaussianBlur(np.roll(np.roll(a, int(round(s(4))), 0), int(round(s(2))), 1), (0, 0), s(2.0)) * 0.30
    return a, sh

_vu_src = {}
def vu_sprite(on, db):
    if not _vu_src:
        off_img, on_img = m2.load_px("vu_off.png"), m2.load_px("vu_on.png")
        yy, xx = np.mgrid[0:off_img.shape[0], 0:off_img.shape[1]]
        cut = np.clip((m2.VU_IMG_R - np.hypot(xx - m2.VU_IMG_C[0], yy - m2.VU_IMG_C[1])) / 1.5 + 0.5, 0, 1) * 255
        for im_ in (off_img, on_img): im_[..., 3] = np.minimum(im_[..., 3], cut)
        _vu_src.update(off=off_img, on=on_img, mech=m2.mech_mask(off_img))
    k = s(m2.VU_OUT_R) / m2.VU_IMG_R
    mix = _vu_src["off"].copy(); mix[..., :3] = _vu_src["off"][..., :3] * (1 - on) + _vu_src["on"][..., :3] * on
    spr = m2.scaled(mix, k)
    mech = cv2.resize(_vu_src["mech"], (spr.shape[1], spr.shape[0]), interpolation=cv2.INTER_AREA)
    a, sh = needle_layer((spr.shape[1], spr.shape[0]), (m2.VU_PIVOT[0] * k, m2.VU_PIVOT[1] * k), m2.VU_NEEDLE * k, m2.vu_angle(db), s(2.6))
    keep = 1 - mech
    rgb = spr[..., :3]
    rgb *= (1 - sh * keep)[..., None]
    na = (a * keep)[..., None]
    rgb[:] = rgb * (1 - na) + np.array([16, 12, 9.0]) * na
    return spr, (m2.VU_IMG_C[0] * k, m2.VU_IMG_C[1] * k)

def toggle_sprite(down):
    sys.path.insert(0, md.REPO + "/tools")
    import photo_controls as pc
    im = pc.clean_alpha(pc.load_rgba("toggle_down.png" if down else "toggle_up.png"), 4)
    ys, xs = np.where(im[..., 3] > 128)
    r = 0.5 * (xs.max() - xs.min()); cx = 0.5 * (xs.max() + xs.min())
    cy = ys.min() + r if down else ys.max() - r
    R = s(m2.TOG_R)
    out_u = (R * 2 * 2.2 / pc.AS, R * 2 * 2.2 / pc.AS)
    big = pc.place(pc.premul(im), (cx, cy), R / r, 0.0, False, out_u, (out_u[0] / 2, out_u[1] / 2))
    return pc.shadow_and_shrink(big, out_u, (R * 0.16 / pc.AS, R * 0.24 / pc.AS), R * 0.16 / pc.AS, 0.7).astype(np.float64)

def text_layer(items):
    L = Image.new("RGBA", (OUT_W, OUT_H), (0, 0, 0, 0)); d = ImageDraw.Draw(L)
    for txt, x, y, size, col, track in items:
        f = ImageFont.truetype(FONT, int(round(size)))
        widths = [d.textlength(ch, font=f) for ch in txt]
        tw = sum(widths) + track * (len(txt) - 1)
        bb = d.textbbox((0, 0), "H", font=f)
        cx, cy = x - tw / 2, y - (bb[1] + bb[3]) / 2
        for ch, w in zip(txt, widths):
            d.text((cx, cy), ch, font=f, fill=col); cx += w + track
    return np.asarray(L).astype(np.float64)

def svg_png(path, width_px, crop_vb=None, vb_w=None):
    subprocess.run(["rsvg-convert", "-w", str(int(round(width_px * 4))), path, "-o", TMP + "svg.png"], check=True)
    im = Image.open(TMP + "svg.png").convert("RGBA")
    if crop_vb:                                   # recadrage en unites du viewBox
        f = im.width / vb_w
        im = im.crop(tuple(int(round(v * f)) for v in crop_vb))
    return im

# ------------------------------------------------------------------ assemblage
def compose(state):
    img = np.zeros((OUT_H, OUT_W, 4)); img[..., 3] = 255
    img[..., :3] = AMP[..., :3] * (AMP[..., 3:4] / 255)
    img = full_panel(img)
    on = state.get("power", True)
    cold, warm = tube_layers(img)
    if not on: img[..., :3] = cold
    else:
        k = state.get("glow", 1.0)
        img[..., :3] = cold + (img[..., :3] - cold) * min(k, 1.0)
        img = bloom(img, warm, max(0.0, k - 0.6))
    # bouton Power
    spr, c = power_sprite(on); px, py = T(*m2.PWR)
    drop_shadow(img, spr, px - c[0], py - c[1], s(4), s(6), s(6), 0.65)
    md.over(img, spr, px - c[0], py - c[1])
    # cadrans
    for i, (vx, vy) in enumerate(m2.VU):
        spr, c = vu_sprite(1.0 if on else 0.0, state.get("vu", [-3.0, -5.5])[i] if on else -60.0)
        x, y = T(vx, vy); r = s(m2.VU_OUT_R)
        drop_shadow(img, spr, x - c[0], y - c[1], 0, s(1.5), s(3.5), 0.85)
        drop_shadow(img, spr, x - c[0], y - c[1], r * 0.07, r * 0.11, r * 0.09, 0.5)
        md.over(img, spr, x - c[0], y - c[1])
    # textes serigraphies
    grey = (212, 212, 212, 255)
    SEL, SEL_R = md.SEL, md.SEL_R
    ly = m2.LABEL_Y
    items = [(nm.upper(), *T(md.KNOBS[nm][0], ly), s(29), grey, s(2.5)) for nm in ("Depth", "Contour", "Volume")]
    items.append(("TUBE", *T(SEL[0], 1258), s(29), grey, s(2.5)))
    for nm, ang in zip(md.TUBE_NAMES, md.SEL_ANG):
        t = math.radians(ang); rr = SEL_R + 46
        items.append((nm, *T(SEL[0] + math.sin(t) * rr, SEL[1] - math.cos(t) * rr), s(24), grey, s(1.0)))
    for x, top, bottom in m2.TOGGLES:
        items.append((top, *T(x, m2.TOG_Y - m2.TOG_LAB_UP), s(22), grey, s(1.5)))
        items.append((bottom, *T(x, m2.TOG_Y + m2.TOG_LAB_DN), s(22), grey, s(1.5)))
    md.over(img, text_layer(items), 0, 0)
    # points de valeur, graduations du selecteur, symbole Power (sur-echantillonnes)
    S4 = 3
    L = Image.new("RGBA", (OUT_W * S4, OUT_H * S4), (0, 0, 0, 0)); d = ImageDraw.Draw(L)
    P = lambda x, y: tuple(v * S4 for v in T(x, y))
    for nm, (kx, ky) in md.KNOBS.items():
        for v in range(11):
            t = math.radians(-135 + 27 * v); rr = md.KNOB_R + 12
            cx, cy = P(kx + math.sin(t) * rr, ky - math.cos(t) * rr); rad = s(2.9) * S4
            d.ellipse([cx - rad, cy - rad, cx + rad, cy + rad], fill=grey)
    for ang in md.SEL_ANG:
        t = math.radians(ang)
        d.line([P(SEL[0] + math.sin(t) * (SEL_R + 10), SEL[1] - math.cos(t) * (SEL_R + 10)),
                P(SEL[0] + math.sin(t) * (SEL_R + 24), SEL[1] - math.cos(t) * (SEL_R + 24))], fill=grey, width=int(round(s(3) * S4)))
    cx, cy = P(m2.PWR[0], 1362); rr = s(11) * S4
    d.arc([cx - rr, cy - rr, cx + rr, cy + rr], start=-60, end=240, fill=grey, width=int(round(s(3) * S4)))
    d.line([(cx, cy - s(15) * S4), (cx, cy - s(2) * S4)], fill=grey, width=int(round(s(3) * S4)))
    md.over(img, np.asarray(L.resize((OUT_W, OUT_H), Image.LANCZOS)).astype(np.float64), 0, 0)
    # logo NINE HANDS | DSP (SVG fourni) : largeur 340 px photo, centre en hauteur a 1236
    lw = s(340) * 0.7                                                  # -30 %
    lg = svg_png(D + "logo_fullwhite.svg", lw * 3000 / 2500, (250, 387, 2750, 613), 3000)
    lg = lg.resize((int(round(lw)), int(round(lw * lg.height / lg.width))), Image.LANCZOS)
    # bas des lettres (y 575 du SVG) sur la ligne du bas de PENTODE, centre sur l'axe du pied gauche
    f = ImageFont.truetype(FONT, int(round(s(22))))
    bb = ImageDraw.Draw(Image.new("L", (8, 8))).textbbox((0, 0), "H", font=f)
    pent_bottom = T(0, m2.TOG_Y + m2.TOG_LAB_DN)[1] + (bb[3] - bb[1]) / 2
    fy = int(OUT_H - 25); xs = np.where(AMP[fy, :OUT_W // 2, 3] > 128)[0]
    foot_cx = (xs.min() + xs.max()) / 2
    kk = lg.width / 2500.0
    md.over(img, np.asarray(lg).astype(np.float64), foot_cx - lg.width / 2, pent_bottom - (575 - 387) * kk)
    # logo ANODE (SVG fourni) : capitales de 52 x 0,7 px photo (-30 %), centre en x 1960,
    # bas des capitales (y 139,5 du SVG) aligne sur le bas des lettres de PENTODE
    vb = (110.00, 33.90, 413.00, 120.20); cap_bottom = 139.5
    k = s(52.0) * 0.7 / 91.0
    f = ImageFont.truetype(FONT, int(round(s(22))))
    bb = ImageDraw.Draw(Image.new("L", (8, 8))).textbbox((0, 0), "H", font=f)
    pent_bottom = T(0, m2.TOG_Y + m2.TOG_LAB_DN)[1] + (bb[3] - bb[1]) / 2
    an = svg_png(D + "logo/anode_logo.svg", vb[2] * k).resize((int(round(vb[2] * k)), int(round(vb[3] * k))), Image.LANCZOS)
    x = T(1960, 0)[0]; md.over(img, np.asarray(an).astype(np.float64), x - vb[2] * k / 2, pent_bottom - (cap_bottom - vb[1]) * k)
    # potards (bouton fourni : corps fixe, seul le trait tourne)
    parts = md.knob_parts()
    vals = state.get("knobs", {"Depth": 5, "Contour": 5, "Volume": 6})
    for nm, (kx, ky) in md.KNOBS.items():
        spr, c = md.knob_sprite(s(md.KNOB_R), -135 + 270 * vals[nm] / 10.0, parts)
        x, y = T(kx, ky); md.over(img, spr, x - c, y - c)
    small_w = max(1.5, parts[3] * s(md.KNOB_R) / parts[1][2])
    spr, c = md.knob_sprite(s(SEL_R), md.SEL_ANG[state.get("tube", 1)], parts, line_w=small_w)
    x, y = T(*SEL); md.over(img, spr, x - c, y - c)
    # inverseurs (photos du kit, verticales)
    for (tx, top, bottom), down in zip(m2.TOGGLES, state.get("toggles", [True, True])):
        spr = toggle_sprite(down); x, y = T(tx, m2.TOG_Y)
        md.over(img, spr, x - spr.shape[1] / 2, y - spr.shape[0] / 2)
    return img

if __name__ == "__main__":
    for name, st in (("anode_on_2737x1800", dict(power=True, glow=1.0, vu=[-3.0, -5.5], tube=1, toggles=[True, True])),
                     ("anode_off_2737x1800", dict(power=False, tube=1, toggles=[False, True]))):
        im = compose(st)
        Image.fromarray(np.clip(im[..., :3], 0, 255).astype(np.uint8)).save(TMP + name + ".png")
        print(name, im.shape)
