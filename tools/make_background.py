# ANODE : fond 2x (1560 x 812) reconstruit d'apres le visuel de reference (780 x 406)
#  - photo agrandie (Lanczos) pour les lampes, transformateurs, bois
#  - facade grise et panneau noir nettoyes (commandes et inscriptions effacees, teinte basse frequence
#    reconstruite par inpainting, grain brosse recree a 2x), puis inscriptions redessinees nettes
#  - lampes : inscription « EL34 » effacee du verre ; calques d'inscription par modele (6L6, EL34, EL84, KT88)
#  - calque de lueur (filaments) pour l'animation
import os, math
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "design", "reference.png")
S = 2                       # facteur de sortie
SS = 4                      # sur-echantillonnage du dessin vectoriel (par rapport a la sortie)
FS = "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf"
FSERIF = "/usr/share/fonts/truetype/dejavu/DejaVuSerifCondensed.ttf"
FINTER = "/usr/share/fonts/opentype/inter/Inter-SemiBold.otf"

# ---------------------------------------------------------------- positions (repere 780 x 406)
KNOBS = [(375.5, 314.0), (475.5, 314.0), (575.0, 314.0)]
KNOB_NAMES = ["Depth", "Contour", "Volume"]
SEL = (243.5, 317.0)
SEL_ANGLES = [-44.0, -13.5, 13.5, 44.0]          # 6L6, EL34, EL84, KT88 (degres, 0 = vers le haut)
TOG = [(401.0, 243.0), (572.0, 243.0)]
PLATE = (150, 210, 655, 391)                     # facade grise (x0, y0, x1, y1)
PANEL = (331, 258, 621, 390)                     # panneau noir
KNOB_SPAN = 135.0                                # 0 = -135 deg, 10 = +135 deg

def load():
    return np.asarray(Image.open(SRC).convert("RGB")).astype(np.float64)

def up2(img):
    return cv2.resize(img, (img.shape[1] * S, img.shape[0] * S), interpolation=cv2.INTER_LANCZOS4)

def smooth_fill(img, mask, rad=6):
    """ remplit la zone masquee par la teinte basse frequence des alentours (inpainting + flou) """
    u8 = np.clip(img, 0, 255).astype(np.uint8)
    out = cv2.inpaint(u8, mask.astype(np.uint8) * 255, rad, cv2.INPAINT_TELEA).astype(np.float64)
    return out

def brushed(h, w, seed, amp, length):
    rng = np.random.default_rng(seed)
    n = rng.normal(0, 1, (h, w))
    k = np.ones((1, length)) / length
    n = cv2.filter2D(n, -1, k, borderType=cv2.BORDER_REFLECT)
    n = n / (n.std() + 1e-9)
    fine = rng.normal(0, 1, (h, w)) * 0.35
    return (n + fine) * amp

def text_layer(size, items):
    """ items : (texte, police, taille_px_orig, centre_xy_orig, couleur RGBA, etirement_x, interlettrage_px_orig, rotation) """
    W, H = size[0] * S * SS, size[1] * S * SS
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    for (txt, font, px, (cx, cy), col, sx, track, rot) in items:
        f = ImageFont.truetype(font, int(round(px * S * SS)))
        # rendu lettre par lettre (interlettrage)
        glyphs = []
        x = 0.0
        for ch in txt:
            w = f.getlength(ch)
            glyphs.append((ch, x)); x += w + track * S * SS
        tw = x - track * S * SS
        asc, desc = f.getmetrics()
        tmp = Image.new("RGBA", (int(tw) + 8, asc + desc + 8), (0, 0, 0, 0))
        d = ImageDraw.Draw(tmp)
        for ch, gx in glyphs:
            d.text((gx + 4, 4), ch, font=f, fill=col)
        # centre vertical sur la hauteur des capitales (les jambages restent visibles en dessous)
        cap = f.getbbox("H")
        capH = cap[3] - cap[1]
        tmp = tmp.crop((0, 4 + cap[1] - capH, tmp.width, 4 + cap[3] + capH))
        if sx < 0:                                   # largeur visee (px repere d'origine)
            tmp = tmp.resize((int(-sx * S * SS), tmp.height), Image.LANCZOS)
        elif sx != 1.0:
            tmp = tmp.resize((int(tmp.width * sx), tmp.height), Image.LANCZOS)
        if rot:
            tmp = tmp.rotate(rot, expand=True, resample=Image.BICUBIC)
        layer.alpha_composite(tmp, (int(cx * S * SS - tmp.width / 2), int(cy * S * SS - tmp.height / 2)))
    return layer

def downsample(layer, size):
    return layer.resize((size[0], size[1]), Image.LANCZOS)

def over(base, layer_rgba):
    a = layer_rgba[..., 3:4] / 255.0
    return base * (1 - a) + layer_rgba[..., :3] * a

def disc(shape, cx, cy, r, feather=1.0):
    yy, xx = np.mgrid[0:shape[0], 0:shape[1]].astype(np.float64)
    d = np.hypot(xx - cx, yy - cy)
    return np.clip((r - d) / feather + 0.5, 0, 1)

def box(shape, x0, y0, x1, y1):
    m = np.zeros(shape[:2], bool); m[int(y0):int(y1), int(x0):int(x1)] = True; return m

def clean_plate(src):
    H, W = src.shape[:2]
    m = np.zeros((H, W), bool)
    for b in [(212, 233, 276, 249), (178, 256, 314, 302), (210, 262, 296, 372), (178, 364, 312, 383),
              (326, 231, 468, 257), (518, 231, 636, 257)]:
        m |= box(src.shape, *b)
    screws = [(163.0, 242.0), (163.0, 373.0), (642.0, 241.0), (642.0, 371.0)]
    for (x, y) in screws: m |= disc(src.shape, x + 1, y + 1, 9.5) > 0.5
    m &= box(src.shape, 151, 212, 654, 389)
    m |= box(src.shape, 329, 256, 623, 391)          # le panneau noir ne doit pas deborder dans l'inpainting
    filled = smooth_fill(src, m, 7)
    # adoucit toute la facade (le grain d'origine est remplace par un brossage net a 2x)
    plate_area = box(src.shape, 151, 212, 654, 389) & ~box(src.shape, 329, 256, 623, 391)
    soft = cv2.GaussianBlur(filled, (0, 0), 2.2)
    out = np.where(plate_area[..., None], soft, src)
    return out, plate_area, screws

def clean_panel(src):
    """ panneau noir : teinte reconstruite par un ajustement quadratique sur le fond nu (sans taches) """
    inner = box(src.shape, 334, 261, 619, 388)
    L = src.mean(2)
    feat = (L > 40)
    for (x, y) in KNOBS: feat |= (disc(src.shape, x + 4, y + 4, 44) > 0.5)
    feat = cv2.dilate(feat.astype(np.uint8), np.ones((5, 5), np.uint8)) > 0
    ok = inner & ~feat
    yy, xx = np.mgrid[0:src.shape[0], 0:src.shape[1]].astype(np.float64)
    X = (xx - 476) / 150.0; Y = (yy - 324) / 70.0
    A = np.stack([np.ones_like(X), X, Y, X * X, X * Y, Y * Y], -1)
    out = src.copy()
    for c in range(3):
        coef, *_ = np.linalg.lstsq(A[ok], src[..., c][ok], rcond=None)
        fit = A @ coef
        out[..., c] = np.where(inner, fit, src[..., c])
    return out, inner

def build():
    src = load()
    H0, W0 = src.shape[:2]
    plate_src, plate_area, screws = clean_plate(src)
    both, panel_inner = clean_panel(plate_src)

    big = up2(src)                               # photo agrandie (lampes, bois, vis, bords)
    clean = up2(both)
    Hb, Wb = big.shape[:2]
    # grain brosse (horizontal) sur la facade, grain tres fin sur le panneau noir
    plate_big = cv2.resize(plate_area.astype(np.float64), (Wb, Hb), interpolation=cv2.INTER_LINEAR)
    panel_big = cv2.resize(panel_inner.astype(np.float64), (Wb, Hb), interpolation=cv2.INTER_LINEAR)
    clean += (brushed(Hb, Wb, 3, 1.25, 110) * plate_big)[..., None]
    clean += (brushed(Hb, Wb, 5, 0.9, 25) * panel_big)[..., None]
    # vis d'origine reposees (photo agrandie), bord adouci
    for (x, y) in screws:
        a = disc((Hb, Wb), (x + 0.8) * S, (y + 0.8) * S, 8.0 * S, 2.0 * S)[..., None]
        clean = clean * (1 - a) + big * a
    out = clean

    # ---------------- inscriptions nettes
    dark = (52, 52, 52, 255)
    items = [("Tube Model", FS, 9.9, (243.0, 241.0), dark, -46.0, 0.1, 0),
             ("6L6",  FS, 8.8, (188.0, 275.5), dark, -15.0, 0.1, 0),
             ("EL34", FS, 8.8, (220.0, 264.5), dark, -17.0, 0.1, 0),
             ("EL84", FS, 8.8, (269.0, 264.5), dark, -17.0, 0.1, 0),
             ("KT88", FS, 8.8, (295.0, 275.5), dark, -17.0, 0.1, 0),
             ("Single-Ended", FS, 8.6, (356.0, 242.5), dark, -43.0, 0.0, 0),
             ("Push-Pull",    FS, 8.6, (439.5, 242.5), dark, -31.0, 0.0, 0),
             ("Triode",       FS, 8.6, (538.5, 242.5), dark, -21.0, 0.0, 0),
             ("Pentode",      FS, 8.6, (610.5, 242.5), dark, -32.0, 0.0, 0)]
    lay = text_layer((W0, H0), items)
    d = ImageDraw.Draw(lay)
    k = S * SS
    # points et graduations du selecteur
    cx, cy = SEL
    for a, rd in zip(SEL_ANGLES, (55.5, 52.5, 52.5, 55.5)):
        r = math.radians(a)
        px, py = cx + rd * math.sin(r), cy - rd * math.cos(r)
        d.ellipse([(px - 1.4) * k, (py - 1.4) * k, (px + 1.4) * k, (py + 1.4) * k], fill=dark)
    line = (62, 62, 62, 230)
    for a, r1 in zip(SEL_ANGLES, (49.0, 46.0, 46.0, 49.0)):
        r = math.radians(a)
        d.line([((cx + r1 * math.sin(r)) * k, (cy - r1 * math.cos(r)) * k),
                ((cx + 30.5 * math.sin(r)) * k, (cy - 30.5 * math.cos(r)) * k)], fill=line, width=int(0.9 * k))
    rr = 30.5
    d.arc([(cx - rr) * k, (cy - rr) * k, (cx + rr) * k, (cy + rr) * k], start=-90 + SEL_ANGLES[0], end=-90 + SEL_ANGLES[3],
          fill=line, width=int(0.9 * k))
    lay = np.asarray(downsample(lay, (Wb, Hb))).astype(np.float64)
    out = over(out, lay)

    # « POWER AMPLIFIER » grave : leger relief (lumiere en haut)
    pa = [("POWER AMPLIFIER", FS, 9.4, (245.0, 373.5), (74, 74, 74, 255), -116.0, 0.9, 0)]
    lo = np.asarray(downsample(text_layer((W0, H0), [(t, f, p, (c[0], c[1] + 0.6), (238, 238, 238, 150), sx, tr, ro) for (t, f, p, c, _, sx, tr, ro) in pa]), (Wb, Hb))).astype(np.float64)
    out = over(out, lo)
    lt = np.asarray(downsample(text_layer((W0, H0), pa), (Wb, Hb))).astype(np.float64)
    out = over(out, lt)

    # panneau noir : graduations, chiffres et noms
    grey = (150, 150, 150, 255)
    items = []
    for (x, y), name in zip(KNOBS, KNOB_NAMES):
        items += [("5", FS, 8.0, (x, y - 43.0), (104, 104, 104, 255), 1.0, 0, 0),
                  ("0", FS, 8.0, (x - 21.5, y + 37.5), (104, 104, 104, 255), 1.0, 0, 0),
                  ("10", FS, 8.0, (x + 21.0, y + 37.5), (104, 104, 104, 255), 1.0, 0, 0),
                  (name, FS, 10.0, (x, y + 59.0), (108, 108, 108, 255), 1.0, 0.1, 0)]
    lay = text_layer((W0, H0), items)
    d = ImageDraw.Draw(lay)
    for (x, y) in KNOBS:
        rr = 30.0
        d.ellipse([(x - rr) * k, (y - rr) * k, (x + rr) * k, (y + rr) * k], outline=(120, 120, 120, 150), width=int(0.8 * k))
        for i in range(11):
            a = math.radians(-KNOB_SPAN + i * 2 * KNOB_SPAN / 10)
            r0, r1 = 31.5, (37.0 if i == 5 else 35.0)
            d.line([((x + r0 * math.sin(a)) * k, (y - r0 * math.cos(a)) * k), ((x + r1 * math.sin(a)) * k, (y - r1 * math.cos(a)) * k)],
                   fill=(150, 150, 150, 255), width=int((1.15 if i == 5 else 0.85) * k))
    lay = np.asarray(downsample(lay, (Wb, Hb))).astype(np.float64)
    out = over(out, lay)

    # marque (fond noir, en haut a gauche)
    br = text_layer((W0, H0), [("NINE HANDS  |  DSP", FINTER, 10.5, (78.0, 21.0), (214, 214, 214, 255), 1.0, 1.3, 0)])
    out = over(out, np.asarray(downsample(br, (Wb, Hb))).astype(np.float64))

    # ---------------- lampes : inscription d'origine effacee
    red = (big[..., 0] - big[..., 1] > 22) & (big[..., 0] > 70)
    lbl = np.zeros((Hb, Wb), bool)
    for (x0, x1) in ((330, 358), (442, 468)):
        lbl[86 * S:156 * S, x0 * S:x1 * S] = True
    mask = cv2.dilate((red & lbl).astype(np.uint8), np.ones((5, 5), np.uint8)) > 0
    u8 = np.clip(out, 0, 255).astype(np.uint8)
    fill = cv2.inpaint(u8, mask.astype(np.uint8) * 255, 6, cv2.INPAINT_TELEA).astype(np.float64)
    rng = np.random.default_rng(9)
    fill += (rng.normal(0, 3.2, (Hb, Wb)) * mask)[..., None]
    out = np.where(mask[..., None], fill, out)

    out = np.clip(out, 0, 255).astype(np.uint8)
    Image.fromarray(out).save(os.path.join(ROOT, "background.jpg"), quality=93, subsampling=0)
    return out

def tube_labels():
    """ calques RGBA (zone des lampes) avec l'inscription de chaque modele, lisible de bas en haut """
    x0, y0, x1, y1 = 320, 80, 480, 160                 # zone (repere 780 x 406)
    names = ["6L6", "EL34", "EL84", "KT88"]
    for n in names:
        W, H = (x1 - x0) * S * SS, (y1 - y0) * S * SS
        lay = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        f = ImageFont.truetype(FSERIF, int(24.5 * S * SS))
        for cx in (343.0, 455.0):
            t = Image.new("RGBA", (int(f.getlength(n)) + 20, int(30 * S * SS)), (0, 0, 0, 0))
            ImageDraw.Draw(t).text((10, 0), n, font=f, fill=(150, 30, 24, 246))
            t = t.crop(t.getbbox())
            # largeur visee : hauteur de l'inscription d'origine (57 px pour « EL34 »)
            target = 57.0 * S * SS * (len(n) / 4.0 if n != "6L6" else 0.78)
            sc = target / t.width
            t = t.resize((int(t.width * sc), int(t.height * min(sc, 1.0))), Image.LANCZOS)
            t = t.rotate(90, expand=True, resample=Image.BICUBIC)
            lay.alpha_composite(t, (int((cx - x0) * S * SS - t.width / 2), int((121.5 - y0) * S * SS - t.height / 2)))
        lay = lay.resize(((x1 - x0) * S, (y1 - y0) * S), Image.LANCZOS).filter(ImageFilter.GaussianBlur(0.45))
        lay.save(os.path.join(ROOT, f"label_{n}.png"))

def glow_layer():
    src = up2(load())
    x0, y0, x1, y1 = 290 * S, 0, 510 * S, 215 * S
    reg = src[y0:y1, x0:x1]
    warm = np.clip((reg[..., 0] - reg[..., 2] - 25) / 90.0, 0, 1) * np.clip((reg[..., 0] - 60) / 120.0, 0, 1)
    halo = cv2.GaussianBlur(warm, (0, 0), 9 * S) * 0.9
    a = np.clip(warm * 0.85 + halo, 0, 1)
    rgb = np.dstack([np.full_like(a, 255), np.full_like(a, 150), np.full_like(a, 62)])
    Image.fromarray(np.dstack([rgb, a * 255]).astype(np.uint8), "RGBA").save(os.path.join(ROOT, "glow.png"))

if __name__ == "__main__":
    build()
    tube_labels()
    glow_layer()
    print("ok")
