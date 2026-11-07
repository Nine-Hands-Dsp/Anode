# Anode — NINE HANDS | DSP

Étage de puissance à lampes (VST3, Windows et macOS) utilisé comme préampli guitare / basse.
Interface d'après le visuel « Power Amplifier » : sélecteur de lampe 6L6 / EL34 / EL84 / KT88,
inverseurs Single-Ended / Push-Pull et Triode / Pentode, potards Depth, Contour et Volume.

## Ce qui est modélisé (v1.1)

| Étage | Modèle |
|---|---|
| V1 (entrée) | triode 12AX7 en cathode commune résolue sur sa droite de charge (Koren), résistance de grille 68 k et conduction de grille, condensateur de cathode dynamique (la polarisation glisse avec l'attaque : bloom, compression douce, 2e harmonique) ; 0 dBFS = 1,5 V crête sur la grille |
| Volume (master) | −24 dB (0) … 0 dB (5) … +24 dB (10) entre V1 et le déphaseur, sortie compensée en partie |
| Déphaseur PP | paire différentielle 12AX7 ou 12AT7 résolue exactement hors ligne (queue commune, charges inégales, conduction de grille), table cubique vérifiée contre une résolution brute |
| Attaque SE | seconde triode 12AX7 (V2) façon Champ, cathode dynamique |
| Liaison | condensateur + résistance de grille + conduction de grille lissée (Langmuir) → glissement de polarisation (« blocking ») |
| Lampes | modèle de Koren pentode, KG1 / KG2 recalés sur les fiches techniques ; paire appariée à 4 % près en PP |
| Pentode / Triode | écran alimenté par le nœud des écrans via sa résistance / écran relié à la plaque |
| Alimentation | deux cellules : B+ des plaques (redresseur + 1er condensateur), puis nœud des écrans (self / résistance + 2e condensateur) ; polarisation cathode (EL84, tous les SE) qui dérive avec le courant moyen |
| Contre-réaction | globale vers l'entrée du déphaseur ; **Depth** la retire dans le grave, **Contour** dans l'aigu (dès 1,8 kHz) ; l'impédance de sortie suit (amortissement du haut-parleur : résonance grave et montée inductive), ce qui agit aussi en saturation ; avec l'EL84 en push-pull, Contour pilote aussi le Cut de l'AC30 (entre déphaseur et lampes), en single-ended un filtre de tonalité après V2 |
| Transformateur | inductance primaire, fuites, saturation du noyau dans le grave |
| Haut-parleur | résonance et montée inductive propres à chaque ampli de référence |

### Voicings (amplis de référence)

| Lampe | Push-pull | Single-ended |
|---|---|---|
| 6L6 | Fender blackface : V1 100 k / 1,5 k / 25 µF, déphaseur 12AT7 (420 V, 82 k / 100 k), contre-réaction forte, 2x12 ouvert | Champ : V1 + V2 12AX7 |
| EL34 | Marshall JMP / JCM800 : V1 « bright » 2,7 k / 0,68 µF, déphaseur 12AX7 (330 V, 82 k / 100 k, queue 10 k), 4x12 fermé | idem |
| EL84 | Vox AC30 : V1 220 k, déphaseur 12AX7 à longue queue (47 k), polarisation cathode, GZ34, presque pas de contre-réaction | AC4 |
| KT88 | Ampeg SVT / hi-fi : déphaseur 12AT7 basse impédance (56 k), énorme marge, 8x10 | idem |

### Lampes (points de calage)

| Lampe | Point de fiche (Va = Vg2 = 250 V) | gm | Montage PP | Montage SE |
|---|---|---|---|---|
| 6L6GC | Vg1 −14 V → 72 mA | 6,0 mA/V | B+ 460 V, Raa 4 k, polarisation fixe (Bassman / Twin) | B+ 350 V, 2,5 k |
| EL34 | Vg1 −13,5 V → 100 mA | 11 mA/V | B+ 455 V, Raa 3,4 k, polarisation fixe (Plexi) | B+ 350 V, 2,7 k |
| EL84 | Vg1 −7,3 V → 48 mA | 11,3 mA/V | B+ 335 V, Raa 8 k, polarisation cathode, GZ34 (AC15 / AC30) | B+ 285 V, 5,2 k |
| KT88 | Vg1 −15 V → 140 mA | 11,5 mA/V | B+ 520 V, Raa 4,5 k, polarisation fixe (SVT / hi-fi) | B+ 385 V, 2 k |

Marge avant saturation : EL84 < EL34 < 6L6 < KT88.

## Technique

- Sur-échantillonnage IIR polyphasé : taux interne ≥ 352,8 kHz (×8 à 44,1 / 48 kHz), latence 6 échantillons à 48 kHz. Repliement à fond (sinus 3 kHz, Volume 10) : −58 à −66 dB en PP, −81 à −85 dB en SE (v1.0 : −31 à −52 dB).
- 16 configurations calibrées et stabilisées au repos dans `prepareToPlay` ; changement de lampe / topologie / mode en fondu enchaîné de 35 ms, sans bruit.
- Canaux identiques (guitare mono sur piste stéréo) : un seul calcul. Environ 6 % d'un cœur à 48 kHz.
- Tables des triodes et du déphaseur construites une fois au premier chargement (≈ 1 s), partagées entre les 16 configurations.
- Bancs de mesure hors ligne dans `tests/` (repliement, harmoniques, réponses, stabilité, vérification du déphaseur).
- Anti-gel v2 : rendu logiciel sous Windows, aucun calcul lourd dans `paint()`, chien de garde, aucune notification à l'hôte depuis le thread audio, aucune recherche de paramètre par chaîne dans `processBlock`.
- MIDI : MIDI Learn uniquement (clic droit sur une commande), aucun CC assigné par défaut ; pédale d'expression sans saccades (rampe + course apprise, CC 14 bits) ; Program Change → presets d'usine.
- Fenêtre : « 60 % » (947 × 493) par défaut et minimum, 75 %, 100 % ; taille par défaut des nouvelles fenêtres réglable (clic droit > Plugin size).
- Niveau de sortie (−12 … +12 dB) : clic droit > Output level (automatisable : paramètre « Output »).

## Visuels

`tools/make_background.py` (fond 2x reconstruit, inscriptions redessinées), `tools/make_tubes.py` (8 images de lampes :
4 lampes en paire ou seule, lueur, plaques rouges), `tools/photo_controls.py` (inverseurs et sélecteur « chicken head »
tirés des photos de `assets/photos`, détourés, posés à l'horizontale / tournés, ombre portée) et `tools/controls_art.py`
(potards rendus en 3D, lumière fixe). `tools/render` compile le plugin sous Linux, exporte des captures et des fichiers audio de test.

## Construire

```
cmake -B build
cmake --build build --config Release
```
Les builds Windows / macOS sont faits par GitHub Actions (`.github/workflows/build.yml`).
