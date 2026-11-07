#include "AnodeDSP_v1.h"
#include "bench.h"
using namespace anode_v1;
int main (int argc, char** argv)
{
    const int os = argc > 1 ? atoi (argv[1]) : 4;
    const double fs = 48000.0 * os;
    const char* tn[] = { "6L6", "EL34", "EL84", "KT88" };
    printf ("== ancien moteur, x%d ==\n", os);
    for (int pp = 1; pp >= 0; --pp) for (int t = 0; t < 4; ++t)
    {
        PowerStage st; st.setup (t, pp, true, fs); st.settle (0.5); st.setControls (0.5, 0.5);
        printf ("%s %-4s", pp ? "PP" : "SE", tn[t]);
        for (double drive : { 1.0, 4.0, 16.0 })
        {
            PowerStage s = st; const int N = 1 << 17; std::vector<double> y;
            for (int i = 0; i < N + 20000; ++i) { const double o = s.process (drive * 0.5 * sin (2 * M_PI * 3011.0 * i / fs)); if (i >= 20000) y.push_back (o); }
            printf ("  alias@%2.0fx %6.1f dB", drive, aliasDb (y, fs, 3011.0));
        }
        printf ("\n");
    }
}
