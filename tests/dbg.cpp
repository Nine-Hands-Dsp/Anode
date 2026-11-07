#define private public
#include "../AnodeDSP.h"
#include <cstdio>
using namespace anode;
int main (int argc, char** argv)
{
    const int t = atoi (argv[1]); const double drive = atof (argv[2]);
    const double fs = 384000;
    PowerStage s; s.setup (t, true, true, fs); s.settle (0.3); s.setControls (0.5, 0.5);
    printf ("inScale %.2f piGain %.2f vBias %.1f nfb %.2f\n", s.cfg.inScale, s.pi->gain, s.cfg.vBias, s.cfg.nfb);
    // LTP transfer
    for (double u : { -40., -20., -10., -5., -2., 0., 2., 5., 10., 20., 40. }) printf ("u %6.1f -> P %7.1f N %7.1f\n", u, s.pi->outP.eval (u), s.pi->outN.eval (u));
    for (int i = 0; i < (int) (fs * 0.4); ++i)
    {
        const double o = s.process (0.3 * sin (2 * M_PI * 110.0 * i / fs), drive);
        if (i > fs * 0.35 && i % 160 == 0)
            printf ("%5d out %7.3f fb %7.3f vc %7.1f %7.1f  drv %7.1f %7.1f  B %6.1f scr %6.1f d %7.4f\n", i, o, s.fbPrev, s.vc[0], s.vc[1], s.drvLP[0], s.drvLP[1], s.bSag, s.vScr, s.d);
    }
}
