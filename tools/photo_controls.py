# ANODE : commandes mobiles tirees des photos fournies (assets/photos)
#   toggle_strip.png   : inverseur a levier (rondelle, ecrou six pans, soufflet, levier chrome) pose a l'horizontale,
#                        3 images : levier a gauche (photo « levier en bas » tournee de 90 deg), milieu (fondu
#                        des deux, montre ~55 ms pendant la bascule), levier a droite (photo « levier en haut » tournee)
#   selector_strip.png : bouton « chicken head » rouge detoure, 10 images (4 positions + 2 intermediaires) ;
#                        la lumiere de la photo vient du haut : on part de la photo pour les positions a droite
#                        et de son miroir pour les positions a gauche (rotation de 46 deg au plus, eclairage conserve)
#   Images 2x du repere 780 x 406 (memes formats que tools/controls_art.py)
import os, math
import numpy as np, cv2
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PH = os.path.join(ROOT, "assets", "photos")
AS, SS = 2, 4                                     # 2x, sur-echantillonnage 4

def load_rgba(name):
    return np.asarray(Image.open(os.path.join(PH, name)).convert("RGBA")).astype(np.float64)

def premul(a):
    out = a.copy(); out[..., :3] *= a[..., 3:4] / 255.0; return out

def unpremul(p):
    a = p[..., 3:4] / 255.0
    rgb = np.where(a > 1e-4, p[..., :3] / np.maximum(a, 1e-4), 0)
    return np.dstack([np.clip(rgb, 0, 255), np.clip(p[..., 3], 0, 255)]).astype(np.uint8)

def place(src_pm, center, k, deg_cw, mirror, out_u, at_u):
    """ src premultipliee -> toile out_u (unites) a 2x : centre de la photo en at_u, echelle k (px 2x par px photo),
        rotation horaire deg_cw, miroir horizontal eventuel ; anti-crenelage par reduction prealable + sur-echantillonnage """
    kk = k * SS
    pre = max(1.0, 1.0 / (kk * 1.5))                 # reduction prealable (INTER_AREA) jusqu'a ~1,5x la taille finale
    src = src_pm
    cx, cy = center
    if pre > 1.0:
        h, w = src.shape[:2]
        src = cv2.resize(src, (max(1, round(w / pre)), max(1, round(h / pre))), interpolation=cv2.INTER_AREA)
        cx, cy = cx * src.shape[1] / w, cy * src.shape[0] / h
        kk = kk * w / src.shape[1]
    if mirror:
        src = src[:, ::-1].copy(); cx = src.shape[1] - cx
    W, H = round(out_u[0] * AS * SS), round(out_u[1] * AS * SS)
    M = cv2.getRotationMatrix2D((cx, cy), -deg_cw, kk)
    M[0, 2] += at_u[0] * AS * SS - cx
    M[1, 2] += at_u[1] * AS * SS - cy
    big = cv2.warpAffine(src, M, (W, H), flags=cv2.INTER_LINEAR, borderMode=cv2.BORDER_CONSTANT, borderValue=0)
    return big

def shadow_and_shrink(big, out_u, cast_u, blur_u, amount, contact=0.35):
    a = big[..., 3] / 255.0
    def shift(img, dx, dy): return cv2.warpAffine(img, np.float32([[1, 0, dx], [0, 1, dy]]), (img.shape[1], img.shape[0]))
    s = AS * SS
    sh = cv2.GaussianBlur(shift(a, cast_u[0] * s, cast_u[1] * s), (0, 0), blur_u * s) * amount
    sh += cv2.GaussianBlur(shift(a, 0.5 * s, 0.8 * s), (0, 0), 1.0 * s) * contact
    sh = np.clip(sh, 0, 0.85) * (1 - a)
    out = big.copy(); out[..., 3] = (a + sh) * 255           # ombre noire (rgb premultiplie inchange)
    W, H = round(out_u[0] * AS), round(out_u[1] * AS)
    return unpremul(cv2.resize(out, (W, H), interpolation=cv2.INTER_AREA))

def clean_alpha(rgba, erode_px):
    a = rgba[..., 3]
    m = (a > 128).astype(np.uint8)
    if erode_px > 0:
        m = cv2.erode(m, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (2 * erode_px + 1, 2 * erode_px + 1)))
    soft = cv2.GaussianBlur(m.astype(np.float64), (0, 0), max(0.8, erode_px * 0.6))
    out = rgba.copy(); out[..., 3] = np.clip(soft * 1.15, 0, 1) * 255
    return out

# ---------------------------------------------------------------------- inverseur
TWU, THU = 46.0, 28.0
TOG_R = 11.0                    # rayon de la rondelle (unites)

def toggle_frames():
    frames = []
    pms = []
    for name, rot in (("toggle_down.png", 90.0), ("toggle_up.png", 90.0)):      # rotation horaire : bas -> gauche, haut -> droite
        im = clean_alpha(load_rgba(name), 4)
        a = im[..., 3] > 128
        ys, xs = np.where(a)
        r = 0.5 * (xs.max() - xs.min())
        cx = 0.5 * (xs.max() + xs.min())
        cy = ys.min() + r if "down" in name else ys.max() - r      # la rondelle est le bord oppose au levier
        k = TOG_R * AS / r
        big = place(premul(im), (cx, cy), k, rot, False, (TWU, THU), (TWU / 2, THU / 2))
        pms.append(big)
    left, right = pms
    mid = 0.5 * (left + right)
    for big in (left, mid, right):
        frames.append(shadow_and_shrink(big, (TWU, THU), (1.4, 2.2), 1.3, 0.6))
    return frames

# ---------------------------------------------------------------------- selecteur
SW = 120.0
SEL_FRAMES = [-44.0, -34.5, -24.0, -13.5, -4.5, 4.5, 13.5, 24.0, 34.5, 44.0]
SEL_R = 22.8                    # rayon du corps (unites), comme le selecteur d'origine

def selector_source():
    rgb = np.asarray(Image.open(os.path.join(PH, "selector_chickenhead.png")).convert("RGB")).astype(np.float64)
    chroma = rgb[..., 0] - np.maximum(rgb[..., 1], rgb[..., 2])
    lum = rgb.mean(2)
    H, W = lum.shape
    # fond blanc a gris clair (degrade vertical) : tout ce qui est colore ou plus sombre que le fond
    bgl = 255.0 - 15.0 * (np.arange(H) / H)[:, None]
    m = ((chroma > 14) | (lum < bgl - 22)).astype(np.uint8)
    m = cv2.morphologyEx(m, cv2.MORPH_CLOSE, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (15, 15)))
    n, lab, st, _ = cv2.connectedComponentsWithStats(m)
    m = (lab == 1 + np.argmax(st[1:, 4])).astype(np.uint8)
    ff = m.copy(); cv2.floodFill(ff, np.zeros((H + 2, W + 2), np.uint8), (0, 0), 1); m = m | (1 - ff)
    # le trait blanc du bec touche le bord : on referme la fente, seulement au bout du bec (loin du corps)
    yy, xx = np.mgrid[0:H, 0:W]
    ys0, xs0 = np.where(m > 0)
    c_est = np.array([xs0.mean(), ys0.mean()])
    wl = (lum > 200) & (chroma < 45)
    d = np.hypot(xx - c_est[0], yy - c_est[1])
    far = d > 0.62 * d[m > 0].max()
    closed = cv2.morphologyEx(m, cv2.MORPH_CLOSE, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (41, 41)))
    m = m | (closed & far & (wl | (m > 0))).astype(np.uint8)
    m = cv2.erode(m, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5)))       # pas de liseré blanc
    a = np.clip(cv2.GaussianBlur(m.astype(np.float64), (0, 0), 1.2) * 1.1, 0, 1)
    # bord : la couleur du fond deborde -> on reprend la couleur de l'interieur proche
    inner = cv2.erode(m, np.ones((9, 9), np.uint8)).astype(np.float64)
    fill = cv2.GaussianBlur(rgb * inner[..., None], (0, 0), 4) / np.maximum(cv2.GaussianBlur(inner, (0, 0), 4), 1e-3)[..., None]
    edge = np.clip((1 - cv2.GaussianBlur(inner, (0, 0), 2.0)) * 1.4, 0, 1)[..., None]
    rgb = rgb * (1 - edge) + np.minimum(rgb, fill) * edge
    # centre du corps rond : cercle ajuste sur le contour hors du bec
    cnt, _ = cv2.findContours(m, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    pts = max(cnt, key=len)[:, 0, :].astype(np.float64)
    white = (lum > 215) & (chroma < 40) & (m > 0)
    wy, wx = np.where(white)
    # axe du bec : du centre approximatif vers le trait blanc
    c0 = np.array([pts[:, 0].mean(), pts[:, 1].mean()])
    tip = np.array([wx.mean(), wy.mean()])
    ang = math.atan2(tip[1] - c0[1], tip[0] - c0[0])
    th = np.arctan2(pts[:, 1] - c0[1], pts[:, 0] - c0[0])
    off = np.abs(np.angle(np.exp(1j * (th - ang))))
    keep = (off > math.radians(50)) & (off < math.radians(130))         # flancs du corps, loin des deux bouts du bec
    P = pts[keep]
    A = np.c_[2 * P[:, 0], 2 * P[:, 1], np.ones(len(P))]
    b = (P ** 2).sum(1)
    cx, cy, c = np.linalg.lstsq(A, b, rcond=None)[0]
    r = math.sqrt(c + cx * cx + cy * cy)
    ang = math.atan2(tip[1] - cy, tip[0] - cx)
    phi0 = math.degrees(ang) + 90.0                                      # angle horaire depuis le haut
    src = np.dstack([rgb, a * 255])
    return premul(src), (cx, cy), r, phi0

def selector_frames():
    src, (cx, cy), r, phi0 = selector_source()
    k = SEL_R * AS / r
    frames = []
    for th in SEL_FRAMES:
        if th >= 0:
            big = place(src, (cx, cy), k, th - phi0, False, (SW, SW), (SW / 2, SW / 2))
        else:
            big = place(src, (cx, cy), k, th + phi0, True, (SW, SW), (SW / 2, SW / 2))
        frames.append(shadow_and_shrink(big, (SW, SW), (4.0, 6.5), 4.2, 0.62, contact=0.45))
    print(f"selecteur : centre ({cx:.0f}, {cy:.0f}) rayon {r:.0f} px, bec a {phi0:.1f} deg")
    return frames

def save_strip(frames, name):
    Image.fromarray(np.vstack(frames), "RGBA").save(os.path.join(ROOT, name), optimize=True)

if __name__ == "__main__":
    import sys
    what = sys.argv[1:] or ["selector", "toggle"]
    if "toggle" in what:
        save_strip(toggle_frames(), "toggle_strip.png")
    if "selector" in what:
        save_strip(selector_frames(), "selector_strip.png")
    print("ok")
