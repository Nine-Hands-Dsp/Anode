# ANODE : rendu physique (champ de hauteurs + eclairage de studio) des commandes mobiles
#   Une seule lumiere (en haut, legerement a gauche, comme le visuel de reference) ; chaque image est
#   rendue separement avec cette lumiere (jamais de miroir ni de rotation d'image).
#   Sorties (images 2x, repere 780 x 406 du visuel) :
#     knob_strip.png     : potard a jupe cannelee noire + chapeau alu repousse, 32 images (rotation 0..45 deg :
#                          8 cannelures -> le motif se repete tous les 45 deg ; le repere blanc est dessine par le plugin)
#   (selector_strip.png et toggle_strip.png viennent maintenant des photos : tools/photo_controls.py ;
#    les fonctions selector() et toggle() ci-dessous ne servent plus que sur demande explicite)
import os, math
import numpy as np, cv2
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AS, SSAA = 2, 3
PX = AS * SSAA

L = np.array([-0.28, -0.72, 0.64]); L /= np.linalg.norm(L)
V = np.array([0.0, 0.0, 1.0])
Hh = (L + V) / np.linalg.norm(L + V)

class Canvas:
    def __init__(self, w_u, h_u):
        self.W, self.H = round(w_u * PX), round(h_u * PX)
        yy, xx = np.mgrid[0:self.H, 0:self.W].astype(np.float64)
        self.x = (xx + 0.5) / PX - w_u / 2
        self.y = (yy + 0.5) / PX - h_u / 2
        self.h = np.zeros((self.H, self.W))
        self.mat = np.zeros((self.H, self.W), np.int32)

    def put(self, hgt, mask, mat_id):
        m = mask & (hgt > self.h - 1e-9)
        self.h[m] = hgt[m]; self.mat[m] = mat_id
        return m

def normals(hmap):
    gy, gx = np.gradient(hmap, 1.0 / PX)
    n = np.dstack([-gx, -gy, np.ones_like(hmap)])
    return n / np.linalg.norm(n, axis=2, keepdims=True)

def soft_shadow(c, samples=6, spread=0.09, max_u=30.0):
    hmap = c.h
    vis = np.zeros_like(hmap)
    rng = np.random.default_rng(1)
    step = 1.0 / PX * 1.5
    nsteps = int(max_u / step)
    for s in range(samples):
        d = L + rng.normal(0, spread, 3) * np.array([1, 1, 0.5]); d /= np.linalg.norm(d)
        horiz = math.hypot(d[0], d[1]); tanE = d[2] / max(horiz, 1e-6)
        dx, dy = d[0] / horiz, d[1] / horiz
        occl = np.zeros_like(hmap, bool)
        hmax = hmap.max()
        for k in range(1, nsteps):
            t = k * step
            if t * tanE > hmax + 0.1: break
            sx, sy = int(round(dx * t * PX)), int(round(dy * t * PX))
            sh = np.full_like(hmap, -1.0)
            ys0, ys1 = max(0, -sy), min(c.H, c.H - sy); xs0, xs1 = max(0, -sx), min(c.W, c.W - sx)
            if ys1 <= ys0 or xs1 <= xs0: break
            sh[ys0:ys1, xs0:xs1] = hmap[ys0 + sy:ys1 + sy, xs0 + sx:xs1 + sx]
            occl |= sh > hmap + t * tanE + 0.05
        vis += ~occl
    return vis / samples

def reflect(n):
    nz = n[..., 2]
    return np.dstack([2 * nz * n[..., 0], 2 * nz * n[..., 1], 2 * nz * nz - 1])

def env_studio(n):
    """ studio : grande boite a lumiere au-dessus (legerement a gauche), murs sombres, sol noir """
    r = reflect(n)
    el = np.degrees(np.arcsin(np.clip(r[..., 2], -1, 1)))
    az = np.degrees(np.arctan2(r[..., 1], r[..., 0]))       # -90 = haut de l'image
    daz = np.abs(((az + 100.0) + 180.0) % 360.0 - 180.0)
    side = np.cos(np.radians(daz))
    sm = lambda v, a, b: np.clip((v - a) / max(b - a, 1e-6), 0, 1)
    base = 18 + 22 * np.clip(side, 0, 1)
    col = base + (70 + 40 * side - base) * sm (el, 10, 40)
    box = 255 * sm (-daz, -55, -30) * sm (el, 18, 28) * (1 - sm (el, 70, 80))
    col = np.maximum(col, box)
    return col

def finish(c, rgb, out_size_u, cast=(3.5, 5.5), blur=3.8, amount=0.62, contact=0.45):
    obj = (c.mat > 0).astype(np.float64)
    obj = cv2.GaussianBlur(obj, (0, 0), 0.4 * SSAA)
    # ombre portee : silhouette ponderee par la hauteur, decalee (lumiere en haut a gauche), adoucie
    hn = np.clip(c.h / max(c.h.max(), 1e-6), 0, 1)
    sil = obj * (0.55 + 0.45 * hn)
    def shift(img, dx, dy): return cv2.warpAffine(img, np.float32([[1, 0, dx], [0, 1, dy]]), (c.W, c.H))
    sh = cv2.GaussianBlur(shift(sil, cast[0] * PX, cast[1] * PX), (0, 0), blur * PX) * amount
    sh += cv2.GaussianBlur(shift(obj, 0.6 * PX, 1.0 * PX), (0, 0), 1.2 * PX) * contact
    sh = np.clip(sh, 0, 0.85) * (1 - obj)
    alpha = np.clip(obj + sh, 0, 1)
    rgba = np.dstack([rgb * obj[..., None], alpha * 255])
    W, H = round(out_size_u[0] * AS), round(out_size_u[1] * AS)
    small = cv2.resize(rgba, (W, H), interpolation=cv2.INTER_AREA)
    a = small[..., 3:4] / 255.0
    col = np.where(a > 1e-4, small[..., :3] / np.maximum(a, 1e-4), 0)
    return np.dstack([np.clip(col, 0, 255), np.clip(small[..., 3], 0, 255)]).astype(np.uint8)

def save_strip(frames, name):
    Image.fromarray(np.vstack(frames), "RGBA").save(os.path.join(ROOT, name), optimize=True)

# ---------------------------------------------------------------------- potard
KW = 84.0
def knob(phi_deg):
    c = Canvas(KW, KW)
    x, y = c.x, c.y
    r = np.hypot(x, y); th = np.arctan2(x, -y) - math.radians(phi_deg)
    # jupe cannelee : 8 lobes arrondis
    Rs = 25.6 + 0.75 * np.cos(8 * th)
    # profil : dessus legerement bombe, conge arrondi au bord, flancs droits
    fil = 2.2
    top = 8.6 - 1.4 * np.clip((r - 15.0) / 9.0, 0, 1) ** 1.5
    edge = np.clip(Rs - r, 0, None)
    hs = np.where(edge < fil, top - fil + np.sqrt(np.clip(fil ** 2 - (fil - edge) ** 2, 0, None)), top)
    # cannelures marquees aussi sur le dessus (creux entre les lobes)
    hs -= 0.9 * np.clip(-np.cos(8 * th), 0, 1) ** 1.5 * np.clip((r - 18.0) / 6.0, 0, 1)
    c.put(np.clip(hs, 0.01, None), r < Rs, 1)
    # chapeau alu repousse (cone tres plat + conge)
    rc = 15.6
    hc = 9.4 + 2.6 * (1 - np.clip(r / rc, 0, 1)) ** 0.9
    ec = np.clip(rc - r, 0, None)
    hc = np.where(ec < 1.0, hc - 1.0 + np.sqrt(np.clip(1.0 - (1.0 - ec) ** 2, 0, None)), hc)
    c.put(hc, r < rc, 2)

    n = normals(c.h)
    vis = soft_shadow(c)
    ndl = np.clip(n @ L, 0, 1) * (0.3 + 0.7 * vis)
    ndh = np.clip(n @ Hh, 0, 1)
    env = env_studio(n)
    # plastique noir brillant
    black = 6 + 14 * ndl + 0.32 * env * (0.4 + 0.6 * vis) + 230 * ndh ** 180 * vis
    black = np.dstack([black * 0.98, black, black * 1.03])
    # alu repousse : reflet anisotrope en « eventail » (fixe : ne tourne pas avec le potard)
    ang = np.arctan2(c.x, -c.y)
    la = math.atan2(L[0], -L[1])
    fan = np.abs(np.cos(ang - la)) ** 6
    fan2 = np.abs(np.cos(ang - la - 0.55)) ** 22
    rr = np.clip(r / 15.6, 0, 1)
    alu = 118 + 62 * fan + 70 * fan2 * (0.3 + 0.7 * rr) + 38 * (1 - rr) + 30 * ndl
    grain = np.sin(r * PX * 2.3) * 3.0
    alu = (alu + grain) * (0.75 + 0.25 * vis) + 200 * ndh ** 220 * vis
    alu = np.dstack([alu * 0.99, alu, alu * 1.02])
    rgb = np.where((c.mat == 2)[..., None], alu, black)
    return finish(c, np.clip(rgb, 0, 255), (KW, KW), cast=(3.0, 5.0), blur=3.4, amount=0.70)

# ---------------------------------------------------------------------- selecteur chicken head
SW = 120.0
SEL_FRAMES = [-44.0, -34.5, -24.0, -13.5, -4.5, 4.5, 13.5, 24.0, 34.5, 44.0]
def selector(ang_deg):
    c = Canvas(SW, SW)
    x, y = c.x, c.y
    r = np.hypot(x, y)
    # embase ronde
    rb = 22.8
    eb = np.clip(rb - r, 0, None)
    hb = np.where(eb < 3.0, 7.0 - 3.0 + np.sqrt(np.clip(9.0 - (3.0 - eb) ** 2, 0, None)), 7.0 + 0.6 * (1 - r / rb))
    c.put(hb, r < rb, 1)
    # bec (repere local : axe vers le haut quand ang = 0)
    a = math.radians(ang_deg)
    u = x * math.cos(a) + y * math.sin(a)          # travers
    v = -x * math.sin(a) + y * math.cos(a)         # le long (negatif = vers la pointe)
    tipv, tailv = -41.0, 37.5
    t = np.clip((v - tipv) / (tailv - tipv), 0, 1)      # 0 pointe -> 1 queue
    half = 1.8 + 8.6 * t ** 0.8
    inside = (v > tipv) & (v < tailv) & (np.abs(u) < half)
    crest = 14.8 - 2.5 * (1 - np.clip(t / 0.18, 0, 1)) ** 2 - 1.2 * np.clip((t - 0.85) / 0.15, 0, 1) ** 2
    prof = 1 - (np.abs(u) / np.maximum(half, 1e-3)) ** 1.6
    hp = 7.5 + (crest - 7.5) * np.clip(prof, 0, 1) ** 0.55
    # bouts arrondis
    endr = np.clip(np.minimum(v - tipv, tailv - v) / 2.0, 0, 1)
    hp = 7.0 + (hp - 7.0) * np.sqrt(endr)
    c.put(np.where(inside, hp, 0), inside, 2)

    n = normals(c.h)
    vis = soft_shadow(c, samples=6, max_u=40.0)
    ndl = np.clip(n @ L, 0, 1) * (0.25 + 0.75 * vis)
    ndh = np.clip(n @ Hh, 0, 1)
    env = env_studio(n)
    base = np.array([178.0, 38.0, 22.0])
    diff = (0.14 + 0.86 * ndl)[..., None] * base
    spec = (255 * ndh ** 90 * vis + 0.35 * env * vis)[..., None] * np.array([1.0, 0.93, 0.88])
    rgb = diff + spec
    rgb = np.where((c.mat == 1)[..., None], rgb * 0.94, rgb)
    return finish(c, np.clip(rgb, 0, 255), (SW, SW), cast=(4.5, 7.0), blur=4.8, amount=0.60)

# ---------------------------------------------------------------------- inverseur horizontal
TWU, THU = 46.0, 28.0
def toggle(pos):            # -1 gauche, 0 milieu, +1 droite
    c = Canvas(TWU, THU)
    x, y = c.x, c.y
    # fente noire (embase vue de face) : rectangle arrondi
    hw, hh, rad = 14.0, 5.6, 5.0
    qx = np.clip(np.abs(x) - (hw - rad), 0, None); qy = np.clip(np.abs(y) - (hh - rad), 0, None)
    d = np.hypot(qx, qy) - rad
    rim = np.clip(-d, 0, 1.2)
    c.put(0.8 + rim * 0.9, d < 0, 1)
    # levier chrome : tige cylindrique + boule
    if pos == 0:
        rr = np.hypot(x, y)
        ball = rr < 3.6
        c.put(np.where(ball, 9.0 + np.sqrt(np.clip(3.6 ** 2 - rr ** 2, 0, None)), 0), ball, 2)
    else:
        sgn = float(pos)
        L0, L1 = 0.0, 10.5
        along = x * sgn
        rod = (along > L0) & (along < L1) & (np.abs(y) < 2.9)
        hr = 3.0 + (along / L1) * 4.5 + np.sqrt(np.clip(2.9 ** 2 - y ** 2, 0, None))
        c.put(np.where(rod, hr, 0), rod, 2)
        bx = sgn * 10.0
        rr = np.hypot(x - bx, y)
        ball = rr < 4.6
        c.put(np.where(ball, 7.4 + np.sqrt(np.clip(4.6 ** 2 - rr ** 2, 0, None)), 0), ball, 2)
        # rondelle / pivot
        rp = np.hypot(x, y)
        piv = rp < 3.6
        c.put(np.where(piv, 2.8 + np.sqrt(np.clip(3.6 ** 2 - rp ** 2, 0, None)) * 0.6, 0), piv, 2)

    n = normals(c.h)
    vis = soft_shadow(c, samples=6, max_u=14.0)
    ndl = np.clip(n @ L, 0, 1) * (0.3 + 0.7 * vis)
    ndh = np.clip(n @ Hh, 0, 1)
    env = env_studio(n)
    dark = 6 + 20 * ndl + 0.22 * env * vis + 150 * ndh ** 120 * vis
    chrome = 55 + env * (0.55 + 0.45 * vis) * 1.25 + 95 * ndl * vis + 255 * ndh ** 200 * vis
    satin = 60 + 120 * ndl + 0.4 * env
    rgb = np.where(c.mat == 1, dark, np.where(c.mat == 2, chrome, satin))
    rgb = np.dstack([rgb, rgb, rgb * 1.02])
    return finish(c, np.clip(rgb, 0, 255), (TWU, THU), cast=(1.6, 2.6), blur=1.4, amount=0.55, contact=0.4)

if __name__ == "__main__":
    import sys
    what = sys.argv[1:] or ["knob"]
    if "knob" in what:
        save_strip([knob(45.0 * i / 32) for i in range(32)], "knob_strip.png")
    if "selector" in what:
        save_strip([selector(a) for a in SEL_FRAMES], "selector_strip.png")
    if "toggle" in what:
        save_strip([toggle(p) for p in (-1, 0, 1)], "toggle_strip.png")
    print("ok")
