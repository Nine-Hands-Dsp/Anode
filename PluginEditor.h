#pragma once
#include "PluginProcessor.h"
#include "NineHandsMenu.h"
#include "NineHandsWindow.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// ANODE v2 (NINE HANDS | DSP) : facade d'apres la photo de l'ampli.
// Le fond contient tout ce qui ne bouge pas (corps des potards, cadrans et Power eteints, lampes froides) ;
// l'editeur dessine par-dessus les lampes qui chauffent et reagissent au jeu, les cadrans eclaires et leurs aiguilles,
// le Power allume, les traits des potards, les inverseurs, et en vectoriel textes, points, graduations et logos.
//==============================================================================

// Potard : seul le trait blanc tourne (le corps brosse est dans le fond, la lumiere ne tourne pas)
class AnodeKnob : public juce::Slider
{
public:
    AnodeKnob() = default;
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    std::function<void()> onRightClick;
    float lineFrom = 0, lineTo = 0, lineW = 0;              // en fraction du rayon du composant
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        juce::Slider::mouseDown (e);
    }
};

// Selecteur TUBE (4 positions) : meme bouton, trait qui tourne en ~100 ms vers la lampe choisie
class TubeSelector : public juce::Component
{
public:
    explicit TubeSelector (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool hitTest (int x, int y) override;
    bool tick();
    std::function<void()> onRightClick, onUserChange;
    float unitsPerPixel = 1.0f;

private:
    void moveTo (int p);
    int positionAt (juce::Point<float> local) const;
    juce::ParameterAttachment attachment;
    int pos = 1;
    float shownAngle = 0.0f;
    float dragStartX = 0.0f, dragRef = 0.0f; int dragStartPos = 0, dragTarget = 0; bool dragging = false;
};

// Inverseur a levier vertical (photos du kit) : haut = Single-Ended / Triode, bas = Push-Pull / Pentode
class LeverToggle : public juce::Component
{
public:
    explicit LeverToggle (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override;
    bool tick (double nowMs);
    juce::Image strip;
    std::function<void()> onRightClick, onUserChange;

private:
    juce::ParameterAttachment attachment;
    bool value = true;
    int shownFrame = 2;
    double transitionUntil = 0.0;
};

// Bouton Power (zone cliquable ; le dessin allume / eteint est fait par l'editeur)
class PowerButton : public juce::Component
{
public:
    explicit PowerButton (juce::RangedAudioParameter& p);
    void mouseDown (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override;
    std::function<void()> onRightClick;

private:
    juce::ParameterAttachment attachment;
    bool value = true;
};

//==============================================================================
class AnodeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit AnodeAudioProcessorEditor (AnodeAudioProcessor&);
    ~AnodeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    // repere : 1368,5 x 900 unites (images 2x = 2737 x 1800 px) ; « 60 % » = taille par defaut et minimale
    static constexpr int defaultH = 493;
    static constexpr int numSizes = 3;
    static int sizeHeight (int idx);
    static int widthFor (int h);
    void setScaledHeight (int h);
    void showStatus();

private:
    void timerCallback() override;
    void useSoftwareRenderer();
    void askRepaint (juce::Rectangle<int> r);
    void rebuildCaches (float scale = 0.0f);
    float physicalScale() const;
    float unit() const { return (float) getHeight() / 900.0f; }
    juce::Rectangle<int> scaled (juce::Rectangle<float> r) const;
    void drawStatic (juce::Graphics& g, float u);           // textes, points, graduations, logos (vectoriel)
    void drawNeedle (juce::Graphics& g, int i, float u);
    float needleAngle (float pos) const;                     // position (1 = 0 VU) -> angle en degres
    juce::String buildStatusLine() const;
    juce::String buildPresetLine() const;
    void showContextMenu (int learnIdx = -1);
public:
    juce::PopupMenu buildContextMenu (int learnIdx = -1);   // menu clic droit (aussi pour l'outil de controle des menus)
    void setGlowPercent (int pc);                            // clic droit > Glow intensity
    static constexpr int maxH = 1437;                        // 150 %
private:
    void showPresetMenu();
    void stepPreset (int delta);
    int presetZoneAt (juce::Point<float> unitsPos) const;

    AnodeAudioProcessor& proc;
    juce::Image background, bgScaled;                        // bgScaled : fond + elements vectoriels, a la resolution de l'ecran
    juce::Image tubesHot, tubesBloom, tubesRed, powerOn, vuOn, mechOff, mechOn;
    juce::Image hotS, bloomS, redS, powerOnS, vuOnS, mechOffS, mechOnS;
    int cacheW = 0, cacheH = 0; float cacheScale = 0.0f; int resizeSettle = 0;
    juce::Rectangle<float> tubeDraw;                         // zone des lampes alignee sur les pixels du fond
    juce::Typeface::Ptr labelFace, menuFace;
    std::unique_ptr<juce::Drawable> logoNineHands, logoAnode;
    NineHandsMenu::MenuLnf menuLnf;
    NineHandsWindow::HostLock hostLock;    // fenetre du DAW : jamais sous 60 %, jamais au-dela de 150 %

    AnodeKnob knobs[3];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAtt[3];
    TubeSelector selector;
    LeverToggle topoSwitch, modeSwitch;
    PowerButton powerButton;

    // anti-gel v2
    void* lastPeer = nullptr;
    double lastPaintMs = 0.0, repaintAskedMs = 0.0, lastScaleCheckMs = 0.0, lastRendererCheckMs = 0.0;

    // animation
    float lamp = 1.0f, drawnLamp = -1.0f;                    // ampoules des cadrans + anneau Power
    int glowPc = 100;                      // clic droit > Glow intensity : halo des lampes (25 a 100 %, settings.xml)
    double lastGlowReadMs = 0.0;
    float glowAmt() const { return (float) glowPc / 100.0f; }
    float heater = 1.0f, glowLevel = 0.0f, heatLevel = 0.0f; // filaments, lueur au jeu, plaques rouges
    float drawnHeater = -1.0f, drawnGlow = -1.0f, drawnHeat = -1.0f;
    float vuPos[2] { 0, 0 }, vuVel[2] { 0, 0 }, drawnAngle[2] { -999.0f, -999.0f };
    int lastChanged = -1;
    float lastValues[AnodeAudioProcessor::numParams] {};
    float statusAlpha = 0.0f, drawnStatus = -1.0f;
    float barAlpha = 0.0f, drawnBar = -1.0f;              // barre du haut : visible pendant un reglage ou au survol
    bool hoverTop = false;
    double barUntil = 0.0;
    double statusUntil = 0.0;
    juce::String lastLine, lastPreset, learnLine;
    double learnLineUntil = 0.0;
    int lastPresetCounter = 0, lastMidiCount = 0, midiFlash = 0, hoverZone = 0, drawnHover = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnodeAudioProcessorEditor)
};
