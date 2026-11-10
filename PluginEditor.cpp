#include "PluginEditor.h"
#include "BinaryData.h"
#include "Layout.h"
#include "WigaWindowSize.h"

using namespace juce;

namespace
{
    const Colour ink (0xffd4d4d4);                 // serigraphie (gris clair de la maquette)
    const Colour lineCol (0xffeeeeec);             // trait des potards
    const Colour needleCol (0xff100c09);
    const Colour accent (0xffd9873e);
    constexpr float capRatio = 0.728f;             // hauteur des capitales d'Inter / taille em
    constexpr double vuRef = 0.12589254;           // 0 VU = -18 dBFS efficace

    Point<float> polar (Point<float> c, float r, float deg)
    {
        const float a = degreesToRadians (deg);
        return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
    }

    std::unique_ptr<Drawable> svgFrom (const void* data, int size)
    {
        if (auto xml = XmlDocument::parse (String::fromUTF8 ((const char*) data, size)))
            return Drawable::createFromSVG (*xml);
        return {};
    }

    void drawLine (Graphics& g, Point<float> c, float fromR, float toR, float deg, float w, Colour col)
    {
        Path p;
        p.startNewSubPath (polar (c, fromR, deg));
        p.lineTo (polar (c, toR, deg));
        g.setColour (col);
        g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
    }
}

//==============================================================================
// POTARD (trait seul)
//==============================================================================
bool AnodeKnob::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    return c.getDistanceFrom ({ (float) x, (float) y }) <= getWidth() * 0.5f * 0.92f;
}

void AnodeKnob::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float half = b.getWidth() * 0.5f;
    const double p01 = valueToProportionOfLength (getValue());
    const float a = (float) (-layout::knobSpan + p01 * 2.0 * layout::knobSpan);
    drawLine (g, b.getCentre(), lineFrom * half, lineTo * half, a, lineW * half, lineCol.withAlpha (0.95f));
}

//==============================================================================
// SELECTEUR TUBE
//==============================================================================
TubeSelector::TubeSelector (RangedAudioParameter& p)
    : attachment (p, [this] (float v)
                  {
                      const int np = jlimit (0, 3, roundToInt (v));
                      if (np != pos) { pos = np; repaint(); }
                  })
{
    attachment.sendInitialUpdate();
    shownAngle = layout::selAngle[pos];
    setMouseCursor (MouseCursor::PointingHandCursor);
}

bool TubeSelector::tick()
{
    const float target = layout::selAngle[pos];
    if (std::abs (target - shownAngle) < 0.05f) { if (shownAngle != target) { shownAngle = target; repaint(); return true; } return false; }
    shownAngle += (target - shownAngle) * 0.5f;                  // ~100 ms d'un bout a l'autre
    repaint();
    return true;
}

void TubeSelector::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float k = 1.0f / unitsPerPixel;                         // pixels par unite
    drawLine (g, c, layout::selLine[0] * k, layout::selLine[1] * k, shownAngle, layout::selLine[2] * k, lineCol.withAlpha (0.95f));
}

bool TubeSelector::hitTest (int x, int y)
{
    const Point<float> c (getWidth() * 0.5f, getHeight() * 0.5f), p ((float) x, (float) y);
    const float k = 1.0f / unitsPerPixel, d = c.getDistanceFrom (p);
    if (d <= layout::selR * k) return true;                                  // le bouton
    return p.y < c.y - 18.0f * k && d < (layout::selR + 40.0f) * k;          // inscriptions et graduations
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
    const float k = 1.0f / unitsPerPixel;
    const Point<float> c (getWidth() * 0.5f, getHeight() * 0.5f);
    if (c.getDistanceFrom (e.position) > layout::selR * k)      // clic sur une inscription : on y va directement
        moveTo (positionAt (e.position));
}

void TubeSelector::mouseDrag (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const float k = 1.0f / unitsPerPixel;
    const float dx = e.position.x - dragStartX + (e.getDistanceFromDragStartY() * -0.4f);
    if (std::abs (dx) > 8.0f * k) dragging = true;
    if (dragging) moveTo (dragStartPos + (int) std::round (dx / (28.0f * k)));
}

void TubeSelector::mouseUp (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const float k = 1.0f / unitsPerPixel;
    const Point<float> c (getWidth() * 0.5f, getHeight() * 0.5f);
    if (! dragging && c.getDistanceFrom (e.mouseDownPosition) <= layout::selR * k)
        moveTo ((pos + 1) % 4);                                  // clic sur le bouton : lampe suivante
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

bool LeverToggle::hitTest (int x, int y)
{
    const Point<float> c (getWidth() * 0.5f, getHeight() * 0.5f);
    return c.getDistanceFrom ({ (float) x, (float) y }) <= getWidth() * (layout::togHitR / layout::togSize);
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
// POWER
//==============================================================================
PowerButton::PowerButton (RangedAudioParameter& p)
    : attachment (p, [this] (float v) { value = v > 0.5f; })
{
    attachment.sendInitialUpdate();
    setMouseCursor (MouseCursor::PointingHandCursor);
}

bool PowerButton::hitTest (int x, int y)
{
    const Point<float> c (getWidth() * 0.5f, getHeight() * 0.5f);
    return c.getDistanceFrom ({ (float) x, (float) y }) <= getWidth() * 0.5f;
}

void PowerButton::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
    attachment.setValueAsCompleteGesture (value ? 0.0f : 1.0f);
}

//==============================================================================
// EDITEUR
//==============================================================================
AnodeAudioProcessorEditor::AnodeAudioProcessorEditor (AnodeAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      selector    (*p.apvts.getParameter ("tube")),
      topoSwitch  (*p.apvts.getParameter ("topology")),
      modeSwitch  (*p.apvts.getParameter ("mode")),
      powerButton (*p.apvts.getParameter ("power"))
{
    using namespace BinaryData;
    background = ImageCache::getFromMemory (background_jpg, background_jpgSize);
    tubesHot   = ImageCache::getFromMemory (tubes_hot_jpg, tubes_hot_jpgSize);
    tubesBloom = ImageCache::getFromMemory (tubes_bloom_png, tubes_bloom_pngSize);
    tubesRed   = ImageCache::getFromMemory (tubes_red_png, tubes_red_pngSize);
    powerOn    = ImageCache::getFromMemory (power_on_png, power_on_pngSize);
    vuOn       = ImageCache::getFromMemory (vu_on_png, vu_on_pngSize);
    mechOff    = ImageCache::getFromMemory (vu_mech_off_png, vu_mech_off_pngSize);
    mechOn     = ImageCache::getFromMemory (vu_mech_on_png, vu_mech_on_pngSize);
    const auto togStrip = ImageCache::getFromMemory (toggle_strip_png, toggle_strip_pngSize);
    labelFace = Typeface::createSystemTypefaceFor (InterMedium_otf, InterMedium_otfSize);
    menuFace  = Typeface::createSystemTypefaceFor (PoppinsMedium_ttf, PoppinsMedium_ttfSize);
    menuLnf.reg = menuLnf.med = menuLnf.bold = menuFace;
    setLookAndFeel (&menuLnf);                                   // menus (et sous-menus) au design d'Octopus
    logoNineHands = svgFrom (nine_hands_logo_svg, nine_hands_logo_svgSize);
    logoAnode     = svgFrom (anode_logo_svg, anode_logo_svgSize);

    const float kr = layout::knobR * 1.18f;                       // demi-cote du composant potard (unites)
    for (int i = 0; i < 3; ++i)
    {
        auto& s = knobs[i];
        s.lineFrom = layout::knobLine[0] / kr; s.lineTo = layout::knobLine[1] / kr; s.lineW = layout::knobLine[2] / kr;
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
    powerButton.onRightClick = [this] { showContextMenu (6); };
    addAndMakeVisible (powerButton);

    for (int i = 0; i < AnodeAudioProcessor::numParams; ++i) lastValues[i] = proc.value (i);
    lastPresetCounter = proc.presetCounter.load();
    lastMidiCount = proc.midiCounter.load();
    // a l'ouverture de la fenetre, l'ampli est deja chaud s'il est allume
    lamp = heater = proc.value (AnodeAudioProcessor::pPower) > 0.5f ? 1.0f : 0.0f;

    // taille par defaut (« 60 % ») = taille minimale ; proportions bloquees
    setResizable (true, true);
    setResizeLimits (widthFor (defaultH), defaultH, widthFor (1350), 1350);
    if (auto* cons = getConstrainer()) cons->setFixedAspectRatio ((double) layout::W / (double) layout::H);
    setOpaque (true);
    setSize (widthFor (defaultH), defaultH);
    setScaledHeight (sizeHeight (WigaWindowSize::defaultIndex (JucePlugin_Name, numSizes)));
    rebuildCaches();
    startTimerHz (30);
}

AnodeAudioProcessorEditor::~AnodeAudioProcessorEditor() { stopTimer(); setLookAndFeel (nullptr); }

int AnodeAudioProcessorEditor::sizeHeight (int idx)
{
    const int h[numSizes] = { (int) defaultH, 719, 958 };
    return h[jlimit (0, numSizes - 1, idx)];
}

int AnodeAudioProcessorEditor::widthFor (int h) { return roundToInt ((float) h * layout::W / layout::H); }

void AnodeAudioProcessorEditor::setScaledHeight (int h)
{
    h = jmax ((int) defaultH, h);
    setSize (widthFor (h), h);
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
// elements fixes vectoriels : textes, points de valeur, graduations, symbole Power, logos
//==============================================================================
void AnodeAudioProcessorEditor::drawStatic (Graphics& g, float u)
{
    // textes : taille em -> hauteur des capitales ; centres sur le milieu des capitales
    static float capAt100 = 0.0f;
    if (capAt100 <= 0.0f)
    {
        GlyphArrangement ga; ga.addLineOfText (Font (FontOptions (labelFace).withHeight (100.0f)), "H", 0.0f, 0.0f);
        Path hp; ga.createPath (hp);                                  // hauteur reelle de l'encre (pas la hauteur de ligne)
        capAt100 = jmax (1.0f, hp.getBounds().getHeight());
    }
    g.setColour (ink);
    for (const auto& l : layout::labels)
    {
        const float cap = capRatio * l.size * u;
        Font f (FontOptions (labelFace).withHeight (100.0f * cap / capAt100));
        f.setExtraKerningFactor ((l.tracking * u) / f.getHeight());            // espacement des lettres de la maquette
        GlyphArrangement ga;
        ga.addLineOfText (f, CharPointer_UTF8 (l.text), 0.0f, 0.0f);
        Path tp; ga.createPath (tp);
        const auto bb = tp.getBounds();
        ga.moveRangeOfGlyphs (0, -1, l.x * u - bb.getCentreX(), l.y * u + cap * 0.5f);
        ga.draw (g);
    }
    // points de valeur (0 a 10) autour des potards
    for (const auto& kc : layout::knobC)
        for (int v = 0; v <= 10; ++v)
        {
            const auto p = polar ({ kc[0] * u, kc[1] * u }, layout::dotRing * u, -135.0f + 27.0f * (float) v);
            g.fillEllipse (Rectangle<float> (layout::dotR * 2 * u, layout::dotR * 2 * u).withCentre (p));
        }
    // graduations du selecteur
    for (float a : layout::selAngle)
    {
        Path pth;
        const Point<float> c (layout::selC[0] * u, layout::selC[1] * u);
        pth.startNewSubPath (polar (c, layout::selTick[0] * u, a));
        pth.lineTo (polar (c, layout::selTick[1] * u, a));
        g.strokePath (pth, PathStrokeType (layout::selTick[2] * u));
    }
    // symbole Power
    {
        const Point<float> c (layout::powerSym[0] * u, layout::powerSym[1] * u);
        const float r = layout::powerSym[2] * u, w = layout::powerSym[5] * u;
        Path arc; arc.addCentredArc (c.x, c.y, r, r, 0.0f, degreesToRadians (30.0f), degreesToRadians (330.0f), true);
        g.strokePath (arc, PathStrokeType (w));
        Path bar; bar.startNewSubPath (c.x, c.y - layout::powerSym[3] * u); bar.lineTo (c.x, c.y - layout::powerSym[4] * u);
        g.strokePath (bar, PathStrokeType (w));
    }
    // logos (SVG fournis)
    auto rect = [u] (const float* r) { return Rectangle<float> (r[0] * u, r[1] * u, r[2] * u, r[3] * u); };
    if (logoNineHands) logoNineHands->drawWithin (g, rect (layout::nineHands), RectanglePlacement::stretchToFit, 1.0f);
    if (logoAnode)     logoAnode->drawWithin (g, rect (layout::anode), RectanglePlacement::stretchToFit, 1.0f);
}

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
    // fond + elements vectoriels, dessines a la resolution physique de l'ecran (nets sur Retina / 4K)
    Image bg (Image::RGB, pw, ph, true);
    {
        Graphics gg (bg);
        gg.addTransform (AffineTransform::scale (sc));
        gg.setImageResamplingQuality (Graphics::highResamplingQuality);
        gg.drawImage (background, Rectangle<float> (0.0f, 0.0f, (float) w, (float) h));
        drawStatic (gg, unit());
    }
    bgScaled = bg;
    const float k = unit() * sc;                                  // pixels physiques par unite
    auto sz = [&] (const Image& src, float wu, float hu)
    {
        const int iw = roundToInt (wu * k), ih = roundToInt (hu * k);
        return (iw >= src.getWidth()) ? src : src.rescaled (iw, ih, Graphics::highResamplingQuality);
    };
    hotS   = sz (tubesHot,   layout::tubeZone[2], layout::tubeZone[3]);
    bloomS = sz (tubesBloom, layout::tubeZone[2], layout::tubeZone[3]);
    redS   = sz (tubesRed,   layout::tubeZone[2], layout::tubeZone[3]);
    powerOnS = sz (powerOn, layout::powerRect[2], layout::powerRect[3]);
    vuOnS    = sz (vuOn,    layout::vuRect[0][2], layout::vuRect[0][3]);
    mechOffS = sz (mechOff, layout::vuRect[0][2], layout::vuRect[0][3]);
    mechOnS  = sz (mechOn,  layout::vuRect[0][2], layout::vuRect[0][3]);
    repaint();
}

void AnodeAudioProcessorEditor::resized()
{
    // anti-gel : jamais de setSize ici
    using namespace layout;
    const float kr = knobR * 1.18f;
    for (int i = 0; i < 3; ++i)
        knobs[i].setBounds (scaled ({ knobC[i][0] - kr, knobC[i][1] - kr, kr * 2, kr * 2 }));
    const float sh = selR + 44.0f;
    selector.setBounds (scaled ({ selC[0] - sh, selC[1] - sh, sh * 2, sh * 2 }));
    selector.unitsPerPixel = 1.0f / unit();
    topoSwitch.setBounds (scaled ({ togC[0][0] - togSize * 0.5f, togC[0][1] - togSize * 0.5f, togSize, togSize }));
    modeSwitch.setBounds (scaled ({ togC[1][0] - togSize * 0.5f, togC[1][1] - togSize * 0.5f, togSize, togSize }));
    powerButton.setBounds (scaled ({ powerC[0] - powerR, powerC[1] - powerR, powerR * 2, powerR * 2 }));
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
        case P::pPower:   return proc.value (P::pPower) > 0.5f ? "Power on" : "Power off (bypass)";
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

float AnodeAudioProcessorEditor::needleAngle (float pos) const
{
    using namespace layout;
    if (pos <= 0.0f) return vuRest;
    const float db = 20.0f * std::log10 (pos);
    if (db >= vuTickDb[vuNumTicks - 1]) return jmin (vuTickDeg[vuNumTicks - 1] + (db - vuTickDb[vuNumTicks - 1]) * 2.5f, 48.0f);
    if (db <= vuTickDb[0]) return vuRest + (vuTickDeg[0] - vuRest) * pos / std::pow (10.0f, vuTickDb[0] / 20.0f);
    for (int i = 0; i + 1 < vuNumTicks; ++i)
        if (db <= vuTickDb[i + 1])
            return vuTickDeg[i] + (vuTickDeg[i + 1] - vuTickDeg[i]) * (db - vuTickDb[i]) / (vuTickDb[i + 1] - vuTickDb[i]);
    return vuTickDeg[vuNumTicks - 1];
}

//==============================================================================
void AnodeAudioProcessorEditor::timerCallback()
{
    using P = AnodeAudioProcessor;
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
    for (int i = 0; i < P::numParams; ++i)
    {
        const float v = proc.value (i);
        if (std::abs (v - lastValues[i]) > 1.0e-4f) { lastValues[i] = v; changed = i; ++numChanged; }
    }
    const int pc = proc.presetCounter.load();
    if (pc != lastPresetCounter) { lastPresetCounter = pc; numChanged = 0; }
    if (numChanged == 1) { lastChanged = changed; showStatus(); learnLineUntil = 0.0; }
    for (int i = 0; i < 3; ++i)
        if (knobs[i].isMouseButtonDown()) { lastChanged = P::learnDef (i).param; showStatus(); learnLineUntil = 0.0; }

    const int lt = proc.learnTarget.load();
    const int learned = proc.lastLearned.exchange (-1);
    if (learned >= 0)
    {
        learnLine = String (P::learnDef (learned).name) + " = CC " + String (proc.ccMap[learned].load());
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
    const auto pl = buildPresetLine();
    const Rectangle<float> presetR (layout::preset[0], layout::preset[1], layout::preset[2], layout::preset[3]);
    if (std::abs (statusAlpha - drawnStatus) > 0.003f || (statusAlpha > 0.0f && textChanged)
        || pl != lastPreset || hoverZone != drawnHover || midiFlash > 0)
    {
        drawnStatus = statusAlpha; lastPreset = pl; drawnHover = hoverZone;
        askRepaint (scaled (presetR.expanded (2.0f)));
    }

    // Power : ampoules des cadrans et anneau (rapides), filaments des lampes (chauffent ~2 s, refroidissent ~3 s)
    const bool on = proc.value (P::pPower) > 0.5f;
    const float dt = 1.0f / 30.0f;
    lamp += ((on ? 1.0f : 0.0f) - lamp) * (on ? 0.30f : 0.16f);
    if (std::abs (lamp - (on ? 1.0f : 0.0f)) < 0.003f) lamp = on ? 1.0f : 0.0f;
    heater += ((on ? 1.0f : 0.0f) - heater) * (1.0f - std::exp (-dt / (on ? 0.75f : 1.1f)));
    if (std::abs (heater - (on ? 1.0f : 0.0f)) < 0.003f) heater = on ? 1.0f : 0.0f;

    // lueur des lampes : suit le courant (moyen en classe AB, alternatif en classe A), puis rougeoiement des plaques
    // echelle logarithmique : la lueur suit chaque attaque et chaque extinction de note sans rester bloquee au maximum
    const float gAvg   = std::log (jmax (1.0f, proc.plateGlow.load())) / std::log (8.0f);
    const float gSwing = std::log1p (jmax (0.0f, proc.plateSwing.load() - 0.04f)) / std::log (5.0f);
    const float gt = (on && proc.outLevel.load() > 1.0e-4f) ? jlimit (0.0f, 1.0f, 0.55f * gAvg + 0.6f * gSwing) : 0.0f;
    glowLevel += (gt - glowLevel) * (gt > glowLevel ? 0.35f : 0.10f);
    if (glowLevel < 0.003f) glowLevel = 0.0f;
    heatLevel = on ? proc.plateHeat.load() : heatLevel * 0.97f;
    if (heatLevel < 0.004f) heatLevel = 0.0f;

    if (std::abs (heater - drawnHeater) > 0.004f || std::abs (glowLevel - drawnGlow) > 0.008f || std::abs (heatLevel - drawnHeat) > 0.008f)
    {
        drawnHeater = heater; drawnGlow = glowLevel; drawnHeat = heatLevel;
        askRepaint (scaled ({ layout::tubeZone[0], layout::tubeZone[1], layout::tubeZone[2], layout::tubeZone[3] }));
    }
    const bool lampChanged = std::abs (lamp - drawnLamp) > 0.004f;
    if (lampChanged)
    {
        drawnLamp = lamp;
        askRepaint (scaled ({ layout::powerRect[0], layout::powerRect[1], layout::powerRect[2], layout::powerRect[3] }));
    }

    // VU-metres : balistique d'un vrai galvanometre (~300 ms, leger depassement), 0 VU = -18 dBFS efficace
    constexpr float wn = 13.0f, zeta = 0.80f;
    for (int i = 0; i < 2; ++i)
    {
        const float tgt = jlimit (0.0f, 1.75f, (float) (std::sqrt ((double) proc.vuLevel[i].load()) / vuRef));
        for (int s = 0; s < 10; ++s)
        {
            const float h = dt / 10.0f;
            const float acc = wn * wn * (tgt - vuPos[i]) - 2.0f * zeta * wn * vuVel[i];
            vuVel[i] += acc * h;
            vuPos[i] += vuVel[i] * h;
            if (vuPos[i] < 0.0f) { vuPos[i] = 0.0f; vuVel[i] = jmax (0.0f, vuVel[i]); }
            if (vuPos[i] > 1.75f) { vuPos[i] = 1.75f; vuVel[i] = jmin (0.0f, vuVel[i]); }
        }
        const float a = needleAngle (vuPos[i]);
        if (std::abs (a - drawnAngle[i]) > 0.05f || lampChanged)
        {
            drawnAngle[i] = a;
            askRepaint (scaled ({ layout::vuRect[i][0], layout::vuRect[i][1], layout::vuRect[i][2], layout::vuRect[i][3] }));
        }
    }
}

//==============================================================================
void AnodeAudioProcessorEditor::drawNeedle (Graphics& g, int i, float u)
{
    const float a = drawnAngle[i] > -900.0f ? drawnAngle[i] : needleAngle (vuPos[i]);
    const Point<float> pv ((layout::vuRect[i][0] + layout::vuPivot[0]) * u, (layout::vuRect[i][1] + layout::vuPivot[1]) * u);
    const float len = layout::vuNeedleLen * u, w = layout::vuNeedleW * u;
    const auto tip = polar (pv, len, a);
    const auto dir = (tip - pv) / jmax (1.0e-3f, len);
    const Point<float> nrm (-dir.y, dir.x);
    Path p;
    p.startNewSubPath (pv + nrm * (w * 0.675f));
    p.lineTo (tip + nrm * (w * 0.275f));
    p.lineTo (tip - nrm * (w * 0.275f));
    p.lineTo (pv - nrm * (w * 0.675f));
    p.closeSubPath();
    g.setColour (Colours::black.withAlpha (0.20f));                   // ombre legere sur le cadran
    g.fillPath (p, AffineTransform::translation (1.2f * u, 2.4f * u));
    g.setColour (needleCol);
    g.fillPath (p);
}

void AnodeAudioProcessorEditor::paint (Graphics& g)
{
    lastPaintMs = Time::getMillisecondCounterHiRes();
    repaintAskedMs = 0.0;
    g.fillAll (Colours::black);
    const float u = unit();
    auto R = [u] (float x, float y, float w, float h) { return Rectangle<float> (x * u, y * u, w * u, h * u); };
    auto pick = [] (const Image& sc, const Image& orig) -> const Image& { return sc.isValid() ? sc : orig; };

    if (bgScaled.isValid())
    {
        g.setImageResamplingQuality (Graphics::lowResamplingQuality);
        g.drawImage (bgScaled, getLocalBounds().toFloat());
    }
    else
    {
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (background, getLocalBounds().toFloat());
        drawStatic (g, u);
    }
    g.setImageResamplingQuality (Graphics::mediumResamplingQuality);

    // lampes : filaments (Power), lueur qui suit le jeu, plaques qui rougissent
    const auto tz = R (layout::tubeZone[0], layout::tubeZone[1], layout::tubeZone[2], layout::tubeZone[3]);
    if (heater > 0.0f && g.clipRegionIntersects (tz.getSmallestIntegerContainer()))
    {
        g.setOpacity (heater);
        g.drawImage (pick (hotS, tubesHot), tz);
        if (glowLevel > 0.0f) { g.setOpacity (heater * glowLevel); g.drawImage (pick (bloomS, tubesBloom), tz); }
        if (heatLevel > 0.0f) { g.setOpacity (heater * heatLevel); g.drawImage (pick (redS, tubesRed), tz); }
        g.setOpacity (1.0f);
    }

    // Power allume
    if (lamp > 0.0f)
    {
        g.setOpacity (lamp);
        g.drawImage (pick (powerOnS, powerOn), R (layout::powerRect[0], layout::powerRect[1], layout::powerRect[2], layout::powerRect[3]));
        g.setOpacity (1.0f);
    }

    // cadrans : eclairage, aiguille, mecanisme devant l'aiguille
    for (int i = 0; i < 2; ++i)
    {
        const auto vr = R (layout::vuRect[i][0], layout::vuRect[i][1], layout::vuRect[i][2], layout::vuRect[i][3]);
        if (! g.clipRegionIntersects (vr.getSmallestIntegerContainer())) continue;
        if (lamp > 0.0f) { g.setOpacity (lamp); g.drawImage (pick (vuOnS, vuOn), vr); g.setOpacity (1.0f); }
        drawNeedle (g, i, u);
        g.drawImage (pick (mechOffS, mechOff), vr);
        if (lamp > 0.0f) { g.setOpacity (lamp); g.drawImage (pick (mechOnS, mechOn), vr); g.setOpacity (1.0f); }
    }

    // barre de presets (sur le haut des transformateurs) ; la valeur d'un reglage qui bouge y apparait 2,5 s
    const auto pr = R (layout::preset[0], layout::preset[1], layout::preset[2], layout::preset[3]);
    g.setColour (Colour (0xe0090909));
    g.fillRoundedRectangle (pr, pr.getHeight() * 0.5f);
    g.setColour (accent.withAlpha (hoverZone != 0 ? 0.55f : 0.28f));
    g.drawRoundedRectangle (pr.reduced (0.5f), pr.getHeight() * 0.5f, jmax (1.0f, 0.9f * u));
    const float aw = 24.0f * u;
    auto arrow = [&] (Rectangle<float> ar, bool left, bool hot)
    {
        Path p; const auto c = ar.getCentre(); const float s = 4.4f * u;
        if (left) p.addTriangle (c.x + s * 0.6f, c.y - s, c.x + s * 0.6f, c.y + s, c.x - s * 0.8f, c.y);
        else      p.addTriangle (c.x - s * 0.6f, c.y - s, c.x - s * 0.6f, c.y + s, c.x + s * 0.8f, c.y);
        g.setColour (ink.withAlpha (hot ? 1.0f : 0.55f));
        g.fillPath (p);
    };
    arrow (pr.withWidth (aw), true, hoverZone == 1);
    arrow (pr.withTrimmedLeft (pr.getWidth() - aw), false, hoverZone == 3);
    const auto tr = pr.reduced (aw, 0.0f).toNearestInt();
    g.setFont (Font (FontOptions (menuFace).withHeight (13.5f * u)));
    if (statusAlpha < 1.0f)
    {
        g.setColour ((midiFlash > 0 ? Colour (0xffffc070) : ink).withAlpha ((1.0f - statusAlpha) * (hoverZone == 2 ? 1.0f : 0.88f)));
        g.drawFittedText (lastPreset, tr, Justification::centred, 1, 0.7f);
    }
    if (statusAlpha > 0.0f)
    {
        g.setColour (Colour (0xffffd6a0).withAlpha (statusAlpha));
        g.drawFittedText (lastLine, tr, Justification::centred, 1, 0.7f);
    }
}

//==============================================================================
// Souris : barre de presets (fleches = precedent / suivant, nom = menu), clic droit = menu (a l'endroit du clic)
//==============================================================================
int AnodeAudioProcessorEditor::presetZoneAt (Point<float> p) const
{
    const Rectangle<float> r (layout::preset[0], layout::preset[1], layout::preset[2], layout::preset[3]);
    if (! r.contains (p)) return 0;
    if (p.x < r.getX() + 24.0f) return 1;
    if (p.x > r.getRight() - 24.0f) return 3;
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
    m.setLookAndFeel (&menuLnf);
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
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (scaled ({ layout::preset[0], layout::preset[1], layout::preset[2], layout::preset[3] }))),
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
    menu.setLookAndFeel (&menuLnf);
    auto ccText = [this] (int i)
    {
        const int cc = proc.ccMap[i].load();
        return cc >= 0 ? "CC " + String (cc) : String ("no CC");
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
    menu.addItem (999, "Anode v" + String (JucePlugin_VersionString) + " - NINE HANDS | DSP", false);

    menu.showMenuAsync (PopupMenu::Options().withMousePosition(),
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
