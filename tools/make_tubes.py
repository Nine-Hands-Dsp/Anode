# ANODE : lampes interchangeables (6L6GC, EL34, EL84, KT88) dans le style exact du visuel de reference
#  1. fond sans lampes : transformateurs reconstruits derriere les lampes (symetrie propre a chaque transfo,
#     lumiere raccordee a la partie visible), espace entre les deux recopie
#  2. lampes du visuel (photo, inscription effacee) detourees puis retaillees aux proportions reelles :
#       toutes a la meme echelle d'apres leurs dimensions reelles (voir MM)
#       EL34  : lampe du visuel (verre droit, plaques grises)
#       6L6GC : verre un peu plus etroit, plaques gris anthracite, depot de getter argente dans le dome
#       KT88  : bouteille « coke » : col etroit puis renflement (52 mm contre 34), plaques plus larges
#       EL84  : petite noval sans culot (22 mm, ~0,66 x), queusot de pompage au sommet
#     les structures internes gardent des proportions rigides (seul le verre suit le profil)
#  3. inscription rouge propre a chaque lampe, lueur des filaments propre a chaque lampe
# Sorties (2x) : tubezone_<lampe>[_SE].jpg + glow_<lampe>[_SE].png + redplate_<lampe>[_SE].png (_SE : une seule lampe, centree) (zone x 290..512, y 0..212 du repere 780 x 406)
import os, math, sys
import numpy as np, cv2
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
S = 2
ZX0, ZY0, ZX1, ZY1 = 290, 0, 512, 212            # zone des lampes (repere d'origine)
FSERIF = "/usr/share/fonts/truetype/dejavu/DejaVuSerifCondensed.ttf"
SRC_T = [dict(cx=347.0), dict(cx=458.0)]          # lampes du visuel
HW, DOME_CY, TOP, GBOT, BASE_HW = 37.0, 43.0, 6.0, 193.0, 32.5
CHASSIS = 211.0
TG = 0.5                                          # transmission du verre (mesuree sur la photo : ~0,4-0,7 + voile)

def load_bg():
    return np.asarray(Image.open(os.path.join(ROOT, "background.jpg")).convert("RGB")).astype(np.float64)

def lum(a): return a.mean(2)

# ------------------------------------------------------------------ silhouette d'une lampe
def silhouette(shape, cx, prof, top, gbot, base_hw, has_base=True, soft=1.0):
    """ prof(y) -> demi-largeur du verre (repere d'origine) ; masque a 2x avec bord adouci """
    H, W = shape[:2]
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float64)
    y = (yy + 0.5) / S; x = (xx + 0.5) / S
    hw = np.vectorize(prof)(y[:, 0])[:, None] * np.ones((1, W))
    inside = np.clip((hw - np.abs(x - cx)) * S / soft + 0.5, 0, 1)
    inside *= np.clip((y - top) * S + 0.5, 0, 1)
    glass = inside * (y <= gbot)
    base = np.zeros_like(glass)
    if has_base:
        base = np.clip((base_hw - np.abs(x - cx)) * S / soft + 0.5, 0, 1) * (y > gbot) * (y < CHASSIS + 1)
    return np.maximum(glass, base)

def src_prof(y):
    if y < TOP: return 0.0
    if y < DOME_CY: return math.sqrt(max(0.0, HW * HW - (DOME_CY - y) ** 2))
    return HW

# ------------------------------------------------------------------ 1. fond sans lampes
def clean_background(bg):
    out = bg.copy()
    H, W = bg.shape[:2]
    # zones cachees (marge de 3 px autour du verre)
    hide = np.zeros((H, W), bool)
    for t in SRC_T:
        m = silhouette(bg.shape, t["cx"], lambda y: src_prof(y) + 3.0 if y >= TOP - 3 else 0.0, TOP - 3, GBOT, BASE_HW + 3)
        hide |= m > 0.02
    # transfo gauche [206, 380] centre 293 ; transfo droit [429, 603] centre 516 (repere d'origine)
    def mirror_fill(x0, x1, ctr, seam_cols, side, y0=40, y1=CHASSIS):
        """ side = +1 : la partie cachee est a droite de la couture ; -1 : a gauche """
        X0, X1 = int(x0 * S), int(x1 * S)
        Y0, Y1 = int(y0 * S), int(y1 * S)
        mir = bg.copy()
        for X in range(X0, X1):
            mir[:, X] = bg[:, int(round(2 * ctr * S - X))]
        sig = 7 * S
        lf_m = cv2.GaussianBlur(mir, (0, 0), sig)
        lf_b = cv2.GaussianBlur(bg, (0, 0), sig)
        sc0, sc1 = int(seam_cols[0] * S), int(seam_cols[1] * S)
        seam_lf = lf_b[:, sc0:sc1].mean(1)                 # lumiere reelle a la couture, par ligne
        seam_lm = lf_m[:, sc0:sc1].mean(1) if False else np.stack([lf_m[:, int(round(2 * ctr * S - X))] for X in range(sc0, sc1)], 1).mean(1)
        for X in range(X0, X1):
            dist = abs(X - (X0 if side > 0 else X1)) / S         # distance a la couture (repere d'origine)
            k = np.clip(dist / 30.0, 0, 1); k = k * k * (3 - 2 * k)
            # la basse frequence part de la lumiere reelle et rejoint progressivement celle du miroir (attenuee)
            lf = seam_lf * (1 - k) + (lf_m[:, X] * 0.55 + seam_lf * 0.45) * k
            hf = mir[:, X] - lf_m[:, X]
            out[Y0:Y1, X] = (lf + hf)[Y0:Y1]
        # fondu sur 10 px cote visible pour effacer toute couture
        F = int(10 * S)
        for i in range(F):
            X = X0 - 1 - i if side > 0 else X1 + i
            w = 1 - (i + 1) / (F + 1)
            Xm = int(round(2 * ctr * S - X))
            lfx = seam_lf
            cont = (lfx + (bg[:, Xm] - lf_m[:, Xm] if False else bg[:, X] - lf_b[:, X]))
            out[Y0:Y1, X] = (bg[:, X] * (1 - w) + cont * w)[Y0:Y1]
        blend_cols.append((X0 - F, X0) if side > 0 else (X1, X1 + F))
    blend_cols = []
    mirror_fill(306, 381, 293.0, (298, 305), +1)       # transfo gauche : partie droite reconstruite
    mirror_fill(427, 500, 516.5, (500, 507), -1)       # transfo droit : partie gauche reconstruite
    # bords exterieurs des transfos -> espace sombre entre eux (recopie de l'espace visible)
    gap = bg[:, int(392 * S):int(412 * S)]
    gapcol = np.median(gap, axis=1)
    for X in list(range(int(381 * S), int(392 * S))) + list(range(int(412 * S), int(427 * S))):
        out[:, X] = gapcol
    # au-dessus des transfos (fond noir derriere les domes)
    # au-dessus des transfos : fond noir (ligne par ligne, mediane de l'espace visible entre les lampes)
    Ytop = int(44 * S)
    for Y in range(Ytop):
        row = bg[Y, int(392 * S):int(412 * S)]
        out[Y, int(300 * S):int(510 * S)] = np.median(row, axis=0)
    # transition douce vers le haut des transfos
    # ne garder la reconstruction que la ou les lampes cachaient le fond (fondu de 3 px)
    h = cv2.GaussianBlur(hide.astype(np.float64), (0, 0), 1.5 * S)
    h = np.clip(h * 1.6, 0, 1)
    for (a, b) in blend_cols:                               # bandes de raccord (deja fondues dans out)
        h[int(40 * S):int(CHASSIS * S), a:b] = 1.0
    clean = bg * (1 - h[..., None]) + out * h[..., None]
    # bande du chassis (y >= 211) toujours prise du visuel
    clean[int(CHASSIS * S):] = bg[int(CHASSIS * S):]
    rng = np.random.default_rng(4)
    clean += (rng.normal(0, 1.4, clean.shape[:2]) * h)[..., None]
    return clean

# ------------------------------------------------------------------ 2. lampes retaillees
# dimensions reelles (corps de la lampe pose sur son support, sans les broches ; diametre du verre), en mm
#   KT88  110 x 52 (JJ / Genalex)   EL34 100 x 34 (JJ 98 x 33, RS 104 x 36,5)
#   6L6GC  97 x 38 (JJ 100, TAD 95, Svetlana 93 ; verre 38)   EL84 70 x 22 (queusot compris, sans broches)
# une seule echelle pour les quatre : la KT88, la plus haute, tient tout juste dans la fenetre
MM = {"KT88": (110.0, 52.0), "EL34": (100.0, 34.0), "6L6": (97.0, 38.0), "EL84": (70.0, 22.0)}
K = (CHASSIS - 3.0) / MM["KT88"][0]                 # px par mm (repere d'origine)
G = K / 2.09                                       # echelle relative a la photo (EL34 de la photo : 2,09 px/mm)
GB = CHASSIS - (CHASSIS - GBOT) * G                # bas du verre des octales (culot a la meme echelle)
BHW = BASE_HW * G

def _oct(name, **kw):
    h, d = MM[name]
    return dict(top=CHASSIS - h * K, gbot=GB, hw=0.5 * d * K, base=True, base_hw=BHW, **kw)

TYPES = {
    "EL34": _oct("EL34", kint=0.5 * MM["EL34"][1] * K / HW),
    "6L6":  _oct("6L6", kint=0.97 * G, plates="dark", getter=True),
    "KT88": _oct("KT88", kint=1.24 * G, kt88=True),
}
TYPES["KT88"]["base_hw"] = BHW * 1.08
_tip = 5.0
TYPES["EL84"] = dict(top=210.0 - MM["EL84"][0] * K + _tip, gbot=210.0, scale=0.5 * MM["EL84"][1] * K / HW, base=False, tip=True)
for _n, _p in TYPES.items():
    _p["kw"] = _p.get("kint", _p.get("scale"))          # echelle horizontale de l'interieur

def ty(p, ysrc):
    """ hauteur source (photo) -> hauteur de la lampe retaillee """
    return p["top"] + (ysrc - TOP) * (p["gbot"] - p["top"]) / (GBOT - TOP)

def kt88_prof(y, top, gbot, hw_max):
    # verre « bouteille » : col (bas) etroit, renflement, dome large
    dome_cy = top + hw_max * 0.92
    if y < top: return 0.0
    if y < dome_cy:
        r = hw_max; dy = (dome_cy - y) / 0.92
        return math.sqrt(max(0.0, r * r - dy * dy))
    t = (gbot - y) / (gbot - dome_cy)                   # 0 en bas du verre, 1 en haut du corps
    neck, bulge = hw_max * 0.743, hw_max
    if t < 0.16: return neck
    if t < 0.46:
        u = (t - 0.16) / 0.30; u = u * u * (3 - 2 * u)
        return neck + (bulge - neck) * u
    return bulge

def warp_tube(bg, si, name, cx):
    """ renvoie (rgb, alpha) de la lampe retaillee, centree en cx, tiree de la lampe source si """
    p = TYPES[name]
    scx = SRC_T[si]["cx"]
    H, W = bg.shape[:2]
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float64)
    y = (yy + 0.5) / S; x = (xx + 0.5) / S
    top, gbot = p["top"], p["gbot"]
    if name == "EL84":
        s = p["scale"]
        # tout le verre source (6..193) ramene sur (top..210) ; largeur a l'echelle s
        ys = TOP + (y - top) * (GBOT - TOP) / (gbot - top)
        xs = scx + (x - cx) / s
        prof = lambda yv: src_prof(TOP + (yv - top) * (GBOT - TOP) / (gbot - top)) * s
        alpha = silhouette(bg.shape, cx, prof, top, gbot, 0, has_base=False)
        # bas du verre arrondi (pas de culot : la noval sort directement du support)
        r = 6.0 * s / 0.66
        corner = np.clip((r - np.hypot(np.maximum(np.abs(x - cx) - (HW * s - r), 0), np.maximum(y - (gbot - r), 0))) * S + 0.5, 0, 1)
        alpha = alpha * corner
    else:
        if p.get("kt88"):
            hwm = p["hw"]
            prof = lambda yv: kt88_prof(yv, top, gbot, hwm)
        else:
            hw = p["hw"]; dcy = top + hw
            prof = lambda yv: 0.0 if yv < top else (math.sqrt(max(0.0, hw * hw - (dcy - yv) ** 2)) if yv < dcy else hw)
        # vertical : verre et culot chacun ramenes lineairement sur leur hauteur reelle
        ys = np.where(y < gbot, TOP + (y - top) * (GBOT - TOP) / (gbot - top),
                      GBOT + (y - gbot) * (CHASSIS - GBOT) / (CHASSIS - gbot))
        # horizontal : interieur rigide (kint), marge de verre etiree pour rejoindre le profil
        hwT = np.vectorize(prof)(y[:, 0])[:, None] * np.ones((1, W))
        hwS = np.vectorize(src_prof)(ys[:, 0])[:, None] * np.ones((1, W))
        hwS = np.where(y >= gbot, BASE_HW, hwS); hwT = np.where(y >= gbot, p["base_hw"], hwT)
        core = 31.0
        k = np.where(y >= gbot, p["base_hw"] / BASE_HW, p["kint"])
        d = np.abs(x - cx)
        coreS = np.clip(np.minimum(core, hwS - 3.0), 0.5, None)
        coreT = np.clip(np.minimum(coreS * k, hwT - 3.0), 0.5, None)
        inner = d <= coreT
        ds = np.where (inner, d * coreS / coreT,
                       coreS + (d - coreT) * (hwS - coreS) / np.maximum (hwT - coreT, 1e-3))
        xs = scx + np.sign(x - cx) * ds
        alpha = silhouette(bg.shape, cx, prof, top, gbot, p["base_hw"])
    mapx = (xs * S - 0.5).astype(np.float32); mapy = (ys * S - 0.5).astype(np.float32)
    return (mapx, mapy), alpha, prof, top, gbot

def remap(img, maps):
    return cv2.remap(img.astype(np.float32), maps[0], maps[1], cv2.INTER_CUBIC, borderMode=cv2.BORDER_REPLICATE).astype(np.float64)

def transmission(shape, cx):
    """ verre = TG ; structure interne opaque (plaques, boite superieure, micas, culot) = 0 """
    H, W = shape[:2]
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float64)
    y = (yy + 0.5) / S; x = (xx + 0.5) / S
    d = np.abs(x - cx)
    op = ((d < 31.0) & (y > 76) & (y < 171))                 # plaques et ailettes laterales
    op |= ((d < 12.5) & (y > 49) & (y < 77))                 # boite / grille superieure
    op |= ((d < 25.0) & (y > 49.5) & (y < 52.5))             # mica superieur
    op |= ((d < 29.0) & (y > 170) & (y < 193.5))             # mica inferieur, tiges, filaments
    op |= (y >= GBOT - 1)                                    # culot
    op = cv2.GaussianBlur(op.astype(np.float64), (0, 0), 1.0 * S)
    return TG * (1 - op)

def plate_mask(shape, cx, name, top, gbot):
    """ zone des plaques (pour teinter les plaques de la 6L6) """
    H, W = shape[:2]
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float64)
    y = (yy + 0.5) / S; x = (xx + 0.5) / S
    p = TYPES[name]
    m = (np.abs(x - cx) < 26.0 * p["kw"]) & (y > ty(p, 77)) & (y < ty(p, 172))
    return cv2.GaussianBlur(m.astype(np.float64), (0, 0), 1.2 * S)

def label_layer(shape, cx, text, cy, length, size):
    """ inscription rouge sur les plaques, lisible de bas en haut (comme le visuel) """
    H, W = shape[:2]
    SS = 4
    f = ImageFont.truetype(FSERIF, int(size * S * SS))
    t = Image.new("RGBA", (int(f.getlength(text)) + 40, int(size * S * SS * 1.6)), (0, 0, 0, 0))
    ImageDraw.Draw(t).text((20, 0), text, font=f, fill=(150, 30, 24, 246))
    t = t.crop(t.getbbox())
    sc = length * S * SS / t.width
    t = t.resize((max(1, int(t.width * sc)), max(1, int(t.height * min(sc, 1.0)))), Image.LANCZOS)
    t = t.rotate(90, expand=True, resample=Image.BICUBIC)
    t = t.resize((max(1, t.width // SS), max(1, t.height // SS)), Image.LANCZOS).filter(ImageFilter.GaussianBlur(0.45))
    lay = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    lay.alpha_composite(t, (int(cx * S - t.width / 2), int(cy * S - t.height / 2)))
    return np.asarray(lay).astype(np.float64)

def getter_flash(shape, cx, top, hw):
    """ depot de getter argente dans le haut du dome (6L6GC) : voile metallique, bord inferieur flou et irregulier """
    H, W = shape[:2]
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float64)
    y = (yy + 0.5) / S; x = (xx + 0.5) / S
    rng = np.random.default_rng(11)
    wob = cv2.GaussianBlur(rng.normal(0, 1, (1, W)), (0, 0), 6 * S).ravel()
    wob = wob / (np.abs(wob).max() + 1e-9) * 2.5
    lower = top + hw * 0.78 + wob[None, :]
    m = np.clip((lower - y) / 7.0, 0, 1) * np.clip((y - top - 1.0) / 3.0, 0, 1)
    shade = 0.45 + 0.55 * np.clip(1 - np.abs((x - cx + hw * 0.32) / (hw * 0.38)), 0, 1) ** 2
    return m, shade

def exhaust_tip(rgb, alpha, cx, top):
    """ queusot de pompage de la noval : petit bouton de verre transparent au sommet (bord clair, coeur transparent) """
    H, W = alpha.shape
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float64)
    y = (yy + 0.5) / S; x = (xx + 0.5) / S
    hwid, hgt = 3.2, 5.0
    # silhouette : petit cylindre arrondi pose sur le dome
    inside = np.clip((hwid - np.abs(x - cx)) * S + 0.5, 0, 1) * np.clip((y - (top - hgt)) * S + 0.5, 0, 1) * (y < top + 2.0)
    capd = np.hypot((x - cx) / hwid, (y - (top - hgt + hwid * 0.6)) / (hwid * 0.6))
    inside = np.where(y < top - hgt + hwid * 0.6, np.clip((1 - capd) * hwid * S + 0.5, 0, 1), inside)
    # verre : transmission du fond + filets clairs sur les bords + reflet a gauche
    edge = np.clip(1 - np.abs(np.abs(x - cx) - (hwid - 0.6)) / 0.7, 0, 1)
    hl = np.clip(1 - np.abs(x - (cx - 1.2)) / 0.6, 0, 1) * np.clip((top - y) / hgt, 0, 1)
    glass = rgb * 0.75 + 40.0 * edge[..., None] + 110.0 * hl[..., None]
    glass = np.minimum(glass, 255)
    rgb = rgb * (1 - inside[..., None]) + glass * inside[..., None]
    return rgb, np.maximum(alpha, inside)

# montages : push-pull = la paire du visuel ; single-ended = une seule lampe, centree entre les transfos
LAYOUTS = {"PP": [(0, SRC_T[0]["cx"]), (1, SRC_T[1]["cx"])], "SE": [(0, 0.5 * (SRC_T[0]["cx"] + SRC_T[1]["cx"]))]}

def build_zone(bg, clean, name, layout="PP"):
    comp = clean.copy()
    p = TYPES[name]
    for si, cx in LAYOUTS[layout]:
        maps, alpha, prof, top, gbot = warp_tube(bg, si, name, cx)
        # source = T * fond + R (reflets, structure) -> on deforme R et T, puis on met le vrai fond derriere
        Tsrc = transmission(bg.shape, SRC_T[si]["cx"])
        R = bg - Tsrc[..., None] * clean
        # sur le corps du verre (fond = transfos), les reflets d'un cylindre sont des bandes verticales :
        # profil par colonne (mediane sur la hauteur), jamais negatif -> aucun bout de transfo transporte
        H, W = bg.shape[:2]
        Y0, Y1 = int(60 * S), int(168 * S)
        prof_r = np.clip(np.median(R[Y0:Y1], axis=0), 0, None) * 0.9
        yy = (np.arange(H) + 0.5) / S
        wy = np.clip((yy - 33.0) / 6.0, 0, 1) * np.clip((GBOT - 2 - yy) / 4.0, 0, 1)
        g = (Tsrc / TG) * wy[:, None]
        R = R * (1 - g[..., None]) + prof_r[None, :, :] * g[..., None]
        blur = {"KT88": 2.6, "EL84": 1.0}.get(name, 1.4)
        seen = cv2.GaussianBlur(clean, (0, 0), blur * S)
        # Fresnel : vers les bords du cylindre le verre reflechit plus et laisse moins passer le fond
        Hh, Ww = bg.shape[:2]
        yy0 = (np.arange(Hh) + 0.5) / S; xx0 = (np.arange(Ww) + 0.5) / S
        hwT = np.array([max(prof(v), 1e-3) for v in yy0])[:, None]
        q = np.clip(np.abs(xx0[None, :] - cx) / hwT, 0, 1)
        fres = 1.0 - 0.75 * q ** 5
        rgb = remap(R, maps) + (remap(Tsrc, maps) * fres)[..., None] * seen
        if p.get("plates") == "dark":
            pm = plate_mask(bg.shape, cx, name, top, gbot)
            g = lum(rgb)
            dark = rgb * 0.42 + np.dstack([g, g, g]) * 0.06
            rgb = rgb * (1 - pm[..., None]) + dark * pm[..., None]
        if p.get("getter"):
            m, shade = getter_flash(bg.shape, cx, top, p["hw"])
            silver = np.dstack([shade * 168, shade * 170, shade * 176]) * 0.5 + rgb * 0.3
            rgb = rgb * (1 - 0.75 * m[..., None]) + silver * 0.75 * m[..., None]
        if p.get("tip"):
            rgb, alpha = exhaust_tip(rgb, alpha, cx, top)
        # inscription (meme place et memes proportions que sur la photo, a l'echelle de la lampe)
        v = (gbot - top) / (GBOT - TOP); kw = p["kw"]
        text, ln, sz = {"EL84": ("EL84", 57.0, 24.5), "KT88": ("KT88", 58.0, 23.0),
                        "6L6": ("6L6GC", 62.0, 22.0), "EL34": ("EL34", 57.0, 24.5)}[name]
        lab = label_layer(bg.shape, cx - 3.0 * kw, text, ty(p, 121.0), ln * v, sz * kw)
        la = lab[..., 3:4] / 255.0
        # l'encre suit la lumiere des plaques
        shade = np.clip(lum(rgb) / 150.0, 0.55, 1.15)[..., None]
        rgb = rgb * (1 - la) + lab[..., :3] * shade * la
        comp = comp * (1 - alpha[..., None]) + rgb * alpha[..., None]
    return comp

def glow_from(zone):
    reg = zone
    warm = np.clip((reg[..., 0] - reg[..., 2] - 25) / 90.0, 0, 1) * np.clip((reg[..., 0] - 60) / 120.0, 0, 1)
    halo = cv2.GaussianBlur(warm, (0, 0), 9 * S) * 0.9
    a = np.clip(warm * 0.85 + halo, 0, 1)
    rgb = np.dstack([np.full_like(a, 255), np.full_like(a, 150), np.full_like(a, 62)])
    return Image.fromarray(np.dstack([rgb, a * 255]).astype(np.uint8), "RGBA")

# courant de chauffage des filaments (fiches techniques, 6,3 V) : eclat au repos relatif a l'EL34 de la photo
HEATER_A = {"6L6": 0.9, "EL34": 1.5, "EL84": 0.76, "KT88": 1.6}

def warm_mask(zone):
    return np.clip((zone[..., 0] - zone[..., 2] - 25) / 90.0, 0, 1) * np.clip((zone[..., 0] - 60) / 120.0, 0, 1)

def heater_level(zone, name):
    """ filaments plus ou moins vifs selon leur courant (puissance ~ I^2, eclat visuel ~ racine) """
    f = (HEATER_A[name] / HEATER_A["EL34"]) ** 0.75
    w = cv2.GaussianBlur(warm_mask(zone), (0, 0), 0.8 * S)
    out = zone * (1 + (f - 1) * w[..., None])
    # un filament plus faible tire aussi vers l'orange sombre
    if f < 1:
        out[..., 2] = out[..., 2] * (1 - (1 - f) * 0.5 * w)
    return np.clip(out, 0, 255)

def plate_rect(name, cx):
    """ zone des plaques dans le repere d'origine (x0, x1, y0, y1) """
    p = TYPES[name]
    hw = (29.0 if name == "6L6" else 30.0) * p["kw"]
    return cx - hw, cx + hw, ty(p, 78), ty(p, 168)

def redplate_layer(shape, name, layout="PP"):
    """ plaques qui rougissent quand on pousse longtemps : lueur rouge sombre, au centre-haut des plaques, marbree """
    H, W = shape[:2]
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float64)
    y = (yy + 0.5) / S; x = (xx + 0.5) / S
    a = np.zeros((H, W))
    rng = np.random.default_rng(21)
    mott = cv2.GaussianBlur(rng.normal(0, 1, (H, W)), (0, 0), 3.0 * S)
    mott = 0.75 + 0.25 * mott / (np.abs(mott).max() + 1e-9)
    for _, tcx in LAYOUTS[layout]:
        x0, x1, y0, y1 = plate_rect(name, tcx)
        cx = 0.5 * (x0 + x1) - 2.0; hw = 0.5 * (x1 - x0); cy = y0 + 0.38 * (y1 - y0); hh = 0.5 * (y1 - y0)
        inside = np.clip((hw - 1.5 - np.abs(x - cx)) * S / 2, 0, 1) * np.clip((y - y0) * S / 2, 0, 1) * np.clip((y1 - y) * S / 2, 0, 1)
        blob = np.exp(-(((x - cx) / (hw * 0.55)) ** 2 + ((y - cy) / (hh * 0.55)) ** 2))
        a = np.maximum(a, inside * blob * mott)
    a = np.clip(a, 0, 1) * 0.85
    rgb = np.dstack([np.full_like(a, 255), np.full_like(a, 62), np.full_like(a, 22)])
    return Image.fromarray(np.dstack([rgb, a * 255]).astype(np.uint8), "RGBA")

def main(outdir):
    bg = load_bg()
    clean = clean_background(bg)
    os.makedirs(outdir, exist_ok=True)
    for lay, suf in (("PP", ""), ("SE", "_SE")):
        for name in ("6L6", "EL34", "EL84", "KT88"):
            comp = heater_level(build_zone(bg, clean, name, lay), name)
            full = np.clip(comp, 0, 255).astype(np.uint8)
            z = Image.fromarray(full).crop((ZX0 * S, ZY0 * S, ZX1 * S, ZY1 * S))
            z.save(os.path.join(outdir, f"tubezone_{name}{suf}.jpg"), quality=93, subsampling=0)
            glow_from(np.asarray(z).astype(np.float64)).save(os.path.join(outdir, f"glow_{name}{suf}.png"), optimize=True)
            redplate_layer(bg.shape, name, lay).crop((ZX0 * S, ZY0 * S, ZX1 * S, ZY1 * S)).save(os.path.join(outdir, f"redplate_{name}{suf}.png"), optimize=True)
    print("ok")

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else ROOT)
