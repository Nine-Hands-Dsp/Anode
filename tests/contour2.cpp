#include "../AnodeDSP.h"
#include <cstdio>
#include <cmath>
using namespace anode;
static double gainAt (PowerStage s, double f, double amp, double fs)
{
    double num = 0, den = 0; const int N = (int) (fs * std::max (0.06, 6.0 / f)); const int skip = N / 2;
    for (int i = 0; i < N; ++i) { const double sn = sin (2 * M_PI * f * i / fs), cs = cos (2 * M_PI * f * i / fs);
        const double o = s.process (amp * sn, 1.0); if (i > skip) { num += o * sn; den += o * cs; } }
    return 2 * sqrt (num * num + den * den) / (N - skip - 1) / amp;
}
// accord (La, Mi, La) sature : energie dans une bande
static double band (PowerStage s, double amp, double drive, double f0, double q, double fs)
{
    // f0 > 1000 : energie au-dessus de 1,5 kHz (passe-haut d'ordre 8) ; sinon 50-160 Hz (passe-bas d'ordre 8 a 160 + passe-haut 50)
    Biquad f[5];
    auto set = [&] (Biquad& b, bool hp, double fc, double qq) { const double w = 2*M_PI*fc/fs, c = cos (w), al = sin (w)/(2*qq);
        if (hp) b.set ((1+c)/2, -(1+c), (1+c)/2, 1+al, -2*c, 1-al); else b.set ((1-c)/2, 1-c, (1-c)/2, 1+al, -2*c, 1-al); };
    const double qs[4] = { 0.51, 0.60, 0.90, 2.56 };
    for (int k = 0; k < 4; ++k) set (f[k], f0 > 1000, f0 > 1000 ? 1500.0 : 160.0, qs[k]);
    set (f[4], f0 <= 1000, f0 > 1000 ? 6000.0 : 50.0, 0.707);
    const int N = (int) (0.25 * fs); double acc = 0;
    for (int i = 0; i < N; ++i) { const double t = i / fs;
        const double x = amp * (sin (2*M_PI*110*t) + sin (2*M_PI*164.8*t) + 0.7 * sin (2*M_PI*220*t)) / 2.7;
        double y = s.process (x, drive); for (auto& b : f) y = b.process (y); if (i > N / 3) acc += y * y; }
    return 10 * log10 (acc + 1e-30);
}
int main()
{
    const double fs = 384000; const char* tn[] = { "6L6", "EL34", "EL84", "KT88" };
    printf ("Contour 0 / 10 par rapport a 5 (dB)    clair 1k   clair 3k   clair 5k | crunch 2-4k | sature 2-4k\n");
    for (int pp = 1; pp >= 0; --pp) for (int pe = 1; pe >= 0; --pe) for (int t = 0; t < 4; ++t)
    {
        PowerStage s; s.setup (t, pp, pe, fs); s.settle (0.3);
        printf ("%s %s %-5s", pp ? "PP" : "SE", pe ? "P" : "T", tn[t]);
        for (double f : { 1000.0, 3000.0, 5000.0 }) {
            PowerStage a = s, b = s, c = s; a.setControls (0.5, 0.0); b.setControls (0.5, 0.5); c.setControls (0.5, 1.0);
            const double g0 = gainAt (a, f, 0.01, fs), g5 = gainAt (b, f, 0.01, fs), g1 = gainAt (c, f, 0.01, fs);
            printf ("  %+5.1f/%+5.1f", 20*log10 (g0/g5), 20*log10 (g1/g5)); }
        for (double dr : { 2.5, 8.0 }) {
            PowerStage a = s, b = s, c = s; a.setControls (0.5, 0.0); b.setControls (0.5, 0.5); c.setControls (0.5, 1.0);
            const double e5 = band (b, 0.3, dr, 2800, 0.7, fs);
            printf ("  %+5.1f/%+5.1f", band (a, 0.3, dr, 2800, 0.7, fs) - e5, band (c, 0.3, dr, 2800, 0.7, fs) - e5); }
        printf ("\n");
    }
    printf ("\nDepth 0 / 10 par rapport a 5 (dB), bande 60-150 Hz : clair | sature\n");
    for (int pp = 1; pp >= 0; --pp) for (int t = 0; t < 4; ++t)
    {
        PowerStage s; s.setup (t, pp, true, fs); s.settle (0.3);
        printf ("%s P %-5s", pp ? "PP" : "SE", tn[t]);
        for (double dr : { 0.3, 8.0 }) {
            PowerStage a = s, b = s, c = s; a.setControls (0.0, 0.5); b.setControls (0.5, 0.5); c.setControls (1.0, 0.5);
            const double e5 = band (b, 0.3, dr, 100, 0.9, fs);
            printf ("  %+5.1f/%+5.1f", band (a, 0.3, dr, 100, 0.9, fs) - e5, band (c, 0.3, dr, 100, 0.9, fs) - e5); }
        printf ("\n");
    }
}
