#include "../AnodeDSP.h"
#include <cstdio>
#include <chrono>
#include <vector>
using namespace anode;
int main (int argc, char** argv)
{
    const double fs = 384000; const int t = atoi (argv[1]); const bool pp = atoi (argv[2]); const double drive = atof (argv[3]);
    PowerStage s; s.setup (t, pp, true, fs); s.settle (0.2); s.setControls (0.5, 0.5);
    double acc = 0; const int N = argc > 4 ? atoi (argv[4]) : (int) fs * 3;
    std::vector<double> in (N); for (int i = 0; i < N; ++i) in[i] = 0.3 * sin (2 * M_PI * 110.0 * i / fs) + 0.1 * sin (2 * M_PI * 331.0 * i / fs);
    auto a = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) acc += s.process (in[i], drive);
    const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - a).count();
    printf ("t%d pp%d drive %.0f : %.0f ns/ech, %.1f %% coeur @48k x8  %g\n", t, pp, drive, sec * 1e9 / N, 100 * sec / 3.0, acc * 0);
}
