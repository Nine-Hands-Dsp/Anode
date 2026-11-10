# ANODE v2 : maquette d'apres la photo de l'ampli (fond transparent), texture brossee et bouton fournis.
# Coordonnees = pixels de la photo (2261 x 2261) ; le plugin travaille en 1/2 (unites), images a 2x = pixels photo.
import sys, math
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont, ImageFilter

import os as _os
REPO = _os.path.dirname(_os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))))
D = _os.path.join(REPO, "design", "v2") + "/"
Y0, Y1 = 150, 1830                     # recadrage vertical (marge en haut pour la barre de presets)
FONT = "/usr/share/fonts/opentype/inter/Inter-Medium.otf"
FONT_SB = "/usr/share/fonts/opentype/inter/Inter-SemiBold.otf"

def load(name): return np.asarray(Image.open(D + name).convert("RGBA")).astype(np.float64)

amp = load("amp.png")
H, W = amp.shape[:2]

# ------------------------------------------------------------------ geometrie
VU = [(572.0, 1452.0), (921.0, 1452.0)]       # centres des cadrans
VU_KEEP = 158.0                                # rayon garde (lunette + ombre)
PWR = (250.0, 1448.0); PWR_R = 49.0
FACE = (98, 1178, 2168, 1744)                  # face avant (sans les arrondis du capot)
KNOBS = {"Depth": (1240.0, 1440.0), "Contour": (1460.0, 1440.0), "Volume": (1680.0, 1440.0)}
KNOB_R = 66.0
SEL = (1960.0, 1452.0); SEL_R = 112.0
TUBE_NAMES = ["6L6", "EL34", "EL84", "KT88"]
SEL_ANG = [-48.0, -16.0, 16.0, 48.0]
TOG_Y = 1638.0
TOGGLES = [(1222.0, "SINGLE-ENDED", "PUSH-PULL"), (1690.0, "TRIODE", "PENTODE")]
TOG_R = 31.0
LABEL_Y = 1330.0

def disk(cx, cy, r, feather):
    yy, xx = np.mgrid[0:H, 0:W]
    return np.clip((r - np.hypot(xx - cx, yy - cy)) / feather + 0.5, 0, 1)

def rrect(x0, y0, x1, y1, r, feather):
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float64)
    dx = np.maximum(np.maximum(x0 + r - xx, xx - (x1 - r)), 0)
    dy = np.maximum(np.maximum(y0 + r - yy, yy - (y1 - r)), 0)
    d = np.hypot(dx, dy) - r
    inside = np.minimum.reduce([xx - x0, x1 - xx, yy - y0, y1 - yy])
    sd = np.where((dx > 0) & (dy > 0), -d, inside)
    return np.clip(sd / feather + 0.5, 0, 1)

def over(dst, src_rgba, x, y):
    """ compose src (RGBA float, non premultiplie) sur dst a (x, y) """
    h, w = src_rgba.shape[:2]
    x0, y0 = int(round(x)), int(round(y))
    xa, ya = max(0, x0), max(0, y0); xb, yb = min(dst.shape[1], x0 + w), min(dst.shape[0], y0 + h)
    s = src_rgba[ya - y0:yb - y0, xa - x0:xb - x0]
    a = s[..., 3:4] / 255.0
    d = dst[ya:yb, xa:xb]
    d[..., :3] = d[..., :3] * (1 - a) + s[..., :3] * a
    d[..., 3:4] = 255 - (255 - d[..., 3:4]) * (1 - a)

def shadow_of(rgba, dx, dy, blur, amount, pad):
    a = np.pad(rgba[..., 3] / 255.0, pad)
    s = cv2.GaussianBlur(np.roll(np.roll(a, int(dy), 0), int(dx), 1), (0, 0), blur) * amount
    out = np.zeros(a.shape + (4,)); out[..., 3] = np.clip(s, 0, 1) * 255
    return out

# ------------------------------------------------------------------ face avant : texture brossee fournie
def face_texture():
    tex = np.asarray(Image.open(D + "texture.jpg").convert("RGB")).astype(np.float64)
    fx0, fy0, fx1, fy1 = FACE
    fw, fh = fx1 - fx0, fy1 - fy0
    t = cv2.resize(tex, (fw, fw), interpolation=cv2.INTER_AREA)
    c0 = int(fw * 0.30)                         # bande de la texture (reflet vertical au milieu)
    t = t[c0:c0 + fh]
    # niveau : on garde le reflet de la texture mais plus discret, noir comme la face d'origine
    lum = t.mean(2, keepdims=True)
    base = cv2.GaussianBlur(lum, (0, 0), 40)[..., None] if lum.ndim == 2 else cv2.GaussianBlur(lum[..., 0], (0, 0), 40)[..., None]
    grain = lum - base
    t = 16.0 + (base - 16.0) * 0.62 + grain * 1.15
    t = np.repeat(t, 3, axis=2) * np.array([1.0, 1.0, 1.03])
    return np.clip(t, 0, 255)

def build_panel(img):
    fx0, fy0, fx1, fy1 = FACE
    m = rrect(fx0, fy0, fx1, fy1, 26, 7)
    for c in VU: m = m * (1 - disk(c[0], c[1], VU_KEEP, 5))
    tex = face_texture()
    full = img[..., :3].copy()
    full[fy0:fy1, fx0:fx1] = tex
    # ombre douce de la lunette sur la nouvelle face (les lunettes gardent la leur a l'interieur du disque)
    sh = np.zeros((H, W))
    for c in VU: sh = np.maximum(sh, disk(c[0] + 6, c[1] + 10, VU_KEEP + 4, 18))
    full *= (1 - 0.45 * sh * m)[..., None]
    img[..., :3] = img[..., :3] * (1 - m[..., None]) + full * m[..., None]
    return img

# ------------------------------------------------------------------ texte serigraphie
def text_layer(items):
    """ items : (texte, x centre, y centre, taille, couleur, tracking) -> calque RGBA pleine taille """
    L = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(L)
    for txt, x, y, size, col, track, *rest in items:
        f = ImageFont.truetype(rest[0] if rest else FONT, int(size))
        widths = [d.textlength(ch, font=f) for ch in txt]
        tw = sum(widths) + track * (len(txt) - 1)
        asc, desc = f.getmetrics()
        bb = d.textbbox((0, 0), "H", font=f)
        cx = x - tw / 2
        cy = y - (bb[1] + bb[3]) / 2
        for ch, w in zip(txt, widths):
            d.text((cx, cy), ch, font=f, fill=col); cx += w + track
    return np.asarray(L).astype(np.float64)

# ------------------------------------------------------------------ bouton fourni (corps fixe + trait)
def knob_parts():
    k = load("knob.png")
    a = k[..., 3]
    ys, xs = np.where(a > 128)
    cx, cy = (xs.min() + xs.max()) / 2.0, (ys.min() + ys.max()) / 2.0
    r = (xs.max() - xs.min()) / 2.0
    rgb = k[..., :3]
    lum = rgb.mean(2)
    # trait blanc : on l'efface (inpaint) pour garder le corps fixe (le brossage conique ne tourne pas)
    line = ((lum > 120) & (np.abs(np.arange(k.shape[1])[None, :] - cx) < 22) & (np.arange(k.shape[0])[:, None] < cy - 100)).astype(np.uint8)
    line = cv2.dilate(line, np.ones((7, 7), np.uint8))
    # le trait et sa rainure sont remplaces par le brossage voisin (corps tourne de +/-14 deg, moyenne) :
    # le brossage conique est continu en angle, la retouche ne se voit pas
    line = cv2.dilate(line, np.ones((5, 5), np.uint8))
    rots = [cv2.warpAffine(rgb, cv2.getRotationMatrix2D((cx, cy), ang, 1.0), (k.shape[1], k.shape[0]), flags=cv2.INTER_LINEAR) for ang in (14, -14)]
    patch = 0.5 * (rots[0] + rots[1])
    wm = cv2.GaussianBlur(line.astype(np.float64), (0, 0), 2.0)[..., None]
    body = rgb * (1 - wm) + patch * wm
    ly, lx = np.where(line > 0)
    seg = (ly.min() - cy, ly.max() - cy)          # trait : distance au centre (negatif = vers le haut)
    lw = lx.max() - lx.min() - 6
    return np.dstack([body, a]), (cx, cy, r), seg, lw

def knob_sprite(radius, angle_deg, parts, line_w=None):
    body, (cx, cy, r), seg, lw = parts
    k = radius / r
    pad = int(radius * 0.5)
    size = int(2 * radius + 2 * pad)
    pm = body.copy(); pm[..., :3] *= pm[..., 3:4] / 255
    sm = cv2.resize(pm, None, fx=k, fy=k, interpolation=cv2.INTER_AREA)
    M = np.float32([[1, 0, size / 2 - cx * k], [0, 1, size / 2 - cy * k]])
    big = cv2.warpAffine(sm, M, (size, size), flags=cv2.INTER_LINEAR)
    # trait blanc tourne
    S4 = 4
    L = Image.new("L", (size * S4, size * S4), 0); dl = ImageDraw.Draw(L)
    t = math.radians(angle_deg)
    ux, uy = math.sin(t), -math.cos(t)
    r0, r1 = -seg[1] * k, -seg[0] * k
    w = line_w if line_w is not None else max(1.5, lw * k)
    p0 = ((size / 2 + ux * r0) * S4, (size / 2 + uy * r0) * S4); p1 = ((size / 2 + ux * r1) * S4, (size / 2 + uy * r1) * S4)
    dl.line([p0, p1], fill=255, width=int(w * S4))
    for p in (p0, p1): dl.ellipse([p[0] - w * S4 / 2, p[1] - w * S4 / 2, p[0] + w * S4 / 2, p[1] + w * S4 / 2], fill=255)
    la = np.asarray(L.resize((size, size), Image.LANCZOS)).astype(np.float64) / 255 * 0.95
    col = np.array([238, 238, 236.0])
    big[..., :3] = big[..., :3] * (1 - la[..., None]) + col * la[..., None] * (big[..., 3:4] / 255)
    a = big[..., 3:4] / 255
    rgb = np.where(a > 1e-3, big[..., :3] / np.maximum(a, 1e-3), 0)
    spr = np.dstack([rgb, big[..., 3]])
    sh = shadow_of(spr, radius * 0.10, radius * 0.16, radius * 0.14, 0.75, 0)
    out = sh.copy();
    a2 = spr[..., 3:4] / 255
    out[..., :3] = spr[..., :3] * a2; out[..., 3:4] = spr[..., 3:4] + sh[..., 3:4] * (1 - a2)
    out[..., :3] = np.where(out[..., 3:4] > 0, out[..., :3] / np.maximum(out[..., 3:4] / 255, 1e-3), 0)
    return out, size / 2

# ------------------------------------------------------------------ inverseurs (photos du kit)
sys.path.insert(0, _os.path.join(REPO, "tools"))
def toggle_sprite(pos_right):
    import photo_controls as pc
    im = pc.clean_alpha(pc.load_rgba("toggle_up.png" if pos_right else "toggle_down.png"), 4)
    a = im[..., 3] > 128
    ys, xs = np.where(a)
    r = 0.5 * (xs.max() - xs.min()); cx = 0.5 * (xs.max() + xs.min())
    cy = ys.min() + r if not pos_right else ys.max() - r
    out_u = (TOG_R * 2 * 1.9 / pc.AS, TOG_R * 2 * 1.9 / pc.AS)
    big = pc.place(pc.premul(im), (cx, cy), TOG_R / r, 90.0, False, out_u, (out_u[0] / 2, out_u[1] / 2))
    spr = pc.shadow_and_shrink(big, out_u, (TOG_R * 0.16 / pc.AS, TOG_R * 0.24 / pc.AS), TOG_R * 0.16 / pc.AS, 0.7)
    return spr.astype(np.float64)

# ------------------------------------------------------------------ cadrans VU
def vu_geometry():
    # pivot cache sous le mecanisme ; echelle -20 VU a -40 deg, 0 VU a ~ +14 deg, +3 a +43 deg (mesure sur la photo)
    return dict(pivot=PIVOTS, length=134.0)

PIVOTS = [(570.0, 1499.0), (919.0, 1499.0)]          # centre du capuchon rouge (axe de l'aiguille)
OLD_NEEDLES = [((480.5, 1400.0), (546.0, 1480.0)), ((825.0, 1398.0), (896.0, 1476.0))]   # aiguilles de la photo

def erase_needles(img):
    # aiguilles de la photo : du pivot vers -20 VU ; masque fin + inpaint sur le cadran
    g = vu_geometry()
    rgb = np.clip(img[..., :3], 0, 255).astype(np.uint8)
    for (px, py), c in zip(g["pivot"], VU):
        m = np.zeros((H, W), np.uint8)
        (ax, ay), (bx, by) = OLD_NEEDLES[VU.index(c)]
        L = math.hypot(bx - ax, by - ay); ux, uy = (bx - ax) / L, (by - ay) / L
        p0 = (int(round(ax - ux * 5)), int(round(ay - uy * 5))); p1 = (int(round(bx)), int(round(by)))
        cv2.line(m, p0, p1, 255, 6)
        rgb = cv2.inpaint(rgb, m, 5, cv2.INPAINT_TELEA)
    img[..., :3] = rgb
    return img

def vu_angle(db):
    v = 10 ** (db / 20.0)
    return -40.0 + (v - 0.1) / (1.413 - 0.1) * 83.0

def mechanism_mask(img, c):
    rgb = img[..., :3]
    yy, xx = np.mgrid[0:H, 0:W]
    face = (rgb[..., 0] > 150) & (rgb[..., 1] > 95) & (rgb[..., 2] < 140)
    px, py = PIVOTS[VU.index(c)]
    # seul le pivot dore et le pont chrome cachent l'aiguille (elle passe devant le capot noir)
    region = (np.hypot(xx - c[0], yy - c[1]) < 140) & ((yy > py - 19) | (np.hypot(xx - px, yy - (py - 24)) < 21))
    m = (region & ~face).astype(np.float64)
    m = cv2.GaussianBlur(m, (0, 0), 1.0)
    return m

def draw_needle(img, idx, db, mech):
    g = vu_geometry()
    px, py = g["pivot"][idx]
    t = math.radians(vu_angle(db))
    ux, uy = math.sin(t), -math.cos(t)
    S4 = 4
    L = Image.new("L", (W // 4 * S4, H // 4 * S4), 0)       # zone locale suffisante
    ox, oy = px - 250, py - 250
    L = Image.new("L", (500 * S4, 500 * S4), 0); dl = ImageDraw.Draw(L)
    p0 = ((px - ox) * S4, (py - oy) * S4)
    p1 = ((px + ux * g["length"] - ox) * S4, (py + uy * g["length"] - oy) * S4)
    dl.line([p0, p1], fill=255, width=int(2.6 * S4))
    la = np.asarray(L.resize((500, 500), Image.LANCZOS)).astype(np.float64) / 255
    # ombre tres legere (lumiere derriere le cadran, un peu en dessous)
    sh = cv2.GaussianBlur(np.roll(np.roll(la, 3, 0), 2, 1), (0, 0), 1.6) * 0.28
    y0i, x0i = int(oy), int(ox)
    sub = img[y0i:y0i + 500, x0i:x0i + 500]
    keep = 1 - mech[y0i:y0i + 500, x0i:x0i + 500]
    sub[..., :3] *= (1 - sh * keep)[..., None]
    a = (la * keep)[..., None]
    sub[..., :3] = sub[..., :3] * (1 - a) + np.array([18, 14, 10.0]) * a

def vu_unlit(img):
    """ lampes eteintes : cadran creme terne, pas de rayonnement """
    out = img.copy()
    for c in VU:
        m = disk(c[0], c[1], 135, 4)
        rgb = out[..., :3]
        lum = rgb.mean(2, keepdims=True)
        cold = (rgb * 0.35 + lum * 0.65) * np.array([0.62, 0.58, 0.52])
        out[..., :3] = rgb * (1 - m[..., None]) + cold * m[..., None]
    return out

def vu_lamp_boost(img, k):
    """ eclairage des lampes du cadran : chaud, plus fort en bas au centre (ampoules derriere le capot) """
    for c in VU:
        yy, xx = np.mgrid[0:H, 0:W]
        d = np.hypot(xx - c[0], (yy - (c[1] + 40)) * 1.3)
        lamp = np.clip(1 - d / 150, 0, 1) ** 1.5 * disk(c[0], c[1], 132, 4)
        img[..., :3] = img[..., :3] * (1 + k * 0.22 * lamp)[..., None] + (np.array([40, 22, 0.0]) * k * lamp[..., None])
    return img

# ------------------------------------------------------------------ lampes : froides / chaudes
TUBE_BOX = [(345, 640, 535, 1170), (775, 470, 985, 1170), (1280, 470, 1495, 1170), (1720, 640, 1915, 1170)]
def tube_layers(img):
    rgb = img[..., :3]
    warm = np.clip((rgb[..., 0] - rgb[..., 2] - 22) / 110.0, 0, 1)
    region = np.zeros((H, W))
    for x0, y0, x1, y1 in TUBE_BOX: region[y0:y1, x0:x1] = 1
    region = cv2.GaussianBlur(region, (0, 0), 6)
    w = (warm * region)[..., None]
    lum = rgb.mean(2, keepdims=True)
    cold = rgb * (1 - w) + (lum * 0.32 * np.array([1.0, 0.92, 0.85])) * w
    return cold, w[..., 0]

def bloom(img, warm, k):
    glow = cv2.GaussianBlur(img[..., :3] * warm[..., None], (0, 0), 28)
    img[..., :3] = img[..., :3] + glow * 0.55 * k
    return img

# ------------------------------------------------------------------ voyant power
def power_led(img, on):
    c = PWR
    yy, xx = np.mgrid[0:H, 0:W]
    d = np.hypot(xx - c[0], yy - c[1])
    ring = np.clip(1 - np.abs(d - 41) / 9, 0, 1)
    rgb = img[..., :3]
    if on:
        halo = np.exp(-((d - 41) / 22.0) ** 2) * (d > 30)
        rgb += np.array([255, 110, 20.0]) * 0.38 * halo[..., None] * (1 - ring[..., None])
    else:
        # anneau eteint : le diffuseur orange devient un anneau de plastique fume, sans lueur
        zone = (d < PWR_R + 12).astype(np.float64)
        warm = np.clip((rgb[..., 0] - rgb[..., 2] - 15) / 110.0, 0, 1) * zone
        lum = rgb.mean(2, keepdims=True)
        dark = 10 + lum * 0.10 * np.array([1.0, 0.9, 0.85]) + 22 * ring[..., None] * np.array([1.0, 0.75, 0.6])
        rgb[:] = rgb * (1 - warm[..., None]) + dark * warm[..., None]
    return img

def power_symbol(items):
    pass

# ------------------------------------------------------------------ assemblage
def compose(state):
    img = amp.copy()
    # fond : noir, l'ampli par-dessus
    canvas = np.zeros_like(img); canvas[..., 3] = 255
    a = img[..., 3:4] / 255
    canvas[..., :3] = img[..., :3] * a
    img = canvas
    # face : on garde la zone d'anneau du bouton power de la photo
    pwr_keep = disk(PWR[0], PWR[1], PWR_R + 9, 3)
    saved = img.copy()
    img = build_panel(img)
    img[..., :3] = img[..., :3] * (1 - pwr_keep[..., None]) + saved[..., :3] * pwr_keep[..., None]
    img = erase_needles(img)
    cold, warm = tube_layers(img)
    on = state.get("power", True)
    if not on:
        img[..., :3] = cold
        img = vu_unlit(img)
    else:
        k = state.get("glow", 1.0)
        img[..., :3] = cold + (img[..., :3] - cold) * min(k, 1.0)
        img = bloom(img, warm, max(0.0, k - 0.6))
        img = vu_lamp_boost(img, 1.0)
    img = power_led(img, on)
    # aiguilles
    for i in range(2):
        mech = mechanism_mask(saved, VU[i])
        draw_needle(img, i, state.get("vu", [-8, -10])[i] if on else -40.0, mech)
        # le mecanisme (capot, pont chrome) repasse par-dessus l'aiguille
        img[..., :3] = img[..., :3] * (1 - mech[..., None]) + (saved[..., :3] if on else vu_unlit(saved)[..., :3]) * mech[..., None]
    # textes
    grey = (212, 212, 212, 255)
    items = [("DEPTH", KNOBS["Depth"][0], LABEL_Y, 29, grey, 2.5), ("CONTOUR", KNOBS["Contour"][0], LABEL_Y, 29, grey, 2.5),
             ("VOLUME", KNOBS["Volume"][0], LABEL_Y, 29, grey, 2.5), ("TUBE", SEL[0], 1258, 29, grey, 2.5),
             ("ANODE", 1960, 1700, 46, (232, 232, 232, 255), 7.0, FONT_SB)]
    for nm, ang in zip(TUBE_NAMES, SEL_ANG):
        t = math.radians(ang); rr = SEL_R + 46
        items.append((nm, SEL[0] + math.sin(t) * rr, SEL[1] - math.cos(t) * rr, 24, grey, 1.0))
    for x, l, r in TOGGLES:
        f = ImageFont.truetype(FONT, 23)
        dd = ImageDraw.Draw(Image.new("L", (10, 10)))
        wl = dd.textlength(l, font=f) + 1.5 * (len(l) - 1); wr = dd.textlength(r, font=f) + 1.5 * (len(r) - 1)
        items.append((l, x - TOG_R * 1.9 - 14 - wl / 2, TOG_Y, 23, grey, 1.5))
        items.append((r, x + TOG_R * 1.9 + 14 + wr / 2, TOG_Y, 23, grey, 1.5))
    T = text_layer(items)
    over(img, T, 0, 0)
    # graduations du selecteur (petits traits)
    L = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(L)
    for ang in SEL_ANG:
        t = math.radians(ang)
        r0, r1 = SEL_R + 10, SEL_R + 24
        d.line([(SEL[0] + math.sin(t) * r0, SEL[1] - math.cos(t) * r0), (SEL[0] + math.sin(t) * r1, SEL[1] - math.cos(t) * r1)], fill=grey, width=3)
    # symbole power
    cx, cy = PWR[0], 1378
    d.arc([cx - 11, cy - 11, cx + 11, cy + 11], start=-60, end=240, fill=grey, width=3)
    d.line([(cx, cy - 15), (cx, cy - 2)], fill=grey, width=3)
    over(img, np.asarray(L).astype(np.float64), 0, 0)
    # logo
    lg = Image.open(D + "logo_full.png").crop((250, 387, 2750, 613))
    lw = 340; lg = lg.resize((lw, round(lw * lg.height / lg.width)), Image.LANCZOS)
    over(img, np.asarray(lg).astype(np.float64), 178, 1236 - lg.height / 2)
    # boutons
    parts = knob_parts()
    vals = state.get("knobs", {"Depth": 5, "Contour": 5, "Volume": 6})
    for nm, (x, y) in KNOBS.items():
        ang = -135 + 270 * vals[nm] / 10.0
        spr, c = knob_sprite(KNOB_R, ang, parts)
        over(img, spr, x - c, y - c)
    spr, c = knob_sprite(SEL_R, SEL_ANG[state.get("tube", 1)], parts)
    over(img, spr, SEL[0] - c, SEL[1] - c)
    # inverseurs
    for (x, l, r), right in zip(TOGGLES, state.get("toggles", [True, True])):
        spr = toggle_sprite(right)
        over(img, spr, x - spr.shape[1] / 2, TOG_Y - spr.shape[0] / 2)
    return img[Y0:Y1]

if __name__ == "__main__":
    S = __import__("tempfile").gettempdir() + "/"
    on = compose(dict(power=True, glow=1.0, vu=[-4.0, -6.5], tube=1, toggles=[True, True]))
    Image.fromarray(np.clip(on, 0, 255).astype(np.uint8)).convert("RGB").save(S + "design_on.png")
    off = compose(dict(power=False, tube=1))
    Image.fromarray(np.clip(off, 0, 255).astype(np.uint8)).convert("RGB").save(S + "design_off.png")
