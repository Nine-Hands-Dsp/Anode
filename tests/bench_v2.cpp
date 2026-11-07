#include "../AnodeDSP.h"
#include "bench.h"
using namespace anode;
int main (int argc, char** argv)
{
    const int os = argc > 1 ? atoi (argv[1]) : 8;
    const double fs = 48000.0 * os;
    const char* tn[] = { "6L6", "EL34", "EL84", "KT88" };
    auto t0 = std::chrono::steady_clock::now();
    PowerStage bank[2][4];
    for (int pp = 0; pp < 2; ++pp) for (int t = 0; t < 4; ++t) { bank[pp][t].setup (t, pp, true, fs); bank[pp][t].settle (0.3); }
    printf ("setup 8 configs : %.2f s\n", std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count());
    // tables
    for (int pp = 1; pp >= 0; --pp) for (int t = 0; t < 4; ++t)
    {
        auto& st = bank[pp][t]; st.setControls (0.5, 0.5);
        printf ("%s %-4s", pp ? "PP" : "SE", tn[t]);
        for (double drive : { 1.0, 4.0, 16.0 })
        {
            PowerStage s = st; const int N = 1 << 17; std::vector<double> y; bool bad = false;
            for (int i = 0; i < N + 30000; ++i) { const double o = s.process (0.5 * sin (2 * M_PI * 3011.0 * i / fs), drive); if (! std::isfinite (o)) bad = true; if (i >= 30000) y.push_back (o); }
            printf ("  alias@%2.0fx %6.1f dB%s", drive, aliasDb (y, fs, 3011.0), bad ? " NaN!" : "");
        }
        printf ("\n");
    }
    // harmoniques a 110 Hz selon le niveau d'entree (Volume 5) et selon le Volume
    printf ("\nHarmoniques (110 Hz) : niveau fondamental dB, puis H2..H6 en dB sous le fondamental\n");
    for (int pp = 1; pp >= 0; --pp) for (int t = 0; t < 4; ++t)
        for (auto [amp, drive] : { std::pair<double,double> { 0.1, 1.0 }, { 0.3, 1.0 }, { 0.3, 4.0 }, { 0.3, 16.0 } })
        {
            PowerStage s = bank[pp][t]; std::vector<double> y; const int N = (int) (fs * 0.2);
            for (int i = 0; i < N + (int) (fs * 0.3); ++i) { const double o = s.process (amp * sin (2 * M_PI * 110.0 * i / fs), drive); if (i >= (int) (fs * 0.3)) y.push_back (o); }
            auto h = harmonicsDb (y, fs, 110.0, 6);
            printf ("%s %-4s in %.1f vol x%-4.0f : %6.1f | %6.1f %6.1f %6.1f %6.1f %6.1f\n", pp ? "PP" : "SE", tn[t], amp, drive, h[0], h[1], h[2], h[3], h[4], h[5]);
        }
    // CPU
    {
        PowerStage s = bank[1][1]; double acc = 0; const int N = (int) fs * 2;
        auto a = std::chrono::steady_clock::now();
        for (int i = 0; i < N; ++i) acc += s.process (0.3 * sin (2 * M_PI * 110.0 * i / fs) + 0.1 * sin (2 * M_PI * 331.0 * i / fs), 6.0);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - a).count();
        printf ("\nCPU PP EL34 sature : %.0f ns / echantillon interne, %.1f %% d'un coeur a 48 kHz (x%d) %g\n", sec * 1e9 / N, 100.0 * sec / 2.0, os, acc * 0);
    }
}
