# ANODE v2, etape 2 : bouton Power et cadrans VU d'apres les photos fournies (allume / eteint),
# aiguilles dessinees, inverseurs verticaux (photos telles quelles), inscriptions dessus / dessous,
# inverseurs alignes en colonne sous DEPTH et VOLUME.
import math
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont
import make_design as md
from make_design import H, W, D, FACE, KNOBS, KNOB_R, SEL, SEL_R, TUBE_NAMES, SEL_ANG, FONT, FONT_SB, over, text_layer

# inscriptions DEPTH / CONTOUR / VOLUME sur la ligne d'EL34 / EL84 (les potards ne bougent pas)
LABEL_Y = SEL[1] - math.cos(math.radians(SEL_ANG[2])) * (SEL_R + 46)
# potards alignes horizontalement sur le bouton TUBE (meme ligne de centre) ; les points suivent
for _k in KNOBS: KNOBS[_k] = (KNOBS[_k][0], SEL[1])

CROP = (11, 336, 2252, 1810)                     # boite englobante de l'ampli dans la photo (x0, y0, x1, y1)
VU = [(572.0, 1452.0), (920.0, 1452.0)]
VU_OUT_R = 154.0                                 # rayon exterieur de la lunette
PWR = (250.0, 1448.0); PWR_OUT_R = 57.0
TOG_R = 30.0
TOGGLES = [(KNOBS["Depth"][0], "SINGLE-ENDED", "PUSH-PULL"), (KNOBS["Volume"][0], "TRIODE", "PENTODE")]
TOG_Y = 1632.0
TOG_LAB_UP, TOG_LAB_DN = 63.0, 63.0               # inscriptions au plus pres du levier

# ------------------------------------------------------------------ photos des cadrans
VU_IMG_C = (742.0, 740.0); VU_IMG_R = 668.0      # lunette dans la photo 1500 x 1500 (contour net, cercle ajuste)
VU_PIVOT = (743.0, 935.0)                        # axe de l'aiguille : centre du capuchon rouge
VU_NEEDLE = 559.2                                # longueur (px photo) : jusqu'aux grands chiffres
# graduations mesurees sur la photo (angle depuis la verticale, au pivot)
VU_TICKS = [(-20, -37.6), (-10, -29.2), (-7, -24.4), (-6, -17.7), (-5, -11.2), (-4, -6.0), (-3, -0.8), (-2, 5.4), (-1, 10.9), (0, 15.9), (1, 22.1), (2, 32.6), (3, 43.5)]
VU_REST = -44.0                                  # butee gauche (pas de signal)

def vu_angle(db):
    if db >= VU_TICKS[-1][0]: return min(VU_TICKS[-1][1] + (db - 3) * 2.5, 48.0)
    if db <= -20:
        f, f20 = 10 ** (db / 20), 10 ** (-1.0)
        return VU_REST + (VU_TICKS[0][1] - VU_REST) * f / f20
    for (d0, a0), (d1, a1) in zip(VU_TICKS, VU_TICKS[1:]):
        if d0 <= db <= d1: return a0 + (a1 - a0) * (db - d0) / (d1 - d0)

def load_px(name):
    """ photo detouree, recoupee net sur sa lunette ronde (les coins gardaient une transparence residuelle
        qui assombrissait la facade en carre et nourrissait l'ombre) """
    img = np.asarray(Image.open(D + name).convert("RGBA")).astype(np.float64)
    a = img[..., 3]
    m = (a > 128).astype(np.uint8)
    n, lab, st, _ = cv2.connectedComponentsWithStats(m)
    m = (lab == 1 + np.argmax(st[1:, 4])).astype(np.uint8)
    ys, xs = np.where(m > 0)
    cx, cy, r = (xs.min() + xs.max()) / 2, (ys.min() + ys.max()) / 2, (xs.max() - xs.min() + ys.max() - ys.min()) / 4
    yy, xx = np.mgrid[0:a.shape[0], 0:a.shape[1]]
    disk = np.clip(r + 0.5 - np.hypot(xx - cx, yy - cy), 0, 1)
    img[..., 3] = np.minimum(a, 255 * disk)
    img[..., 3][img[..., 3] < 10] = 0
    return img

def mech_mask(vu_off):
    """ pieces du mecanisme devant l'aiguille : pont chrome, bobines, capuchon, etrier, ressorts (pas le capot noir) """
    rgb = vu_off[..., :3]; Hh, Ww = rgb.shape[:2]
    yy, xx = np.mgrid[0:Hh, 0:Ww]
    lum = rgb.mean(2)
    beige = ((rgb[..., 0] - rgb[..., 2]) > 55) & (lum > 110) & (rgb[..., 1] > 120)
    box = (yy > 772) & (np.abs(xx - VU_PIVOT[0]) < 470) & (yy < 1010)
    m = (box & (lum > 52) & ~beige).astype(np.float64)
    m = cv2.morphologyEx(m, cv2.MORPH_OPEN, np.ones((3, 3)))
    return cv2.GaussianBlur(m, (0, 0), 1.2)

def scaled(img, s):
    pm = img.copy(); pm[..., :3] *= pm[..., 3:4] / 255
    sm = cv2.resize(pm, None, fx=s, fy=s, interpolation=cv2.INTER_AREA)
    a = sm[..., 3:4] / 255
    sm[..., :3] = np.where(a > 1e-3, sm[..., :3] / np.maximum(a, 1e-3), 0)
    return sm

def needle_layer(size, pivot, length, angle, width):
    """ aiguille fine (noire, legerement effilee) + son ombre ; renvoie (alpha aiguille, alpha ombre) """
    S4 = 4
    L = Image.new("L", (size[0] * S4, size[1] * S4), 0); dl = ImageDraw.Draw(L)
    t = math.radians(angle); ux, uy = math.sin(t), -math.cos(t)
    px, py = pivot
    # effilee : large pres du pivot, fine au bout
    tip = (px + ux * length, py + uy * length)
    nx, ny = -uy, ux
    w0, w1 = width * 1.35, width * 0.55
    poly = [((px + nx * w0 / 2) * S4, (py + ny * w0 / 2) * S4), ((tip[0] + nx * w1 / 2) * S4, (tip[1] + ny * w1 / 2) * S4),
            ((tip[0] - nx * w1 / 2) * S4, (tip[1] - ny * w1 / 2) * S4), ((px - nx * w0 / 2) * S4, (py - ny * w0 / 2) * S4)]
    dl.polygon(poly, fill=255)
    a = np.asarray(L.resize(size, Image.LANCZOS)).astype(np.float64) / 255
    sh = cv2.GaussianBlur(np.roll(np.roll(a, 4, 0), 2, 1), (0, 0), 2.0) * 0.30
    return a, sh

def vu_sprite(on, db):
    """ cadran complet a l'echelle (RGBA), aiguille comprise ; on in [0,1] = lampes du cadran """
    off_img, on_img = load_px("vu_off.png"), load_px("vu_on.png")
    # l'ombre integree a la photo (anneau sombre semi-transparent, plus epais en bas) est retiree :
    # detourage net sur le cercle de la lunette, ajuste sur son bord
    yy, xx = np.mgrid[0:off_img.shape[0], 0:off_img.shape[1]]
    cut = np.clip((VU_IMG_R - np.hypot(xx - VU_IMG_C[0], yy - VU_IMG_C[1])) / 1.5 + 0.5, 0, 1) * 255
    for im_ in (off_img, on_img): im_[..., 3] = np.minimum(im_[..., 3], cut)
    s = VU_OUT_R / VU_IMG_R
    mix = off_img.copy(); mix[..., :3] = off_img[..., :3] * (1 - on) + on_img[..., :3] * on
    mech = mech_mask(off_img)
    spr = scaled(mix, s)
    mech_s = cv2.resize(mech, (spr.shape[1], spr.shape[0]), interpolation=cv2.INTER_AREA)
    piv = (VU_PIVOT[0] * s, VU_PIVOT[1] * s)
    a, sh = needle_layer((spr.shape[1], spr.shape[0]), piv, VU_NEEDLE * s, vu_angle(db), 2.6)
    keep = 1 - mech_s
    rgb = spr[..., :3]
    rgb *= (1 - sh * keep)[..., None]
    na = (a * keep)[..., None]
    rgb[:] = rgb * (1 - na) + np.array([16, 12, 9.0]) * na
    return spr, (VU_IMG_C[0] * s, VU_IMG_C[1] * s)

def power_sprite(on):
    img = load_px("power_on.png" if on else "power_off.png")
    a = img[..., 3] > 128; ys, xs = np.where(a)
    c = ((xs.min() + xs.max()) / 2, (ys.min() + ys.max()) / 2); r = (xs.max() - xs.min()) / 2
    s = PWR_OUT_R / r
    return scaled(img, s), (c[0] * s, c[1] * s)

def drop_shadow(img, spr, x, y, dx, dy, blur, amount):
    # marge autour de l'image : l'ombre decalee et floutee ne doit jamais etre coupee par le cadre (sinon : rectangle)
    pad = int(abs(dx) + abs(dy) + 4 * blur + 4)
    sh = md.shadow_of(spr, dx, dy, blur, amount, pad)
    over(img, sh, x - pad, y - pad)

# ------------------------------------------------------------------ inverseurs verticaux
def toggle_sprite(handle_down_right):
    import photo_controls as pc
    # photos telles quelles (verticales) : levier en haut = inscription du dessus, levier en bas = inscription du dessous
    im = pc.clean_alpha(pc.load_rgba("toggle_down.png" if handle_down_right else "toggle_up.png"), 4)
    a = im[..., 3] > 128
    ys, xs = np.where(a)
    r = 0.5 * (xs.max() - xs.min()); cx = 0.5 * (xs.max() + xs.min())
    cy = ys.min() + r if handle_down_right else ys.max() - r
    out_u = (TOG_R * 2 * 2.2 / pc.AS, TOG_R * 2 * 2.2 / pc.AS)
    big = pc.place(pc.premul(im), (cx, cy), TOG_R / r, 0.0, False, out_u, (out_u[0] / 2, out_u[1] / 2))
    spr = pc.shadow_and_shrink(big, out_u, (TOG_R * 0.16 / pc.AS, TOG_R * 0.24 / pc.AS), TOG_R * 0.16 / pc.AS, 0.7)
    return spr.astype(np.float64)

# ------------------------------------------------------------------ facade complete : texture jusqu'aux bords + bevels
PANEL = (13, 1163, 2250, 1757)                   # facade entiere (arrondis compris), d'apres la photo

def bevel_profiles():
    """ relief des arrondis releve sur la photo : luminance relative a la face, par colonne (cotes) et par ligne (haut, bas) """
    lum = md.amp[..., :3].mean(2)
    x0, y0, x1, y1 = PANEL
    face = np.median(lum[1250:1700, 1100:1150])
    px = np.median(lum[1250:1700, x0:x1], axis=0) / face
    py = np.median(lum[y0:y1, 1000:2100], axis=0 if False else 1) / face
    # seules les bandes des arrondis gardent leur relief ; la face (et ce qui y etait imprime) vaut 1
    xs = np.arange(x0, x1); ys = np.arange(y0, y1)
    wx = np.clip(np.maximum((150 - xs) / 20.0, (xs - 2105) / 20.0), 0, 1)
    wy = np.clip(np.maximum((1192 - ys) / 6.0, (ys - 1736) / 6.0), 0, 1)
    px = 1 + (cv2.GaussianBlur(px[None, :], (0, 0), 1.2)[0] - 1) * wx
    py = 1 + (cv2.GaussianBlur(py[:, None], (0, 0), 1.0)[:, 0] - 1) * wy
    return np.clip(px, 0, 3.2), np.clip(py, 0, 3.2)

def full_panel(img):
    x0, y0, x1, y1 = PANEL
    saved = md.FACE; md.FACE = PANEL
    tex = md.face_texture(); md.FACE = saved
    px, py = bevel_profiles()
    tex = tex * px[None, :, None] * py[:, None, None]
    al = md.amp[y0:y1, x0:x1, 3:4] / 255.0          # contour exact de la facade (coins arrondis)
    sub = img[y0:y1, x0:x1, :3]
    sub[:] = sub * (1 - al) + tex * al
    return img

# ------------------------------------------------------------------ assemblage
def compose(state):
    img = md.amp.copy()
    canvas = np.zeros_like(img); canvas[..., 3] = 255
    canvas[..., :3] = img[..., :3] * (img[..., 3:4] / 255)
    img = canvas
    img = full_panel(img)
    on = state.get("power", True)
    cold, warm = md.tube_layers(img)
    if not on:
        img[..., :3] = cold
    else:
        k = state.get("glow", 1.0)
        img[..., :3] = cold + (img[..., :3] - cold) * min(k, 1.0)
        img = md.bloom(img, warm, max(0.0, k - 0.6))
    # bouton power (+ halo sur la facade quand il est allume)
    spr, c = power_sprite(on)
    drop_shadow(img, spr, PWR[0] - c[0], PWR[1] - c[1], 4, 6, 6, 0.65)
    over(img, spr, PWR[0] - c[0], PWR[1] - c[1])
    # cadrans
    for i, (x, y) in enumerate(VU):
        spr, c = vu_sprite(1.0 if on else 0.0, state.get("vu", [-8, -10])[i] if on else -60.0)
        # occlusion de contact (serree, tout autour de la lunette) + ombre portee vers le bas a droite
        r = VU_OUT_R
        drop_shadow(img, spr, x - c[0], y - c[1], 0, 1.5, 3.5, 0.85)
        drop_shadow(img, spr, x - c[0], y - c[1], r * 0.07, r * 0.11, r * 0.09, 0.5)
        over(img, spr, x - c[0], y - c[1])
    # textes
    grey = (212, 212, 212, 255)
    items = [("DEPTH", KNOBS["Depth"][0], LABEL_Y, 29, grey, 2.5), ("CONTOUR", KNOBS["Contour"][0], LABEL_Y, 29, grey, 2.5),
             ("VOLUME", KNOBS["Volume"][0], LABEL_Y, 29, grey, 2.5), ("TUBE", SEL[0], 1258, 29, grey, 2.5),
             ]
    for nm, ang in zip(TUBE_NAMES, SEL_ANG):
        t = math.radians(ang); rr = SEL_R + 46
        items.append((nm, SEL[0] + math.sin(t) * rr, SEL[1] - math.cos(t) * rr, 24, grey, 1.0))
    for x, top, bottom in TOGGLES:
        items.append((top, x, TOG_Y - TOG_LAB_UP, 22, grey, 1.5))
        items.append((bottom, x, TOG_Y + TOG_LAB_DN, 22, grey, 1.5))
    over(img, text_layer(items), 0, 0)
    L = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(L)
    # points de valeur autour des potards : 0 a 10, tous de meme taille, cercle serre centre sur l'axe du potard
    for nm, (kx, ky) in KNOBS.items():
        for v in range(11):
            t = math.radians(-135 + 27 * v); rr = KNOB_R + 12
            dx, dy = kx + math.sin(t) * rr, ky - math.cos(t) * rr
            rad = 2.9
            d.ellipse([dx - rad, dy - rad, dx + rad, dy + rad], fill=grey)
    for ang in SEL_ANG:
        t = math.radians(ang); r0, r1 = SEL_R + 10, SEL_R + 24
        d.line([(SEL[0] + math.sin(t) * r0, SEL[1] - math.cos(t) * r0), (SEL[0] + math.sin(t) * r1, SEL[1] - math.cos(t) * r1)], fill=grey, width=3)
    cx, cy = PWR[0], 1362
    d.arc([cx - 11, cy - 11, cx + 11, cy + 11], start=-60, end=240, fill=grey, width=3)
    d.line([(cx, cy - 15), (cx, cy - 2)], fill=grey, width=3)
    over(img, np.asarray(L).astype(np.float64), 0, 0)
    # logo ANODE (logo/anode_logo.svg, Lambda a pointes vives) a la place du texte :
    # hauteur des capitales 52 px, capitales centrees sur l'ancien texte (1960, 1696)
    import subprocess
    vb = (110.00, 33.90, 413.00, 120.20); cap, cap_mid = 91.0, 94.0          # repere du SVG
    k = 52.0 / cap
    tmp = __import__("tempfile").gettempdir() + "/anode_al.png"
    subprocess.run(["rsvg-convert", "-w", str(int(round(vb[2] * k * 4))), D + "logo/anode_logo.svg", "-o", tmp], check=True)
    an = Image.open(tmp).convert("RGBA").resize((int(round(vb[2] * k)), int(round(vb[3] * k))), Image.LANCZOS)
    over(img, np.asarray(an).astype(np.float64), 1960 - vb[2] * k / 2, 1696 - (cap_mid - vb[1]) * k)
    lg = Image.open(D + "logo_full.png").crop((250, 387, 2750, 613))
    lw = 340; lg = lg.resize((lw, round(lw * lg.height / lg.width)), Image.LANCZOS)
    over(img, np.asarray(lg).astype(np.float64), 178, 1236 - lg.height / 2)
    parts = md.knob_parts()
    vals = state.get("knobs", {"Depth": 5, "Contour": 5, "Volume": 6})
    for nm, (x, y) in KNOBS.items():
        spr, c = md.knob_sprite(KNOB_R, -135 + 270 * vals[nm] / 10.0, parts)
        over(img, spr, x - c, y - c)
    small_w = max(1.5, parts[3] * KNOB_R / parts[1][2])          # meme epaisseur de trait que les petits potards
    spr, c = md.knob_sprite(SEL_R, SEL_ANG[state.get("tube", 1)], parts, line_w=small_w)
    over(img, spr, SEL[0] - c, SEL[1] - c)
    for (x, top, bottom), down in zip(TOGGLES, state.get("toggles", [True, True])):
        spr = toggle_sprite(down)
        over(img, spr, x - spr.shape[1] / 2, TOG_Y - spr.shape[0] / 2)
    # plein cadre : recadrage au ras de l'ampli (capot, haut des transformateurs, bas des pieds)
    return img[CROP[1]:CROP[3], CROP[0]:CROP[2]]

if __name__ == "__main__":
    S = __import__("tempfile").gettempdir() + "/"
    on = compose(dict(power=True, glow=1.0, vu=[-3.0, -5.5], tube=1, toggles=[True, True]))
    Image.fromarray(np.clip(on, 0, 255).astype(np.uint8)).convert("RGB").save(S + "design_on.png")
    off = compose(dict(power=False, tube=1, toggles=[False, True]))
    Image.fromarray(np.clip(off, 0, 255).astype(np.uint8)).convert("RGB").save(S + "design_off.png")
