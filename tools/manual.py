# ANODE (NINE HANDS | DSP) - notice PDF bilingue (anglais puis francais), illustree avec le rendu reel du plugin.
# Usage : python3 tools/manual.py <dossier des captures de tools/render> <fichier PDF de sortie>
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.platypus import (BaseDocTemplate, PageTemplate, NextPageTemplate, Frame, Paragraph, Spacer, Image as RLImage,
                                Table, TableStyle, PageBreak, KeepTogether)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS, OUT = sys.argv[1], sys.argv[2]
VERSION = "1.2.2"
TMP = os.path.dirname(os.path.abspath(OUT))
FONTS = os.path.join(ROOT, "assets", "fonts")
for n, f in (("Pop", "Poppins-Regular"), ("PopL", "Poppins-Light"), ("PopM", "Poppins-Medium"), ("PopB", "Poppins-Bold")):
    pdfmetrics.registerFont(TTFont(n, os.path.join(FONTS, f + ".ttf")))

ACC = colors.HexColor("#C9762F"); ACC_L = colors.HexColor("#E6A15F")       # lueur des lampes
DARK = colors.HexColor("#17171A"); GREY = colors.HexColor("#66666E"); LIGHT = colors.HexColor("#F2EEE8")
K = 812.0 / 406.0                                                           # captures 1560 x 812 = 2 x repere 780 x 406

# ---------------- illustrations
src = Image.open(os.path.join(SHOTS, "anode_status.png")).convert("RGB")
ann = src.copy(); dr = ImageDraw.Draw(ann)
fnt = ImageFont.truetype(os.path.join(FONTS, "Poppins-Bold.ttf"), 30)
marks = [(1, 300, 330), (2, 401, 224), (3, 572, 224), (4, 337, 288), (5, 437, 288), (6, 537, 288),
         (7, 600, 19.5), (8, 600, 41), (9, 400, 60), (10, 78, 40)]
for n, x, y in marks:
    cx, cy, r = x * K, y * K, 22
    dr.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(255, 255, 255), outline=(201, 118, 47), width=5)
    dr.text((cx, cy + 1), str(n), font=fnt, fill=(23, 23, 26), anchor="mm")
annotated = os.path.join(TMP, "anode_annotated.png"); ann.save(annotated)

def crop(path, box):
    im = Image.open(os.path.join(SHOTS, path)).convert("RGB")
    return im.crop(tuple(int(v * K) for v in box))
gap = 16
def strip(ims, name):
    s = Image.new("RGB", (sum(i.width for i in ims) + gap * (len(ims) - 1), max(i.height for i in ims)), (255, 255, 255))
    x = 0
    for i in ims: s.paste(i, (x, 0)); x += i.width + gap
    p = os.path.join(TMP, name); s.save(p); return p
tubes_img = strip([crop(p, (292, 0, 512, 212)) for p in ("anode_6l6_se.png", "anode_status.png", "anode_el84.png", "anode_kt88_triode.png")], "anode_tubes.png")
sel_img = strip([crop(p, (176, 252, 312, 362)) for p in ("anode_6l6_se.png", "anode_status.png", "anode_el84.png", "anode_kt88_triode.png")], "anode_selector.png")
bar_img = crop("anode_status.png", (600, 4, 780, 52)); bar_path = os.path.join(TMP, "anode_bar.png"); bar_img.save(bar_path)

# couverture : l'ampli (fond deja noir), reflet au sol, halo ambre tres doux qui retombe a zero avant les bords
_full = Image.open(os.path.join(SHOTS, "anode_el84.png")).convert("RGB")
_d = ImageDraw.Draw(_full)                                   # marque et barre de presets retirees (la couverture a son propre texte)
_d.rectangle((0, 0, int(160 * K), int(40 * K)), fill=(0, 0, 0)); _d.rectangle((int(600 * K), 0, _full.width, int(56 * K)), fill=(0, 0, 0))
amp = _full.crop((int(110 * K), int(0 * K), int(700 * K), int(395 * K)))
aw, ah = amp.size
pad = int(aw * 0.10)
canvas = Image.new("RGB", (aw + 2 * pad, int(ah * 1.22) + pad), (0, 0, 0))
yy, xx = np.mgrid[0:canvas.height, 0:canvas.width].astype(np.float64)
glow = np.exp(-(((xx - canvas.width / 2) / (aw * 0.55)) ** 2 + ((yy - (pad + ah * 0.35)) / (ah * 0.45)) ** 2)) * 16
edge = np.clip(np.minimum.reduce([xx, canvas.width - 1 - xx, yy, canvas.height - 1 - yy]) / (pad * 0.9), 0, 1)
glow *= edge ** 2
cv = np.dstack([glow * 1.0, glow * 0.55, glow * 0.22])
canvas = Image.fromarray(np.clip(cv, 0, 255).astype(np.uint8))
a = np.asarray(amp).astype(np.float64)
lum = a.mean(2)
mask = np.clip((lum - 10) / 22.0, 0, 1)                                 # le fond noir de la capture se fond dans la page
_yy, _xx = np.mgrid[0:ah, 0:aw].astype(np.float64)
mask *= np.clip(np.minimum.reduce([_xx, aw - 1 - _xx, _yy * 3, (ah - 1 - _yy) * 4]) / 60.0, 0, 1) ** 1.5   # aucun bord visible
base = np.asarray(canvas).astype(np.float64)
region = base[pad:pad + ah, pad:pad + aw]
region[:] = region * (1 - mask[..., None]) + a * mask[..., None]
refl = a[::-1]
fade = np.clip(1 - np.arange(ah) / (ah * 0.20), 0, 1) ** 1.6 * 0.18
rm = mask[::-1] * fade[:, None]
y0 = pad + ah + 4
h = min(ah, base.shape[0] - y0)
reg2 = base[y0:y0 + h, pad:pad + aw]
reg2[:] = reg2 * (1 - rm[:h, ..., None]) + refl[:h] * rm[:h, ..., None]
cover = os.path.join(TMP, "anode_cover.png"); Image.fromarray(np.clip(base, 0, 255).astype(np.uint8)).save(cover)

def logo(width):
    # logo officiel NINE HANDS | DSP (SVG vectoriel de la marque, rendu PNG transparent), centre
    p = os.path.join(ROOT, "assets", "nine_hands_logo_blanc.png")
    im = Image.open(p); i = RLImage(p, width=width, height=width * im.height / im.width); i.hAlign = "CENTER"; return i

def img(path, width):
    im = Image.open(path); return RLImage(path, width=width, height=width * im.height / im.width)

st = {
    "h1": ParagraphStyle("h1", fontName="PopB", fontSize=19, leading=23, textColor=ACC, spaceAfter=8, spaceBefore=4, keepWithNext=1),
    "h2": ParagraphStyle("h2", fontName="PopM", fontSize=12.5, leading=16, textColor=DARK, spaceAfter=4, spaceBefore=9, keepWithNext=1),
    "p": ParagraphStyle("p", fontName="Pop", fontSize=9.4, leading=13.8, textColor=DARK, spaceAfter=5),
    "small": ParagraphStyle("small", fontName="Pop", fontSize=8, leading=11, textColor=GREY, alignment=TA_CENTER),
    "cell": ParagraphStyle("cell", fontName="Pop", fontSize=8.3, leading=11, textColor=DARK),
    "cellb": ParagraphStyle("cellb", fontName="PopM", fontSize=8.3, leading=11, textColor=DARK),
    "cover": ParagraphStyle("cover", fontName="PopL", fontSize=40, leading=48, textColor=colors.white, alignment=TA_CENTER),
    "coversub": ParagraphStyle("coversub", fontName="Pop", fontSize=11.5, leading=16, textColor=colors.HexColor("#BDBAB4"), alignment=TA_CENTER),
    "brand": ParagraphStyle("brand", fontName="PopM", fontSize=13, leading=16, textColor=colors.white, alignment=TA_CENTER),
    "coverlang": ParagraphStyle("coverlang", fontName="PopM", fontSize=8.5, leading=11, textColor=ACC_L),
}
def P(t, s="p"): return Paragraph(t, st[s])
def bullets(items): return [P("&bull;&nbsp;&nbsp;" + i) for i in items]
def table(rows, widths):
    data = [[P(c, "cellb" if r == 0 else "cell") for c in row] for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1)
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.3, colors.HexColor("#DDD6CB")),
                           ("TOPPADDING", (0, 0), (-1, -1), 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
                           ("BACKGROUND", (0, 0), (-1, 0), LIGHT), ("LINEBELOW", (0, 0), (-1, 0), 0.8, ACC)]))
    return t

# ---------------- donnees (identiques au code livre : PluginProcessor.cpp)
TUBES = ["6L6", "EL34", "EL84", "KT88"]
import re as _re
FACTORY = [(m[0], int (m[1]), int (m[2]), int (m[3]), float (m[4]), float (m[5]), float (m[6]), float (m[7]))
           for m in _re.findall (r'\{ "([^"]+)",\s+(\d),\s+(\d),\s+(\d),\s+([\d.]+)f,\s+([\d.]+)f,\s+([\d.]+)f,\s+(-?[\d.]+)f \}',
                                 open (os.path.join (ROOT, "PluginProcessor.cpp"), encoding="utf-8").read())]
assert len (FACTORY) == 14, len (FACTORY)
DESC = {
 "en": ["Marshall-style EL34 pair pushed into crunch: mid-forward bark, Depth and Contour slightly up.",
        "The same EL34 stage close to full volume: compressed, singing lead with the presence up.",
        "Fender-style 6L6 pair at low volume: wide, clean and round, extra headroom.",
        "6L6 pushed like a tweed Bassman: thick low end, soft and dynamic break-up.",
        "Vox-style EL84 pair (cathode bias, tube rectifier): chimey top end, early, glassy break-up.",
        "EL84 pair cranked: sag and cathode compression, gritty and touch-sensitive.",
        "KT88 pair as in a big bass head: huge headroom, firm low end, Contour down.",
        "KT88 triode-strapped at low volume: hi-fi clean, warm and very linear, ideal as a studio colour.",
        "One 6L6 in class A, Champ-like: second-harmonic richness, breaks up early and blooms.",
        "One EL84 in class A, wide open: compressed, fuzzy, saturated small-amp tone.",
        "One EL34 in class A, triode connection: soft, warm, low distortion, very musical.",
        "6L6 pair in triode connection at mid volume: gentle compression and glue on DI tracks.",
        "EL34 pair with Depth and Contour high, to follow a high-gain preamp or drive pedal: tight resonance, cutting presence.",
        "One KT88 in class A, deep resonance, dark: thick, saturated doom bass."],
 "fr": ["Paire d'EL34 façon Marshall poussée en crunch : médiums en avant, Depth et Contour un peu montés.",
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
        "Une KT88 en classe A, résonance profonde, son sombre : basse doom épaisse et saturée."]}

def section(lang):
    E = lang == "en"
    T = (lambda en, fr: en if E else fr)
    W = 174 * mm
    f = []
    # couverture
    f += [Spacer(1, 6 * mm), P(T("ENGLISH", "FRANÇAIS"), "coverlang"), Spacer(1, 22 * mm), img(cover, W), Spacer(1, 12 * mm),
          P("A&nbsp;&nbsp;N&nbsp;&nbsp;O&nbsp;&nbsp;D&nbsp;&nbsp;E", "cover"), Spacer(1, 3 * mm),
          P(T("Tube power stage as a guitar and bass preamp<br/>6L6 · EL34 · EL84 · KT88 — VST3 for Windows and macOS",
              "Étage de puissance à lampes en préampli guitare et basse<br/>6L6 · EL34 · EL84 · KT88 — VST3 pour Windows et macOS"), "coversub"),
          Spacer(1, 4 * mm), P(T("User manual", "Notice d'utilisation") + " · version " + VERSION, "coversub"),
          Spacer(1, 26 * mm), logo(62 * mm),
          NextPageTemplate("p"), PageBreak()]

    f += [P(T("1. Introduction", "1. Présentation"), "h1"),
          P(T("<b>Anode</b> recreates the <b>power section of a tube amplifier</b> — the part with the big output tubes and the output transformer — and lets you use it as a preamp for guitar or bass: in front of an amp simulator, a cab IR, or on its own to warm up a DI track. The tube, the topology and the way the tube is wired are chosen on the front panel exactly as on the reference design.",
              "<b>Anode</b> recrée la <b>section de puissance d'un ampli à lampes</b> — la partie avec les grosses lampes de sortie et le transformateur de sortie — et permet de l'utiliser en préampli guitare ou basse : devant un simulateur d'ampli, une IR de baffle, ou seule pour réchauffer une piste DI. La lampe, la topologie et le montage se choisissent sur la façade, exactement comme sur le visuel d'origine.")),
          P(T("Nothing here is a generic waveshaper: every stage of a real tube amp is computed, at 352.8 kHz or more inside the plugin, from published tube models and the values of real circuits:",
              "Rien ici n'est une simple courbe de saturation : chaque étage d'un vrai ampli à lampes est calculé, à 352,8 kHz ou plus à l'intérieur du plugin, à partir de modèles de lampes publiés et des valeurs de vrais circuits :")),
          *bullets([T("<b>Input triode (V1, 12AX7)</b> on its load line, with grid stopper, grid conduction and a cathode capacitor that really charges: the bias moves with your attack, so the amp « blooms », compresses softly and adds a touch of second harmonic as you dig in.",
                      "<b>Triode d'entrée (V1, 12AX7)</b> sur sa droite de charge, avec résistance de grille, conduction de grille et un condensateur de cathode qui se charge vraiment : la polarisation bouge avec l'attaque, l'ampli « s'épanouit », compresse doucement et ajoute une pointe de 2e harmonique quand on attaque fort."),
                    T("<b>Long-tailed-pair phase inverter</b> (12AX7 or 12AT7 depending on the amp) solved exactly: unequal plate loads, the in-phase side that cuts off cleanly while the other keeps going, its own clipping before or after the output tubes.",
                      "<b>Déphaseur à paire différentielle</b> (12AX7 ou 12AT7 selon l'ampli) résolu exactement : charges de plaque inégales, côté en phase qui se coupe net pendant que l'autre continue, son propre écrêtage avant ou après les lampes de sortie."),
                    T("<b>Output tubes</b> fitted to the datasheets, solved on the transformer load; in push-pull the pair is matched within 4 %, like a real pair. Coupling capacitors with grid conduction give the bias shift that makes a pushed amp « choke ».",
                      "<b>Lampes de sortie</b> calées sur les fiches techniques, résolues sur la charge du transformateur ; en push-pull la paire est appariée à 4 % près, comme une vraie paire. Les condensateurs de liaison et la conduction de grille donnent le glissement de polarisation qui fait « s'étrangler » un ampli poussé."),
                    T("<b>Two-cell power supply</b>: the plate B+ sags first, the screen node follows later, so the compression « breathes » with your playing.",
                      "<b>Alimentation à deux cellules</b> : le B+ des plaques s'affaisse d'abord, le nœud des écrans suit avec un retard, la compression « respire » avec le jeu."),
                    T("<b>Global negative feedback</b> shaped by Depth and Contour, <b>output transformer</b> (primary inductance, leakage, core saturation) and the <b>speaker load</b> of each reference amp.",
                      "<b>Contre-réaction globale</b> façonnée par Depth et Contour, <b>transformateur de sortie</b> (inductance primaire, fuites, saturation du noyau) et <b>charge du haut-parleur</b> de chaque ampli de référence.")]),
          P(T("Installation", "Installation"), "h2"),
          *bullets([T("<b>Windows</b>: unzip <b>Anode-VST3-Windows</b> and copy <b>Anode.vst3</b> to <b>C:\\Program Files\\Common Files\\VST3\\</b>.",
                      "<b>Windows</b> : décompressez <b>Anode-VST3-Windows</b> et copiez <b>Anode.vst3</b> dans <b>C:\\Program Files\\Common Files\\VST3\\</b>."),
                    T("<b>macOS</b> (Apple Silicon and Intel): unzip <b>Anode-VST3-macOS.zip</b>, copy <b>Anode.vst3</b> to <b>~/Library/Audio/Plug-Ins/VST3/</b>. If macOS blocks it, run <b>xattr -cr ~/Library/Audio/Plug-Ins/VST3/Anode.vst3</b> in Terminal.",
                      "<b>macOS</b> (Apple Silicon et Intel) : décompressez <b>Anode-VST3-macOS.zip</b>, copiez <b>Anode.vst3</b> dans <b>~/Library/Audio/Plug-Ins/VST3/</b>. Si macOS le bloque : <b>xattr -cr ~/Library/Audio/Plug-Ins/VST3/Anode.vst3</b> dans le Terminal."),
                    T("<b>Reaper</b>: Options > Preferences > Plug-ins > VST > <b>Re-scan</b>. The plugin appears as <b>Anode (NINE HANDS | DSP)</b>. To use MIDI control, send the MIDI input to the track (record-arm with your controller as input, or a MIDI send).",
                      "<b>Reaper</b> : Options > Preferences > Plug-ins > VST > <b>Re-scan</b>. Le plugin apparaît sous <b>Anode (NINE HANDS | DSP)</b>. Pour le pilotage MIDI, envoyez l'entrée MIDI à la piste (armement avec votre contrôleur en entrée, ou envoi MIDI).")]),
          PageBreak()]

    f += [P(T("2. Interface overview", "2. Vue d'ensemble de l'interface"), "h1"), img(annotated, W), Spacer(1, 3 * mm),
          table([["#", T("Control", "Commande"), T("Role", "Rôle")],
                 ["1", "Tube Model", T("Output tube: 6L6, EL34, EL84, KT88", "Lampe de sortie : 6L6, EL34, EL84, KT88")],
                 ["2", "Single-Ended / Push-Pull", T("One tube in class A, or two tubes in opposition (class AB)", "Une lampe en classe A, ou deux lampes en opposition (classe AB)")],
                 ["3", "Triode / Pentode", T("Screen grid tied to the plate, or fed from the supply", "Écran relié à la plaque, ou alimenté par l'alimentation")],
                 ["4", "Depth", T("Low-frequency resonance (feedback removed in the bass)", "Résonance grave (contre-réaction retirée dans le grave)")],
                 ["5", "Contour", T("Presence (feedback removed in the treble; Cut with the EL84 and in single-ended)", "Présence (contre-réaction retirée dans l'aigu ; Cut avec l'EL84 et en single-ended)")],
                 ["6", "Volume", T("Master volume into the power stage: level and saturation", "Volume maître vers l'étage de puissance : niveau et saturation")],
                 ["7", T("Preset bar", "Barre de presets"), T("Previous / next preset (arrows), preset menu (name)", "Preset précédent / suivant (flèches), menu des presets (nom)")],
                 ["8", T("Status line", "Ligne d'état"), T("Value of the control you move, for 2.5 s", "Valeur de la commande qui bouge, pendant 2,5 s")],
                 ["9", T("Tubes", "Lampes"), T("The tubes shown follow the selector; the glow follows the current, the plates redden after long hard playing", "Les lampes affichées suivent le sélecteur ; la lueur suit le courant, les plaques rougissent après un long jeu poussé")],
                 ["10", "NINE HANDS | DSP", T("Right-click anywhere: size, presets, output level, MIDI", "Clic droit n'importe où : taille, presets, niveau de sortie, MIDI")]],
                [10 * mm, 42 * mm, 122 * mm]),
          PageBreak()]

    f += [P(T("3. Controls", "3. Les commandes"), "h1"),
          P(T("Tube Model", "Tube Model (sélecteur de lampe)"), "h2"),
          img(sel_img, W), P(T("6L6 · EL34 · EL84 · KT88 — click a label, click the knob (next position), drag sideways or use the mouse wheel.",
                               "6L6 · EL34 · EL84 · KT88 — cliquez sur une inscription, sur le bouton (position suivante), glissez de côté ou utilisez la molette."), "small"),
          Spacer(1, 2 * mm),
          table([[T("Tube", "Lampe"), T("Datasheet point used for calibration", "Point de fiche utilisé pour le calage"), T("Reference amp (push-pull)", "Ampli de référence (push-pull)"), T("Character", "Caractère")],
                 ["6L6GC", "Va = Vg2 = 250 V, Vg1 −14 V → 72 mA, gm 6 mA/V",
                  T("Fender blackface: 460 V, Raa 4 kΩ, fixed bias, 12AT7 phase inverter, strong feedback", "Fender blackface : 460 V, Raa 4 kΩ, polarisation fixe, déphaseur 12AT7, contre-réaction forte"),
                  T("American: wide, round, big clean headroom, firm bass", "Américain : large, rond, beaucoup de clair, grave ferme")],
                 ["EL34", "Va = Vg2 = 250 V, Vg1 −13,5 V → 100 mA, gm 11 mA/V",
                  T("Marshall JMP / JCM800: 455 V, Raa 3.4 kΩ, fixed bias, « bright » V1 (2.7 kΩ / 0.68 µF), 12AX7 phase inverter, 4x12", "Marshall JMP / JCM800 : 455 V, Raa 3,4 kΩ, polarisation fixe, V1 « bright » (2,7 kΩ / 0,68 µF), déphaseur 12AX7, 4x12"),
                  T("British: tight low end, mid-forward, aggressive break-up", "Britannique : grave serré, médiums en avant, saturation agressive")],
                 ["EL84", "Va = Vg2 = 250 V, Vg1 −7,3 V → 48 mA, gm 11,3 mA/V",
                  T("Vox AC30: 335 V, Raa 8 kΩ, cathode bias, GZ34 rectifier, 12AX7 phase inverter, almost no feedback", "Vox AC30 : 335 V, Raa 8 kΩ, polarisation cathode, redresseur GZ34, déphaseur 12AX7, presque pas de contre-réaction"),
                  T("Chimey, compresses and breaks up first", "Carillonnant, compresse et sature en premier")],
                 ["KT88", "Va = Vg2 = 250 V, Vg1 −15 V → 140 mA, gm 11,5 mA/V",
                  T("Ampeg SVT / hi-fi: 520 V, Raa 4.5 kΩ, fixed bias, low-impedance 12AT7 phase inverter, 8x10", "Ampeg SVT / hi-fi : 520 V, Raa 4,5 kΩ, polarisation fixe, déphaseur 12AT7 basse impédance, 8x10"),
                  T("Most headroom, deep and tight: bass and hi-fi", "La plus grande marge, grave profond et tenu : basse et hi-fi")]],
                [15 * mm, 46 * mm, 66 * mm, 47 * mm]),
          P(T("The tube models (Koren) were adjusted so that each tube draws its datasheet current with its datasheet transconductance. Changing the tube while playing crossfades over 35 ms, without any click.",
              "Les modèles de lampes (Koren) ont été ajustés pour que chaque lampe tire le courant et la pente de sa fiche technique. Changer de lampe en jouant fait un fondu de 35 ms, sans clic.")),
          P(T("The pair of tubes on top of the amp changes with the selector, with its real shape, all four drawn to the same scale from their real dimensions (KT88 110 × 52 mm, EL34 100 × 34, 6L6GC 97 × 38, EL84 70 × 22): 6L6GC a little wider than the EL34, with dark plates and a silver getter; slender noval EL84 with no base; tall « coke bottle » KT88. The filaments glow according to their real heater current (KT88 1.6 A brightest, EL84 0.76 A dimmest). While you play, the glow follows the current through the tubes (average current in class AB, current swing in class A), and after several seconds of hard playing the plates slowly turn dull red, then cool down when you stop.",
              "La paire de lampes posée sur l'ampli change avec le sélecteur, avec sa vraie forme, les quatre à la même échelle d'après leurs dimensions réelles (KT88 110 × 52 mm, EL34 100 × 34, 6L6GC 97 × 38, EL84 70 × 22) : 6L6GC un peu plus large que l'EL34, aux plaques sombres et getter argenté ; EL84 noval élancée sans culot ; grande KT88 en « bouteille ». Les filaments brillent selon leur vrai courant de chauffage (KT88 1,6 A la plus vive, EL84 0,76 A la plus discrète). Quand on joue, la lueur suit le courant dans les lampes (courant moyen en classe AB, variations du courant en classe A), et après plusieurs secondes de jeu très poussé les plaques rougissent lentement, puis refroidissent quand on s'arrête.")),
          img(tubes_img, W * 0.9),
          P(T("Single-Ended / Push-Pull", "Single-Ended / Push-Pull"), "h2"),
          *bullets([T("<b>Single-Ended</b>: one tube in class A, cathode bias, DC current through the transformer, driven by two 12AX7 stages as in a Fender Champ or a Vox AC4. A strong <b>second harmonic</b>, soft and early compression, rich and « fat » saturation. Less low end (smaller transformer and cabinet).",
                      "<b>Single-Ended</b> : une seule lampe en classe A, polarisation cathode, courant continu dans le transformateur, attaquée par deux étages 12AX7 comme dans un Fender Champ ou un Vox AC4. <b>2e harmonique</b> fort, compression douce et précoce, saturation riche et « grasse ». Moins de grave (transformateur et baffle plus petits)."),
                    T("<b>Push-Pull</b>: two tubes in opposition (class AB), driven by a phase inverter. Even harmonics cancel, <b>odd harmonics</b> dominate when pushed, more power and headroom; crossover distortion appears when the coupling capacitors charge (hard playing).",
                      "<b>Push-Pull</b> : deux lampes en opposition (classe AB), attaquées par un déphaseur. Les harmoniques pairs s'annulent, les <b>impairs</b> dominent quand on pousse, plus de puissance et de marge ; distorsion de croisement quand les condensateurs de liaison se chargent (jeu appuyé).")]),
          P(T("Triode / Pentode", "Triode / Pentode"), "h2"),
          *bullets([T("<b>Pentode</b>: the screen grid is fed from the power supply (through its resistor, so it sags too). More gain, more power, a harder « knee » when the plate swings low, high output impedance: the speaker resonance and the treble rise are emphasised.",
                      "<b>Pentode</b> : l'écran est alimenté par l'alimentation (via sa résistance, il s'affaisse donc aussi). Plus de gain, plus de puissance, « coude » plus dur quand la plaque descend, impédance de sortie élevée : la résonance du haut-parleur et la montée d'aigu ressortent."),
                    T("<b>Triode</b>: screen tied to the plate. Same bias (as on a real amp), lower gain and power, softer clipping, lower output impedance: smoother, more hi-fi, slightly quieter.",
                      "<b>Triode</b> : écran relié à la plaque. Même polarisation (comme sur un ampli réel), moins de gain et de puissance, écrêtage plus doux, impédance de sortie plus basse : plus lisse, plus hi-fi, un peu moins fort.")]),
          P("Depth", "h2"),
          P(T("Low-frequency resonance. Depth removes the negative feedback in the bass (below about 170 Hz). The stage gives more low end, and since the amp then holds the speaker less firmly (higher output impedance), the cabinet resonance comes through: the amp « breathes ». This also works when the power tubes are saturated. 5 = reference voicing; 0 = tight and controlled (about −3 dB around 100 Hz); 10 = up to about +5 dB clean and +3 to +4 dB driven in pentode push-pull (less in triode and with the EL84, which has almost no feedback). Because the bass is less controlled, high Depth also saturates sooner in the low end.",
              "Résonance grave. Depth retire la contre-réaction dans le grave (sous environ 170 Hz). L'étage donne plus de bas, et comme l'ampli tient alors moins le haut-parleur (impédance de sortie plus élevée), la résonance du baffle passe : l'ampli « respire ». Cela agit aussi quand les lampes de puissance saturent. 5 = réglage de référence ; 0 = serré et tenu (environ −3 dB vers 100 Hz) ; 10 = jusqu'à environ +5 dB en clair et +3 à +4 dB en saturation en pentode push-pull (moins en triode et avec l'EL84, qui n'a presque pas de contre-réaction). Le grave étant moins tenu, un Depth élevé sature aussi plus tôt dans le grave.")),
          P("Contour", "h2"),
          P(T("Presence and cut. Contour removes the feedback in the treble (from about 1.8 kHz), like the Presence control of a Marshall or a Fender, and lets the speaker's rising impedance through, which also works when the power tubes are saturated. With the EL84 in push-pull (AC30, almost no feedback), Contour also drives the AC30 <b>Cut</b> filter between the phase inverter and the power tubes; in single-ended, a tone filter after the driver tube (AC4 style). 5 = reference voicing; 0 = dark, about −4 dB at 3 kHz (−8 dB with the EL84 and in single-ended); 10 = bright, about +6 to +7 dB at 3 kHz clean and +2 to +5 dB in the 2–4 kHz band when driven. Use it to place the guitar in the mix before the cab simulator; turn it down for dark doom or bass tones.",
              "Présence et Cut. Contour retire la contre-réaction dans l'aigu (à partir d'environ 1,8 kHz), comme le Presence d'un Marshall ou d'un Fender, et laisse passer la montée d'impédance du haut-parleur, ce qui agit aussi quand les lampes de puissance saturent. Avec l'EL84 en push-pull (AC30, presque sans contre-réaction), Contour pilote aussi le filtre <b>Cut</b> de l'AC30, entre le déphaseur et les lampes de puissance ; en single-ended, un filtre de tonalité après la lampe d'attaque (façon AC4). 5 = réglage de référence ; 0 = sombre, environ −4 dB à 3 kHz (−8 dB avec l'EL84 et en single-ended) ; 10 = brillant, environ +6 à +7 dB à 3 kHz en clair et +2 à +5 dB dans la bande 2–4 kHz en saturation. À utiliser pour placer la guitare dans le mix avant le simulateur de baffle ; à baisser pour les sons doom ou basse sombres.")),
          P("Volume", "h2"),
          P(T("Master volume into the phase inverter and the output tubes: −24 dB at 0, 0 dB at 5, +24 dB at 10. Turning it up gives more level, then grid clipping, sag, bias shift and power-tube saturation, exactly as cranking a real power amp. The output is partly compensated, so the sound gets louder and dirtier without jumping by 24 dB. With a guitar at normal level: about 5 = clean, 7 = break-up, 10 = saturated (EL84 earliest, KT88 latest). The extra output level (−12 to +12 dB) is in the right-click menu > <b>Output level</b> (automatable parameter « Output »).",
              "Volume maître vers le déphaseur et les lampes de sortie : −24 dB à 0, 0 dB à 5, +24 dB à 10. En montant : plus de niveau, puis écrêtage de grille, sag, glissement de polarisation et saturation des lampes de puissance, exactement comme en poussant un vrai ampli. La sortie est en partie compensée : le son devient plus fort et plus sale sans sauter de 24 dB. Avec une guitare à niveau normal : environ 5 = clair, 7 = début de saturation, 10 = saturé (EL84 en premier, KT88 en dernier). Le niveau de sortie (−12 à +12 dB) est dans le clic droit > <b>Output level</b> (paramètre automatisable « Output »).")),
          PageBreak()]

    f += [P(T("4. MIDI and expression pedals", "4. MIDI et pédales d'expression"), "h1"),
          P(T("No CC is assigned by default: everything goes through <b>MIDI Learn</b>. Right-click a control > <b>MIDI Learn</b>, then move the pedal or press the switch on your controller; the status line confirms « Depth = CC 11 ». Right-click > MIDI lists every assignment and lets you clear them.",
              "Aucun CC n'est assigné par défaut : tout passe par le <b>MIDI Learn</b>. Clic droit sur une commande > <b>MIDI Learn</b>, puis bougez la pédale ou appuyez sur le switch de votre contrôleur ; la ligne d'état confirme « Depth = CC 11 ». Clic droit > MIDI liste toutes les assignations et permet de les effacer.")),
          table([[T("Learnable", "Assignable"), T("Behaviour", "Comportement")],
                 ["Depth, Contour, Volume", T("Continuous, no zipper: linear ramp between messages then smoothing; the pedal travel is learned automatically (a pedal sending only 2–116 still reaches 0 and 100 %); 14-bit CC (CC n + n+32) accepted for CC 0–31",
                                              "Continu, sans saccade : rampe linéaire entre deux messages puis lissage ; la course de la pédale est apprise automatiquement (une pédale qui n'envoie que 2–116 atteint quand même 0 et 100 %) ; CC 14 bits (CC n + n+32) accepté pour les CC 0–31")],
                 ["Tube Model", T("0–31 6L6, 32–63 EL34, 64–95 EL84, 96–127 KT88", "0–31 6L6, 32–63 EL34, 64–95 EL84, 96–127 KT88")],
                 ["Single-Ended / Push-Pull", T("≥ 64 = Push-Pull", "≥ 64 = Push-Pull")],
                 ["Triode / Pentode", T("≥ 64 = Pentode", "≥ 64 = Pentode")],
                 ["Program Change", T("1–14 = factory presets", "1–14 = presets d'usine")]],
                [44 * mm, 130 * mm]),
          P(T("5. Window", "5. Fenêtre"), "h1"),
          P(T("The window opens at <b>60 %</b> and can never be made smaller. Right-click > <b>Plugin size</b>: 60 % / 75 % / 100 %, and <b>Default size (new windows)</b> to choose the size of every new Anode window. Drag the corner to resize freely (proportions locked). The <b>status line</b> under the preset bar shows the value of the control you move.",
              "La fenêtre s'ouvre à <b>60 %</b> et ne peut jamais être réduite en dessous. Clic droit > <b>Plugin size</b> : 60 % / 75 % / 100 %, et <b>Default size (new windows)</b> pour choisir la taille de toutes les nouvelles fenêtres d'Anode. Tirez le coin pour redimensionner librement (proportions bloquées). La <b>ligne d'état</b> sous la barre de presets affiche la valeur de la commande qui bouge.")),
          img(bar_path, W * 0.55), Spacer(1, 2 * mm),
          P(T("6. Presets", "6. Presets"), "h1"),
          P(T("Arrows of the preset bar = previous / next; click the name for the menu. 9 user slots (Save / Load user preset). Any change shows « Edited ».",
              "Flèches de la barre = précédent / suivant ; clic sur le nom pour le menu. 9 emplacements utilisateur (Save / Load user preset). Toute modification affiche « Edited ».")),
          table([[T("Preset", "Preset"), T("Tube · stage · D / C / V · out", "Lampe · montage · D / C / V · sortie"), T("Description", "Description")]] +
                [[f"{i + 1}. {p[0]}", f"{TUBES[p[1]]} · {'PP' if p[2] else 'SE'} {'pent.' if p[3] else 'triode'} · {p[4]:g} / {p[5]:g} / {p[6]:g} · {p[7]:+g} dB", DESC[lang][i]]
                 for i, p in enumerate(FACTORY)],
                [44 * mm, 46 * mm, 84 * mm]),
          PageBreak()]

    f += [P(T("7. Tips", "7. Conseils d'utilisation"), "h1"),
          *bullets([T("<b>Chain in Reaper</b>: guitar > drive / EQ pedals > <b>Anode</b> > cab IR or amp simulator (Two Notes Genome with its power amp bypassed, or its preamp after Anode for a full amp). Anode replaces or adds the power-tube stage that most amp sims simplify.",
                      "<b>Chaîne dans Reaper</b> : guitare > pédales de saturation / EQ > <b>Anode</b> > IR de baffle ou simulateur d'ampli (Two Notes Genome avec sa section de puissance coupée, ou son préampli après Anode pour un ampli complet). Anode remplace ou ajoute l'étage de lampes de puissance que la plupart des simulateurs simplifient."),
                    T("<b>7-string in G and lower</b>: keep Depth at 4–6 in pentode push-pull; push-pull KT88 or 6L6 keep the low string tight, single-ended and EL84 will « bloom » and saturate the bottom. Contour 6–7 keeps the definition.",
                      "<b>7 cordes en G et plus bas</b> : gardez Depth entre 4 et 6 en pentode push-pull ; en push-pull, KT88 ou 6L6 gardent la corde grave serrée, le single-ended et l'EL84 vont « s'épanouir » et saturer le bas. Contour 6–7 garde la définition."),
                    T("<b>Bass</b>: KT88 push-pull, Volume 5–6, Contour 3–4: SVT-like weight. Single-ended KT88 for fuzz-like doom.",
                      "<b>Basse</b> : KT88 push-pull, Volume 5–6, Contour 3–4 : poids façon SVT. KT88 single-ended pour un doom proche du fuzz."),
                    T("<b>Studio colour</b>: triode, Volume 4–5: a few percent of harmonics and gentle compression on DI guitars, bass or even keys.",
                      "<b>Couleur de studio</b> : triode, Volume 4–5 : quelques pourcents d'harmoniques et une compression douce sur guitares DI, basse ou même claviers."),
                    T("<b>Expression pedal on Volume</b>: from clean to saturated with the foot, exactly like riding the master of a tube amp.",
                      "<b>Pédale d'expression sur Volume</b> : du clair au saturé au pied, exactement comme en jouant avec le master d'un ampli à lampes.")]),
          P(T("8. Specifications", "8. Caractéristiques techniques"), "h1"),
          table([[T("Item", "Élément"), T("Value", "Valeur")],
                 [T("Format", "Format"), T("VST3, Windows (x64) and macOS (Apple Silicon + Intel), mono or stereo", "VST3, Windows (x64) et macOS (Apple Silicon + Intel), mono ou stéréo")],
                 [T("Internal rate", "Fréquence interne"), T("≥ 352.8 kHz (×8 at 44.1 / 48 kHz, ×4 at 88.2 / 96 kHz, ×2 at 176.4 / 192 kHz), polyphase IIR filters", "≥ 352,8 kHz (×8 à 44,1 / 48 kHz, ×4 à 88,2 / 96 kHz, ×2 à 176,4 / 192 kHz), filtres IIR polyphasés")],
                 [T("Latency", "Latence"), T("6 samples at 48 kHz (reported to the host)", "6 échantillons à 48 kHz (signalée à l'hôte)")],
                 [T("CPU", "Processeur"), T("About 6 % of one core at 48 kHz (identical channels computed once)", "Environ 6 % d'un cœur à 48 kHz (canaux identiques calculés une fois)")],
                 [T("Tube models", "Modèles de lampes"), T("Koren pentode / triode-strapped output tubes fitted to datasheets; Koren 12AX7 / 12AT7 input stage, driver (SE) and long-tailed-pair phase inverter (PP), solved exactly", "Lampes de sortie Koren pentode / montage triode calées sur les fiches ; 12AX7 / 12AT7 (Koren) pour l'entrée, l'attaque (SE) et le déphaseur à paire différentielle (PP), résolus exactement")],
                 [T("Circuit effects", "Effets du circuit"), T("Input-triode cathode dynamics, grid conduction and bias shift, two-cell supply (B+ then screens), cathode bias drift, pair matched within 4 %, global feedback, output transformer (primary inductance, leakage, core saturation), speaker load per reference amp", "Dynamique de cathode de la triode d'entrée, conduction de grille et glissement de polarisation, alimentation à deux cellules (B+ puis écrans), dérive de la polarisation cathode, paire appariée à 4 %, contre-réaction globale, transformateur de sortie (inductance primaire, fuites, saturation du noyau), charge du haut-parleur propre à chaque ampli de référence")],
                 [T("Parameters", "Paramètres"), "Tube Model, Topology, Triode / Pentode, Depth, Contour, Volume, Output"],
                 [T("Switching", "Commutations"), T("35 ms crossfade between configurations, no click", "Fondu de 35 ms entre configurations, sans clic")],
                 [T("Presets", "Presets"), T("14 factory, 9 user", "14 d'usine, 9 utilisateur")],
                 ["MIDI", T("MIDI Learn only, expression pedal smoothing, 14-bit CC, Program Change", "MIDI Learn uniquement, lissage des pédales d'expression, CC 14 bits, Program Change")],
                 [T("Window", "Fenêtre"), T("60 % (default and minimum), 75 %, 100 %; anti-freeze v2 (software renderer on Windows)", "60 % (par défaut et minimum), 75 %, 100 % ; anti-gel v2 (rendu logiciel sous Windows)")]],
                [44 * mm, 130 * mm])]
    return f

def deco(c, doc):
    c.saveState()
    c.setFillColor(DARK); c.rect(0, A4[1] - 6 * mm, A4[0], 6 * mm, stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, A4[1] - 6.8 * mm, A4[0], 0.8 * mm, stroke=0, fill=1)
    c.setFont("Pop", 7.5); c.setFillColor(GREY)
    c.drawString(18 * mm, 10 * mm, "Anode (NINE HANDS | DSP) · v" + VERSION)
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()

def cover_bg(c, doc):
    c.saveState(); c.setFillColor(colors.black); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, A4[1] - 3 * mm, A4[0], 3 * mm, stroke=0, fill=1); c.restoreState()

doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=18 * mm,
                      title="Anode - User manual / Notice", author="NINE HANDS | DSP")
doc.addPageTemplates([PageTemplate(id="cover", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="fc")], onPage=cover_bg),
                      PageTemplate(id="p", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")], onPage=deco)])
story = section("en") + [NextPageTemplate("cover"), PageBreak()] + section("fr")
doc.build(story)
print("ok", OUT)
