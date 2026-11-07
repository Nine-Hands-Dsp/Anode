#include "../AnodeDSP.h"
#include <cstdio>
using namespace anode;
// verification brute (bisections emboitees, sans table) d'un point du dephaseur
int main()
{
    for (int tube : { 0, 1 })
    {
        auto c = makeConfig (tube, true, true);
        const auto& t = triodeType (c.piType);
        const double B = c.piB, Rl1 = c.piRl1, Rl2 = c.piRl2, Rt = c.piRt, Rb = c.piRb, Rs = c.piRs, Rtot = Rt + Rb;
        auto plate = [&] (double vgk, double Rl) { double a = 0, b = B; for (int k = 0; k < 90; ++k) { const double m = 0.5 * (a + b); (B - Rl * korenTriode (t, vgk, m) > m ? a : b) = m; } return (B - 0.5 * (a + b)) / Rl; };
        auto gridV = [&] (double w) { double lo = -300, hi = w; for (int k = 0; k < 90; ++k) { const double m = 0.5 * (lo + hi); (m + Rs * gridCurrent (t.gk, m) > w ? hi : lo) = m; } return 0.5 * (lo + hi); };
        double lo = 0, hi = B;
        for (int k = 0; k < 90; ++k) { const double vk = 0.5 * (lo + hi); const double vj = vk * Rt / Rtot; ((vk - Rtot * (plate (vj - vk, Rl1) + plate (vj - vk, Rl2))) > 0 ? hi : lo) = vk; }
        const double vkq = 0.5 * (lo + hi), Vj = vkq * Rt / Rtot;
        const double vp1q = B - Rl1 * plate (Vj - vkq, Rl1), vp2q = B - Rl2 * plate (Vj - vkq, Rl2);
        const auto* tab = TableCache::get().ltp (c.piType, c.piB, c.piRl1, c.piRl2, c.piRt, c.piRb, c.piRs);
        for (double u : { -10.0, -3.0, 1.0, 5.0, 8.0, 10.0, 20.0, 40.0 })
        {
            double a = -50, b = B, i1 = 0, i2 = 0;
            for (int k = 0; k < 90; ++k) { const double vk = 0.5 * (a + b); const double g = gridV (Vj + u - vk); i1 = plate (g, Rl1); i2 = plate (Vj - vk, Rl2);
                ((vk - Rtot * (i1 + i2 + gridCurrent (t.gk, g))) > 0 ? b : a) = vk; }
            printf ("tube %d u %6.1f  exact P %8.2f N %8.2f   table P %8.2f N %8.2f\n", tube, u, (B - Rl2 * i2) - vp2q, (B - Rl1 * i1) - vp1q, tab->outP.eval (u), tab->outN.eval (u));
        }
    }
}
