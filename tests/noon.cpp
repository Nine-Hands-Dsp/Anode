#include HDR
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
int main()
{
    const double fs = 384000;
    for (int k = 0; k < 16; ++k) { PowerStage s; s.setup (k / 4, (k >> 1) & 1, k & 1, fs); s.settle (0.3); s.setControls (0.5, 0.5);
        for (double f : { 80.0, 250.0, 1000.0, 2000.0, 3000.0, 5000.0 }) printf ("%7.2f", 20*log10 (gainAt (s, f, 0.01, fs))); printf ("\n"); }
}
