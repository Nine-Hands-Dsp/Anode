#pragma once
// Positions des commandes dans le repere du visuel de reference (780 x 406 unites ; fond 2x = 1560 x 812 px)
// (memes valeurs que tools/make_background.py et tools/controls_art.py)
namespace layout
{
    constexpr float W = 780.0f, H = 406.0f;
    constexpr float knobX[3] = { 375.5f, 475.5f, 575.0f };
    constexpr float knobY = 314.0f;
    constexpr float knobSprite = 84.0f;            // sprite carre (ombre comprise)
    constexpr float knobHitR = 28.0f;
    constexpr float knobSpan = 135.0f;             // 0 = -135 deg, 10 = +135 deg
    constexpr int   knobFrames = 32;               // rotation de la jupe 0..45 deg (8 cannelures)
    constexpr float selX = 243.5f, selY = 317.0f, selSprite = 120.0f;
    constexpr int   selFrames = 10;
    constexpr int   selPosFrame[4] = { 0, 3, 6, 9 };
    constexpr float selAngle[4] = { -44.0f, -13.5f, 13.5f, 44.0f };
    constexpr float togX[2] = { 401.0f, 572.0f }, togY = 243.0f, togW = 46.0f, togH = 28.0f;
    constexpr float labelX = 320.0f, labelY = 80.0f, labelW = 160.0f, labelH = 80.0f;   // inscriptions des lampes
    constexpr float glowX = 290.0f, glowY = 0.0f, glowW = 220.0f, glowH = 215.0f;
    // barre de presets et ligne d'etat : fond noir en haut a droite
    constexpr float presetX = 612.0f, presetY = 10.0f, presetW = 158.0f, presetH = 19.0f;
    constexpr float statusX = 612.0f, statusY = 33.0f, statusW = 158.0f, statusH = 16.0f;
}
