#pragma once
// =====================================================================================
// ANODE (NINE HANDS | DSP) - moteur d'ampli a lampes (v1.1), utilise en preampli guitare / basse
//
// Chaine modelisee, entierement au taux sur-echantillonne (>= 352,8 kHz) :
//   entree guitare -> V1 : triode 12AX7 en cathode commune (resistance de grille 68 k, conduction de grille,
//        condensateur de cathode dynamique : la polarisation glisse quand on attaque, « bloom » et compression
//        douce ; condensateur de cathode « bright » 0,68 uF sur le voicing EL34 = grave serre facon JCM800)
//   -> liaison (condensateur + resistance de fuite) -> Volume (master)
//   -> etage d'attaque
//        PP : dephaseur a paire differentielle (12AX7 ou 12AT7 selon l'ampli) resolu exactement
//             (3 inconnues : deux courants de plaque + grille, queue commune, charges 82 k / 100 k inegales,
//             conduction de grille) -> table monotone interpolee en cubique
//        SE : seconde triode 12AX7 (V2) facon Champ, cathode dynamique
//   -> condensateurs de liaison + resistances de grille, conduction de grille douce (diode de Langmuir lissee)
//      -> glissement de polarisation (« blocking distortion »)
//   -> lampe(s) de puissance (Koren pentode / montage triode, calees sur les fiches techniques) resolues
//      par Newton sur la charge du transformateur ; en PP, paire appariee a 4 % pres (comme une vraie paire)
//   -> alimentation a deux cellules : B+ des plaques (redresseur + 1er condensateur) puis noeud des ecrans
//      (resistance / self + 2e condensateur) : l'ecran s'affaisse en retard -> compression qui « respire »
//   -> polarisation fixe (6L6, EL34, KT88 en PP) ou par la cathode (EL84, tous les SE)
//   -> contre-reaction globale vers l'entree du dephaseur, filtree par DEPTH (grave) et CONTOUR (aigu)
//   -> transformateur de sortie (inductance primaire, fuites, saturation du noyau) et charge d'un haut-parleur
//      propre a chaque voicing (resonance du baffle, montee inductive de la bobine)
//
// Voicings (montages de reference) :
//   6L6  PP : Fender blackface (V1 100 k / 1,5 k / 25 uF, dephaseur 12AT7, contre-reaction forte, 2x12 ouvert)
//   EL34 PP : Marshall JMP / JCM800 (V1 « bright » 2,7 k / 0,68 uF, dephaseur 12AX7, 4x12 ferme)
//   EL84 PP : Vox AC30 (V1 220 k, dephaseur 12AX7 a longue queue, polarisation cathode, redresseur GZ34,
//             presque pas de contre-reaction)
//   KT88 PP : Ampeg SVT / hi-fi (dephaseur 12AT7 basse impedance, enorme marge, 8x10)
//   SE      : Fender Champ / Vox AC4 (V1 + V2 12AX7, une lampe en classe A)
// =====================================================================================
#include <cmath>
#include <algorithm>
#include <cstring>
#include <vector>
#include <memory>
#include <mutex>

namespace anode
{
    constexpr int numTubes = 4;           // ordre du selecteur : 6L6, EL34, EL84, KT88
    inline const char* tubeName (int t) { static const char* n[] = { "6L6", "EL34", "EL84", "KT88" }; return n[std::clamp (t, 0, 3)]; }
    constexpr double kPi = 3.14159265358979323846;

    // ---------------------------------------------------------------------------------
    // Lampes de puissance : modele de Koren (pentode), KG1 / KG2 recales sur les fiches techniques :
    //   6L6GC : Va = Vg2 = 250 V, Vg1 = -14 V   -> Ia  72 mA, Ig2 5 mA,  gm 6,0 mA/V
    //   EL34  : Va = Vg2 = 250 V, Vg1 = -13,5 V -> Ia 100 mA, Ig2 15 mA, gm 11 mA/V
    //   EL84  : Va = Vg2 = 250 V, Vg1 = -7,3 V  -> Ia  48 mA, Ig2 5,5 mA, gm 11,3 mA/V
    //   KT88  : Va = Vg2 = 250 V, Vg1 = -15 V   -> Ia 140 mA, Ig2 3 mA,  gm 11,5 mA/V
    // ---------------------------------------------------------------------------------
    struct TubeModel { double mu, ex, kg1, kg2, kp, kvb; };
    inline const TubeModel& tubeModel (int t)
    {
        static const TubeModel m[numTubes] =
        {
            {  8.7, 1.35, 821.0,  7764.0,  48.0, 12.0 },   // 6L6GC (tetrode a faisceaux diriges)
            { 11.0, 1.35, 315.0,  1424.0,  60.0, 24.0 },   // EL34 (vraie pentode)
            { 19.2, 1.35, 325.0,  1921.0, 200.0, 24.0 },   // EL84 / 6BQ5 (pentode noval, grande pente)
            {  8.8, 1.35, 405.0, 12554.0,  32.0, 16.0 },   // KT88 (tetrode a faisceaux, grosse ampoule)
        };
        return m[std::clamp (t, 0, numTubes - 1)];
    }

    inline double softplusE1 (double vs, double kp, double arg)
    {
        const double z = kp * arg;
        const double l = z > 30.0 ? z : (z < -30.0 ? std::exp (z) : std::log1p (std::exp (z)));
        return vs / kp * l;
    }

    inline void pentodeCurrents (const TubeModel& m, double vg, double vp, double vs, double& ia, double& ig2, double& dIaDvp)
    {
        vs = std::max (vs, 1.0);
        const double e1 = softplusE1 (vs, m.kp, 1.0 / m.mu + vg / vs);
        const double ep = e1 > 0.0 ? std::pow (e1, m.ex) : 0.0;
        const double i0 = ep / m.kg1;
        const double vpp = std::max (vp, 0.5);
        ia = i0 * std::atan (vpp / m.kvb);
        dIaDvp = i0 * m.kvb / (m.kvb * m.kvb + vpp * vpp);
        const double knee = 1.0 + 1.6 * (1.5707963 - std::atan (vpp / m.kvb));
        ig2 = ep / m.kg2 * knee;
    }

    // ---------------------------------------------------------------------------------
    // Triodes de preampli / dephaseur : modele de Koren (parametres publies)
    // ---------------------------------------------------------------------------------
    struct TriodeType { double mu, ex, kg1, kp, kvb, gk; };
    enum { T12AX7 = 0, T12AT7 = 1, T12AU7 = 2 };
    inline const TriodeType& triodeType (int t)
    {
        static const TriodeType tt[3] =
        {
            { 100.0, 1.40, 1060.0, 600.0, 300.0, 2.5e-4 },  // 12AX7 / ECC83 : mu 100, rp ~ 62 k
            {  60.0, 1.35,  460.0, 300.0, 300.0, 4.0e-4 },  // 12AT7 / ECC81 : mu 60, rp ~ 11 k
            {  21.5, 1.30, 1180.0,  84.0, 300.0, 5.0e-4 },  // 12AU7 / ECC82 : mu 20, rp ~ 7,7 k
        };
        return tt[std::clamp (t, 0, 2)];
    }

    inline double korenTriode (const TriodeType& t, double vgk, double vp)
    {
        vp = std::max (vp, 0.01);
        const double z = t.kp * (1.0 / t.mu + vgk / std::sqrt (t.kvb + vp * vp));
        const double l = z > 30.0 ? z : (z < -30.0 ? std::exp (z) : std::log1p (std::exp (z)));
        const double e1 = vp / t.kp * l;
        return e1 > 0.0 ? std::pow (e1, t.ex) / t.kg1 : 0.0;
    }

    // conduction de grille : diode de Langmuir (v^1,5) lissee pres de 0 V (potentiel de contact) -> pas de cassure
    inline double gridCurrent (double gk, double vgk)
    {
        constexpr double vt = 0.15;
        const double z = vgk / vt;
        const double sp = z > 30.0 ? vgk : vt * std::log1p (std::exp (z));
        return gk * sp * std::sqrt (sp);
    }
    inline double gridCurrentD (double gk, double vgk)
    {
        constexpr double vt = 0.15;
        const double z = vgk / vt;
        const double sp = z > 30.0 ? vgk : vt * std::log1p (std::exp (z));
        const double sig = z > 30.0 ? 1.0 : 1.0 / (1.0 + std::exp (-z));
        return 1.5 * gk * std::sqrt (sp) * sig;
    }

    // ---------------------------------------------------------------------------------
    // Interpolation cubique (Catmull-Rom) sur table uniforme : valeur et derivee continues
    // ---------------------------------------------------------------------------------
    struct CubicTable
    {
        std::vector<double> y;
        double x0 = 0, h = 1, invh = 1;
        int n = 0;
        void init (double xMin, double xMax, int N) { n = N; x0 = xMin; h = (xMax - xMin) / (N - 1); invh = 1.0 / h; y.assign ((size_t) N, 0.0); }
        double xAt (int i) const { return x0 + h * i; }
        inline double eval (double x, double* deriv = nullptr) const
        {
            double p = (x - x0) * invh;
            if (p <= 1.0 || p >= n - 2.0)
            {
                // bords : prolongement lineaire (les tables couvrent largement la plage utile)
                const int i = p <= 1.0 ? 0 : n - 2;
                const double s = (y[(size_t) i + 1] - y[(size_t) i]) * invh;
                if (deriv) *deriv = s;
                return y[(size_t) i] + s * (x - xAt (i));
            }
            const int i = (int) p; const double t = p - i;
            const double p0 = y[(size_t) i - 1], p1 = y[(size_t) i], p2 = y[(size_t) i + 1], p3 = y[(size_t) i + 2];
            const double a = -0.5 * p0 + 1.5 * p1 - 1.5 * p2 + 0.5 * p3;
            const double b = p0 - 2.5 * p1 + 2.0 * p2 - 0.5 * p3;
            const double c = -0.5 * p0 + 0.5 * p2;
            if (deriv) *deriv = ((3.0 * a * t + 2.0 * b) * t + c) * invh;
            return ((a * t + b) * t + c) * t + p1;
        }
    };

    // conduction de grille normalisee (gk = 1), tabulee : pas d'exp / log dans le thread audio
    inline const CubicTable& gridTable()
    {
        static const CubicTable* g = []
        {
            auto* t = new CubicTable(); t->init (-1.5, 120.0, 16384);
            for (int k = 0; k < t->n; ++k) t->y[(size_t) k] = gridCurrent (1.0, t->xAt (k));
            return t;
        }();
        return *g;
    }

    // ---------------------------------------------------------------------------------
    // Etage triode en cathode commune : table exacte en fonction de w = Vgrille_entree - Vcathode
    //   (la droite de charge et la conduction de grille a travers Rs sont resolues hors ligne),
    //   la cathode (Rk // Ck) reste un etat dynamique -> glissement de polarisation, compression, bloom
    // ---------------------------------------------------------------------------------
    struct TriodeTable
    {
        int type = T12AX7; double B = 250, Rl = 100e3, Rk = 1500, Rs = 68e3;
        CubicTable vp, ik;
        double vkq = 1.0, vpq = 0.0, ikq = 0.0, gainBypassed = 1.0;

        void build()
        {
            const auto& t = triodeType (type);
            auto solve = [&] (double w, double& vpo, double& iko)
            {
                // tension grille-cathode : w = vgk + Rs * ig(vgk)
                double lo = -200.0, hi = std::min (w, 50.0) + 1e-9;
                if (w < lo) { hi = w; lo = w - 1.0; }
                for (int k = 0; k < 64; ++k) { const double m = 0.5 * (lo + hi); (m + Rs * gridCurrent (t.gk, m) > w ? hi : lo) = m; }
                const double vgk = 0.5 * (lo + hi);
                double a = 0.0, b = B;
                for (int k = 0; k < 64; ++k) { const double m = 0.5 * (a + b); (B - Rl * korenTriode (t, vgk, m) > m ? a : b) = m; }
                vpo = 0.5 * (a + b);
                iko = (B - vpo) / Rl + gridCurrent (t.gk, vgk);
            };
            // repos : Vk = Rk Ik(-Vk)
            double lo = 0.0, hi = 20.0;
            for (int k = 0; k < 60; ++k) { const double m = 0.5 * (lo + hi); double v, i; solve (-m, v, i); (Rk * i > m ? lo : hi) = m; }
            vkq = 0.5 * (lo + hi);
            constexpr int N = 12288;
            vp.init (-vkq - 70.0, -vkq + 40.0, N); ik.init (-vkq - 70.0, -vkq + 40.0, N);
            for (int k = 0; k < N; ++k) { double v, i; solve (vp.xAt (k), v, i); vp.y[(size_t) k] = v; ik.y[(size_t) k] = i; }
            double d; ikq = ik.eval (-vkq); vpq = vp.eval (-vkq, &d);
            gainBypassed = -d;
        }
        bool same (int ty, double b, double rl, double rk, double rs) const { return ty == type && b == B && rl == Rl && rk == Rk && rs == Rs; }
    };

    struct TriodeStage
    {
        const TriodeTable* t = nullptr;
        double Ck = 25e-6, dt = 1.0 / 384000.0, vk = 1.0;
        void reset() { vk = t ? t->vkq : 0.0; }
        // vin : tension de grille (V) ; renvoie la variation de la tension de plaque (V, etage inverseur)
        inline double process (double vin)
        {
            double dik;
            const double w = vin - vk;
            const double v = t->vp.eval (w);
            const double i = t->ik.eval (w, &dik);
            const double a = dt / Ck;
            vk += a * (i - vk / t->Rk) / (1.0 + a * (dik + 1.0 / t->Rk));      // semi-implicite : stable
            return v - t->vpq;
        }
    };

    // ---------------------------------------------------------------------------------
    // Dephaseur a paire differentielle (« long-tailed pair »), resolu exactement hors ligne :
    //   grilles polarisees a la jonction Rtail / Rbias, grille 2 a la masse alternative,
    //   charges de plaque inegales (82 k / 100 k sur Fender / Marshall), conduction de la grille 1 a travers Rs.
    //   Sorties : variation de la plaque 2 (en phase) et de la plaque 1 (en opposition)
    // ---------------------------------------------------------------------------------
    struct LTPTable
    {
        int type = T12AX7; double B = 330, Rl1 = 82e3, Rl2 = 100e3, Rt = 10e3, Rb = 820, Rs = 100e3;
        CubicTable outP, outN;
        double gain = 30.0;

        void build()
        {
            const auto& t = triodeType (type);
            const double Rtot = Rt + Rb;
            // courant de plaque d'une triode sur sa charge, pour une tension grille-cathode donnee (Newton encadre)
            auto plate = [&] (double vgk, double Rl)
            {
                double lo = 0.0, hi = B, vp = 0.5 * B;
                for (int k = 0; k < 60; ++k)
                {
                    const double i = korenTriode (t, vgk, vp);
                    const double g = vp - (B - Rl * i);                      // croissante en vp
                    if (g > 0.0) hi = vp; else lo = vp;
                    const double i2 = korenTriode (t, vgk, vp + 1e-3);
                    const double dg = 1.0 + Rl * (i2 - i) / 1e-3;
                    double nv = vp - g / dg;
                    if (! (nv > lo && nv < hi)) nv = 0.5 * (lo + hi);
                    if (std::fabs (nv - vp) < 1e-9) { vp = nv; break; }
                    vp = nv;
                }
                return (B - vp) / Rl;
            };
            // tension grille-cathode quand la grille est attaquee a travers Rs (conduction de grille) : Newton encadre
            const auto& gt = gridTable();
            auto gridV = [&] (double w)
            {
                if (w < -1.4) return w;
                double lo = -1.5, hi = w, v = std::min (w, 0.3);          // la solution est dans ]-1,5 ; w]
                for (int k = 0; k < 40; ++k)
                {
                    double dg; const double g = v + Rs * t.gk * gt.eval (v, &dg) - w;
                    if (g > 0.0) hi = v; else lo = v;
                    double nv = v - g / (1.0 + Rs * t.gk * dg);
                    if (! (nv > lo && nv < hi)) nv = 0.5 * (lo + hi);
                    if (std::fabs (nv - v) < 1e-10) { v = nv; break; }
                    v = nv;
                }
                return v;
            };
            // repos : grilles a la jonction Vj = Itot Rt, cathode a Itot (Rt + Rb)
            double vkq = 1.0;
            {
                double lo = 0.0, hi = B;
                for (int k = 0; k < 80; ++k)
                {
                    const double vk = 0.5 * (lo + hi);
                    const double vj = vk * Rt / Rtot;
                    const double r = vk - Rtot * (plate (vj - vk, Rl1) + plate (vj - vk, Rl2));
                    (r > 0.0 ? hi : lo) = vk;
                }
                vkq = 0.5 * (lo + hi);
            }
            const double Vj = vkq * Rt / Rtot;
            const double i1q = plate (Vj - vkq, Rl1), i2q = plate (Vj - vkq, Rl2);
            const double vp1q = B - Rl1 * i1q, vp2q = B - Rl2 * i2q;

            // pour u donne : residu en vk (croissant) -> resolution encadree
            auto solve = [&] (double u, double vkGuess, double& o1, double& o2)
            {
                auto resid = [&] (double vk, double& i1, double& i2)
                {
                    const double vgk1 = gridV (Vj + u - vk);
                    i1 = plate (vgk1, Rl1);
                    i2 = plate (Vj - vk, Rl2);
                    return vk - Rtot * (i1 + i2 + gridCurrent (t.gk, vgk1));
                };
                double i1, i2;
                // Newton encadre sur vk (residu croissant)
                double lo = vkGuess - 0.5, hi = vkGuess + 0.5;
                while (resid (lo, i1, i2) > 0.0 && lo > -50.0) lo -= 5.0;
                while (resid (hi, i1, i2) < 0.0 && hi < B) hi += 5.0;
                double vk = std::clamp (vkGuess, lo, hi);
                for (int k = 0; k < 50; ++k)
                {
                    const double r = resid (vk, i1, i2);
                    if (r > 0.0) hi = vk; else lo = vk;
                    if (std::fabs (r) < 1e-8) break;
                    const double r2 = resid (vk + 1e-5, i1, i2);
                    const double dr = (r2 - r) / 1e-5;
                    double nv = dr > 0.0 ? vk - r / dr : 0.5 * (lo + hi);
                    if (! (nv > lo && nv < hi)) nv = 0.5 * (lo + hi);
                    if (std::fabs (nv - vk) < 1e-11) { vk = nv; break; }
                    vk = nv;
                }
                resid (vk, i1, i2);
                o1 = (B - Rl2 * i2) - vp2q;      // plaque 2 : en phase
                o2 = (B - Rl1 * i1) - vp1q;      // plaque 1 : en opposition
                return vk;
            };
            constexpr int N = 6144;
            constexpr double uMax = 90.0;
            outP.init (-uMax, uMax, N); outN.init (-uMax, uMax, N);
            const int mid = N / 2;
            for (int dir = -1; dir <= 1; dir += 2)
            {
                double vk = vkq;
                for (int k = (dir < 0 ? mid : mid + 1); k >= 0 && k < N; k += dir)
                {
                    double o1, o2;
                    vk = solve (outP.xAt (k), vk, o1, o2);
                    outP.y[(size_t) k] = o1; outN.y[(size_t) k] = o2;
                }
            }
            const double o0 = outP.eval (0.0), n0 = outN.eval (0.0);
            for (auto& v : outP.y) v -= o0;
            for (auto& v : outN.y) v -= n0;
            double d; outP.eval (0.0, &d); gain = d;
        }
        bool same (int ty, double b, double r1, double r2, double rt, double rb, double rs) const
        { return ty == type && b == B && r1 == Rl1 && r2 == Rl2 && rt == Rt && rb == Rb && rs == Rs; }
    };

    // registre des tables (construites une fois hors thread audio, partagees entre configurations et canaux)
    struct TableCache
    {
        std::mutex lock;
        std::vector<std::unique_ptr<TriodeTable>> triodes;
        std::vector<std::unique_ptr<LTPTable>> ltps;
        static TableCache& get() { static TableCache* c = new TableCache(); return *c; }
        const TriodeTable* triode (int ty, double B, double Rl, double Rk, double Rs)
        {
            std::lock_guard<std::mutex> g (lock);
            for (auto& t : triodes) if (t->same (ty, B, Rl, Rk, Rs)) return t.get();
            auto t = std::make_unique<TriodeTable>(); t->type = ty; t->B = B; t->Rl = Rl; t->Rk = Rk; t->Rs = Rs; t->build();
            triodes.push_back (std::move (t)); return triodes.back().get();
        }
        const LTPTable* ltp (int ty, double B, double R1, double R2, double Rt, double Rb, double Rs)
        {
            std::lock_guard<std::mutex> g (lock);
            for (auto& t : ltps) if (t->same (ty, B, R1, R2, Rt, Rb, Rs)) return t.get();
            auto t = std::make_unique<LTPTable>(); t->type = ty; t->B = B; t->Rl1 = R1; t->Rl2 = R2; t->Rt = Rt; t->Rb = Rb; t->Rs = Rs; t->build();
            ltps.push_back (std::move (t)); return ltps.back().get();
        }
    };

    // ---------------------------------------------------------------------------------
    // Lampes de puissance : versions rapides (tables) pour le thread audio
    //   E1^ex = Vs^ex * S(Vg / Vs), S(r) = (ln(1 + exp(kp (1/mu + r))) / kp)^ex
    // ---------------------------------------------------------------------------------
    inline double fastAtanPos (double x)          // x >= 0, erreur < 2e-6 rad
    {
        const bool inv = x > 1.0;
        const double z = inv ? 1.0 / x : x, z2 = z * z;
        const double a = z * (0.99997726 + z2 * (-0.33262347 + z2 * (0.19354346 + z2 * (-0.11643287 + z2 * (0.05265332 - 0.01172120 * z2)))));
        return inv ? 1.5707963267948966 - a : a;
    }

    struct FastTube
    {
        static constexpr int N = 32768;
        static constexpr double rMin = -0.6, rMax = 0.6;
        std::vector<double> s;
        double invKg1 = 0, invKg2 = 0, kvb = 1, ex = 1.35;
        void build (const TubeModel& m)
        {
            s.assign (N + 2, 0.0);
            ex = m.ex; kvb = m.kvb; invKg1 = 1.0 / m.kg1; invKg2 = 1.0 / m.kg2;
            for (int i = 0; i <= N + 1; ++i)
            {
                const double r = rMin + (rMax - rMin) * i / N;
                const double sp = softplusE1 (1.0, m.kp, 1.0 / m.mu + r);
                s[(size_t) i] = sp > 0.0 ? std::pow (sp, m.ex) : 0.0;
            }
        }
        inline double S (double r) const
        {
            double p = (r - rMin) * (N / (rMax - rMin));
            if (p <= 0.0) return 0.0;
            if (p >= N) return s[N] + (p - N) * (s[N] - s[N - 1]);
            const int i = (int) p; const double f = p - i;
            return s[(size_t) i] + f * (s[(size_t) i + 1] - s[(size_t) i]);
        }
    };

    struct PowTable                               // v^1.35, 0..1600 V
    {
        static constexpr int N = 6400; static constexpr double vMax = 1600.0;
        double t[N + 2] {};
        PowTable() { for (int i = 0; i <= N + 1; ++i) t[i] = std::pow (vMax * i / N, 1.35); }
        inline double operator() (double v) const
        {
            double p = v * (N / vMax);
            if (p <= 0.0) return 0.0;
            if (p >= N) return std::pow (v, 1.35);
            const int i = (int) p; const double f = p - i;
            return t[i] + f * (t[i + 1] - t[i]);
        }
    };

    struct Tables
    {
        FastTube tube[numTubes];
        PowTable pow135;
        Tables() { for (int t = 0; t < numTubes; ++t) tube[t].build (tubeModel (t)); gridTable(); }
        static const Tables& get() { static const Tables* tb = new Tables(); return *tb; }
    };

    // ---------------------------------------------------------------------------------
    // Configuration : points de fonctionnement et voicing (d'apres des amplis reels)
    // ---------------------------------------------------------------------------------
    struct Config
    {
        bool   pushPull = true, pentode = true, cathodeBias = false;
        // etage de puissance
        double bPlus = 450.0, rLoad = 3400.0, iq = 0.036;
        double rSupply = 60.0, cFilter = 50e-6;      // redresseur + 1er condensateur (plaques)
        double rDrop = 1000.0, cScreen = 32e-6;      // self / resistance + 2e condensateur (ecrans, preampli)
        double rScreen = 1000.0;
        double cCouple = 0.022e-6, rGrid = 220e3, rSource = 38e3;
        double cCathode = 100e-6;
        double match = 0.96;                         // appariement de la paire (PP)
        double headroom = 1.0, nfb = 1.8;
        double otLow = 35.0, otHigh = 14000.0, otQ = 0.75, coreSat = 0.10;
        double spkF = 100.0, spkQ = 1.6, spkBump = 0.0, spkTop = 0.0;
        // V1 (entree)
        int v1Type = T12AX7; double v1B = 250, v1Rl = 100e3, v1Rk = 1500, v1Ck = 25e-6, v1Rs = 68e3, v1Hp = 7.0;
        // dephaseur (PP)
        int piType = T12AX7; double piB = 330, piRl1 = 82e3, piRl2 = 100e3, piRt = 10e3, piRb = 820, piRs = 100e3;
        // V2 (SE)
        double v2Rl = 100e3, v2Rk = 1500, v2Ck = 25e-6, v2Rs = 47e3;
        // calcules
        double vBias = -40.0, rK = 0.0, inScale = 1.0, levelTrim = 1.0;
    };

    inline Config makeConfig (int tube, bool pp, bool pent)
    {
        Config c;
        c.pushPull = pp; c.pentode = pent;
        if (pp)
        {
            switch (tube)
            {
                case 0: // 6L6 : Fender blackface (Deluxe / Bassman / Twin)
                    c.bPlus = 460; c.rLoad = 4000; c.iq = 0.040; c.rSupply = 55; c.cFilter = 70e-6; c.rDrop = 470; c.cScreen = 40e-6; c.rScreen = 470;
                    c.cCouple = 0.1e-6; c.headroom = 0.95; c.otLow = 30; c.otHigh = 15000; c.nfb = 2.0;
                    c.spkF = 85; c.spkQ = 1.3;
                    c.v1Rl = 100e3; c.v1Rk = 1500; c.v1Ck = 25e-6; c.v1Hp = 7.0;
                    c.piType = T12AT7; c.piB = 420; c.piRl1 = 82e3; c.piRl2 = 100e3; c.piRt = 22e3; c.piRb = 470; c.piRs = 100e3;
                    break;
                case 1: // EL34 : Marshall JMP / JCM800 (V1 « bright », 4x12 ferme)
                    c.bPlus = 455; c.rLoad = 3400; c.iq = 0.036; c.rSupply = 60; c.cFilter = 50e-6; c.rDrop = 1000; c.cScreen = 50e-6; c.rScreen = 1000;
                    c.cCouple = 0.022e-6; c.headroom = 1.15; c.otLow = 38; c.otHigh = 13500; c.nfb = 1.6;
                    c.spkF = 105; c.spkQ = 2.0;
                    c.v1B = 280; c.v1Rl = 100e3; c.v1Rk = 2700; c.v1Ck = 0.68e-6; c.v1Hp = 15.0;
                    c.piType = T12AX7; c.piB = 330; c.piRl1 = 82e3; c.piRl2 = 100e3; c.piRt = 10e3; c.piRb = 820; c.piRs = 100e3;
                    break;
                case 2: // EL84 : Vox AC30 (polarisation cathode, GZ34, presque pas de contre-reaction)
                    c.bPlus = 335; c.rLoad = 8000; c.iq = 0.036; c.rSupply = 190; c.cFilter = 32e-6; c.rDrop = 1000; c.cScreen = 32e-6; c.rScreen = 470;
                    c.cCouple = 0.047e-6; c.headroom = 1.55; c.otLow = 45; c.otHigh = 12500; c.cathodeBias = true; c.cCathode = 50e-6;
                    c.nfb = 0.55; c.spkF = 80; c.spkQ = 1.5;
                    c.v1B = 230; c.v1Rl = 220e3; c.v1Rk = 1500; c.v1Ck = 25e-6; c.v1Hp = 7.0;
                    c.piType = T12AX7; c.piB = 300; c.piRl1 = 100e3; c.piRl2 = 100e3; c.piRt = 47e3; c.piRb = 1500; c.piRs = 100e3;
                    break;
                default: // KT88 : Ampeg SVT / hi-fi (dephaseur basse impedance, 8x10)
                    c.bPlus = 520; c.rLoad = 4500; c.iq = 0.045; c.rSupply = 45; c.cFilter = 100e-6; c.rDrop = 470; c.cScreen = 60e-6; c.rScreen = 1000;
                    c.cCouple = 0.1e-6; c.headroom = 0.72; c.otLow = 25; c.otHigh = 16000; c.nfb = 2.4;
                    c.spkF = 70; c.spkQ = 1.7;
                    c.v1B = 300; c.v1Rl = 100e3; c.v1Rk = 1500; c.v1Ck = 2.2e-6; c.v1Hp = 3.5;
                    c.piType = T12AT7; c.piB = 420; c.piRl1 = 56e3; c.piRl2 = 56e3; c.piRt = 22e3; c.piRb = 1000; c.piRs = 47e3;
                    break;
            }
            if (! pent) c.nfb *= 0.65;
            c.coreSat = 0.08;
        }
        else
        {
            // Fender Champ / Vox AC4 : V1 + V2 12AX7, une lampe en classe A, polarisation cathode
            c.cathodeBias = true; c.cCathode = 47e-6; c.match = 1.0;
            switch (tube)
            {
                case 0: c.bPlus = 350; c.rLoad = 2500; c.iq = 0.060; c.rSupply = 120; c.cFilter = 40e-6; c.rScreen = 470;  c.headroom = 1.0;  break;
                case 1: c.bPlus = 350; c.rLoad = 2700; c.iq = 0.066; c.rSupply = 120; c.cFilter = 40e-6; c.rScreen = 1000; c.headroom = 1.15; break;
                case 2: c.bPlus = 285; c.rLoad = 5200; c.iq = 0.044; c.rSupply = 200; c.cFilter = 22e-6; c.rScreen = 470;  c.headroom = 1.45; break;
                default: c.bPlus = 385; c.rLoad = 2000; c.iq = 0.082; c.rSupply = 90; c.cFilter = 60e-6; c.rScreen = 1000; c.headroom = 0.8; break;
            }
            c.rDrop = 1000; c.cScreen = 16e-6;
            c.cCouple = 0.022e-6; c.rGrid = 470e3; c.rSource = 30e3;
            c.nfb = pent ? 1.0 : 0.65;
            c.otLow = 70; c.otHigh = 11000; c.otQ = 0.8; c.coreSat = 0.22;
            c.spkF = 115; c.spkQ = 1.6;
            c.v1B = 250; c.v1Rl = 100e3; c.v1Rk = 1500; c.v1Ck = 25e-6; c.v1Hp = 7.0;
        }
        // haut-parleur : fort en pentode (impedance de sortie elevee), attenue en triode
        c.spkBump = pent ? 0.55 : 0.18;
        c.spkTop  = pent ? 0.45 : 0.12;

        // polarisation : courant de repos vise en pentode (meme polarisation reutilisee en triode)
        const auto& m = tubeModel (tube);
        const double vs = c.bPlus - 0.02 * c.bPlus;
        auto iaAt = [&] (double vg) { double ia, ig2, d; pentodeCurrents (m, vg, c.bPlus, vs, ia, ig2, d); return ia + ig2; };
        double lo = -150.0, hi = 0.0;
        for (int i = 0; i < 80; ++i) { const double mid = 0.5 * (lo + hi); (iaAt (mid) > c.iq ? hi : lo) = mid; }
        const double vgk = 0.5 * (lo + hi);
        if (c.cathodeBias)
        {
            const double ik = iaAt (vgk) * (pp ? 2.0 : 1.0);
            c.rK = -vgk / ik;
            c.vBias = 0.0;
        }
        else
            c.vBias = vgk;
        return c;
    }

    // ---------------------------------------------------------------------------------
    // Filtres elementaires
    // ---------------------------------------------------------------------------------
    struct OnePole
    {
        double a = 0.0, z = 0.0;
        void setLP (double fc, double fs) { a = 1.0 - std::exp (-2.0 * kPi * fc / fs); }
        double lp (double x) { z += a * (x - z); return z; }
        double hp (double x) { z += a * (x - z); return x - z; }
    };
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
        void reset() { z1 = z2 = 0.0; }
        void set (double B0, double B1, double B2, double A0, double A1, double A2)
        { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
        void lowpass (double f, double q, double fs)
        { const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
          set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
        void highpass (double f, double q, double fs)
        { const double w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
          set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
        void peak (double f, double q, double dB, double fs)
        { const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
          set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A); }
        void highShelf (double f, double dB, double fs)
        { const double A = std::pow (10.0, dB / 40.0), w = 2 * kPi * f / fs, c = std::cos (w), s = std::sin (w);
          const double al = s / 2 * std::sqrt (2.0), sq = 2 * std::sqrt (A) * al;
          set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
               (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq); }
    };

    // ---------------------------------------------------------------------------------
    // Ampli complet (un canal, une configuration)
    // ---------------------------------------------------------------------------------
    class PowerStage
    {
    public:
        static constexpr double inVolts = 1.5;     // 0 dBFS = 1,5 V crete sur la grille de V1 (micro double tres chaud)

        void setup (int tubeIdx, bool pp, bool pent, double sampleRate)
        {
            tube = tubeIdx; fs = sampleRate; dt = 1.0 / fs;
            cfg = makeConfig (tubeIdx, pp, pent);
            m = &tubeModel (tubeIdx);
            fast = &Tables::get().tube[tubeIdx];
            powTab = &Tables::get().pow135;
            auto& cache = TableCache::get();
            v1.t = cache.triode (cfg.v1Type, cfg.v1B, cfg.v1Rl, cfg.v1Rk, cfg.v1Rs);
            v1.Ck = cfg.v1Ck; v1.dt = dt;
            v1Norm = -1.0 / (v1.t->gainBypassed * inVolts);
            if (pp) pi = cache.ltp (cfg.piType, cfg.piB, cfg.piRl1, cfg.piRl2, cfg.piRt, cfg.piRb, cfg.piRs);
            else
            {
                v2.t = cache.triode (T12AX7, cfg.v1B, cfg.v2Rl, cfg.v2Rk, cfg.v2Rs);
                v2.Ck = cfg.v2Ck; v2.dt = dt;
            }
            calibrate();
            buildFilters();
            reset();
        }

        void reset()
        {
            bSag = quiescentB; vScr = quiescentScr; vk = quiescentVk; vc[0] = vc[1] = 0.0;
            d = 0.0; iSE = cfg.iq; iRef = cfg.iq; ig2Prev[0] = ig2Prev[1] = qIg2;
            fbPrev = 0.0; drvLP[0] = drvLP[1] = 0.0;
            depthLP.z = 0; contourHP.z = 0; plateLP.z = 0; flux.z = 0; v1Hp.z = 0; v2Hp.z = 0;
            otHP1.reset(); otLP.reset(); spkPeak.reset(); spkShelf.reset(); dcBlock.z = 0;
            iAvg.z = 0; iVar = 0.0; lastPlate = lastScreen = 0.0;
            v1.reset(); if (v2.t) v2.reset();
        }

        void settle (double seconds) { for (int i = 0, n = (int) (seconds * fs); i < n; ++i) process (0.0, 1.0); }
        void setControls (double depth01, double contour01) { depth = depth01; contour = contour01; }

        // x : signal d'entree (1,0 = 0 dBFS) ; drive : gain lineaire du potard Volume (master, apres V1)
        // renvoie la sortie normalisee (gain petit signal = 1 a Volume 5)
        inline double process (double x, double drive)
        {
            // V1 : triode d'entree (avant le master), sortie remise a l'echelle et en phase
            const double a1 = v1Hp.hp (v1.process (x * inVolts)) * v1Norm;
            const double xin = a1 * drive;

            // contre-reaction globale (retard d'un echantillon au taux sur-echantillonne)
            const double y = fbPrev;
            const double bassPart = depthLP.lp (y);
            const double topPart  = contourHP.hp (y);
            const double fbSig = cfg.nfb * (y - depth * bassPart - contour * topPart);
            const double u = (xin - fbSig) * cfg.inScale;

            double out;
            if (cfg.pushPull)
            {
                double d1 = pi->outP.eval (u), d2 = pi->outN.eval (u);
                d1 = drvLP[0] += drvA * (d1 - drvLP[0]);   // effet Miller + resistance de grille
                d2 = drvLP[1] += drvA * (d2 - drvLP[1]);
                const double g1 = gridNode (0, d1);
                const double g2 = gridNode (1, d2);
                prepareScreens();
                out = solvePP (g1 - vk, g2 - vk);
            }
            else
            {
                const double drv = -v2Hp.hp (v2.process (u));
                const double d1 = drvLP[0] += drvA * (drv - drvLP[0]);
                const double g1 = gridNode (0, d1);
                prepareScreens();
                out = solveSE (g1 - vk);
            }

            // alimentation a deux cellules
            const double iDrop = (bSag - vScr) / cfg.rDrop;
            bSag += dt / cfg.cFilter * ((cfg.bPlus - bSag) / cfg.rSupply - lastPlate - iDrop);
            bSag = std::clamp (bSag, 0.4 * cfg.bPlus, 1.05 * cfg.bPlus);
            vScr += dt / cfg.cScreen * (iDrop - lastScreen - 0.004);
            vScr = std::clamp (vScr, 0.3 * cfg.bPlus, 1.05 * cfg.bPlus);
            if (cfg.cathodeBias)
                vk += dt / cfg.cCathode * (lastPlate + lastScreen - vk / cfg.rK);

            const double yN = plateLP.lp (out * invGain);
            fbPrev = yN;

            // transformateur de sortie
            double o = otHP1.process (yN);
            const double phi = flux.lp (o) * fluxScale;
            o -= cfg.coreSat * phi * phi * phi / (1.0 + std::fabs (phi * phi));
            o = otLP.process (o);
            // haut-parleur
            o = spkPeak.process (o);
            o = spkShelf.process (o);
            o = dcBlock.hp (o);

            const double iNow = lastPlate + lastScreen;
            iAvg.lp (iNow);
            const double dev = iNow - iAvg.z;
            iVar += iAvg.a * (dev * dev - iVar);           // variation du courant (classe A : le courant moyen bouge peu)
            return o * (1.0 + cfg.nfb) * cfg.levelTrim;
        }

        // niveau de sortie en saturation franche de l'etage de puissance : egalisation des configurations
        double saturatedRms() const
        {
            PowerStage t (*this);
            t.setControls (0.5, 0.5);
            const int n = (int) (0.2 * fs);
            double acc = 0.0; int cnt = 0;
            for (int i = 0; i < n; ++i)
            {
                const double y = t.process (0.5 * std::sin (2 * kPi * 200.0 * i / fs), 6.0);
                if (i > n / 2) { acc += y * y; ++cnt; }
            }
            return std::sqrt (acc / std::max (1, cnt));
        }
        void scaleLevel (double k) { cfg.levelTrim *= k; }

        double currentNow() const { return iAvg.z; }
        double currentSwing() const { return std::sqrt (std::max (0.0, iVar)); }    // courant alternatif efficace (A)
        double quiescentCurrent() const { return qCurrent; }
        const Config& config() const { return cfg; }

    private:
        // noeud de grille de puissance : condensateur de liaison, resistance de grille, conduction de grille douce
        inline double gridNode (int k, double vd)
        {
            const double Vb = cfg.vBias;
            const double w = vd - vc[k];
            const double rs = cfg.rSource, rg = cfg.rGrid;
            double vg = Vb + w * rg / (rg + rs);
            double ig = 0.0;
            if (vg - vk > -1.4)
            {
                const auto& gt = gridTable();
                for (int it = 0; it < 4; ++it)
                {
                    double dig;
                    ig = gridK * gt.eval (vg - vk, &dig);
                    const double f  = vg + rs * ((vg - Vb) / rg + ig) - (w + Vb);
                    const double fp = 1.0 + rs * (1.0 / rg + gridK * dig);
                    const double step = f / fp;
                    vg -= step;
                    if (std::fabs (step) < 1e-6) break;
                }
                ig = gridK * gt.eval (vg - vk);
            }
            vc[k] += dt * ((vg - Vb) / rg + ig) / cfg.cCouple;
            return vg;
        }

        inline void currents (double vg, double vp, int k, double& i, double& di)
        {
            const auto& ft = *fast;
            const double vpp = std::max (vp, 0.5);
            const double mk = k == 1 ? cfg.match : 1.0;
            if (cfg.pentode)
            {
                const double ep = vsEx[k] * ft.S (vg * invVs[k]) * mk;
                const double i0 = ep * ft.invKg1;
                const double q = vpp / ft.kvb;
                const double at = fastAtanPos (q);
                i = i0 * at;
                di = i0 / (ft.kvb * (1.0 + q * q));
                screenNow[k] = ep * ft.invKg2 * (1.0 + 1.6 * (1.5707963267948966 - at));
            }
            else
            {
                auto cur = [&] (double v) { return mk * powTab->operator() (v) * ft.S (vg / v) * (fastAtanPos (v / ft.kvb) * ft.invKg1 + ft.invKg2); };
                i = cur (vpp);
                di = (cur (vpp + 0.05) - i) * 20.0;
                screenNow[k] = 0.0;
            }
        }

        inline void prepareScreens()
        {
            if (! cfg.pentode) return;
            for (int k = 0; k < (cfg.pushPull ? 2 : 1); ++k)
            {
                const double vs = std::max (vScr - cfg.rScreen * ig2Prev[k], 10.0);
                vsEx[k] = powTab->operator() (vs);
                invVs[k] = 1.0 / vs;
            }
        }

        inline double solvePP (double vg1, double vg2)
        {
            const double rq = cfg.rLoad * 0.25;
            double i1 = 0, i2 = 0, di1 = 0, di2 = 0;
            for (int it = 0; it < 5; ++it)
            {
                const double vp1 = bSag - d * rq, vp2 = bSag + d * rq;
                currents (vg1, vp1, 0, i1, di1);
                currents (vg2, vp2, 1, i2, di2);
                const double f = d - (i1 - i2);
                const double fp = 1.0 + rq * (di1 + di2);
                const double step = f / fp;
                d -= step;
                if (std::fabs (step) < 1e-6) break;
            }
            lastPlate = i1 + i2; lastScreen = screenNow[0] + screenNow[1];
            ig2Prev[0] = screenNow[0]; ig2Prev[1] = screenNow[1];
            return -d * rq;
        }

        inline double solveSE (double vg)
        {
            const double rl = cfg.rLoad;
            double ia = 0, dia = 0;
            for (int it = 0; it < 5; ++it)
            {
                const double vp = bSag - rl * (iSE - iRef);
                currents (vg, vp, 0, ia, dia);
                const double f = iSE - ia;
                const double fp = 1.0 + rl * dia;
                const double step = f / fp;
                iSE -= step;
                if (std::fabs (step) < 5e-7) break;
            }
            iRef += refA * (iSE - iRef);
            lastPlate = iSE; lastScreen = screenNow[0];
            ig2Prev[0] = screenNow[0];
            return -(iSE - iRef) * rl;
        }

        void calibrate()
        {
            gridK = 3.5e-4;
            drvA = 1.0 - std::exp (-2.0 * kPi * 22000.0 / fs);
            refA = 1.0 - std::exp (-2.0 * kPi * 12.0 / fs);
            const double drvGain = cfg.pushPull ? pi->gain : v2.t->gainBypassed;
            // la grille de puissance atteint 0 V (attaque maxi) pour ~0,6 (normalise) a Volume 5
            const double vClip = cfg.cathodeBias ? std::fabs (biasMagnitudeCathode()) : std::fabs (cfg.vBias);
            cfg.inScale = vClip / (drvGain * 0.6) * cfg.headroom * (1.0 + cfg.nfb);

            invGain = 1.0; cfg.levelTrim = 1.0;
            const double nfbSave = cfg.nfb; cfg.nfb = 0.0;
            quiescentB = cfg.bPlus; quiescentScr = cfg.bPlus * 0.97; quiescentVk = cfg.cathodeBias ? -biasMagnitudeCathode() : 0.0;
            buildFilters();
            reset();
            for (int i = 0; i < (int) (0.4 * fs); ++i) process (0.0, 1.0);
            quiescentB = bSag; quiescentScr = vScr; quiescentVk = vk; qCurrent = iAvg.z; qIg2 = ig2Prev[0];
            const double v1vk = v1.vk, v2vk = v2.t ? v2.vk : 0.0;
            // gain petit signal (1 kHz)
            double num = 0.0, den = 0.0;
            const int n = (int) (0.05 * fs);
            const double amp = 0.002;
            for (int i = 0; i < n; ++i)
            {
                const double s = amp * std::sin (2 * kPi * 1000.0 * i / fs);
                process (s, 1.0);
                if (i > n / 2) { num += fbPrev * s; den += s * s; }
            }
            const double g = num / std::max (den, 1e-30);
            invGain = std::fabs (g) > 1e-12 ? 1.0 / g : 1.0;
            cfg.nfb = nfbSave;
            cfg.levelTrim = cfg.pentode ? 1.0 : 0.84;
            v1.vk = v1vk; if (v2.t) v2.vk = v2vk;
            buildFilters();
            reset();
            v1.vk = v1vk; if (v2.t) v2.vk = v2vk;
        }

        double biasMagnitudeCathode() const
        {
            const double vs = cfg.bPlus * 0.98;
            double lo = -150.0, hi = 0.0;
            for (int i = 0; i < 80; ++i)
            {
                const double mid = 0.5 * (lo + hi);
                double ia, ig2, dd; pentodeCurrents (*m, mid, cfg.bPlus, vs, ia, ig2, dd);
                ((ia + ig2) > cfg.iq ? hi : lo) = mid;
            }
            return 0.5 * (lo + hi);
        }

        void buildFilters()
        {
            depthLP.setLP (170.0, fs);
            contourHP.setLP (2300.0, fs);
            plateLP.setLP (38000.0, fs);
            v1Hp.setLP (cfg.v1Hp, fs);
            v2Hp.setLP (7.0, fs);
            otHP1.highpass (cfg.otLow, cfg.pushPull ? 0.6 : 0.75, fs);
            otLP.lowpass (cfg.otHigh, cfg.otQ, fs);
            flux.setLP (9.0, fs);
            const double fRef = cfg.pushPull ? 40.0 : 70.0;
            fluxScale = (2 * kPi * fRef / 9.0) / 1.2;
            const double zOut = 1.0 / (1.0 + cfg.nfb * 0.8);
            spkPeak.peak (cfg.spkF, cfg.spkQ, 7.0 * cfg.spkBump * zOut, fs);
            spkShelf.highShelf (3200.0, 6.0 * cfg.spkTop * zOut, fs);
            dcBlock.setLP (6.0, fs);
            iAvg.setLP (4.0, fs);
        }

        Config cfg;
        const TubeModel* m = nullptr;
        const FastTube* fast = nullptr;
        const PowTable* powTab = nullptr;
        const LTPTable* pi = nullptr;
        TriodeStage v1, v2;
        double v1Norm = 1.0;
        double vsEx[2] {}, invVs[2] {};
        int tube = 1;
        double fs = 384000.0, dt = 1.0 / 384000.0;
        double depth = 0.5, contour = 0.5;
        double invGain = 1.0, gridK = 3.5e-4, drvA = 0.5, refA = 0.001, fluxScale = 1.0;
        double bSag = 450.0, vScr = 440.0, vk = 0.0, vc[2] {}, d = 0.0, iSE = 0.0, iRef = 0.0, ig2Prev[2] {}, screenNow[2] {};
        double lastPlate = 0.0, lastScreen = 0.0, qCurrent = 0.0, qIg2 = 0.0, quiescentB = 450.0, quiescentScr = 440.0, quiescentVk = 0.0;
        double fbPrev = 0.0, drvLP[2] {}, iVar = 0.0;
        OnePole depthLP, contourHP, plateLP, flux, dcBlock, iAvg, v1Hp, v2Hp;
        Biquad otHP1, otLP, spkPeak, spkShelf;
    };
}
