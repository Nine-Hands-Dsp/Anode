#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "LevelComp.h"

using namespace juce;

//==============================================================================
// Presets d'usine (potards 0..10, 5 = midi)
//==============================================================================
static const AnodePreset factoryPresets[] =
{
    //  nom                       tube PP pent  depth contour volume  out dB
    { "EL34 Plexi Crunch",          1,  1,  1,   6.0f, 6.0f,  7.2f,   0.5f },
    { "EL34 Hot Lead",              1,  1,  1,   7.0f, 7.0f,  9.0f,  -3.5f },
    { "6L6 Blackface Clean",        0,  1,  1,   4.0f, 5.0f,  4.0f,   -2.0f },
    { "6L6 Tweed Push",             0,  1,  1,   6.0f, 4.5f,  7.6f,   -2.0f },
    { "EL84 Top Boost Chime",       2,  1,  1,   4.0f, 6.5f,  5.8f,   5.8f },
    { "EL84 Class A Grit",          2,  1,  1,   5.0f, 6.0f,  8.0f,   2.1f },
    { "KT88 Bass Authority",        3,  1,  1,   6.5f, 4.0f,  6.2f,   1.0f },
    { "KT88 Hi-Fi Triode",          3,  1,  0,   5.0f, 5.0f,  3.5f,   1.9f },
    { "SE 6L6 Champ Breakup",       0,  0,  1,   5.0f, 5.5f,  7.0f,  -8.6f },
    { "SE EL84 Cranked",            2,  0,  1,   5.0f, 5.0f,  9.0f,  -1.9f },
    { "SE EL34 Triode Warmth",      1,  0,  0,   5.0f, 4.0f,  5.0f,   0.7f },
    { "Triode Studio Glue",         0,  1,  0,   5.0f, 5.0f,  5.5f,   2.3f },
    { "Modern Tight (after drive)", 1,  1,  1,   8.0f, 7.5f,  6.5f,   -4.0f },
    { "Doom Bass SE",               3,  0,  1,   7.0f, 3.0f,  8.5f,  -5.1f },
    { "Diezel 7",                   1,  1,  1,   4.0f, 3.5f,  5.0f,   0.5f },   // devant une capture NAM VH4 + IR : grave serre, medium plein, aigu doux
};

int AnodeAudioProcessor::getNumFactoryPresets() { return (int) std::size (factoryPresets); }
const AnodePreset& AnodeAudioProcessor::getFactoryPreset (int i)
{
    return factoryPresets[(size_t) jlimit (0, getNumFactoryPresets() - 1, i)];
}

const char* AnodeAudioProcessor::paramId (int i)
{
    static const char* ids[numParams] = { "tube", "topology", "mode", "depth", "contour", "volume", "output", "power" };
    return ids[jlimit (0, numParams - 1, i)];
}

const AnodeAudioProcessor::LearnDef& AnodeAudioProcessor::learnDef (int i)
{
    static const LearnDef defs[numLearn] =
    {
        { pDepth,   "Depth",                         learnContinuous, 0 },
        { pContour, "Contour",                       learnContinuous, 0 },
        { pVolume,  "Volume",                        learnContinuous, 0 },
        { pTube,    "Tube Model",                    learnSelector,   4 },
        { pTopo,    "Single-Ended / Push-Pull",      learnToggle,     2 },
        { pMode,    "Triode / Pentode",              learnToggle,     2 },
        { pPower,   "Power",                         learnToggle,     2 },
    };
    return defs[jlimit (0, numLearn - 1, i)];
}

void AnodeAudioProcessor::clearMidiMap()
{
    for (auto& c : ccMap) c.store (-1);
}

//==============================================================================
AnodeAudioProcessor::AnodeAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ANODE", createLayout())
{
    anode::Tables::get();                         // tables des lampes construites hors thread audio
    for (int i = 0; i < numParams; ++i) rawPtr[i] = apvts.getRawParameterValue (paramId (i));
    for (int i = 0; i < numParams; ++i) { pendingValue[i] = 0.0f; pendingSet[i] = false; }
    clearMidiMap();
    startTimerHz (60);
}

AudioProcessorValueTreeState::ParameterLayout AnodeAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const auto& d = factoryPresets[0];
    auto knob = [] (const String& id, const String& name, float def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float v, int) { return String (roundToInt (v * 10.0f)) + " %"; }));
    };
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "tube", 1 }, "Tube Model", tubeNames(), d.tube));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "topology", 1 }, "Topology",
                                                         StringArray { "Single-Ended", "Push-Pull" }, d.pushPull));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Triode / Pentode",
                                                         StringArray { "Triode", "Pentode" }, d.pentode));
    p.push_back (knob ("depth",   "Depth",   d.depth));
    p.push_back (knob ("contour", "Contour", d.contour));
    p.push_back (knob ("volume",  "Volume",  d.volume));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; })));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "power", 1 }, "Power", true));
    return { p.begin(), p.end() };
}

bool AnodeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return in == out || in == AudioChannelSet::mono();
}

void AnodeAudioProcessor::setParamValue (const String& id, float plain)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

// anti-gel : le thread audio met la valeur en file, le timer du thread de messages la transmet a l'hote
void AnodeAudioProcessor::queueParam (int param, float plain)
{
    pendingValue[param].store (plain, std::memory_order_relaxed);
    pendingSet[param].store (true, std::memory_order_release);
}

void AnodeAudioProcessor::timerCallback()
{
    for (int i = 0; i < numParams; ++i)
        if (pendingSet[i].exchange (false, std::memory_order_acquire))
            if (auto* prm = apvts.getParameter (paramId (i)))
                prm->setValueNotifyingHost (prm->convertTo0to1 (pendingValue[i].load (std::memory_order_relaxed)));
}

//==============================================================================
void AnodeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    // taux interne >= 352,8 kHz (x8 a 44,1 / 48 kHz) : saturation sans repliement audible
    osFactorLog2 = sr <= 50000.0 ? 3 : (sr <= 100000.0 ? 2 : (sr <= 200000.0 ? 1 : 0));
    oversampler = std::make_unique<dsp::Oversampling<float>> (2, (size_t) osFactorLog2,
                       dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing ((size_t) jmax (1, samplesPerBlock));
    osRate = sr * (double) (1 << osFactorLog2);

    // 16 configurations calibrees et stabilisees au repos (une seule fois par frequence)
    if (bankRate != osRate || bank.size() != 16)
    {
        bank.assign (16, anode::PowerStage());
        for (int t = 0; t < 4; ++t)
            for (int pp = 0; pp < 2; ++pp)
                for (int pe = 0; pe < 2; ++pe)
                {
                    auto& s = bank[(size_t) configIndex (t, pp == 1, pe == 1)];
                    s.setup (t, pp == 1, pe == 1, osRate);
                    s.settle (0.5);
                }
        // egalisation partielle des configurations en saturation (une EL84 SE reste moins puissante
        // qu'une paire de KT88, mais l'ecart est ramene a quelques dB comme sur des amplis regles a l'oreille)
        const double ref = bank[(size_t) configIndex (1, true, true)].saturatedRms();
        std::vector<double> k (16);
        for (int i = 0; i < 16; ++i) k[(size_t) i] = std::pow (ref / std::max (1e-6, bank[(size_t) i].saturatedRms()), 0.7);
        for (int i = 0; i < 16; ++i) bank[(size_t) i].scaleLevel (k[(size_t) i]);
        bankRate = osRate;
    }
    // alignement de niveau des lampes (table mesuree hors ligne, voir tools/render « calib »)
    for (int i = 0; i < 16; ++i) bank[(size_t) i].setVolumeComp (useLevelComp ? anode::kLevelComp[i] : nullptr);

    currentConfig = configIndex (jlimit (0, 3, roundToInt (value (pTube))), value (pTopo) > 0.5f, value (pMode) > 0.5f);
    for (auto& c : chans) { c.active = bank[(size_t) currentConfig]; c.fadeLeft = 0; }
    fadeLen = (int) (0.035 * osRate);
    smoothA = 1.0 - std::exp (-1.0 / (0.020 * osRate));
    sDepth = value (pDepth) / 10.0; sContour = value (pContour) / 10.0;
    sDrive = std::pow (10.0, (-24.0 + 4.8 * value (pVolume)) / 20.0);
    sOut = Decibels::decibelsToGain ((double) value (pOutput));
    sMake = std::pow (sDrive, -0.35);
    for (auto& r : ramps) { r.active = false; r.left = 0; }

    // latence des filtres du sur-echantillonnage
    latency = roundToInt (oversampler->getLatencyInSamples());
    setLatencySamples (latency);
    dryRing.setSize (2, jmax (64, latency + 1) + jmax (1, samplesPerBlock) * 2);
    dryRing.clear(); dryPos = 0;
    sPower = value (pPower) > 0.5f ? 1.0 : 0.0;
    vuMs[0] = vuMs[1] = 0.0;
}

//==============================================================================
// MIDI : MIDI Learn uniquement (aucun CC par defaut), pedale d'expression sans saccades
//==============================================================================
float AnodeAudioProcessor::mapCC (int k, float raw01)
{
    auto& r = ramps[k];
    if (raw01 < r.lo) r.lo = raw01;
    if (raw01 > r.hi) r.hi = raw01;
    float v = raw01;
    if (r.hi - r.lo > 0.4f) v = (raw01 - r.lo) / (r.hi - r.lo);
    constexpr float dz = 0.02f;
    return jlimit (0.0f, 1.0f, (v - dz) / (1.0f - 2.0f * dz));
}

void AnodeAudioProcessor::handleMidi (MidiBuffer& midi)
{
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int64 when = sampleClock + meta.samplePosition;
        if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            const int lt = learnTarget.load();
            if (lt >= 0 && lt < numLearn && cc < 120)
            {
                for (int j = 0; j < numLearn; ++j)
                    if (j != lt && ccMap[j].load() == cc) ccMap[j].store (-1);
                ccMap[lt].store (cc);
                if (lt < 3) ramps[lt].msb = -1;
                lastLearned.store (lt);
                learnTarget.store (-1);
                ++midiCounter;
                continue;
            }

            int ctl = -1, lsbOf = -1;
            for (int j = 0; j < numLearn; ++j)
                if (ccMap[j].load (std::memory_order_relaxed) == cc) { ctl = j; break; }
            if (ctl < 0)
                for (int k = 0; k < 3; ++k)
                {
                    const int msbCC = ccMap[k].load (std::memory_order_relaxed);
                    if (msbCC >= 0 && msbCC < 32 && cc == msbCC + 32) { lsbOf = k; break; }
                }

            auto knob = [&] (int k, float raw01)
            {
                auto& r = ramps[k];
                const int prm = learnDef (k).param;
                const float target = mapCC (k, raw01) * 10.0f;
                if (! r.active) r.value = value (prm);
                r.left = (int) jlimit (0.002 * sr, 0.040 * sr, (double) (when - r.lastTime));
                r.lastTime = when;
                r.target = target;
                r.step = (r.target - r.value) / (float) r.left;
                r.active = true;
                queueParam (prm, target);
                currentPreset = -1;
            };

            if (ctl >= 0 && ctl < 3) { ramps[ctl].msb = v; knob (ctl, (float) v / 127.0f); }
            else if (lsbOf >= 0) { if (ramps[lsbOf].msb >= 0) knob (lsbOf, (float) (ramps[lsbOf].msb * 128 + v) / 16383.0f); }
            else if (ctl == 3) { queueParam (pTube, (float) jlimit (0, 3, v * 4 / 128)); currentPreset = -1; }
            else if (ctl == 4) { queueParam (pTopo, v >= 64 ? 1.0f : 0.0f); currentPreset = -1; }
            else if (ctl == 5) { queueParam (pMode, v >= 64 ? 1.0f : 0.0f); currentPreset = -1; }
            else if (ctl == 6)
            {
                // footswitch MIDI : latching (127 = on / 0 = off) ou momentary (bascule a chaque appui)
                if (! midiFsMomentary.load (std::memory_order_relaxed)) queueParam (pPower, v >= 64 ? 1.0f : 0.0f);
                else if (v >= 64 && lastPowerCC < 64)                     queueParam (pPower, value (pPower) > 0.5f ? 0.0f : 1.0f);
                lastPowerCC = v;
            }
            else continue;
            ++midiCounter;
        }
        else if (m.isProgramChange())
        {
            const int pc = m.getProgramChangeNumber();
            if (pc < getNumFactoryPresets()) { pendingPreset = pc; triggerAsyncUpdate(); }
            ++midiCounter;
        }
    }
    midi.clear();
}

void AnodeAudioProcessor::handleAsyncUpdate()
{
    const int p = pendingPreset.exchange (-1);
    if (p >= 0) applyFactoryPreset (p);
}

//==============================================================================
void AnodeAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    handleMidi (midi);
    if (n == 0 || oversampler == nullptr || bank.size() != 16) return;

    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    if (numIn == 1 && numOut > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    for (int c = jmax (numIn, 1); c < numOut; ++c)
        if (numIn != 1) buffer.clear (c, 0, n);

    // Power : copie du signal direct (bypass) avant traitement
    const int ringN = dryRing.getNumSamples();
    const bool ringOk = ringN >= n + latency + 1;
    if (ringOk)
        for (int c = 0; c < 2; ++c)
        {
            const float* src = buffer.getReadPointer (jmin (c, numOut - 1));
            float* r = dryRing.getWritePointer (c);
            for (int i = 0; i < n; ++i) r[(dryPos + i) % ringN] = src[i];
        }

    // commandes : rampes MIDI (pedale d'expression) ou valeur du parametre
    float knobVal[3];
    for (int k = 0; k < 3; ++k)
    {
        auto& r = ramps[k];
        if (r.active)
        {
            const int adv = jmin (r.left, n);
            r.value += r.step * (float) adv;
            r.left -= adv;
            if (r.left <= 0) { r.value = r.target; r.active = false; }
            knobVal[k] = r.value;
        }
        else knobVal[k] = value (learnDef (k).param);
    }
    const double tDepth   = knobVal[0] / 10.0;
    const double tContour = knobVal[1] / 10.0;
    // Volume (master) : -24 dB (0) .. 0 dB (5) .. +24 dB (10) vers le dephaseur ; sortie compensee en partie
    const double tDrive   = std::pow (10.0, (-24.0 + 4.8 * knobVal[2]) / 20.0);
    const double tOut     = std::pow (10.0, value (pOutput) / 20.0);

    // changement de lampe / topologie / mode : fondu enchaine 35 ms entre l'ancien et le nouvel etage
    const int cfgNow = configIndex (jlimit (0, 3, roundToInt (value (pTube))), value (pTopo) > 0.5f, value (pMode) > 0.5f);
    if (cfgNow != currentConfig)
    {
        for (auto& c : chans)
        {
            c.fading = c.active;
            c.active = bank[(size_t) cfgNow];
            c.fadeLeft = fadeLen;
        }
        currentConfig = cfgNow;
    }
    for (auto& c : chans) { c.active.setVolume (knobVal[2]); if (c.fadeLeft > 0) c.fading.setVolume (knobVal[2]); }

    // canaux identiques (guitare mono sur piste stereo) : un seul calcul
    const int nCh = jmin (2, numOut);
    bool same = nCh == 2;
    if (same)
    {
        const float* a = buffer.getReadPointer (0); const float* b = buffer.getReadPointer (1);
        for (int i = 0; i < n && same; ++i) same = a[i] == b[i];
    }
    const int procCh = same ? 1 : nCh;
    if (! same && prevSame && nCh == 2) chans[1] = chans[0];     // le canal droit repart de l'etat du gauche
    prevSame = same;

    float peak = 0.0f;
    double glowAcc = 0.0, swingAcc = 0.0;
    float* chPtr[2] = { buffer.getWritePointer (0), nCh > 1 ? buffer.getWritePointer (1) : nullptr };
    dsp::AudioBlock<float> block (chPtr, (size_t) procCh, (size_t) n);
    auto up = oversampler->processSamplesUp (block);
    const int un = (int) up.getNumSamples();
    const double tMake = std::pow (tDrive, -0.35);   // compensation partielle du volume (plus fort et plus sature en montant)
    double d1 = sDepth, c1 = sContour, g1 = sDrive, o1 = sOut, m1 = sMake;
    for (int c = 0; c < procCh; ++c)
    {
        float* x = up.getChannelPointer ((size_t) c);
        auto& ch = chans[c];
        // le lissage est commun aux canaux : chaque canal repart des memes valeurs
        double d0 = sDepth, c0 = sContour, g0 = sDrive, o0 = sOut, m0 = sMake;
        for (int i = 0; i < un; ++i)
        {
            d0 += smoothA * (tDepth - d0);
            c0 += smoothA * (tContour - c0);
            g0 += smoothA * (tDrive - g0);
            o0 += smoothA * (tOut - o0);
            m0 += smoothA * (tMake - m0);
            const double in = (double) x[i];
            ch.active.setControls (d0, c0);
            double y = ch.active.process (in, g0);
            if (ch.fadeLeft > 0)
            {
                ch.fading.setControls (d0, c0);
                const double yo = ch.fading.process (in, g0);
                const double t = (double) ch.fadeLeft / (double) fadeLen;      // 1 -> 0
                y = y * std::cos (t * 1.5707963267948966) + yo * std::sin (t * 1.5707963267948966);   // puissance constante
                --ch.fadeLeft;
            }
            x[i] = (float) (y * m0 * o0);
        }
        d1 = d0; c1 = c0; g1 = g0; o1 = o0; m1 = m0;
        glowAcc += ch.active.currentNow() / jmax (1e-6, ch.active.quiescentCurrent());
        swingAcc += ch.active.currentSwing() / jmax (1e-6, ch.active.quiescentCurrent());
    }
    sDepth = d1; sContour = c1; sDrive = g1; sOut = o1; sMake = m1;
    oversampler->processSamplesDown (block);
    for (int c = 0; c < procCh; ++c) peak = jmax (peak, buffer.getMagnitude (c, 0, n));
    if (same) buffer.copyFrom (1, 0, buffer, 0, 0, n);

    plateGlow = (float) (glowAcc / procCh);
    plateSwing = (float) (swingAcc / procCh);
    // echauffement des plaques : la dissipation au-dela du repos chauffe lentement (~4 s) et refroidit plus lentement (~8 s)
    {
        const double over = jmax (0.0, glowAcc / procCh - 1.0) * 1.6 + (swingAcc / procCh) * 0.9;
        const double target = jlimit (0.0, 1.0, (over - 0.35) / 0.9);
        const double dtb = (double) n / sr;
        const double tau = target > heat ? 4.0 : 8.0;
        heat += (target - heat) * (1.0 - std::exp (-dtb / tau));
        plateHeat = (float) heat;
    }
    // Power : fondu de 20 ms entre l'ampli et le signal direct (retarde de la latence : pas de filtrage en peigne)
    {
        const double tP = value (pPower) > 0.5f ? 1.0 : 0.0;
        const double aP = 1.0 - std::exp (-1.0 / (0.020 * sr));
        double p = sPower;
        if (ringOk && (tP < 1.0 || p < 1.0))
        {
            for (int c = 0; c < nCh; ++c)
            {
                float* y = buffer.getWritePointer (c);
                const float* r = dryRing.getReadPointer (c);
                p = sPower;
                for (int i = 0; i < n; ++i)
                {
                    p += aP * (tP - p);
                    const float dry = r[(dryPos + i - latency + ringN) % ringN];
                    y[i] = (float) (p * y[i] + (1.0 - p) * dry);
                }
            }
        }
        else p = tP;
        sPower = std::abs (p - tP) < 1.0e-4 ? tP : p;
        if (ringOk) dryPos = (dryPos + n) % ringN;
    }
    // VU-metres : valeur efficace de sortie lissee (~50 ms), lue par l'interface
    {
        const double a = 1.0 - std::exp (-(double) n / (0.050 * sr));
        for (int c = 0; c < 2; ++c)
        {
            const float* y = buffer.getReadPointer (jmin (c, nCh - 1));
            double ms = 0.0;
            for (int i = 0; i < n; ++i) ms += (double) y[i] * y[i];
            ms /= (double) n;
            vuMs[c] += a * (ms - vuMs[c]);
            vuLevel[c].store ((float) (sPower * vuMs[c]), std::memory_order_relaxed);
        }
    }
    outLevel = peak;
    sampleClock += n;
}

//==============================================================================
// Presets
//==============================================================================
int AnodeAudioProcessor::getCurrentProgram()
{
    const int c = currentPreset.load();
    return (c >= 0 && c < getNumFactoryPresets()) ? c : 0;
}

void AnodeAudioProcessor::applyFactoryPreset (int index)
{
    const auto& p = getFactoryPreset (index);
    setParamValue ("tube", (float) p.tube);
    setParamValue ("topology", (float) p.pushPull);
    setParamValue ("mode", (float) p.pentode);
    setParamValue ("depth", p.depth);
    setParamValue ("contour", p.contour);
    setParamValue ("volume", p.volume);
    setParamValue ("output", p.output);
    currentPreset = jlimit (0, getNumFactoryPresets() - 1, index);
    ++presetCounter;
}

File AnodeAudioProcessor::getUserPresetFile()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
             .getChildFile ("NINE HANDS DSP").getChildFile ("Anode").getChildFile ("UserPresets.xml");
}

bool AnodeAudioProcessor::saveUserPreset (int slot)
{
    if (slot < 1 || slot > 9) return false;
    auto file = getUserPresetFile();
    file.getParentDirectory().createDirectory();
    std::unique_ptr<XmlElement> root;
    if (file.existsAsFile()) root = XmlDocument::parse (file);
    if (root == nullptr || ! root->hasTagName ("UserPresets")) root = std::make_unique<XmlElement> ("UserPresets");
    for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
        if (e->getIntAttribute ("slot") == slot) { root->removeChildElement (e, true); break; }
    auto* e = root->createNewChildElement ("Preset");
    e->setAttribute ("slot", slot);
    for (int i = 0; i < numPresetParams; ++i) e->setAttribute (paramId (i), (double) value (i));
    const bool ok = root->writeTo (file);
    if (ok) { currentPreset = 100 + slot; ++presetCounter; }
    return ok;
}

bool AnodeAudioProcessor::loadUserPreset (int slot)
{
    const auto file = getUserPresetFile();
    if (! file.existsAsFile()) return false;
    auto root = XmlDocument::parse (file);
    if (root == nullptr) return false;
    for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
        if (e->getIntAttribute ("slot") == slot)
        {
            for (int i = 0; i < numPresetParams; ++i)
                if (e->hasAttribute (paramId (i))) setParamValue (paramId (i), (float) e->getDoubleAttribute (paramId (i)));
            currentPreset = 100 + slot; ++presetCounter;
            return true;
        }
    return false;
}

bool AnodeAudioProcessor::hasUserPreset (int slot) const
{
    const auto file = getUserPresetFile();
    if (! file.existsAsFile()) return false;
    if (auto root = XmlDocument::parse (file))
        for (auto* e = root->getFirstChildElement(); e != nullptr; e = e->getNextElement())
            if (e->getIntAttribute ("slot") == slot) return true;
    return false;
}

//==============================================================================
void AnodeAudioProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPreset", currentPreset.load(), nullptr);
    state.setProperty ("uiSize", uiSizeIdx.load(), nullptr);
    state.setProperty ("midiFsMode", midiFsMomentary.load() ? 1 : 0, nullptr);
    for (int i = 0; i < numLearn; ++i)
        state.setProperty (String ("cc_") + paramId (learnDef (i).param), ccMap[i].load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void AnodeAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = ValueTree::fromXml (*xml);
            currentPreset = (int) state.getProperty ("currentPreset", 0);
            uiSizeIdx = jlimit (-1, 2, (int) state.getProperty ("uiSize", -1));
            midiFsMomentary = (int) state.getProperty ("midiFsMode", 0) != 0;
            for (int i = 0; i < numLearn; ++i)
                ccMap[i].store (jlimit (-1, 127, (int) state.getProperty (String ("cc_") + paramId (learnDef (i).param), -1)));
            apvts.replaceState (state);
            ++presetCounter;
        }
}

//==============================================================================
AudioProcessorEditor* AnodeAudioProcessor::createEditor() { return new AnodeAudioProcessorEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AnodeAudioProcessor(); }
