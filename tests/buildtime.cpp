#include "../AnodeDSP.h"
#include <cstdio>
#include <chrono>
using namespace anode;
int main()
{
    auto T = [] { return std::chrono::steady_clock::now(); };
    auto a = T(); Tables::get(); printf ("tables puissance %.3f s\n", std::chrono::duration<double> (T() - a).count());
    for (int t = 0; t < 4; ++t) { auto c = makeConfig (t, true, true); a = T();
        TableCache::get().ltp (c.piType, c.piB, c.piRl1, c.piRl2, c.piRt, c.piRb, c.piRs);
        printf ("LTP %d : %.3f s\n", t, std::chrono::duration<double> (T() - a).count());
        a = T(); TableCache::get().triode (c.v1Type, c.v1B, c.v1Rl, c.v1Rk, c.v1Rs);
        printf ("V1 %d : %.3f s\n", t, std::chrono::duration<double> (T() - a).count()); }
    a = T(); PowerStage s; s.setup (1, true, true, 384000); printf ("setup (tables deja faites) %.3f s\n", std::chrono::duration<double> (T() - a).count());
    a = T(); s.settle (0.6); printf ("settle 0.6 %.3f s\n", std::chrono::duration<double> (T() - a).count());
    a = T(); s.saturatedRms(); printf ("satRms %.3f s\n", std::chrono::duration<double> (T() - a).count());
}
