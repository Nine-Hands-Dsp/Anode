#pragma once
// Alignement de niveau des 16 configurations (lampe x Push-Pull/Single-Ended x Pentode/Triode), en dB, Volume 0..10.
// Gain fixe applique en sortie de l'etage de puissance : la lampe recoit le meme signal, sature au meme moment
// et garde sa compression ; seul le niveau de sortie est aligne sur l'EL34 de la meme topologie.
// Table generee par : AnodeRender <dossier> calib   (index = tube * 4 + PP * 2 + pentode)
namespace anode
{
    inline const float kLevelComp[16][11] =
    {
        { 0.97f, 0.97f, 0.97f, 0.97f, 0.97f, 0.97f, 0.97f, 0.96f, 0.88f, 0.70f, 0.52f },   // 6L6 SE triode
        { 3.11f, 3.11f, 3.11f, 3.10f, 3.09f, 3.04f, 2.93f, 2.65f, 2.12f, 1.56f, 0.96f },   // 6L6 SE pentode
        { -0.34f, -0.34f, -0.34f, -0.34f, -0.34f, -0.33f, -0.30f, -0.26f, -0.20f, -0.16f, -0.02f },   // 6L6 PP triode
        { -0.33f, -0.33f, -0.33f, -0.33f, -0.32f, -0.30f, -0.26f, -0.19f, -0.12f, -0.03f, 0.10f },   // 6L6 PP pentode
        { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },   // EL34 SE triode
        { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },   // EL34 SE pentode
        { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },   // EL34 PP triode
        { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f },   // EL34 PP pentode
        { -1.41f, -1.41f, -1.41f, -1.41f, -1.41f, -1.42f, -1.42f, -1.42f, -1.34f, -1.05f, -0.60f },   // EL84 SE triode
        { -2.29f, -2.29f, -2.29f, -2.29f, -2.30f, -2.30f, -2.26f, -2.09f, -1.73f, -1.16f, -0.50f },   // EL84 SE pentode
        { -3.66f, -3.66f, -3.66f, -3.66f, -3.65f, -3.64f, -3.59f, -3.48f, -3.11f, -2.51f, -1.75f },   // EL84 PP triode
        { -7.22f, -7.22f, -7.22f, -7.21f, -7.17f, -7.07f, -6.79f, -6.16f, -5.03f, -3.61f, -2.13f },   // EL84 PP pentode
        { 2.59f, 2.59f, 2.59f, 2.59f, 2.59f, 2.59f, 2.59f, 2.57f, 2.44f, 2.03f, 1.58f },   // KT88 SE triode
        { 3.58f, 3.58f, 3.58f, 3.58f, 3.56f, 3.52f, 3.41f, 3.11f, 2.54f, 1.88f, 1.21f },   // KT88 SE pentode
        { 2.13f, 2.13f, 2.13f, 2.13f, 2.13f, 2.14f, 2.16f, 2.19f, 2.19f, 1.92f, 1.58f },   // KT88 PP triode
        { 0.77f, 0.77f, 0.77f, 0.77f, 0.77f, 0.78f, 0.80f, 0.81f, 0.79f, 0.69f, 0.57f }   // KT88 PP pentode
    };
}
