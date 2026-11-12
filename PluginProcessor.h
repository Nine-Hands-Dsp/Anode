#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>
#include "AnodeDSP.h"

//==============================================================================
// ANODE (NINE HANDS | DSP) - etage de puissance a lampes utilise en preampli
// guitare / basse : 6L6, EL34, EL84, KT88, Single-Ended / Push-Pull, Triode / Pentode,
// Depth, Contour, Volume. Voir AnodeDSP.h et README.md.
//==============================================================================

struct AnodePreset
{
    const char* name;
    int   tube, pushPull, pentode;      // 0..3 (6L6, EL34, EL84, KT88), 0 = SE / 1 = PP, 0 = triode / 1 = pentode
    float depth, contour, volume;       // 0..10
    float output;                       // dB
};

class AnodeAudioProcessor : public juce::AudioProcessor,
                            private juce::AsyncUpdater,
                            private juce::Timer
{
public:
    AnodeAudioProcessor();
    ~AnodeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Anode"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // presets d'usine (menu du DAW, menu de la fenetre, Program Change)
    static int getNumFactoryPresets();
    static const AnodePreset& getFactoryPreset (int index);
    int getNumPrograms() override { return getNumFactoryPresets(); }
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override { applyFactoryPreset (index); }
    const juce::String getProgramName (int index) override { return getFactoryPreset (index).name; }
    void changeProgramName (int, const juce::String&) override {}
    void applyFactoryPreset (int index);

    bool saveUserPreset (int slot);
    bool loadUserPreset (int slot);
    bool hasUserPreset (int slot) const;
    static juce::File getUserPresetFile();

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParamValue (const juce::String& id, float plain);
    static inline bool useLevelComp = true;          // false : mesure hors ligne de la table d'alignement

    // anti-gel v2 : pointeurs obtenus une fois, jamais de recherche par chaine dans processBlock
    enum ParamIndex { pTube, pTopo, pMode, pDepth, pContour, pVolume, pOutput, pPower, numParams };
    static constexpr int numPresetParams = pPower;      // Power ne fait pas partie des presets
    static const char* paramId (int i);
    float value (int i) const { return rawPtr[i]->load (std::memory_order_relaxed); }

    static juce::StringArray tubeNames()  { return { "6L6", "EL34", "EL84", "KT88" }; }

    // ---- informations pour la fenetre ----
    std::atomic<bool> midiFsMomentary { false };   // footswitch MIDI (Power) : false = latching (127 on / 0 off), true = bascule a chaque appui
    int lastPowerCC = 0;
    std::atomic<int> uiSizeIdx { -1 };   // taille de fenetre choisie au menu pour ce projet (-1 = taille par defaut)
    std::atomic<int>   currentPreset { 0 };      // >= 0 usine, 100 + n utilisateur, -1 = modifie
    std::atomic<int>   presetCounter { 0 };
    std::atomic<int>   midiCounter { 0 };
    std::atomic<float> plateGlow { 1.0f };       // courant des lampes / courant de repos (lueur)
    std::atomic<float> plateSwing { 0.0f };      // courant alternatif / courant de repos (lueur en classe A)
    std::atomic<float> plateHeat { 0.0f };       // echauffement des plaques 0..1 (rougeoiement apres une longue saturation)
    std::atomic<float> outLevel { 0.0f };
    std::atomic<float> vuLevel[2] { { 0.0f }, { 0.0f } };   // valeur efficace de sortie (lissee ~50 ms), gauche / droite

    // MIDI Learn (aucun CC assigne par defaut)
    enum LearnType { learnContinuous, learnToggle, learnSelector };
    struct LearnDef { int param; const char* name; int type; int positions; };
    static constexpr int numLearn = 7;
    static const LearnDef& learnDef (int i);
    std::atomic<int> ccMap[numLearn];
    std::atomic<int> learnTarget { -1 };
    std::atomic<int> lastLearned { -1 };
    void clearMidiMap();

private:
    std::atomic<float>* rawPtr[numParams] {};
    void handleMidi (juce::MidiBuffer& midi);
    void queueParam (int param, float plain);     // thread audio -> file -> thread de messages
    void timerCallback() override;
    void handleAsyncUpdate() override;
    std::atomic<float> pendingValue[numParams];
    std::atomic<bool>  pendingSet[numParams];
    std::atomic<int>   pendingPreset { -1 };

    // pedale d'expression : rampe sur l'intervalle mesure entre deux CC + course apprise
    struct ExprRamp
    {
        bool active = false;
        float value = 0.0f, target = 0.0f, step = 0.0f;
        int left = 0;
        juce::int64 lastTime = 0;
        int msb = -1;
        float lo = 1.0f, hi = 0.0f;
    } ramps[3];
    juce::int64 sampleClock = 0;
    float mapCC (int k, float raw01);

    // ---- moteur ----
    static int configIndex (int tube, bool pp, bool pent) { return tube * 4 + (pp ? 2 : 0) + (pent ? 1 : 0); }
    std::vector<anode::PowerStage> bank;          // 16 configurations calibrees (au repos)
    struct Channel
    {
        anode::PowerStage active, fading;
        int fadeLeft = 0;
    };
    Channel chans[2];
    int currentConfig = -1;
    int fadeLen = 1;
    double sr = 48000.0, osRate = 192000.0, bankRate = 0.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int osFactorLog2 = 2;
    // lissage des commandes (par echantillon au taux sur-echantillonne)
    double sDepth = 0.5, sContour = 0.5, sDrive = 1.0, sOut = 1.0, sMake = 1.0, smoothA = 0.001;
    bool prevSame = true;
    // Power : bypass en fondu (20 ms), signal direct retarde de la latence du sur-echantillonnage
    juce::AudioBuffer<float> dryRing;
    int dryPos = 0, latency = 0;
    double sPower = 1.0, vuMs[2] { 0.0, 0.0 };
    double heat = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnodeAudioProcessor)
};
