#include "../AnodeDSP.h"
#include <cstdio>
#include <vector>
#include <cmath>
using namespace anode;
static double gainAt (PowerStage s, double f, double amp, double drive, double fs)
{
    double num = 0, den = 0; const int N = (int) (fs * std::max (0.06, 6.0 / f)); const int skip = N / 2;
    for (int i = 0; i < N; ++i) { const double sn = sin (2 * M_PI * f * i / fs), cs = cos (2 * M_PI * f * i / fs);
        const double o = s.process (amp * sn, drive); if (i > skip) { num += o * sn; den += o * cs; } }
    return 2 * sqrt (num * num + den * den) / (N - skip - 1) / amp;
}
int main (int argc, char** argv)
{
    const double fs = 384000; const char* tn[] = { "6L6", "EL34", "EL84", "KT88" };
    const double amp = argc > 1 ? atof (argv[1]) : 0.01, drive = argc > 2 ? atof (argv[2]) : 1.0;
    const double fr[] = { 500, 1000, 2000, 3000, 5000, 8000 };
    printf ("ecart Contour 10 - Contour 0 (dB), amp %.3f drive %.1f\n            ", amp, drive);
    for (double f : fr) printf ("%6.0f", f); printf ("\n");
    for (int pp = 1; pp >= 0; --pp) for (int pe = 1; pe >= 0; --pe) for (int t = 0; t < 4; ++t)
    {
        PowerStage s; s.setup (t, pp, pe, fs); s.settle (0.3);
        printf ("%s %s %-5s", pp ? "PP" : "SE", pe ? "P" : "T", tn[t]);
        for (double f : fr) { PowerStage a = s, b = s; a.setControls (0.5, 0.0); b.setControls (0.5, 1.0);
            printf ("%6.1f", 20 * log10 (gainAt (b, f, amp, drive, fs) / gainAt (a, f, amp, drive, fs))); }
        printf ("\n");
    }
}
