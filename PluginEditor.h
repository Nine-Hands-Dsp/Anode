#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Potard : jupe cannelee noire + chapeau alu repousse (rendus 3D, lumiere fixe) ;
// la jupe est choisie parmi 32 images (motif periodique de 45 deg), le repere blanc tourne.
//==============================================================================
class AnodeKnob : public juce::Slider
{
public:
    AnodeKnob() = default;
    juce::Image strip;
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        juce::Slider::mouseDown (e);
    }
};

//==============================================================================
// Selecteur de lampe « chicken head » (4 positions, animation par images intermediaires)
//==============================================================================
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
    bool tick();                           // timer de l'editeur ; true si l'image a change
    juce::Image strip;
    std::function<void()> onRightClick, onUserChange;
    int getPosition() const { return pos; }

private:
    void moveTo (int p);
    int positionAt (juce::Point<float> local) const;
    juce::ParameterAttachment attachment;
    int pos = 1, shownFrame = 3;
    float dragStartX = 0.0f; int dragStartPos = 0; bool dragging = false;
};

//==============================================================================
// Inverseur a levier horizontal (gauche / droite), image intermediaire ~55 ms pendant la bascule
//==============================================================================
class LeverToggle : public juce::Component
{
public:
    explicit LeverToggle (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool tick (double nowMs);
    juce::Image strip;
    std::function<void()> onRightClick, onUserChange;

private:
    juce::ParameterAttachment attachment;
    bool value = true;
    int shownFrame = 2;
    double transitionUntil = 0.0;
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

    // repere : visuel 780 x 406 ; 100 % = fond 2x (1560 x 812) ; « 60 % » = taille par defaut et minimale
    static constexpr int baseW = 1560, baseH = 812, defaultH = 493;
    static constexpr int numSizes = 3;
    static int sizeHeight (int idx);
    void setScaledHeight (int h);
    void showStatus();

private:
    void timerCallback() override;
    void useSoftwareRenderer();
    void askRepaint (juce::Rectangle<int> r);
    void rebuildCaches (float scale = 0.0f);
    float physicalScale() const;
    float unit() const { return (float) getHeight() / 406.0f; }
    juce::Rectangle<int> scaled (juce::Rectangle<float> r) const;
    juce::String buildStatusLine() const;
    juce::String buildPresetLine() const;
    void showContextMenu (int learnIdx = -1);
    void showPresetMenu();
    void stepPreset (int delta);
    int presetZoneAt (juce::Point<float> unitsPos) const;

    AnodeAudioProcessor& proc;
    juce::Image background, bgScaled;
    // 0..3 : paire (push-pull) 6L6 / EL34 / EL84 / KT88 ; 4..7 : une seule lampe (single-ended)
    juce::Image tubeImg[8], tubeScaled[8], glowImg[8], glowScaled[8], redImg[8], redScaled[8];
    int cacheW = 0, cacheH = 0; float cacheScale = 0.0f; int resizeSettle = 0;
    juce::Typeface::Ptr labelFace;

    AnodeKnob knobs[3];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAtt[3];
    TubeSelector selector;
    LeverToggle topoSwitch, modeSwitch;

    // anti-gel v2
    void* lastPeer = nullptr;
    double lastPaintMs = 0.0, repaintAskedMs = 0.0, lastScaleCheckMs = 0.0, lastRendererCheckMs = 0.0;

    // affichage
    float glowLevel = 0.0f, drawnGlow = -1.0f, heatLevel = 0.0f, drawnHeat = -1.0f;
    int shownTube = -1, prevTube = -1;
    float tubeFade = 1.0f;                 // fondu entre l'ancienne et la nouvelle lampe (0 -> 1)
    int lastChanged = -1;
    float lastValues[AnodeAudioProcessor::numParams] {};
    float statusAlpha = 0.0f, drawnStatus = -1.0f;
    double statusUntil = 0.0;
    juce::String lastLine, lastPreset, learnLine;
    double learnLineUntil = 0.0;
    int lastPresetCounter = 0, lastMidiCount = 0, midiFlash = 0, hoverZone = 0, drawnHover = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnodeAudioProcessorEditor)
};
