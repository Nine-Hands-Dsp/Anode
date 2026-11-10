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

    // ---------------- animations (images pour GIF) : AnodeRender <dossier> gif
    if (argc > 2 && String (argv[2]) == "gif")
    {
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        auto* aed = static_cast<AnodeAudioProcessorEditor*> (ed.get());
        // accords plaques (Karplus-Strong, drop G), rejoues toutes les « period » secondes
        auto strums = [&] (double seconds, double period, float amp, double stopAt)
        {
            AudioBuffer<float> b (2, (int) (seconds * fs)); b.clear();
            Random rnd (7);
            const double f0s[] = { 49.0, 73.4, 98.0, 146.8 };
            for (double t0 = 0.0; t0 < stopAt; t0 += period)
                for (int k = 0; k < 4; ++k)
                {
                    const int N = (int) (fs / f0s[k]);
                    std::vector<float> d ((size_t) N);
                    for (auto& v : d) v = rnd.nextFloat() * 2.0f - 1.0f;
                    int idx = 0;
                    const int start = (int) ((t0 + 0.012 * k) * fs), len = (int) (period * fs);
                    for (int i = start; i < jmin (b.getNumSamples(), start + len); ++i)
                    {
                        const int nx = (idx + 1) % N;
                        const float y = 0.4975f * (d[(size_t) idx] + d[(size_t) nx]);
                        d[(size_t) idx] = y; idx = nx;
                        b.addSample (0, i, amp * y);
                    }
                }
            b.copyFrom (1, 0, b, 0, 0, b.getNumSamples());
            return b;
        };
        auto run = [&] (const String& tag, AudioBuffer<float> sig, double fps, int height, Rectangle<float> unitsArea)
        {
            aed->setScaledHeight (height); pump (8);
            const float u = (float) height / 900.0f;
            const auto area = (unitsArea * u).getSmallestIntegerContainer();
            const int per = (int) (fs / fps);
            MidiBuffer m; int frame = 0;
            for (int pos = 0; pos + per <= sig.getNumSamples(); pos += per, ++frame)
            {
                for (int q = pos; q < pos + per; q += block)
                {
                    const int n = jmin (block, pos + per - q);
                    AudioBuffer<float> sub (sig.getArrayOfWritePointers(), 2, q, n);
                    proc.processBlock (sub, m);
                }
                pump ((int) std::round (30.0 / fps));               // le timer de l'interface avance au meme rythme
                if (frame % 8 == 0) std::printf ("  t %.2f s  glow %.3f  swing %.3f  heat %.3f  vu %.4f\n", pos / fs,
                                                  proc.plateGlow.load(), proc.plateSwing.load(), proc.plateHeat.load(), proc.vuLevel[0].load());
                auto img = ed->createComponentSnapshot (area, true, 1.0f);
                File f (out + "/" + tag + String (frame).paddedLeft ('0', 4) + ".png"); f.deleteFile();
                FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
            }
            std::printf ("%s : %d images\n", tag.toRawUTF8(), frame);
        };
        proc.applyFactoryPreset (0); proc.setParamValue ("volume", 6.0f); pump (4);
        // VU : jeu normal, accords toutes les 1,4 s puis silence (retour en butee)
        run ("gif_vu_", strums (7.0, 1.4, 0.10f, 5.0), 30.0, 958, { 228.0f, 568.0f, 444.0f, 228.0f });
        // lampes : jeu tres pousse (Volume 10) 9 s, puis silence ; les plaques chauffent puis refroidissent
        proc.setParamValue ("volume", 10.0f); pump (4);
        run ("gif_tubes_", strums (14.0, 1.6, 0.35f, 9.0), 12.0, 719, { 176.0f, 57.0f, 1014.0f, 480.0f });
        return 0;
    }

    // ---------------- captures de la fenetre
    std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
    auto* aed = static_cast<AnodeAudioProcessorEditor*> (ed.get());
    std::printf ("taille par defaut : %d x %d\n", ed->getWidth(), ed->getHeight());
    auto feed = [&] (float amp, double seconds, float freq = 0.0144f)
    {
        AudioBuffer<float> b (2, block); MidiBuffer m;
        const int nb = jmax (1, (int) (seconds * fs / block));
        for (int k = 0; k < nb; ++k)
        {
            for (int s2 = 0; s2 < block; ++s2) b.setSample (0, s2, amp * std::sin ((float) (k * block + s2) * freq));
            b.copyFrom (1, 0, b, 0, 0, block); proc.processBlock (b, m);
            if (k % 16 == 0) pump (1);
        }
    };
    auto shot = [&] (const String& name, int preset, int height, std::function<void()> tweak)
    {
        aed->setScaledHeight (height);
        if (preset >= 0) proc.applyFactoryPreset (preset);
        pump (8);
        if (tweak) tweak();
        pump (12);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1800.0f / (float) height);
        File f (out + "/" + name + ".png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    };
    const int H = 1350;                       // taille maximale : captures nettes (fond et vectoriel a cette resolution)
    shot ("anode_idle", 0, H, [&] { feed (0.0f, 1.5); });                                   // au repos : aiguilles en butee
    shot ("anode_play", 0, H, [&] { feed (0.035f, 1.5); });                                  // jeu normal
    shot ("anode_status", 0, 958, [&] { proc.setParamValue ("volume", 7.6f); feed (0.12f, 0.8); });
    shot ("anode_hot", 6, H, [&]                                                            // 8 s de jeu tres pousse
    {
        proc.setParamValue ("volume", 10.0f);
        feed (0.5f, 8.0);
    });
    shot ("anode_off", 0, H, [&] { proc.setParamValue ("power", 0.0f); feed (0.12f, 4.0); });   // Power coupe : lampes refroidies
    shot ("anode_warmup", 0, H, [&] { proc.setParamValue ("power", 1.0f); feed (0.0f, 0.5); }); // rallumage : filaments qui chauffent
    // bypass : le signal ressort tel quel (au retard de latence pres)
    {
        proc.setParamValue ("power", 0.0f); pump (2);
        AudioBuffer<float> b (2, block); MidiBuffer m; double err = 0.0, ref = 0.0;
        std::vector<float> hist;
        for (int k = 0; k < 200; ++k)
        {
            for (int s2 = 0; s2 < block; ++s2) { const float v = 0.2f * std::sin ((float) (k * block + s2) * 0.031f); b.setSample (0, s2, v); hist.push_back (v); }
            b.copyFrom (1, 0, b, 0, 0, block); proc.processBlock (b, m);
            if (k > 20)
                for (int s2 = 0; s2 < block; ++s2)
                {
                    const int idx = k * block + s2 - proc.getLatencySamples();
                    const double d = b.getSample (0, s2) - hist[(size_t) idx];
                    err += d * d; ref += (double) hist[(size_t) idx] * hist[(size_t) idx];
                }
        }
        std::printf ("bypass : ecart %.1f dB sous le signal\n", 10.0 * std::log10 (err / ref + 1e-30));
        proc.setParamValue ("power", 1.0f);
    }
    return 0;
}
