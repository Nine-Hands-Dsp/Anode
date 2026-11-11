#pragma once
// ANODE v2 - genere par tools/v2/make_layers.py : repere 1368,5 x 900 unites (images 2x = 2737 x 1800 px)
namespace layout
{
    constexpr float W = 1368.50f, H = 900.00f;
    constexpr float tubeZone[4] = { 176.50f, 57.50f, 1013.50f, 473.00f };
    constexpr float powerRect[4] = { 110.00f, 643.00f, 72.00f, 72.50f }, powerC[2] = { 145.93f, 678.97f }, powerR = 34.80f;
    constexpr float powerSym[6] = { 145.93f, 626.46f, 6.72f, 9.16f, 1.22f, 1.83f };      // x, y, rayon, haut du trait, bas du trait, epaisseur
    constexpr float vuRect[2][4] = { { 238.09f, 577.25f, 211.00f, 211.00f }, { 450.57f, 577.25f, 211.00f, 211.00f } };
    constexpr float vuPivot[2] = { 104.59f, 131.61f };                 // dans le rectangle du cadran
    constexpr float vuNeedleLen = 78.71f, vuNeedleW = 1.59f, vuRest = -44.00f;
    constexpr int   vuNumTicks = 13;
    constexpr float vuTickDb[vuNumTicks] = { -20.00f, -10.00f, -7.00f, -6.00f, -5.00f, -4.00f, -3.00f, -2.00f, -1.00f, 0.00f, 1.00f, 2.00f, 3.00f };
    constexpr float vuTickDeg[vuNumTicks] = { -37.60f, -29.20f, -24.40f, -17.70f, -11.20f, -6.00f, -0.80f, 5.40f, 10.90f, 15.90f, 22.10f, 32.60f, 43.50f };
    constexpr float knobC[3][2] = { { 750.41f, 681.41f }, { 884.74f, 681.41f }, { 1019.06f, 681.41f } };
    constexpr float knobR = 40.30f, knobLine[3] = { 16.36f, 42.92f, 5.36f };   // trait : debut, fin (du centre), epaisseur
    constexpr float knobSpan = 135.0f;
    constexpr float dotRing = 47.63f, dotR = 1.77f;
    constexpr float selC[2] = { 1190.03f, 681.41f }, selR = 68.39f, selLine[3] = { 27.77f, 72.83f, 5.36f };
    constexpr float selAngle[4] = { -48.00f, -16.00f, 16.00f, 48.00f };
    constexpr float selTick[3] = { 74.49f, 83.04f, 1.83f };
    constexpr float togC[2][2] = { { 750.41f, 791.32f }, { 1019.06f, 791.32f } };
    constexpr float togSize = 76.00f, togHitR = 22.90f;
    constexpr float nineHands[4] = { 108.84f, 823.85f, 145.32f, 13.14f }, anode[4] = { 1139.59f, 808.99f, 100.87f, 29.36f };
    constexpr float preset[4] = { 307.33f, 531.25f, 285.00f, 30.00f };
    struct Label { const char* text; float x, y, size, tracking; };   // centre des capitales, taille (em), espacement
    constexpr int numLabels = 12;
    constexpr Label labels[numLabels] = {
        { "DEPTH", 750.41f, 588.68f, 17.71f, 1.53f },
        { "CONTOUR", 884.74f, 588.68f, 17.71f, 1.53f },
        { "VOLUME", 1019.06f, 588.68f, 17.71f, 1.53f },
        { "TUBE", 1190.03f, 562.96f, 17.71f, 1.53f },
        { "6L6", 1118.33f, 616.86f, 14.65f, 0.61f },
        { "EL34", 1163.44f, 588.68f, 14.65f, 0.61f },
        { "EL84", 1216.62f, 588.68f, 14.65f, 0.61f },
        { "KT88", 1261.72f, 616.86f, 14.65f, 0.61f },
        { "SINGLE-ENDED", 750.41f, 752.85f, 13.43f, 0.92f },
        { "PUSH-PULL", 750.41f, 829.78f, 13.43f, 0.92f },
        { "TRIODE", 1019.06f, 752.85f, 13.43f, 0.92f },
        { "PENTODE", 1019.06f, 829.78f, 13.43f, 0.92f },
    };
}
