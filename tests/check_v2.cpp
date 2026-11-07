#include "../AnodeDSP.h"
#include <cstdio>
#include <vector>
#include <cmath>
using namespace anode;
static double gainAt (PowerStage s, double f, double amp, double drive, double fs)
{
    double num = 0, den = 0; const int N = (int) (fs * std::max (0.08, 6.0 / f)); const int skip = N / 2;
    for (int i = 0; i < N; ++i) { const double sn = sin (2 * M_PI * f * i / fs), cs = cos (2 * M_PI * f * i / fs);
        const double o = s.process (amp * sn, drive); if (i > skip) { num += o * sn; den += o * cs; } }
    return 2 * sqrt (num * num + den * den) / (N - skip - 1) / amp;
}
int main()
{
    const double fs = 384000; const char* tn[] = { "6L6", "EL34", "EL84", "KT88" };
    std::vector<PowerStage> bank (16);
    for (int t = 0; t < 4; ++t) for (int pp = 0; pp < 2; ++pp) for (int pe = 0; pe < 2; ++pe) { auto& s = bank[t * 4 + pp * 2 + pe]; s.setup (t, pp, pe, fs); s.settle (0.3); }
    const double fr[] = { 40, 60, 80, 120, 250, 500, 1000, 2000, 4000, 8000, 12000 };
    printf ("Reponse petit signal (dB, Volume 5), depth/contour 0.5 :\n         ");
    for (double f : fr) printf ("%6.0f", f); printf ("\n");
    for (int pp = 1; pp >= 0; --pp) for (int pe = 1; pe >= 0; --pe) for (int t = 0; t < 4; ++t)
    {
        PowerStage s = bank[t * 4 + pp * 2 + pe]; s.setControls (0.5, 0.5);
        printf ("%s %s %-4s", pp ? "PP" : "SE", pe ? "P" : "T", tn[t]);
        for (double f : fr) printf ("%6.1f", 20 * log10 (gainAt (s, f, 0.01, 1.0, fs)));
        printf ("\n");
    }
    printf ("\nDepth / Contour 0 -> 1 (PP pentode EL34) :\n");
    for (double dc : { 0.0, 0.5, 1.0 }) { PowerStage s = bank[1 * 4 + 3]; s.setControls (dc, dc); printf ("d=c=%.1f", dc);
        for (double f : fr) printf ("%6.1f", 20 * log10 (gainAt (s, f, 0.01, 1.0, fs))); printf ("\n"); }
    printf ("\nCourbe entree -> sortie (1 kHz fondamental, dB) a Volume 5 / 7 / 10 :\n");
    for (int t = 0; t < 4; ++t) for (int pp = 1; pp >= 0; --pp)
    {
        printf ("%s %-4s", pp ? "PP" : "SE", tn[t]);
        for (double vol : { 5.0, 7.0, 10.0 })
        {
            const double drive = pow (10.0, (-24.0 + 4.8 * vol) / 20.0);
            printf (" |V%2.0f", vol);
            for (double inDb : { -30.0, -20.0, -12.0, -6.0, 0.0 })
            {
                PowerStage s = bank[t * 4 + pp * 2 + 1]; s.setControls (0.5, 0.5);
                const double a = pow (10.0, inDb / 20.0);
                printf (" %5.1f", 20 * log10 (gainAt (s, 1000.0, a, drive, fs) * a));
            }
        }
        printf ("\n");
    }
    // stabilite : extremes
    int bad = 0;
    for (auto& b : bank) for (double dc : { 0.0, 1.0 }) for (double drive : { 0.06, 1.0, 16.0 }) for (int same = 0; same < 2; ++same)
    {
        PowerStage s = b; s.setControls (dc, same ? dc : 1.0 - dc);
        for (int i = 0; i < (int) (fs * 0.3); ++i)
        {
            const double x = ((i / 3000) % 2 ? 1.5 : 0.0) * sin (2 * M_PI * 49 * i / fs) + ((i % 977) == 0 ? 3.0 : 0.0);
            const double o = s.process (x, drive);
            if (! std::isfinite (o) || std::fabs (o) > 60) { ++bad; break; }
        }
        // retour au silence : pas d'oscillation
        double tail = 0; for (int i = 0; i < (int) (fs * 0.5); ++i) { const double o = s.process (0.0, drive); if (i > fs * 0.4) tail = std::max (tail, std::fabs (o)); }
        if (tail > 1e-3) { ++bad; printf ("residu %.4f\n", tail); }
    }
    printf ("\nStabilite : %d probleme(s) sur %d essais\n", bad, 16 * 12);
}
