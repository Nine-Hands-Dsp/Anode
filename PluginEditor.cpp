#include "PluginEditor.h"
#include "BinaryData.h"
#include "Layout.h"
#include "WigaWindowSize.h"

using namespace juce;

namespace
{
    const Colour inkWhite (0xffe6e6e6);
    const Colour inkDim   (0xff9a9a9a);
    const Colour accent   (0xffd9873e);     // lueur des lampes

    float knobAngleDeg (double pos01) { return (float) (-layout::knobSpan + pos01 * 2.0 * layout::knobSpan); }
}

//==============================================================================
// POTARD
//==============================================================================
bool AnodeKnob::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float r = (float) getWidth() * (layout::knobHitR / layout::knobSprite);
    return c.getDistanceFrom ({ (float) x, (float) y }) <= r;
}

void AnodeKnob::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float u = b.getWidth() / layout::knobSprite;             // pixels par unite
    const auto c = b.getCentre();
    const double p01 = valueToProportionOfLength (getValue());
    const float a = knobAngleDeg (p01);

    if (strip.isValid())
    {
        // jupe : image correspondant a l'angle modulo 45 deg (la lumiere ne tourne pas)
        float m = std::fmod (a, 45.0f); if (m < 0) m += 45.0f;
        const int fi = jlimit (0, layout::knobFrames - 1, (int) std::round (m / 45.0f * layout::knobFrames) % layout::knobFrames);
        const int fs = strip.getWidth();
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (strip, b.getX(), b.getY(), b.getWidth(), b.getHeight(), 0, fi * fs, fs, fs);
    }

    // repere blanc peint sur la jupe, du bord du chapeau au bord de la jupe
    const float ar = degreesToRadians (a);
    const Point<float> dir (std::sin (ar), -std::cos (ar));
    const auto p0 = c + dir * (16.0f * u), p1 = c + dir * (25.4f * u);
    Path line; line.addLineSegment ({ p0, p1 }, 2.5f * u);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillPath (line, AffineTransform::translation (0.35f * u, 0.55f * u));
    const float facing = 0.5f + 0.5f * std::cos (ar + 0.35f);             // un peu plus clair tourne vers la lumiere
    g.setColour (Colour::greyLevel (0.86f + 0.12f * facing));
    g.fillPath (line);
}

//==============================================================================
// SELECTEUR DE LAMPE
//==============================================================================
TubeSelector::TubeSelector (RangedAudioParameter& p)
    : attachment (p, [this] (float v)
                  {
                      const int np = jlimit (0, 3, roundToInt (v));
                      if (np != pos) { pos = np; repaint(); }
                  })
{
    attachment.sendInitialUpdate();
    shownFrame = layout::selPosFrame[pos];
    setMouseCursor (MouseCursor::PointingHandCursor);
}

bool TubeSelector::tick()
{
    const int target = layout::selPosFrame[pos];
    if (shownFrame == target) return false;
    const int step = jlimit (-3, 3, target - shownFrame);       // ~100 ms d'un bout a l'autre
    shownFrame += step;
    repaint();
    return true;
}

void TubeSelector::paint (Graphics& g)
{
    if (! strip.isValid()) return;
    const int fs = strip.getWidth();
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (strip, 0, 0, getWidth(), getHeight(), 0, shownFrame * fs, fs, fs);
}

bool TubeSelector::hitTest (int x, int y)
{
    const float u = (float) getWidth() / layout::selSprite;
    const Point<float> c (getWidth() * 0.5f, getHeight() * 0.5f);
    const Point<float> p ((float) x, (float) y);
    if (c.getDistanceFrom (p) < 40.0f * u) return true;                // bouton et bec
    // inscriptions et points des 4 positions
    return p.y < c.y - 18.0f * u && c.getDistanceFrom (p) < 62.0f * u;
}

int TubeSelector::positionAt (Point<float> local) const
{
    const Point<float> c (getWidth() * 0.5f, getHeight() * 0.5f);
    const auto d = local - c;
    const float a = radiansToDegrees (std::atan2 (d.x, -d.y));
    int best = 0; float bd = 1e9f;
    for (int i = 0; i < 4; ++i)
        if (std::abs (a - layout::selAngle[i]) < bd) { bd = std::abs (a - layout::selAngle[i]); best = i; }
    return best;
}

void TubeSelector::moveTo (int np)
{
    np = jlimit (0, 3, np);
    if (np == pos) return;
    attachment.setValueAsCompleteGesture ((float) np);
    if (onUserChange) onUserChange();
}

void TubeSelector::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
    dragStartX = e.position.x; dragStartPos = pos; dragging = false;
    const float u = (float) getWidth() / layout::selSprite;
    const Point<float> c (getWidth() * 0.5f, getHeight() * 0.5f);
    // clic sur une inscription / un point : on y va directement
    if (c.getDistanceFrom (e.position) > 26.0f * u && e.position.y < c.y - 12.0f * u)
        moveTo (positionAt (e.position));
}

void TubeSelector::mouseDrag (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const float u = (float) getWidth() / layout::selSprite;
    const float dx = e.position.x - dragStartX + (e.getDistanceFromDragStartY() * -0.4f);
    if (std::abs (dx) > 6.0f * u) dragging = true;
    if (dragging) moveTo (dragStartPos + (int) std::round (dx / (20.0f * u)));
}

void TubeSelector::mouseUp (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const float u = (float) getWidth() / layout::selSprite;
    const Point<float> c (getWidth() * 0.5f, getHeight() * 0.5f);
    // simple clic sur le bouton lui-meme : position suivante (retour au debut apres KT88)
    if (! dragging && c.getDistanceFrom (e.mouseDownPosition) <= 26.0f * u)
        moveTo ((pos + 1) % 4);
}

void TubeSelector::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    if (std::abs (w.deltaY) > 0.0f) moveTo (pos + (w.deltaY > 0 ? 1 : -1));
}

//==============================================================================
// INVERSEUR
//==============================================================================
LeverToggle::LeverToggle (RangedAudioParameter& p)
    : attachment (p, [this] (float v)
                  {
                      const bool nv = v > 0.5f;
                      if (nv != value)
                      {
                          value = nv;
                          shownFrame = 1;                                   // levier au milieu pendant la bascule
                          transitionUntil = Time::getMillisecondCounterHiRes() + 55.0;
                          repaint();
                      }
                  })
{
    attachment.sendInitialUpdate();
    shownFrame = value ? 2 : 0;
    setMouseCursor (MouseCursor::PointingHandCursor);
}

bool LeverToggle::tick (double now)
{
    const int target = value ? 2 : 0;
    if (shownFrame != target && now >= transitionUntil) { shownFrame = target; repaint(); return true; }
    return false;
}

void LeverToggle::paint (Graphics& g)
{
    if (! strip.isValid()) return;
    const int fw = strip.getWidth(), fh = strip.getHeight() / 3;
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (strip, 0, 0, getWidth(), getHeight(), 0, shownFrame * fh, fw, fh);
}

void LeverToggle::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
    attachment.setValueAsCompleteGesture (value ? 0.0f : 1.0f);
    if (onUserChange) onUserChange();
}

//==============================================================================
// EDITEUR
//==============================================================================
AnodeAudioProcessorEditor::AnodeAudioProcessorEditor (AnodeAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      selector   (*p.apvts.getParameter ("tube")),
      topoSwitch (*p.apvts.getParameter ("topology")),
      modeSwitch (*p.apvts.getParameter ("mode"))
{
    background = ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);
    {
        using namespace BinaryData;
        const void* z[8]  = { tubezone_6L6_jpg, tubezone_EL34_jpg, tubezone_EL84_jpg, tubezone_KT88_jpg,
                              tubezone_6L6_SE_jpg, tubezone_EL34_SE_jpg, tubezone_EL84_SE_jpg, tubezone_KT88_SE_jpg };
        const int   zs[8] = { tubezone_6L6_jpgSize, tubezone_EL34_jpgSize, tubezone_EL84_jpgSize, tubezone_KT88_jpgSize,
                              tubezone_6L6_SE_jpgSize, tubezone_EL34_SE_jpgSize, tubezone_EL84_SE_jpgSize, tubezone_KT88_SE_jpgSize };
        const void* gw[8] = { glow_6L6_png, glow_EL34_png, glow_EL84_png, glow_KT88_png,
                              glow_6L6_SE_png, glow_EL34_SE_png, glow_EL84_SE_png, glow_KT88_SE_png };
        const int   gs[8] = { glow_6L6_pngSize, glow_EL34_pngSize, glow_EL84_pngSize, glow_KT88_pngSize,
                              glow_6L6_SE_pngSize, glow_EL34_SE_pngSize, glow_EL84_SE_pngSize, glow_KT88_SE_pngSize };
        const void* rp[8] = { redplate_6L6_png, redplate_EL34_png, redplate_EL84_png, redplate_KT88_png,
                              redplate_6L6_SE_png, redplate_EL34_SE_png, redplate_EL84_SE_png, redplate_KT88_SE_png };
        const int   rs[8] = { redplate_6L6_pngSize, redplate_EL34_pngSize, redplate_EL84_pngSize, redplate_KT88_pngSize,
                              redplate_6L6_SE_pngSize, redplate_EL34_SE_pngSize, redplate_EL84_SE_pngSize, redplate_KT88_SE_pngSize };
        for (int i = 0; i < 8; ++i)
        {
            tubeImg[i] = ImageCache::getFromMemory (z[i], zs[i]);
            glowImg[i] = ImageCache::getFromMemory (gw[i], gs[i]);
            redImg[i]  = ImageCache::getFromMemory (rp[i], rs[i]);
        }
    }
    const auto knobStrip = ImageCache::getFromMemory (BinaryData::knob_strip_png, BinaryData::knob_strip_pngSize);
    selector.strip = ImageCache::getFromMemory (BinaryData::selector_strip_png, BinaryData::selector_strip_pngSize);
    const auto togStrip = ImageCache::getFromMemory (BinaryData::toggle_strip_png, BinaryData::toggle_strip_pngSize);
    labelFace = Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, BinaryData::PoppinsMedium_ttfSize);

    for (int i = 0; i < 3; ++i)
    {
        auto& s = knobs[i];
        s.strip = knobStrip;
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setMouseDragSensitivity (240);
        s.setDoubleClickReturnValue (true, 5.0);
        s.setMouseCursor (MouseCursor::PointingHandCursor);
        s.onDragStart = [this] { proc.currentPreset = -1; };
        s.onRightClick = [this, i] { showContextMenu (i); };
        addAndMakeVisible (s);
        knobAtt[i] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
            proc.apvts, AnodeAudioProcessor::paramId (AnodeAudioProcessor::learnDef (i).param), s);
    }
    selector.onRightClick = [this] { showContextMenu (3); };
    selector.onUserChange = [this] { proc.currentPreset = -1; };
    addAndMakeVisible (selector);
    topoSwitch.strip = togStrip; modeSwitch.strip = togStrip;
    topoSwitch.onRightClick = [this] { showContextMenu (4); };
    modeSwitch.onRightClick = [this] { showContextMenu (5); };
    topoSwitch.onUserChange = modeSwitch.onUserChange = [this] { proc.currentPreset = -1; };
    addAndMakeVisible (topoSwitch);
    addAndMakeVisible (modeSwitch);

    for (int i = 0; i < AnodeAudioProcessor::numParams; ++i) lastValues[i] = proc.value (i);
    lastPresetCounter = proc.presetCounter.load();
    lastMidiCount = proc.midiCounter.load();

    // taille par defaut (« 60 % ») = taille minimale ; proportions bloquees
    const int defaultW = roundToInt ((float) defaultH * (float) baseW / (float) baseH);
    setResizable (true, true);
    setResizeLimits (defaultW, defaultH, baseW * 3 / 2, baseH * 3 / 2);
    if (auto* cons = getConstrainer()) cons->setFixedAspectRatio ((double) baseW / (double) baseH);
    setOpaque (true);
    setSize (defaultW, defaultH);
    setScaledHeight (sizeHeight (WigaWindowSize::defaultIndex (JucePlugin_Name, numSizes)));
    rebuildCaches();
    startTimerHz (30);
}

AnodeAudioProcessorEditor::~AnodeAudioProcessorEditor() { stopTimer(); }

int AnodeAudioProcessorEditor::sizeHeight (int idx)
{
    const int h[numSizes] = { (int) defaultH, roundToInt (baseH * 0.75f), (int) baseH };
    return h[jlimit (0, numSizes - 1, idx)];
}

void AnodeAudioProcessorEditor::setScaledHeight (int h)
{
    h = jmax ((int) defaultH, h);
    setSize (roundToInt ((float) h * (float) baseW / (float) baseH), h);
}

Rectangle<int> AnodeAudioProcessorEditor::scaled (Rectangle<float> r) const
{
    return r.transformedBy (AffineTransform::scale (unit())).getSmallestIntegerContainer();
}

float AnodeAudioProcessorEditor::physicalScale() const
{
    return jlimit (1.0f, 4.0f, (float) Component::getApproximateScaleFactorForComponent (this));
}

void AnodeAudioProcessorEditor::showStatus() { statusUntil = Time::getMillisecondCounterHiRes() + 2500.0; }

//==============================================================================
// caches d'images (reconstruits par le timer seulement)
//==============================================================================
void AnodeAudioProcessorEditor::rebuildCaches (float sc)
{
    if (sc <= 0.0f) sc = cacheScale > 0.0f ? cacheScale : physicalScale();
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    if (w == cacheW && h == cacheH && std::abs (sc - cacheScale) < 0.01f && bgScaled.isValid()) return;
    cacheW = w; cacheH = h; cacheScale = sc;
    const int pw = roundToInt ((float) w * sc), ph = roundToInt ((float) h * sc);
    bgScaled = (pw >= background.getWidth()) ? background : background.rescaled (pw, ph, Graphics::highResamplingQuality);
    const float k = unit() * sc;
    auto sz = [&] (const Image& src, float wu, float hu)
    {
        const int iw = roundToInt (wu * k), ih = roundToInt (hu * k);
        return (iw >= src.getWidth()) ? src : src.rescaled (iw, ih, Graphics::highResamplingQuality);
    };
    for (int i = 0; i < 8; ++i)
    {
        tubeScaled[i] = sz (tubeImg[i], layout::zoneW, layout::zoneH);
        glowScaled[i] = sz (glowImg[i], layout::zoneW, layout::zoneH);
        redScaled[i]  = sz (redImg[i],  layout::zoneW, layout::zoneH);
    }
    repaint();
}

void AnodeAudioProcessorEditor::resized()
{
    // anti-gel : jamais de setSize ici
    using namespace layout;
    for (int i = 0; i < 3; ++i)
        knobs[i].setBounds (scaled ({ knobX[i] - knobSprite * 0.5f, knobY - knobSprite * 0.5f, knobSprite, knobSprite }));
    selector.setBounds (scaled ({ selX - selSprite * 0.5f, selY - selSprite * 0.5f, selSprite, selSprite }));
    topoSwitch.setBounds (scaled ({ togX[0] - togW * 0.5f, togY - togH * 0.5f, togW, togH }));
    modeSwitch.setBounds (scaled ({ togX[1] - togW * 0.5f, togY - togH * 0.5f, togW, togH }));
    resizeSettle = 4;
}

//==============================================================================
// Anti-gel v2 (rendu logiciel sous Windows, aucun calcul lourd dans paint, chien de garde)
//==============================================================================
void AnodeAudioProcessorEditor::useSoftwareRenderer()
{
    auto* peer = getPeer();
    if (peer != lastPeer) { lastPeer = peer; repaint(); }
   #if JUCE_WINDOWS
    if (peer != nullptr)
    {
        int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
        if (sw < 0) sw = 0;
        if (peer->getCurrentRenderingEngine() != sw) peer->setCurrentRenderingEngine (sw);
    }
   #endif
}

void AnodeAudioProcessorEditor::parentHierarchyChanged() { useSoftwareRenderer(); }
void AnodeAudioProcessorEditor::visibilityChanged()      { useSoftwareRenderer(); if (isShowing()) repaint(); }

void AnodeAudioProcessorEditor::askRepaint (Rectangle<int> r)
{
    if (repaintAskedMs <= 0.0) repaintAskedMs = Time::getMillisecondCounterHiRes();
    repaint (r);
}

//==============================================================================
String AnodeAudioProcessorEditor::buildStatusLine() const
{
    using P = AnodeAudioProcessor;
    switch (lastChanged)
    {
        case P::pDepth:   return "Depth  " + String (roundToInt (proc.value (P::pDepth) * 10.0f)) + " %";
        case P::pContour: return "Contour  " + String (roundToInt (proc.value (P::pContour) * 10.0f)) + " %";
        case P::pVolume:  return "Volume  " + String (roundToInt (proc.value (P::pVolume) * 10.0f)) + " %";
        case P::pTube:    return "Tube  " + P::tubeNames()[jlimit (0, 3, roundToInt (proc.value (P::pTube)))];
        case P::pTopo:    return proc.value (P::pTopo) > 0.5f ? "Push-Pull" : "Single-Ended";
        case P::pMode:    return proc.value (P::pMode) > 0.5f ? "Pentode" : "Triode";
        case P::pOutput:  { const float v = proc.value (P::pOutput); return "Output  " + String (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; }
        default: return {};
    }
}

String AnodeAudioProcessorEditor::buildPresetLine() const
{
    const int cp = proc.currentPreset.load();
    if (cp >= 0 && cp < AnodeAudioProcessor::getNumFactoryPresets()) return AnodeAudioProcessor::getFactoryPreset (cp).name;
    if (cp > 100) return "User " + String (cp - 100);
    return "Edited";
}

//==============================================================================
void AnodeAudioProcessorEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    if (getPeer() != lastPeer || now - lastRendererCheckMs > 1000.0) { lastRendererCheckMs = now; useSoftwareRenderer(); }

    if (resizeSettle > 0 && --resizeSettle == 0) rebuildCaches();
    else if (resizeSettle == 0 && now - lastScaleCheckMs > 250.0)
    {
        lastScaleCheckMs = now;
        const float sc = physicalScale();
        if (std::abs (sc - cacheScale) > 0.01f) rebuildCaches (sc);
    }

    if (repaintAskedMs > 0.0 && isShowing() && now - repaintAskedMs > 1500.0 && lastPaintMs < repaintAskedMs)
    {
        repaintAskedMs = 0.0;
        lastPeer = nullptr;
        useSoftwareRenderer();
        repaint();
    }

    selector.tick();
    topoSwitch.tick (now);
    modeSwitch.tick (now);

    // ligne d'etat : apparait quand un reglage bouge, reste 2,5 s puis s'efface
    int changed = -1, numChanged = 0;
    for (int i = 0; i < AnodeAudioProcessor::numParams; ++i)
    {
        const float v = proc.value (i);
        if (std::abs (v - lastValues[i]) > 1.0e-4f) { lastValues[i] = v; changed = i; ++numChanged; }
    }
    const int pc = proc.presetCounter.load();
    if (pc != lastPresetCounter) { lastPresetCounter = pc; numChanged = 0; }
    if (numChanged == 1) { lastChanged = changed; showStatus(); learnLineUntil = 0.0; }
    for (int i = 0; i < 3; ++i)
        if (knobs[i].isMouseButtonDown()) { lastChanged = AnodeAudioProcessor::learnDef (i).param; showStatus(); learnLineUntil = 0.0; }

    const int lt = proc.learnTarget.load();
    const int learned = proc.lastLearned.exchange (-1);
    if (learned >= 0)
    {
        learnLine = String (AnodeAudioProcessor::learnDef (learned).name) + " = CC " + String (proc.ccMap[learned].load());
        learnLineUntil = now + 2000.0;
        statusUntil = jmax (statusUntil, learnLineUntil);
    }
    else if (lt >= 0)
    {
        learnLine = "MIDI learn: move a control";
        learnLineUntil = now + 200.0;
        statusUntil = jmax (statusUntil, now + 200.0);
    }
    const int mc = proc.midiCounter.load();
    if (mc != lastMidiCount) { lastMidiCount = mc; midiFlash = 8; }
    if (midiFlash > 0) --midiFlash;

    const float target = now < statusUntil ? 1.0f : 0.0f;
    statusAlpha += (target - statusAlpha) * (target > statusAlpha ? 0.45f : 0.12f);
    if (statusAlpha < 0.005f) statusAlpha = 0.0f;
    if (statusAlpha > 0.995f) statusAlpha = 1.0f;

    const auto line = (learnLineUntil > 0.0 && now < learnLineUntil + 800.0) ? learnLine : buildStatusLine();
    const bool textChanged = line != lastLine;
    lastLine = line;
    const Rectangle<float> statusR (layout::statusX, layout::statusY, layout::statusW, layout::statusH);
    const Rectangle<float> presetR (layout::presetX, layout::presetY, layout::presetW, layout::presetH);
    if (std::abs (statusAlpha - drawnStatus) > 0.003f || (statusAlpha > 0.0f && textChanged))
    {
        drawnStatus = statusAlpha;
        askRepaint (scaled (statusR.expanded (2.0f)));
    }
    const auto pl = buildPresetLine();
    if (pl != lastPreset || hoverZone != drawnHover || midiFlash > 0)
    {
        lastPreset = pl; drawnHover = hoverZone;
        askRepaint (scaled (presetR.expanded (2.0f)));
    }

    // lueur des lampes : suit le courant (moyen en classe AB, alternatif en classe A), puis rougeoiement des plaques
    const Rectangle<float> zoneR (layout::zoneX, layout::zoneY, layout::zoneW, layout::zoneH);
    const float gAvg   = (proc.plateGlow.load() - 1.0f) * 1.4f;
    const float gSwing = (proc.plateSwing.load() - 0.04f) * 1.5f;
    const float g = jlimit (0.0f, 1.0f, jmax (gAvg, gSwing));
    const float gt = proc.outLevel.load() > 1.0e-4f ? g : 0.0f;
    glowLevel += (gt - glowLevel) * (gt > glowLevel ? 0.35f : 0.10f);
    if (glowLevel < 0.003f) glowLevel = 0.0f;
    heatLevel = proc.plateHeat.load();
    if (heatLevel < 0.004f) heatLevel = 0.0f;
    // changement de lampe ou de montage (paire en push-pull, une lampe en single-ended) : fondu de ~160 ms
    const int tube = jlimit (0, 3, roundToInt (proc.value (AnodeAudioProcessor::pTube)))
                   + (proc.value (AnodeAudioProcessor::pTopo) > 0.5f ? 0 : 4);
    if (shownTube < 0) shownTube = tube;
    if (tube != shownTube) { prevTube = shownTube; shownTube = tube; tubeFade = 0.0f; }
    bool zoneDirty = false;
    if (tubeFade < 1.0f) { tubeFade = jmin (1.0f, tubeFade + 0.2f); zoneDirty = true; if (tubeFade >= 1.0f) prevTube = -1; }
    if (std::abs (glowLevel - drawnGlow) > 0.01f) { drawnGlow = glowLevel; zoneDirty = true; }
    if (std::abs (heatLevel - drawnHeat) > 0.01f) { drawnHeat = heatLevel; zoneDirty = true; }
    if (zoneDirty) askRepaint (scaled (zoneR));
}

//==============================================================================
void AnodeAudioProcessorEditor::paint (Graphics& g)
{
    lastPaintMs = Time::getMillisecondCounterHiRes();
    repaintAskedMs = 0.0;
    g.fillAll (Colours::black);
    const Image& bg = bgScaled.isValid() ? bgScaled : background;
    g.setImageResamplingQuality (&bg == &background ? Graphics::highResamplingQuality : Graphics::lowResamplingQuality);
    if (bg.isValid()) g.drawImage (bg, getLocalBounds().toFloat());

    const float u = unit();
    // lampes : image de la lampe choisie (fondu depuis la precedente), lueur des filaments, plaques qui rougissent
    {
        const Rectangle<float> zr = Rectangle<float> (layout::zoneX, layout::zoneY, layout::zoneW, layout::zoneH) * u;
        const int cur = shownTube >= 0 ? shownTube : jlimit (0, 3, roundToInt (proc.value (AnodeAudioProcessor::pTube)))
                                                     + (proc.value (AnodeAudioProcessor::pTopo) > 0.5f ? 0 : 4);
        auto pick = [] (const Image& sc, const Image& orig) -> const Image& { return sc.isValid() ? sc : orig; };
        auto layer = [&] (int t, float alpha)
        {
            if (alpha <= 0.0f) return;
            g.setOpacity (alpha);
            g.drawImage (pick (tubeScaled[t], tubeImg[t]), zr);
            if (glowLevel > 0.0f) { g.setOpacity (alpha * 0.55f * glowLevel); g.drawImage (pick (glowScaled[t], glowImg[t]), zr); }
            if (heatLevel > 0.0f) { g.setOpacity (alpha * 0.6f * heatLevel); g.drawImage (pick (redScaled[t], redImg[t]), zr); }
        };
        if (prevTube >= 0 && tubeFade < 1.0f) layer (prevTube, 1.0f);
        layer (cur, prevTube >= 0 ? tubeFade : 1.0f);
        g.setOpacity (1.0f);
    }

    // barre de presets
    const Rectangle<float> pr = Rectangle<float> (layout::presetX, layout::presetY, layout::presetW, layout::presetH) * u;
    g.setColour (Colour (0xff0b0b0b));
    g.fillRoundedRectangle (pr, pr.getHeight() * 0.5f);
    g.setColour (accent.withAlpha (hoverZone != 0 ? 0.55f : 0.28f));
    g.drawRoundedRectangle (pr.reduced (0.5f), pr.getHeight() * 0.5f, jmax (1.0f, 0.5f * u));
    const float aw = 15.0f * u;
    auto arrow = [&] (Rectangle<float> ar, bool left, bool hot)
    {
        Path p; const auto c = ar.getCentre(); const float s = 2.8f * u;
        if (left) p.addTriangle (c.x + s * 0.6f, c.y - s, c.x + s * 0.6f, c.y + s, c.x - s * 0.8f, c.y);
        else      p.addTriangle (c.x - s * 0.6f, c.y - s, c.x - s * 0.6f, c.y + s, c.x + s * 0.8f, c.y);
        g.setColour (inkWhite.withAlpha (hot ? 1.0f : 0.55f));
        g.fillPath (p);
    };
    arrow (pr.withWidth (aw), true, hoverZone == 1);
    arrow (pr.withTrimmedLeft (pr.getWidth() - aw), false, hoverZone == 3);
    g.setFont (Font (FontOptions (labelFace).withHeight (8.6f * u)));
    g.setColour ((midiFlash > 0 ? Colour (0xffffc070) : inkWhite).withAlpha (hoverZone == 2 ? 1.0f : 0.86f));
    g.drawFittedText (lastPreset, pr.reduced (aw, 0.0f).toNearestInt(), Justification::centred, 1, 0.7f);

    // ligne d'etat (valeur du reglage qui bouge)
    if (statusAlpha > 0.0f)
    {
        const Rectangle<float> sr = Rectangle<float> (layout::statusX, layout::statusY, layout::statusW, layout::statusH) * u;
        g.setFont (Font (FontOptions (labelFace).withHeight (8.8f * u)));
        g.setColour (inkWhite.withAlpha (statusAlpha * 0.95f));
        g.drawFittedText (lastLine, sr.toNearestInt(), Justification::centred, 1, 0.7f);
    }
}

//==============================================================================
// Souris : barre de presets (fleches = precedent / suivant, nom = menu), clic droit = menu
//==============================================================================
int AnodeAudioProcessorEditor::presetZoneAt (Point<float> p) const
{
    const Rectangle<float> r (layout::presetX, layout::presetY, layout::presetW, layout::presetH);
    if (! r.contains (p)) return 0;
    if (p.x < r.getX() + 15.0f) return 1;
    if (p.x > r.getRight() - 15.0f) return 3;
    return 2;
}

void AnodeAudioProcessorEditor::mouseMove (const MouseEvent& e)
{
    hoverZone = presetZoneAt (e.position / unit());
    setMouseCursor (hoverZone != 0 ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void AnodeAudioProcessorEditor::mouseExit (const MouseEvent&) { hoverZone = 0; }

void AnodeAudioProcessorEditor::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showContextMenu(); return; }
    switch (presetZoneAt (e.position / unit()))
    {
        case 1: stepPreset (-1); break;
        case 2: showPresetMenu(); break;
        case 3: stepPreset (1); break;
        default: break;
    }
}

void AnodeAudioProcessorEditor::stepPreset (int delta)
{
    const int n = AnodeAudioProcessor::getNumFactoryPresets();
    int cp = proc.currentPreset.load();
    if (cp < 0 || cp >= n) cp = delta > 0 ? -1 : 0;
    proc.applyFactoryPreset ((cp + delta + n) % n);
}

void AnodeAudioProcessorEditor::showPresetMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Factory presets");
    for (int i = 0; i < AnodeAudioProcessor::getNumFactoryPresets(); ++i)
        m.addItem (100 + i, String (i + 1) + ". " + AnodeAudioProcessor::getFactoryPreset (i).name, true, proc.currentPreset.load() == i);
    m.addSeparator();
    PopupMenu load, save;
    for (int slot = 1; slot <= 9; ++slot)
    {
        const bool exists = proc.hasUserPreset (slot);
        load.addItem (200 + slot, "User " + String (slot) + (exists ? "" : "  (empty)"), exists, proc.currentPreset.load() == 100 + slot);
        save.addItem (300 + slot, "User " + String (slot) + (exists ? "  (overwrite)" : ""));
    }
    m.addSubMenu ("Load user preset", load);
    m.addSubMenu ("Save user preset", save);
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (scaled ({ layout::presetX, layout::presetY, layout::presetW, layout::presetH }))),
        [safe = Component::SafePointer<AnodeAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& pr = safe->proc;
            if      (r >= 100 && r < 200) pr.applyFactoryPreset (r - 100);
            else if (r > 200 && r < 300)  pr.loadUserPreset (r - 200);
            else if (r > 300 && r < 400)  pr.saveUserPreset (r - 300);
        });
}

void AnodeAudioProcessorEditor::showContextMenu (int learnIdx)
{
    using P = AnodeAudioProcessor;
    PopupMenu menu;
    auto ccText = [this] (int i)
    {
        const int cc = proc.ccMap[i].load();
        return cc >= 0 ? "CC " + String (cc) : String (CharPointer_UTF8 ("\xe2\x80\x94"));
    };
    if (learnIdx >= 0)
    {
        const String name = P::learnDef (learnIdx).name;
        menu.addItem (1000 + learnIdx, "MIDI Learn: " + name + " (" + ccText (learnIdx) + ")", true, proc.learnTarget.load() == learnIdx);
        menu.addItem (1100 + learnIdx, "Clear MIDI: " + name, proc.ccMap[learnIdx].load() >= 0);
        if (proc.learnTarget.load() >= 0) menu.addItem (1200, "Cancel MIDI learn");
        menu.addSeparator();
    }
    {
        const char* names[numSizes] = { "60 %", "75 %", "100 %" };
        const int def = WigaWindowSize::defaultIndex (JucePlugin_Name, numSizes);
        PopupMenu sizes, defSizes;
        for (int i = 0; i < numSizes; ++i)
        {
            sizes.addItem (1 + i, String (names[i]) + (i == def ? "  (default)" : ""), true, std::abs (getHeight() - sizeHeight (i)) <= 2);
            defSizes.addItem (20 + i, names[i], true, i == def);
        }
        sizes.addSeparator();
        sizes.addSubMenu ("Default size (new windows)", defSizes);
        menu.addSubMenu ("Plugin size", sizes);
    }
    menu.addSeparator();
    PopupMenu factory;
    for (int i = 0; i < P::getNumFactoryPresets(); ++i)
        factory.addItem (100 + i, String (i + 1) + ". " + P::getFactoryPreset (i).name, true, proc.currentPreset.load() == i);
    menu.addSubMenu ("Factory presets", factory);
    PopupMenu load, save;
    for (int slot = 1; slot <= 9; ++slot)
    {
        const bool exists = proc.hasUserPreset (slot);
        load.addItem (200 + slot, "User " + String (slot) + (exists ? "" : "  (empty)"), exists, proc.currentPreset.load() == 100 + slot);
        save.addItem (300 + slot, "User " + String (slot) + (exists ? "  (overwrite)" : ""));
    }
    menu.addSubMenu ("Load user preset", load);
    menu.addSubMenu ("Save user preset", save);
    menu.addSeparator();

    PopupMenu out;
    const float o = proc.value (P::pOutput);
    const int steps[] = { -12, -9, -6, -3, 0, 3, 6, 9, 12 };
    for (int i = 0; i < 9; ++i)
        out.addItem (60 + i, (steps[i] > 0 ? "+" : "") + String (steps[i]) + " dB", true, std::abs (o - (float) steps[i]) < 0.05f);
    menu.addSubMenu ("Output level", out);

    PopupMenu midi;
    for (int i = 0; i < P::numLearn; ++i)
        midi.addItem (1000 + i, "Learn: " + String (P::learnDef (i).name) + " (" + ccText (i) + ")", true, proc.learnTarget.load() == i);
    midi.addSeparator();
    if (proc.learnTarget.load() >= 0) midi.addItem (1200, "Cancel MIDI learn");
    midi.addItem (1201, "Clear all MIDI assignments");
    midi.addSeparator();
    midi.addItem (910, "CC n+32 = fine (14-bit) for knobs on CC 0-31", false);
    midi.addItem (911, "Program Change 1-" + String (P::getNumFactoryPresets()) + "  Factory presets", false);
    menu.addSubMenu ("MIDI", midi);
    menu.addSeparator();
    menu.addItem (999, "Anode tube power stage v" + String (JucePlugin_VersionString) + " - NINE HANDS | DSP", false);

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this),
        [safe = Component::SafePointer<AnodeAudioProcessorEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& ed = *safe;
            auto& pr = ed.proc;
            const int stepsDb[] = { -12, -9, -6, -3, 0, 3, 6, 9, 12 };
            if      (r >= 1 && r < 1 + numSizes) ed.setScaledHeight (sizeHeight (r - 1));
            else if (r >= 20 && r < 20 + numSizes)
            {
                WigaWindowSize::setDefaultIndex (JucePlugin_Name, r - 20);
                ed.setScaledHeight (sizeHeight (r - 20));
            }
            else if (r >= 60 && r < 69) pr.setParamValue ("output", (float) stepsDb[r - 60]);
            else if (r >= 100 && r < 200) pr.applyFactoryPreset (r - 100);
            else if (r > 200 && r < 300)  pr.loadUserPreset (r - 200);
            else if (r > 300 && r < 400)  pr.saveUserPreset (r - 300);
            else if (r >= 1000 && r < 1000 + P::numLearn) pr.learnTarget.store (r - 1000);
            else if (r >= 1100 && r < 1100 + P::numLearn) pr.ccMap[r - 1100].store (-1);
            else if (r == 1200) pr.learnTarget.store (-1);
            else if (r == 1201) { pr.learnTarget.store (-1); pr.clearMidiMap(); }
            ed.showStatus();
        });
}
