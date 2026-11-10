# ANODE v2 : redessine proprement, parfaitement droits, la barre chromee au pied de la cage des lampes
# et le bandeau superieur en alu brosse du chassis (la maquette les avait ondules).
# Repere 2737 x 1800 px (2x des unites du plugin). Applique sur la base (facade + lampes chaudes)
# avant la separation des calques : fond, lampes chaudes, lueur et plaques restent coherents.
import numpy as np, cv2

# geometrie mesuree sur la maquette (px)
RAIL_X0, RAIL_X1 = 325.0, 2411.0          # extremites (bouts arrondis compris)
RAIL_Y0, RAIL_Y1 = 996.5, 1009.5          # dessus / dessous de la barre
CH_TOP = 1010                              # arete superieure du chassis
BAND_Y1 = 1036                             # fin du bandeau refait (fondu jusqu'a BAND_Y1 + 6)
SRC_DY = 92                                # texture brossee prise 92 px plus bas (zone propre)
EDGE_KEEP = (70, 2667)                     # coins arrondis du chassis conserves


def _blur_x(a, sigma):
    return cv2.GaussianBlur(a, (0, 0), sigmaX=sigma, sigmaY=0.0001)


def chassis_band(img):
    """ bandeau superieur du chassis : profil vertical moyen (droit) x eclairage gauche-droite x brossage propre """
    out = img.copy()
    rows = np.arange(CH_TOP - 1, BAND_Y1 + 6)
    xa, xb = EDGE_KEEP
    rgb = img[..., :3]
    # profil vertical : mediane sur la largeur utile
    prof = np.median(rgb[rows][:, 150:2590], axis=1)                              # (rows, 3)
    # eclairage gauche-droite (et ombrage des flancs) : bande propre juste en dessous, lissee
    ref = rgb[BAND_Y1 + 4:BAND_Y1 + 30].mean(0)
    refm = np.median(ref[150:2590], axis=0)
    gx = _blur_x((ref / np.maximum(refm, 1e-3)).astype(np.float32)[None], 60)[0]   # (W, 3)
    # brossage : detail relatif des rangees propres plus bas (stries horizontales reelles de la texture)
    src = rgb[rows + SRC_DY].astype(np.float32)
    low = np.stack([_blur_x(src[..., c], 40) for c in range(3)], -1)
    det = np.clip(src / np.maximum(low, 1.0), 0.6, 1.6)
    new = prof[:, None, :] * gx[None] * det
    # fondu : debut (arete) net, bas fondu dans l'existant ; flancs (coins arrondis) conserves
    wy = np.ones(len(rows)); wy[-6:] = np.linspace(1, 0, 8)[1:7]
    wx = np.zeros(img.shape[1]); wx[xa:xb] = 1
    wx = np.convolve(wx, np.ones(21) / 21, mode="same")
    w = (wy[:, None] * wx[None])[..., None]
    out[rows, :, :3] = rgb[rows] * (1 - w) + new * w
    return out


def _capsule_cov(h, w, x0, y0, ss=4):
    """ couverture anti-crenelee d'une capsule horizontale (barre ronde a bouts arrondis), en coordonnees image """
    r = (RAIL_Y1 - RAIL_Y0) / 2; cy = (RAIL_Y0 + RAIL_Y1) / 2
    ys = y0 + (np.arange(h * ss) + 0.5) / ss
    xs = x0 + (np.arange(w * ss) + 0.5) / ss
    X, Y = np.meshgrid(xs, ys)
    xc = np.clip(X, RAIL_X0 + r, RAIL_X1 - r)
    inside = ((X - xc) ** 2 + (Y - cy) ** 2) <= r * r
    return inside.reshape(h, ss, w, ss).mean((1, 3))


def chrome_rail(img):
    out = img.copy()
    rgb = img[..., :3]
    y0, y1 = 992, 1010
    x0, x1 = int(RAIL_X0) - 4, int(RAIL_X1) + 4
    h, w = y1 - y0, x1 - x0
    cov = _capsule_cov(h, w, x0, y0)
    r = (RAIL_Y1 - RAIL_Y0) / 2; cy = (RAIL_Y0 + RAIL_Y1) / 2
    yy = y0 + np.arange(h) + 0.5
    xx = x0 + np.arange(w) + 0.5
    # position sur la section du tube : -1 (dessus) .. +1 (dessous), avec les bouts arrondis
    dxe = np.maximum(0, np.maximum((RAIL_X0 + r) - xx, xx - (RAIL_X1 - r)))       # distance dans le bout arrondi
    t = (yy[:, None] - cy) / r
    tt = np.clip(t / np.sqrt(np.maximum(1e-3, 1 - (dxe[None] / r) ** 2)), -1, 1)
    # chrome : meme profil que les barres horizontales de la cage (mediane mesuree sur la barre basse de la cage)
    P = np.median(rgb[742:758, 1250:1500], axis=1)                               # 16 rangees : bord, reflet, bas chaud, bord
    pos = (tt + 1) / 2 * (len(P) - 1)
    i0 = np.clip(np.floor(pos).astype(int), 0, len(P) - 2); f = (pos - i0)[..., None]
    prof = P[i0] * (1 - f) + P[i0 + 1] * f
    # reflets de l'environnement (lueur des lampes, montants) : discrets, tres lisses le long de la barre
    above = rgb[984:993, x0:x1].mean(0).astype(np.float32)
    env = np.stack([_blur_x(above[None, :, c], 22)[0] for c in range(3)], -1)
    envn = env / np.maximum(env.mean(1, keepdims=True), 1.0)
    envw = np.clip(env.mean(1) / 120.0, 0, 1)[None, :, None] * 0.15
    col = prof * (1 - envw) + np.clip(prof.mean(-1, keepdims=True) * envn, 0, 255) * envw
    # bouts arrondis un peu plus sombres sur le pourtour
    col *= (1 - 0.25 * np.clip(dxe / r, 0, 1) ** 2)[None, :, None]
    # fond derriere la barre : la plaque du dessus, en miroir (les anciens bords ondules disparaissent)
    bg = np.stack([rgb[min(2 * 992 - y, 992), x0:x1] for y in range(y0, y1)])
    out[y0:y1, x0:x1, :3] = bg * (1 - cov[..., None]) + col * cov[..., None]
    return out


def fix(img):
    img = chrome_rail(img)
    return img
