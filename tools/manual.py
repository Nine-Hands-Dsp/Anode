# ANODE (NINE HANDS | DSP) - notice PDF en francais, illustree avec le rendu reel du plugin (tools/render).
# Usage : python3 tools/manual.py <dossier des captures de tools/render> <fichier PDF de sortie>
# Couverture « minimale » commune a toutes les notices NINE HANDS | DSP (gabarit fixe, voir cover_bg).
import os, sys, re, subprocess
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (BaseDocTemplate, PageTemplate, NextPageTemplate, Frame, Paragraph, Spacer, Image as RLImage,
                                Table, TableStyle, PageBreak, KeepTogether)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS, OUT = sys.argv[1], sys.argv[2]
VERSION = re.search(r"^project\(Anode VERSION ([0-9.]+)\)", open(os.path.join(ROOT, "CMakeLists.txt")).read(), re.M).group(1)
TMP = os.path.dirname(os.path.abspath(OUT))
FONTS = os.path.join(ROOT, "assets", "fonts")
for n, f in (("Pop", "Poppins-Regular"), ("PopL", "Poppins-Light"), ("PopM", "Poppins-Medium"), ("PopB", "Poppins-Bold"),
             ("Title", "BarlowCondensed-ExtraBold")):
    pdfmetrics.registerFont(TTFont(n, os.path.join(FONTS, f + ".ttf")))

ACC = colors.HexColor("#C9762F"); ACC_L = colors.HexColor("#E6A15F")       # lueur des lampes
DARK = colors.HexColor("#17171A"); GREY = colors.HexColor("#66666E"); LIGHT = colors.HexColor("#F2EEE8")
K = 2.0                                                                     # captures 2737 x 1800 = 2 x repere 1368,5 x 900 unites

# ---------------- illustrations (captures reelles du plugin)
def shot(name): return Image.open(os.path.join(SHOTS, name)).convert("RGB")
def crop_u(im, box): return im.crop(tuple(int(round(v * K)) for v in box))
def save(im, name):
    p = os.path.join(TMP, name); im.save(p); return p
def strip(ims, name, gap=14):
    h = max(i.height for i in ims); w = sum(i.width for i in ims) + gap * (len(ims) - 1)
    o = Image.new("RGB", (w, h), (255, 255, 255)); x = 0
    for i in ims: o.paste(i, (x, 0)); x += i.width + gap
    return save(o, name)

src = shot("anode_play.png")
ann = src.copy(); dr = ImageDraw.Draw(ann)
fnt = ImageFont.truetype(os.path.join(FONTS, "Poppins-Bold.ttf"), 40)
marks = [(1, 145, 742), (2, 450, 562), (3, 703, 628), (4, 838, 628), (5, 972, 628), (6, 1098, 700),
         (7, 700, 792), (8, 969, 792), (9, 525, 22), (10, 684, 300)]
for n, x, y in marks:
    cx, cy, r = x * K, y * K, 30
    dr.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(255, 255, 255), outline=(201, 118, 47), width=6)
    dr.text((cx, cy + 1), str(n), font=fnt, fill=(23, 23, 26), anchor="mm")
annotated = save(ann.resize((ann.width // 2, ann.height // 2), Image.LANCZOS), "anode_annotated.png")
zone = (176, 57, 1190, 530)
tubes_img = strip([crop_u(shot(n), zone).resize((680, 317), Image.LANCZOS) for n in ("anode_off.png", "anode_play.png", "anode_hot.png")], "anode_tubes_states.png")
vu_box = (228, 568, 672, 796)
vu_img = strip([crop_u(shot(n), vu_box).resize((560, 288), Image.LANCZOS) for n in ("anode_idle.png", "anode_play.png", "anode_off.png")], "anode_vu_states.png")
bar_img = save(crop_u(shot("anode_status.png"), (530, 0, 840, 46)), "anode_bar.png")

# couverture : ampli detoure (masque de la photo), reflet au sol, halo orange tres doux
msk = Image.open(os.path.join(ROOT, "design", "v2", "amp_alpha.png")).resize(src.size, Image.LANCZOS)
cut = Image.new("RGBA", src.size, (0, 0, 0, 0)); cut.paste(src, (0, 0), msk)
cut = cut.crop(cut.getbbox()); cw, ch = cut.size
pad = int(cw * 0.16)
canvas = Image.new("RGBA", (cw + 2 * pad, int(ch * 1.30) + pad), (0, 0, 0, 255))
yy, xx = np.mgrid[0:canvas.height, 0:canvas.width].astype(np.float64)
gcx, gcy = canvas.width / 2, pad + ch * 0.45
glow = np.exp(-(((xx - gcx) / (cw * 0.62)) ** 2 + ((yy - gcy) / (ch * 0.48)) ** 2)) * 30
edge = np.clip(np.minimum.reduce([xx, canvas.width - 1 - xx, yy, canvas.height - 1 - yy]) / (pad * 0.9), 0, 1)
glow *= edge ** 2
canvas = Image.alpha_composite(canvas, Image.fromarray(np.dstack([glow * 0.95, glow * 0.50, glow * 0.15, np.full_like(glow, 255)]).astype(np.uint8), "RGBA"))
refl = cut.transpose(Image.FLIP_TOP_BOTTOM); ra = np.array(refl).astype(np.float64)
ra[..., 3] *= (np.clip(1 - np.arange(refl.height) / (refl.height * 0.22), 0, 1) ** 1.6 * 0.22)[:, None]
canvas.alpha_composite(Image.fromarray(ra.astype(np.uint8), "RGBA"), (pad, pad + ch + 6))
canvas.alpha_composite(cut, (pad, pad))
canvas = canvas.crop((0, 0, canvas.width, pad + int(ch * 1.20)))
cover = save(canvas.convert("RGB"), "anode_cover_cutout.png")
LOGO_PNG = os.path.join(TMP, "nine_hands_logo_blanc.png")
subprocess.run(["rsvg-convert", "-w", "1200", os.path.join(ROOT, "assets", "nine_hands_logo_blanc.svg"), "-o", LOGO_PNG], check=True)

# ---------------- styles
st = {
    "h1": ParagraphStyle("h1", fontName="PopM", fontSize=17, leading=22, textColor=DARK, spaceBefore=4 * mm, spaceAfter=3 * mm),
    "h2": ParagraphStyle("h2", fontName="PopM", fontSize=11.5, leading=15, textColor=ACC, spaceBefore=3.5 * mm, spaceAfter=1.5 * mm),
    "p": ParagraphStyle("p", fontName="Pop", fontSize=9.4, leading=13.6, textColor=DARK, spaceAfter=2 * mm),
    "small": ParagraphStyle("small", fontName="PopL", fontSize=8, leading=11, textColor=GREY, spaceAfter=2 * mm),
    "cell": ParagraphStyle("cell", fontName="Pop", fontSize=8.4, leading=11.4, textColor=DARK),
    "cellb": ParagraphStyle("cellb", fontName="PopM", fontSize=8.4, leading=11.4, textColor=DARK),
}
def P(t, s="p"): return Paragraph(t, st[s])
def bullets(items): return [P("&bull;&nbsp;&nbsp;" + i) for i in items]
def img(path, width):
    w, h = Image.open(path).size
    return RLImage(path, width, width * h / w)
def table(rows, widths):
    data = [[P(c, "cellb" if r == 0 else "cell") for c in row] for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1)
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.3, colors.HexColor("#DDD6CB")),
                           ("TOPPADDING", (0, 0), (-1, -1), 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
                           ("BACKGROUND", (0, 0), (-1, 0), LIGHT), ("LINEBELOW", (0, 0), (-1, 0), 0.8, ACC)]))
    return t

# ---------------- donnees (identiques au code livre : PluginProcessor.cpp)
TUBES = ["6L6", "EL34", "EL84", "KT88"]
FACTORY = [(m[0], int(m[1]), int(m[2]), int(m[3]), float(m[4]), float(m[5]), float(m[6]), float(m[7]))
           for m in re.findall(r'\{ "([^"]+)",\s+(\d),\s+(\d),\s+(\d),\s+([\d.]+)f,\s+([\d.]+)f,\s+([\d.]+)f,\s+(-?[\d.]+)f \}',
                               open(os.path.join(ROOT, "PluginProcessor.cpp"), encoding="utf-8").read())]
assert len(FACTORY) == 14, len(FACTORY)
DESC = ["Paire d'EL34 façon Marshall poussée en crunch : médiums en avant, Depth et Contour un peu montés.",
        "Le même étage EL34 presque à fond : lead compressé qui chante, présence relevée.",
        "Paire de 6L6 façon Fender à faible volume : clair large et rond, beaucoup de marge.",
        "6L6 poussées comme un Bassman tweed : grave épais, saturation douce et dynamique.",
        "Paire d'EL84 façon Vox (polarisation cathode, redresseur à lampe) : aigus carillonnants, saturation précoce et cristalline.",
        "Paire d'EL84 à fond : sag et compression de cathode, son granuleux et sensible au toucher.",
        "Paire de KT88 comme dans une grosse tête basse : énorme marge, grave ferme, Contour baissé.",
        "KT88 montées en triode à faible volume : clair hi-fi, chaud et très linéaire, idéal comme couleur de studio.",
        "Une 6L6 en classe A, façon Champ : richesse en 2e harmonique, sature tôt et s'épanouit.",
        "Une EL84 en classe A, grande ouverte : son de petit ampli saturé, compressé, presque fuzz.",
        "Une EL34 en classe A, montage triode : douce, chaude, peu de distorsion, très musicale.",
        "Paire de 6L6 en triode à volume moyen : compression légère qui « colle » les pistes DI.",
        "Paire d'EL34, Depth et Contour hauts, après un préampli high-gain ou une pédale de saturation : résonance serrée, présence tranchante.",
        "Une KT88 en classe A, résonance profonde, son sombre : basse doom épaisse et saturée."]

W = 174 * mm
f = [Spacer(1, 1), NextPageTemplate("p"), PageBreak()]                     # couverture (dessinee par cover_bg)

f += [P("1. Présentation", "h1"),
      P("<b>Anode</b> recrée la <b>section de puissance d'un ampli à lampes</b> — les grosses lampes de sortie et le transformateur de sortie — et permet de l'utiliser en préampli guitare ou basse : devant un simulateur d'ampli, une IR de baffle, ou seule pour réchauffer une piste DI. La façade reprend un ampli à lampes intégré : lampes allumées sous leur cage, deux VU-mètres éclairés, bouton Power, sélecteur de lampe et trois potards."),
      P("Rien ici n'est une simple courbe de saturation : chaque étage d'un vrai ampli à lampes est calculé, à 352,8 kHz ou plus à l'intérieur du plugin, à partir de modèles de lampes publiés et des valeurs de vrais circuits :"),
      *bullets(["<b>Triode d'entrée (V1, 12AX7)</b> sur sa droite de charge, avec résistance de grille, conduction de grille et un condensateur de cathode qui se charge vraiment : la polarisation bouge avec l'attaque, l'ampli « s'épanouit », compresse doucement et ajoute une pointe de 2e harmonique quand on attaque fort.",
                "<b>Déphaseur à paire différentielle</b> (12AX7 ou 12AT7 selon l'ampli) résolu exactement : charges de plaque inégales, côté en phase qui se coupe net pendant que l'autre continue, son propre écrêtage avant ou après les lampes de sortie.",
                "<b>Lampes de sortie</b> calées sur les fiches techniques, résolues sur la charge du transformateur ; en push-pull la paire est appariée à 4 % près, comme une vraie paire. Les condensateurs de liaison et la conduction de grille donnent le glissement de polarisation qui fait « s'étrangler » un ampli poussé.",
                "<b>Alimentation à deux cellules</b> : le B+ des plaques s'affaisse d'abord, le nœud des écrans suit avec un retard, la compression « respire » avec le jeu.",
                "<b>Contre-réaction globale</b> façonnée par Depth et Contour, <b>transformateur de sortie</b> (inductance primaire, fuites, saturation du noyau) et <b>charge du haut-parleur</b> de chaque ampli de référence."]),
      P("Installation", "h2"),
      *bullets(["<b>Windows</b> : décompressez <b>Anode-VST3-Windows.zip</b> et copiez <b>Anode.vst3</b> dans <b>C:\\Program Files\\Common Files\\VST3\\</b>.",
                "<b>macOS</b> (Apple Silicon et Intel) : décompressez <b>Anode-VST3-macOS.zip</b>, copiez <b>Anode.vst3</b> dans <b>~/Library/Audio/Plug-Ins/VST3/</b>. Si macOS le bloque : <b>xattr -cr ~/Library/Audio/Plug-Ins/VST3/Anode.vst3</b> dans le Terminal.",
                "<b>Reaper</b> : Options > Preferences > Plug-ins > VST > <b>Re-scan</b>. Le plugin apparaît sous <b>Anode (NINE HANDS | DSP)</b>. Pour le pilotage MIDI, envoyez l'entrée MIDI à la piste (armement avec votre contrôleur en entrée, ou envoi MIDI)."]),
      PageBreak()]

f += [P("2. Vue d'ensemble de l'interface", "h1"), img(annotated, W), Spacer(1, 3 * mm),
      table([["#", "Commande", "Rôle"],
             ["1", "Power", "Allume l'ampli ou le met en bypass (signal direct, sans clic) ; les lampes et les cadrans s'éteignent"],
             ["2", "VU-mètres", "Niveau de sortie gauche et droite, balistique d'un vrai VU-mètre"],
             ["3", "Depth", "Résonance grave (contre-réaction retirée dans le grave)"],
             ["4", "Contour", "Présence (contre-réaction retirée dans l'aigu ; Cut avec l'EL84 et en single-ended)"],
             ["5", "Volume", "Volume maître vers l'étage de puissance : niveau et saturation"],
             ["6", "TUBE", "Lampe de sortie : 6L6, EL34, EL84, KT88"],
             ["7", "Single-Ended / Push-Pull", "Levier en haut : une lampe en classe A ; en bas : deux lampes en opposition (classe AB)"],
             ["8", "Triode / Pentode", "Levier en haut : écran relié à la plaque ; en bas : écran alimenté par l'alimentation"],
             ["9", "Barre de presets", "Preset précédent / suivant (flèches), menu des presets (nom) ; la valeur d'un réglage qui bouge y apparaît 2,5 s"],
             ["10", "Lampes", "Filaments allumés ; la lueur suit le jeu et les plaques rougissent après un long jeu poussé"]],
            [10 * mm, 42 * mm, 122 * mm]),
      P("Clic droit n'importe où sur la façade : taille de la fenêtre, presets, niveau de sortie, MIDI. Le menu s'ouvre à l'endroit du clic ; sur une commande, il propose aussi son MIDI Learn.", "small"),
      PageBreak()]

f += [P("3. Les commandes", "h1"),
      P("Power", "h2"),
      P("Le bouton Power met l'ampli en marche ou en <b>bypass</b>. Éteint, le signal ressort tel quel : fondu de 20 ms, et le signal direct est retardé de la latence de l'ampli, sans filtrage en peigne. L'anneau orange s'éteint, les VU-mètres passent au crème et leurs aiguilles retombent, puis les lampes refroidissent en environ 3 s ; au rallumage, les filaments chauffent en environ 2 s. Power est automatisable et assignable en MIDI ; il ne fait pas partie des presets (changer de preset ne coupe jamais l'ampli)."),
      P("VU-mètres", "h2"),
      img(vu_img, W), P("Au repos (aiguilles en butée), en jeu, puis ampli éteint (cadrans non éclairés).", "small"),
      P("Les deux VU-mètres indiquent le niveau efficace de sortie, gauche et droite, avec la balistique d'un vrai galvanomètre : environ 300 ms pour atteindre la valeur, léger dépassement, retour en butée à gauche sans signal. <b>0 VU = −18 dBFS efficace</b> : avec une guitare à niveau normal, les aiguilles bougent autour de −10 à −5 VU ; dans le rouge, la sortie est chaude (baissez Output level dans le clic droit si besoin)."),
      P("TUBE (sélecteur de lampe)", "h2"),
      P("6L6 · EL34 · EL84 · KT88 — cliquez sur une inscription, sur le bouton (lampe suivante), glissez de côté ou utilisez la molette. Changer de lampe en jouant fait un fondu de 35 ms, sans clic.", "small"),
      table([["Lampe", "Point de fiche utilisé pour le calage", "Ampli de référence (push-pull)", "Caractère"],
             ["6L6GC", "Va = Vg2 = 250 V, Vg1 −14 V → 72 mA, gm 6 mA/V", "Fender blackface : 460 V, Raa 4 kΩ, polarisation fixe, déphaseur 12AT7, contre-réaction forte", "Américain : large, rond, beaucoup de clair, grave ferme"],
             ["EL34", "Va = Vg2 = 250 V, Vg1 −13,5 V → 100 mA, gm 11 mA/V", "Marshall JMP / JCM800 : 455 V, Raa 3,4 kΩ, polarisation fixe, V1 « bright » (2,7 kΩ / 0,68 µF), déphaseur 12AX7, 4x12", "Britannique : grave serré, médiums en avant, saturation agressive"],
             ["EL84", "Va = Vg2 = 250 V, Vg1 −7,3 V → 48 mA, gm 11,3 mA/V", "Vox AC30 : 335 V, Raa 8 kΩ, polarisation cathode, redresseur GZ34, déphaseur 12AX7, presque pas de contre-réaction", "Carillonnant, compresse et sature en premier"],
             ["KT88", "Va = Vg2 = 250 V, Vg1 −15 V → 140 mA, gm 11,5 mA/V", "Ampeg SVT / hi-fi : 520 V, Raa 4,5 kΩ, polarisation fixe, déphaseur 12AT7 basse impédance, 8x10", "La plus grande marge, grave profond et tenu : basse et hi-fi"]],
            [15 * mm, 46 * mm, 66 * mm, 47 * mm]),
      P("Les modèles de lampes (Koren) ont été ajustés pour que chaque lampe tire le courant et la pente de sa fiche technique."),
      KeepTogether([P("Les lampes de la façade", "h2"),
                    img(tubes_img, W), P("Ampli éteint (lampes froides), en jeu, puis après plusieurs secondes de jeu très poussé (plaques qui rougissent).", "small"),
                    P("Les lampes de la photo restent les mêmes quel que soit le réglage, mais elles vivent avec le son : quand on joue, la lueur suit le courant qui traverse les lampes de puissance (courant moyen en classe AB, variations du courant en classe A), avec un halo qui déborde du verre ; après plusieurs secondes de jeu très poussé, les plaques rougissent lentement (environ 4 s), puis refroidissent quand on s'arrête (environ 8 s).")]),
      P("Single-Ended / Push-Pull", "h2"),
      *bullets(["<b>Single-Ended</b> (levier en haut) : une seule lampe en classe A, polarisation cathode, courant continu dans le transformateur, attaquée par deux étages 12AX7 comme dans un Fender Champ ou un Vox AC4. <b>2e harmonique</b> fort, compression douce et précoce, saturation riche et « grasse ». Moins de grave (transformateur et baffle plus petits).",
                "<b>Push-Pull</b> (levier en bas) : deux lampes en opposition (classe AB), attaquées par un déphaseur. Les harmoniques pairs s'annulent, les <b>impairs</b> dominent quand on pousse, plus de puissance et de marge ; distorsion de croisement quand les condensateurs de liaison se chargent (jeu appuyé)."]),
      P("Triode / Pentode", "h2"),
      *bullets(["<b>Triode</b> (levier en haut) : écran relié à la plaque. Même polarisation (comme sur un ampli réel), moins de gain et de puissance, écrêtage plus doux, impédance de sortie plus basse : plus lisse, plus hi-fi, un peu moins fort.",
                "<b>Pentode</b> (levier en bas) : l'écran est alimenté par l'alimentation (via sa résistance, il s'affaisse donc aussi). Plus de gain, plus de puissance, « coude » plus dur quand la plaque descend, impédance de sortie élevée : la résonance du haut-parleur et la montée d'aigu ressortent."]),
      P("Depth", "h2"),
      P("Résonance grave. Depth retire la contre-réaction dans le grave (sous environ 170 Hz). L'étage donne plus de bas, et comme l'ampli tient alors moins le haut-parleur (impédance de sortie plus élevée), la résonance du baffle passe : l'ampli « respire ». Cela agit aussi quand les lampes de puissance saturent. 5 = réglage de référence ; 0 = serré et tenu (environ −3 dB vers 100 Hz) ; 10 = jusqu'à environ +5 dB en clair et +3 à +4 dB en saturation en pentode push-pull (moins en triode et avec l'EL84, qui n'a presque pas de contre-réaction). Le grave étant moins tenu, un Depth élevé sature aussi plus tôt dans le grave."),
      P("Contour", "h2"),
      P("Présence et Cut. Contour retire la contre-réaction dans l'aigu (à partir d'environ 1,8 kHz), comme le Presence d'un Marshall ou d'un Fender, et laisse passer la montée d'impédance du haut-parleur, ce qui agit aussi quand les lampes de puissance saturent. Avec l'EL84 en push-pull (AC30, presque sans contre-réaction), Contour pilote aussi le filtre <b>Cut</b> de l'AC30, entre le déphaseur et les lampes de puissance ; en single-ended, un filtre de tonalité après la lampe d'attaque (façon AC4). 5 = réglage de référence ; 0 = sombre, environ −4 dB à 3 kHz (−8 dB avec l'EL84 et en single-ended) ; 10 = brillant, environ +6 à +7 dB à 3 kHz en clair et +2 à +5 dB dans la bande 2–4 kHz en saturation."),
      P("Volume", "h2"),
      P("Volume maître vers le déphaseur et les lampes de sortie : −24 dB à 0, 0 dB à 5, +24 dB à 10. En montant : plus de niveau, puis écrêtage de grille, sag, glissement de polarisation et saturation des lampes de puissance, exactement comme en poussant un vrai ampli. La sortie est en partie compensée : le son devient plus fort et plus sale sans sauter de 24 dB. Avec une guitare à niveau normal : environ 5 = clair, 7 = début de saturation, 10 = saturé (EL84 en premier, KT88 en dernier). Le niveau de sortie (−12 à +12 dB) est dans le clic droit > <b>Output level</b> (paramètre automatisable « Output »)."),
      P("Les points autour des potards marquent les valeurs 0 à 10 ; double-clic sur un potard = retour à 5.", "small")]

f += [P("4. MIDI et pédales d'expression", "h1"),
      P("Aucun CC n'est assigné par défaut : tout passe par le <b>MIDI Learn</b>. Clic droit sur une commande > <b>MIDI Learn</b>, puis bougez la pédale ou appuyez sur le switch de votre contrôleur ; la barre de presets confirme « Depth = CC 11 ». Clic droit > MIDI liste toutes les assignations et permet de les effacer."),
      table([["Assignable", "Comportement"],
             ["Depth, Contour, Volume", "Continu, sans saccade : rampe linéaire entre deux messages puis lissage ; la course de la pédale est apprise automatiquement (une pédale qui n'envoie que 2–116 atteint quand même 0 et 100 %) ; CC 14 bits (CC n + n+32) accepté pour les CC 0–31"],
             ["Tube Model", "0–31 6L6, 32–63 EL34, 64–95 EL84, 96–127 KT88"],
             ["Single-Ended / Push-Pull", "≥ 64 = Push-Pull"],
             ["Triode / Pentode", "≥ 64 = Pentode"],
             ["Power", "≥ 64 = allumé, < 64 = bypass"],
             ["Program Change", "1–14 = presets d'usine"]],
            [44 * mm, 130 * mm]),
      P("5. Fenêtre", "h1"),
      P("La fenêtre s'ouvre à <b>60 %</b> et ne peut jamais être réduite en dessous. Clic droit > <b>Plugin size</b> : 60 % / 75 % / 100 %, et <b>Default size (new windows)</b> pour choisir la taille de toutes les nouvelles fenêtres d'Anode. Tirez le coin pour redimensionner librement (proportions bloquées). Textes, graduations et logos sont dessinés en vectoriel : nets à toutes les tailles, y compris sur les écrans Retina / 4K."),
      img(bar_img, W * 0.5), Spacer(1, 2 * mm),
      P("6. Presets", "h1"),
      P("Flèches de la barre = précédent / suivant ; clic sur le nom pour le menu. 9 emplacements utilisateur (Save / Load user preset). Toute modification affiche « Edited ». Les presets ne changent jamais l'état du bouton Power."),
      table([["Preset", "Lampe · montage · D / C / V · sortie", "Description"]] +
            [[f"{i + 1}. {p[0]}", f"{TUBES[p[1]]} · {'PP' if p[2] else 'SE'} {'pent.' if p[3] else 'triode'} · {p[4]:g} / {p[5]:g} / {p[6]:g} · {p[7]:+g} dB", DESC[i]]
             for i, p in enumerate(FACTORY)],
            [44 * mm, 46 * mm, 84 * mm]),
      PageBreak()]

f += [P("7. Conseils d'utilisation", "h1"),
      *bullets(["<b>Chaîne dans Reaper</b> : guitare > pédales de saturation / EQ > <b>Anode</b> > IR de baffle (Impulse) ou simulateur d'ampli (Ampire avec une capture NAM de préampli seul avant Anode, pour un ampli complet). Anode remplace ou ajoute l'étage de lampes de puissance que la plupart des simulateurs simplifient.",
                "<b>7 cordes en G et plus bas</b> : gardez Depth entre 4 et 6 en pentode push-pull ; en push-pull, KT88 ou 6L6 gardent la corde grave serrée, le single-ended et l'EL84 vont « s'épanouir » et saturer le bas. Contour 6–7 garde la définition.",
                "<b>Basse</b> : KT88 push-pull, Volume 5–6, Contour 3–4 : poids façon SVT. KT88 single-ended pour un doom proche du fuzz.",
                "<b>Couleur de studio</b> : triode, Volume 4–5 : quelques pourcents d'harmoniques et une compression douce sur guitares DI, basse ou même claviers.",
                "<b>Pédale d'expression sur Volume</b> : du clair au saturé au pied, exactement comme en jouant avec le master d'un ampli à lampes.",
                "<b>Comparer avec et sans</b> : le bouton Power (ou un switch MIDI assigné à Power) compare l'ampli et le signal direct au même instant, sans clic."]),
      KeepTogether([P("8. Caractéristiques techniques", "h1"),
      table([["Élément", "Valeur"],
             ["Format", "VST3, Windows (x64) et macOS (Apple Silicon + Intel), mono ou stéréo"],
             ["Fréquence interne", "≥ 352,8 kHz (×8 à 44,1 / 48 kHz, ×4 à 88,2 / 96 kHz, ×2 à 176,4 / 192 kHz), filtres IIR polyphasés"],
             ["Latence", "6 échantillons à 48 kHz (signalée à l'hôte)"],
             ["Processeur", "Environ 6 % d'un cœur à 48 kHz (canaux identiques calculés une fois)"],
             ["Modèles de lampes", "Lampes de sortie Koren pentode / montage triode calées sur les fiches ; 12AX7 / 12AT7 (Koren) pour l'entrée, l'attaque (SE) et le déphaseur à paire différentielle (PP), résolus exactement"],
             ["Effets du circuit", "Dynamique de cathode de la triode d'entrée, conduction de grille et glissement de polarisation, alimentation à deux cellules (B+ puis écrans), dérive de la polarisation cathode, paire appariée à 4 %, contre-réaction globale, transformateur de sortie (inductance primaire, fuites, saturation du noyau), charge du haut-parleur propre à chaque ampli de référence"],
             ["Paramètres", "Tube Model, Topology, Triode / Pentode, Depth, Contour, Volume, Output, Power"],
             ["Commutations", "Fondu de 35 ms entre configurations, 20 ms pour Power, sans clic"],
             ["VU-mètres", "Niveau efficace de sortie, 0 VU = −18 dBFS, balistique ~300 ms"],
             ["Presets", "14 d'usine, 9 utilisateur"],
             ["MIDI", "MIDI Learn uniquement, lissage des pédales d'expression, CC 14 bits, Program Change"],
             ["Fenêtre", "60 % (par défaut et minimum), 75 %, 100 % ; textes et logos vectoriels ; anti-gel v2 (rendu logiciel sous Windows)"]],
            [36 * mm, 138 * mm])])]

def deco(c, doc):
    c.saveState()
    c.setFillColor(DARK); c.rect(0, A4[1] - 6 * mm, A4[0], 6 * mm, stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, A4[1] - 6.8 * mm, A4[0], 0.8 * mm, stroke=0, fill=1)
    c.setFont("Pop", 7.5); c.setFillColor(GREY)
    c.drawString(18 * mm, 10 * mm, "Anode (NINE HANDS | DSP) · v" + VERSION)
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()

# ---- Couverture « minimale » (modele 6, valide nov. 2026) : gabarit fixe commun a toutes les notices ----
# Mesures en mm depuis le HAUT de la page A4, sauf le logo et le bandeau (depuis le bas). Aucun autre element.
COVER_NAME, COVER_SUB, COVER_LANG = "ANODE", "Étage de puissance à lampes", "FRANÇAIS"
PEDAL_H, PEDAL_MAXW, PEDAL_TOP = 82 * mm, 115 * mm, 84 * mm
LANG_TOP = 18 * mm
NAME_TOP, NAME_PT = 194 * mm, 30
SUB_TOP, SUB_PT = 208 * mm, 10
LOGO_W, LOGO_BOTTOM = 38 * mm, 22 * mm
BAND_H = 3 * mm
def cover_bg(c, doc):
    c.saveState(); c.setFillColor(colors.black); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, 0, A4[0], BAND_H, stroke=0, fill=1)
    def line(text, top, font, size, cs, col):
        w = pdfmetrics.stringWidth(text, font, size) + cs * (len(text) - 1)
        t = c.beginText((A4[0] - w) / 2, A4[1] - top - size * 0.8); t.setFont(font, size); t.setCharSpace(cs)
        t.setFillColor(col); t.textOut(text); c.drawText(t)
    line(COVER_LANG, LANG_TOP, "PopM", 8.5, 0, ACC_L)
    iw, ih = Image.open(cover).size
    cw_ = iw / 1.32; ch_ = (ih - int(cw_ * 0.16)) / 1.2; pd = int(cw_ * 0.16)
    k = min(PEDAL_H / ch_, PEDAL_MAXW / cw_)
    dw, dh = iw * k, ih * k
    y0 = A4[1] - (PEDAL_TOP + (PEDAL_H - ch_ * k) / 2) + pd * k
    c.drawImage(cover, (A4[0] - dw) / 2, y0 - dh, dw, dh)
    line(COVER_NAME, NAME_TOP - NAME_PT * 0.02, "Title", NAME_PT, NAME_PT * 0.5, colors.white)
    line(COVER_SUB, SUB_TOP, "Pop", SUB_PT, 1.5, colors.HexColor("#BDC4B8"))
    lw, lh = Image.open(LOGO_PNG).size
    c.drawImage(LOGO_PNG, (A4[0] - LOGO_W) / 2, LOGO_BOTTOM, LOGO_W, LOGO_W * lh / lw, mask="auto")
    c.restoreState()

doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=18 * mm,
                      title="ANODE - Notice v" + VERSION, author="NINE HANDS | DSP")
doc.addPageTemplates([PageTemplate(id="cover", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="fc")], onPage=cover_bg),
                      PageTemplate(id="p", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")], onPage=deco)])
doc.build(f)
print(OUT)
