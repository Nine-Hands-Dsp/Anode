// Captures PNG de la fenetre reelle d'ANODE (meme code que le plugin) + tests audio hors ligne
//   AnodeRender <dossier de sortie> [fichier.wav d'entree]
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <chrono>
#include <juce_audio_formats/juce_audio_formats.h>
using namespace juce;

static void pump (int n) { for (int i = 0; i < n; ++i) MessageManager::getInstance()->runDispatchLoopUntil (35); }

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const String out = argc > 1 ? argv[1] : ".";
    const double fs = 48000.0;
    const int block = 256;
    AnodeAudioProcessor proc;
    auto t0 = std::chrono::steady_clock::now();
    proc.prepareToPlay (fs, block);
    std::printf ("prepareToPlay : %.0f ms, latence %d echantillons\n",
                 std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count(), proc.getLatencySamples());

    // ---------------- audio : fichier d'entree (guitare DI) ou accord synthetique
    AudioBuffer<float> input;
    if (argc > 2)
    {
        AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<AudioFormatReader> r (fm.createReaderFor (File (argv[2])));
        if (r != nullptr)
        {
            input.setSize (2, (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (fs * 12)));
            r->read (&input, 0, input.getNumSamples(), 0, true, true);
        }
    }
    if (input.getNumSamples() == 0)
    {
        // corde pincee synthetique (Karplus-Strong) : accord de puissance en drop G sur 7 cordes
        input.setSize (2, (int) (fs * 6));
        input.clear();
        Random rnd (3);
        const double f0s[] = { 49.0, 73.4, 98.0 };
        for (int k = 0; k < 3; ++k)
        {
            const int N = (int) (fs / f0s[k]);
            std::vector<float> d ((size_t) N);
            for (auto& v : d) v = rnd.nextFloat() * 2.0f - 1.0f;
            int idx = 0;
            for (int i = 0; i < input.getNumSamples(); ++i)
            {
                const int nx = (idx + 1) % N;
                const float y = 0.4985f * (d[(size_t) idx] + d[(size_t) nx]);
                d[(size_t) idx] = y; idx = nx;
                const float env = i < (int) (fs * 2.5) ? 1.0f : 0.0f;
                input.addSample (0, i, 0.18f * y * env);
            }
        }
        input.copyFrom (1, 0, input, 0, 0, input.getNumSamples());
    }

    auto render = [&] (int preset, const String& name)
    {
        proc.applyFactoryPreset (preset);
        pump (2);
        AudioBuffer<float> buf (input);
        MidiBuffer m;
        double secs = 0.0; float peak = 0.0f; bool bad = false;
        for (int pos = 0; pos < buf.getNumSamples(); pos += block)
        {
            const int n = jmin (block, buf.getNumSamples() - pos);
            AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, pos, n);
            const auto a = std::chrono::steady_clock::now();
            proc.processBlock (sub, m);
            secs += std::chrono::duration<double> (std::chrono::steady_clock::now() - a).count();
        }
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) bad = true;
                peak = jmax (peak, std::abs (v));
            }
        const double rms = buf.getRMSLevel (0, 0, buf.getNumSamples());
        std::printf ("%-28s CPU %.2f %% (temps reel)  crete %.2f  RMS %.1f dBFS %s\n", name.toRawUTF8(),
                     100.0 * secs / (buf.getNumSamples() / fs), peak, Decibels::gainToDecibels (rms), bad ? "NaN !" : "");
        File f (out + "/audio_" + name.replaceCharacter (' ', '_') + ".wav"); f.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), fs, 2, 24, {}, 0));
        if (w) w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    };
    {
        File f (out + "/audio_input.wav"); f.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), fs, 2, 24, {}, 0));
        if (w) w->writeFromAudioSampleBuffer (input, 0, input.getNumSamples());
    }
    for (int p = 0; p < AnodeAudioProcessor::getNumFactoryPresets(); ++p)
        render (p, String (p + 1).paddedLeft ('0', 2) + "_" + AnodeAudioProcessor::getFactoryPreset (p).name);

    // changement de lampe en cours de jeu (fondu enchaine) : recherche de clic
    {
        proc.applyFactoryPreset (0); pump (2);
        AudioBuffer<float> buf (input); MidiBuffer m;
        float maxJump = 0.0f; float prev = 0.0f;
        for (int pos = 0, b = 0; pos < buf.getNumSamples(); pos += block, ++b)
        {
            if (b % 40 == 20) { proc.apvts.getParameter ("tube")->setValueNotifyingHost ((float) ((b / 40) % 4) / 3.0f); }
            if (b % 60 == 30) { auto* t = proc.apvts.getParameter ("topology"); t->setValueNotifyingHost (t->getValue() > 0.5f ? 0.0f : 1.0f); }
            const int n = jmin (block, buf.getNumSamples() - pos);
            AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, pos, n);
            proc.processBlock (sub, m);
            for (int i = 0; i < n; ++i) { const float v = sub.getSample (0, i); maxJump = jmax (maxJump, std::abs (v - prev)); prev = v; }
        }
        std::printf ("commutations en jeu : plus grand saut entre echantillons %.3f\n", maxJump);
        File f (out + "/audio_switching.wav"); f.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), fs, 2, 24, {}, 0));
        if (w) w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    }

    // ---------------- captures de la fenetre
    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    auto* aed = static_cast<AnodeAudioProcessorEditor*> (ed.get());
    std::printf ("taille par defaut : %d x %d\n", ed->getWidth(), ed->getHeight());
    auto shot = [&] (const String& name, int preset, int height, std::function<void()> tweak)
    {
        aed->setScaledHeight (height);
        proc.applyFactoryPreset (preset);
        pump (8);
        if (tweak) tweak();
        AudioBuffer<float> b (2, block); MidiBuffer m;
        for (int i = 0; i < 40; ++i) { b.clear(); for (int s = 0; s < block; ++s) b.setSample (0, s, 0.3f * std::sin (s * 0.05f)); b.copyFrom (1, 0, b, 0, 0, block); proc.processBlock (b, m); }
        pump (25);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, (float) AnodeAudioProcessorEditor::baseH / (float) height);
        File f (out + "/" + name + ".png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    };
    shot ("anode_default", 0, AnodeAudioProcessorEditor::defaultH, nullptr);
    shot ("anode_status", 0, AnodeAudioProcessorEditor::baseH, [&] { proc.setParamValue ("volume", 7.6f); });
    shot ("anode_6l6_se", 8, AnodeAudioProcessorEditor::baseH, nullptr);
    shot ("anode_kt88_triode", 7, AnodeAudioProcessorEditor::baseH, nullptr);
    shot ("anode_el84", 4, AnodeAudioProcessorEditor::baseH, nullptr);
    shot ("anode_extremes", 0, AnodeAudioProcessorEditor::baseH, [&] { proc.setParamValue ("depth", 0.0f); proc.setParamValue ("contour", 10.0f); proc.setParamValue ("volume", 2.5f); });
    return 0;
}
