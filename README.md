# Anode — NINE HANDS | DSP

Étage de puissance à lampes (VST3, Windows et macOS) utilisé comme préampli guitare / basse.
Interface d'après le visuel « Power Amplifier » : sélecteur de lampe 6L6 / EL34 / EL84 / KT88,
inverseurs Single-Ended / Push-Pull et Triode / Pentode, potards Depth, Contour et Volume.

## Ce qui est modélisé

| Étage | Modèle |
|---|---|
| Volume (master) | −24 dB (0) … 0 dB (5) … +24 dB (10) vers l'étage d'attaque, sortie compensée en partie (plus fort et plus saturé en montant) |
| Attaque PP | déphaseur à paire différentielle (ECC83 / 12AT7) : transfert en tanh, sorties légèrement déséquilibrées |
| Attaque SE | étage triode 12AX7 résolu sur sa droite de charge (modèle de Koren, B+ 300 V, Ra 100 k, Rk 1,5 k) |
| Liaison | condensateur + résistance de grille + conduction de grille (diode de Child-Langmuir) → décalage de polarisation dynamique (« blocking ») |
| Lampes | modèle de Koren pentode, KG1 / KG2 recalés sur les fiches techniques (courant de repos, pente gm, courant d'écran) ; résolution par Newton sur la charge du transformateur |
| Pentode / Triode | écran alimenté par le B+ via sa résistance / écran relié à la plaque (même polarisation, comme sur un ampli réel) |
| Single-Ended | une lampe en classe A, polarisation par la cathode, courant continu dans le primaire → 2e harmonique dominant |
| Push-Pull | deux lampes en opposition, classe AB, charge plaque à plaque Raa → harmoniques impairs, distorsion de croisement quand la polarisation glisse |
| Alimentation | affaissement du B+ (redresseur silicium ou à lampe GZ34 pour l'EL84), l'écran suit ; polarisation automatique (EL84, tous les SE) qui se décale avec le courant moyen |
| Contre-réaction | globale vers l'entrée du déphaseur ; **Depth** la retire dans le grave (résonance), **Contour** dans l'aigu (présence) |
| Transformateur | inductance primaire (passe-haut), fuites (passe-bas résonant), saturation du noyau dans le grave |
| Haut-parleur | bosse de résonance et montée inductive de l'impédance, fortes en pentode, réduites par la contre-réaction |

### Lampes (points de calage)

| Lampe | Point de fiche (Va = Vg2 = 250 V) | gm | Montage PP | Montage SE |
|---|---|---|---|---|
| 6L6GC | Vg1 −14 V → 72 mA | 6,0 mA/V | B+ 460 V, Raa 4 k, polarisation fixe (Bassman / Twin) | B+ 350 V, 2,5 k |
| EL34 | Vg1 −13,5 V → 100 mA | 11 mA/V | B+ 455 V, Raa 3,4 k, polarisation fixe (Plexi) | B+ 350 V, 2,7 k |
| EL84 | Vg1 −7,3 V → 48 mA | 11,3 mA/V | B+ 335 V, Raa 8 k, polarisation cathode, GZ34 (AC15 / AC30) | B+ 285 V, 5,2 k |
| KT88 | Vg1 −15 V → 140 mA | 11,5 mA/V | B+ 520 V, Raa 4,5 k, polarisation fixe (SVT / hi-fi) | B+ 385 V, 2 k |

Marge avant saturation : EL84 < EL34 < 6L6 < KT88.

## Technique

- Sur-échantillonnage IIR polyphasé : taux interne ≥ 176,4 kHz (×4 à 44,1 / 48 kHz), latence 6 échantillons à 48 kHz.
- 16 configurations calibrées et stabilisées au repos dans `prepareToPlay` ; changement de lampe / topologie / mode en fondu enchaîné de 35 ms, sans bruit.
- Canaux identiques (guitare mono sur piste stéréo) : un seul calcul. Environ 3 % d'un cœur à 48 kHz.
- Anti-gel v2 : rendu logiciel sous Windows, aucun calcul lourd dans `paint()`, chien de garde, aucune notification à l'hôte depuis le thread audio, aucune recherche de paramètre par chaîne dans `processBlock`.
- MIDI : MIDI Learn uniquement (clic droit sur une commande), aucun CC assigné par défaut ; pédale d'expression sans saccades (rampe + course apprise, CC 14 bits) ; Program Change → presets d'usine.
- Fenêtre : « 60 % » (947 × 493) par défaut et minimum, 75 %, 100 % ; taille par défaut des nouvelles fenêtres réglable (clic droit > Plugin size).
- Niveau de sortie (−12 … +12 dB) : clic droit > Output level (automatisable : paramètre « Output »).

## Visuels

`tools/make_background.py` (fond 2x reconstruit, inscriptions redessinées, inscriptions des lampes par modèle, lueur) et
`tools/controls_art.py` (potards, sélecteur, inverseurs rendus en 3D, lumière fixe). `tools/render` compile le plugin sous Linux,
exporte des captures et des fichiers audio de test.

## Construire

```
cmake -B build
cmake --build build --config Release
```
Les builds Windows / macOS sont faits par GitHub Actions (`.github/workflows/build.yml`).
