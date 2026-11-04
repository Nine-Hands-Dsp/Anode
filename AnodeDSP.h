#pragma once
// =====================================================================================
// ANODE (NINE HANDS | DSP) - moteur d'etage de puissance a lampes, utilise en preampli
//
// Chaine modelisee (au taux sur-echantillonne, >= 176,4 kHz) :
//   entree -> Volume (master) -> etage d'attaque
//        PP : dephaseur a paire differentielle (ECC83 / 12AT7, transfert en tanh, leger desequilibre)
//        SE : etage triode 12AX7 resolu sur sa droite de charge (modele de Koren)
//   -> condensateurs de liaison + resistances de grille, conduction de grille (diode de Child-Langmuir)
//      -> decalage de polarisation dynamique (« blocking distortion »)
//   -> lampe(s) de puissance (modele de Koren pentode / triode, parametres cales sur les fiches techniques :
//      courant de repos, pente gm, courant d'ecran) resolues par Newton sur la charge du transformateur
//        PP : deux lampes en opposition, charge plaque a plaque Raa (Raa/4 par demi-primaire)
//        SE : une lampe en classe A, charge Rl, courant continu dans le primaire
//      Pentode : ecran a tension fixe (alim + resistance d'ecran) ; Triode : ecran relie a la plaque
//   -> alimentation : affaissement (sag) du B+ selon le redresseur (silicium / lampe), ecran qui suit
//   -> polarisation fixe (6L6, EL34, KT88 en PP) ou automatique par la cathode (EL84, tous les SE)
//   -> contre-reaction globale vers l'entree du dephaseur, filtree par DEPTH (graves) et CONTOUR (aigus)
//   -> transformateur de sortie : inductance primaire (passe-haut), fuite (passe-bas resonant),
//      saturation du noyau dans le grave, interaction avec l'impedance d'un haut-parleur
// =====================================================================================
#include <cmath>
#include <algorithm>
#include <cstring>

namespace anode
{
    constexpr int numTubes = 4;           // ordre du selecteur : 6L6, EL34, EL84, KT88
    inline const char* tubeName (int t) { static const char* n[] = { "6L6", "EL34", "EL84", "KT88" }; return n[std::clamp (t, 0, 3)]; }

    // ---------------------------------------------------------------------------------
    // Modele de Koren (pentode), KG1 / KG2 recales sur les points de fonctionnement des fiches :
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
            {  8.7, 1.35, 821.0,  7764.0,  48.0, 12.0 },   // 6L6GC (tetrode a faisceaux dirigés)
            { 11.0, 1.35, 315.0,  1424.0,  60.0, 24.0 },   // EL34 (vraie pentode)
            { 19.2, 1.35, 325.0,  1921.0, 200.0, 24.0 },   // EL84 / 6BQ5 (pentode noval, grande pente)
            {  8.8, 1.35, 405.0, 12554.0,  32.0, 16.0 },   // KT88 (tetrode a faisceaux, grosse ampoule)
        };
        return m[std::clamp (t, 0, numTubes - 1)];
    }

    inline double softplusE1 (double vs, double kp, double arg)
    {
        // vs / kp * ln(1 + exp(kp * arg)), sans debordement
        const double z = kp * arg;
        const double l = z > 30.0 ? z : (z < -30.0 ? std::exp (z) : std::log1p (std::exp (z)));
        return vs / kp * l;
    }

    // courant de plaque (pentode, ecran a vs) et courant d'ecran
    inline void pentodeCurrents (const TubeModel& m, double vg, double vp, double vs, double& ia, double& ig2, double& dIaDvp)
    {
        vs = std::max (vs, 1.0);
        const double e1 = softplusE1 (vs, m.kp, 1.0 / m.mu + vg / vs);
        const double ep = e1 > 0.0 ? std::pow (e1, m.ex) : 0.0;
        const double i0 = ep / m.kg1;
        const double vpp = std::max (vp, 0.5);
        ia = i0 * std::atan (vpp / m.kvb);
        dIaDvp = i0 * m.kvb / (m.kvb * m.kvb + vpp * vpp);
        // ecran : le courant d'ecran grimpe quand la plaque descend sous le coude (partage du courant cathodique)
        const double knee = 1.0 + 1.6 * (1.5707963 - std::atan (vpp / m.kvb));
        ig2 = ep / m.kg2 * knee;
    }

    // courant cathodique d'une pentode montee en triode (ecran relie a la plaque)
    inline double triodeCurrent (const TubeModel& m, double vg, double vp)
    {
        vp = std::max (vp, 0.5);
        const double e1 = softplusE1 (vp, m.kp, 1.0 / m.mu + vg / vp);
        const double ep = e1 > 0.0 ? std::pow (e1, m.ex) : 0.0;
        return ep * (std::atan (vp / m.kvb) / m.kg1 + 1.0 / m.kg2);
    }

    // triode de preampli (12AX7, Koren)
    inline double koren12AX7 (double vg, double vp)
    {
        const double mu = 100.0, ex = 1.4, kg1 = 1060.0, kp = 600.0, kvb = 300.0;
        vp = std::max (vp, 0.5);
        const double e1 = vp / kp * [&] { const double z = kp * (1.0 / mu + vg / std::sqrt (kvb + vp * vp));
                                           return z > 30.0 ? z : std::log1p (std::exp (z)); }();
        return e1 > 0.0 ? std::pow (e1, ex) / kg1 : 0.0;
    }


    // ---------------------------------------------------------------------------------
    // Versions rapides (tables precalculees) pour le thread audio
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
        static constexpr int N = 8192;
        static constexpr double rMin = -0.6, rMax = 0.6;
        double s[N + 2] {};
        double invKg1 = 0, invKg2 = 0, kvb = 1, ex = 1.35;
        void build (const TubeModel& m)
        {
            ex = m.ex; kvb = m.kvb; invKg1 = 1.0 / m.kg1; invKg2 = 1.0 / m.kg2;
            for (int i = 0; i <= N + 1; ++i)
            {
                const double r = rMin + (rMax - rMin) * i / N;
                const double sp = softplusE1 (1.0, m.kp, 1.0 / m.mu + r);
                s[i] = sp > 0.0 ? std::pow (sp, m.ex) : 0.0;
            }
        }
        inline double S (double r) const
        {
            double p = (r - rMin) * (N / (rMax - rMin));
            if (p <= 0.0) return s[0] * std::max (0.0, 1.0 + p * 0.05);
            if (p >= N) return s[N] + (p - N) * (s[N] - s[N - 1]);
            const int i = (int) p; const double f = p - i;
            return s[i] + f * (s[i + 1] - s[i]);
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
        Tables() { for (int t = 0; t < numTubes; ++t) tube[t].build (tubeModel (t)); }
        static const Tables& get() { static const Tables* tb = new Tables(); return *tb; }   // construit une fois (hors thread audio)
    };

    // ---------------------------------------------------------------------------------
    // Points de fonctionnement (d'apres des amplis reels)
    // ---------------------------------------------------------------------------------
    struct Config
    {
        bool   pushPull = true, pentode = true, cathodeBias = false;
        double bPlus = 450.0;        // B+ a vide (V)
        double rLoad = 3400.0;       // PP : Raa plaque a plaque ; SE : charge reflechie
        double iq = 0.036;           // courant de repos vise par lampe (A)
        double rSupply = 60.0;       // resistance interne de l'alimentation (redresseur + transfo)
        double cFilter = 50e-6;      // condensateur de filtrage
        double rScreen = 1000.0;     // resistance d'ecran
        double cCouple = 0.022e-6, rGrid = 220e3, rSource = 38e3;   // liaison dephaseur -> grille
        double cCathode = 100e-6;    // decouplage de la cathode (polarisation automatique)
        double headroom = 1.0;       // > 1 : sature plus tot
        double nfb = 1.8;            // gain de boucle de la contre-reaction (au repos)
        double otLow = 35.0, otHigh = 14000.0, otQ = 0.75;   // transformateur de sortie
        double coreSat = 0.10;       // saturation du noyau dans le grave
        double spkBump = 0.0, spkTop = 0.0;                  // interaction haut-parleur (sans contre-reaction)
        // calcules
        double vBias = -40.0, rK = 0.0, gain = 1.0, inScale = 1.0, levelTrim = 1.0;
    };

    // tube 0..3, pp, pentode
    inline Config makeConfig (int tube, bool pp, bool pent)
    {
        Config c;
        c.pushPull = pp; c.pentode = pent;
        if (pp)
        {
            switch (tube)
            {
                case 0: c.bPlus = 460; c.rLoad = 4000; c.iq = 0.040; c.rSupply = 55;  c.cFilter = 70e-6;  c.rScreen = 470;
                        c.cCouple = 0.1e-6;  c.headroom = 0.95; c.otLow = 30; c.otHigh = 15000; break;   // 6L6 : Bassman / Twin
                case 1: c.bPlus = 455; c.rLoad = 3400; c.iq = 0.036; c.rSupply = 60;  c.cFilter = 50e-6;  c.rScreen = 1000;
                        c.cCouple = 0.022e-6; c.headroom = 1.15; c.otLow = 38; c.otHigh = 13500; break;  // EL34 : Plexi / JTM
                case 2: c.bPlus = 335; c.rLoad = 8000; c.iq = 0.036; c.rSupply = 190; c.cFilter = 32e-6;  c.rScreen = 470;
                        c.cCouple = 0.047e-6; c.headroom = 1.55; c.otLow = 45; c.otHigh = 12500; c.cathodeBias = true;
                        c.cCathode = 50e-6; break;                                                             // EL84 : AC15/AC30, GZ34
                default: c.bPlus = 520; c.rLoad = 4500; c.iq = 0.045; c.rSupply = 45; c.cFilter = 100e-6; c.rScreen = 1000;
                        c.cCouple = 0.1e-6;  c.headroom = 0.72; c.otLow = 25; c.otHigh = 16000; break;   // KT88 : SVT / hi-fi
            }
            c.nfb = pent ? 1.8 : 1.25;
            c.coreSat = 0.08;
        }
        else
        {
            c.cathodeBias = true; c.cCathode = 47e-6;
            switch (tube)
            {
                case 0: c.bPlus = 350; c.rLoad = 2500; c.iq = 0.060; c.rSupply = 120; c.cFilter = 40e-6; c.rScreen = 470;  c.headroom = 1.0;  break;
                case 1: c.bPlus = 350; c.rLoad = 2700; c.iq = 0.066; c.rSupply = 120; c.cFilter = 40e-6; c.rScreen = 1000; c.headroom = 1.15; break;
                case 2: c.bPlus = 285; c.rLoad = 5200; c.iq = 0.044; c.rSupply = 200; c.cFilter = 22e-6; c.rScreen = 470;  c.headroom = 1.45; break;
                default: c.bPlus = 385; c.rLoad = 2000; c.iq = 0.082; c.rSupply = 90; c.cFilter = 60e-6; c.rScreen = 1000; c.headroom = 0.8; break;
            }
            c.cCouple = 0.022e-6; c.rGrid = 470e3; c.rSource = 30e3;
            c.nfb = pent ? 1.0 : 0.75;
            c.otLow = 70; c.otHigh = 11000; c.otQ = 0.8; c.coreSat = 0.22;
        }
        // haut-parleur : bosse de resonance et montee inductive, fortes en pentode (impedance de sortie elevee)
        c.spkBump = pent ? 0.55 : 0.18;
        c.spkTop  = pent ? 0.45 : 0.12;

        // polarisation : courant de repos vise en pentode (meme polarisation reutilisee en triode, comme sur un ampli reel)
        const auto& m = tubeModel (tube);
        const double vs = c.bPlus - 0.02 * c.bPlus;
        auto iaAt = [&] (double vg) { double ia, ig2, d; pentodeCurrents (m, vg, c.bPlus, vs, ia, ig2, d); return ia + ig2; };
        double lo = -150.0, hi = 0.0;
        for (int i = 0; i < 80; ++i) { const double mid = 0.5 * (lo + hi); (iaAt (mid) > c.iq ? hi : lo) = mid; }
        const double vgk = 0.5 * (lo + hi);
        if (c.cathodeBias)
        {
            // polarisation automatique : Vk = Rk x Ik (resistance commune aux deux lampes en PP)
            const double ik = iaAt (vgk) * (pp ? 2.0 : 1.0);
            c.rK = -vgk / ik;
            c.vBias = 0.0;
        }
        else
            c.vBias = vgk;
        return c;
    }

    // ---------------------------------------------------------------------------------
    // Filtres elementaires (double precision)
    // ---------------------------------------------------------------------------------
    struct OnePole
    {
        double a = 0.0, z = 0.0;
        void setLP (double fc, double fs) { a = 1.0 - std::exp (-2.0 * 3.14159265358979 * fc / fs); }
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
        { const double w = 2 * 3.14159265358979 * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
          set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
        void highpass (double f, double q, double fs)
        { const double w = 2 * 3.14159265358979 * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
          set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
        void peak (double f, double q, double dB, double fs)
        { const double A = std::pow (10.0, dB / 40.0), w = 2 * 3.14159265358979 * f / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
          set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A); }
        void highShelf (double f, double dB, double fs)
        { const double A = std::pow (10.0, dB / 40.0), w = 2 * 3.14159265358979 * f / fs, c = std::cos (w), s = std::sin (w);
          const double al = s / 2 * std::sqrt (2.0), sq = 2 * std::sqrt (A) * al;
          set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
               (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq); }
    };

    // ---------------------------------------------------------------------------------
    // Etage de puissance complet (un canal, une configuration)
    // ---------------------------------------------------------------------------------
    class PowerStage
    {
    public:
        void setup (int tubeIdx, bool pp, bool pent, double sampleRate)
        {
            tube = tubeIdx; fs = sampleRate; dt = 1.0 / fs;
            cfg = makeConfig (tubeIdx, pp, pent);
            m = &tubeModel (tubeIdx);
            fast = &Tables::get().tube[tubeIdx];
            powTab = &Tables::get().pow135;
            if (! pp) buildSEDriver();
            calibrate();
            buildFilters();
            reset();
        }

        void reset()
        {
            bSag = quiescentB; vk = quiescentVk; vc[0] = vc[1] = 0.0;
            d = 0.0; iSE = cfg.iq; iRef = cfg.iq; ig2Prev[0] = ig2Prev[1] = qIg2;
            fbPrev = 0.0; drvLP[0] = drvLP[1] = 0.0;
            depthLP.z = 0; contourHP.z = 0; plateLP.z = 0; flux.z = 0;
            otHP1.reset(); otLP.reset(); spkPeak.reset(); spkShelf.reset(); dcBlock.z = 0;
            iAvg.z = 0; lastCurrent = 0.0;
        }

        // laisse l'etage se stabiliser sans signal (alimentation, cathode, liaisons) : etat de repos exact
        void settle (double seconds) { for (int i = 0, n = (int) (seconds * fs); i < n; ++i) process (0.0); }

        // depth / contour 0..1
        void setControls (double depth01, double contour01) { depth = depth01; contour = contour01; }

        // x : volts a l'entree (sortie du potard de volume) ; renvoie la sortie normalisee (gain petit signal = 1)
        inline double process (double x)
        {
            // contre-reaction globale (retard d'un echantillon au taux sur-echantillonne)
            const double y = fbPrev;
            const double bassPart  = depthLP.lp (y);
            const double topPart   = contourHP.hp (y);
            const double fbSig = cfg.nfb * (y - depth * bassPart - contour * topPart);
            const double u = (x - fbSig) * cfg.inScale;

            double out;
            if (cfg.pushPull)
            {
                // dephaseur a paire differentielle : transfert en tanh, sorties legerement desequilibrees
                const double v  = piSat * std::tanh (piGain * u / piSat);
                double d1 =  v;
                double d2 = -v * 0.94;
                d1 = drvLP[0] += drvA * (d1 - drvLP[0]);   // effet Miller / resistance de grille
                d2 = drvLP[1] += drvA * (d2 - drvLP[1]);
                const double g1 = gridNode (0, d1);
                const double g2 = gridNode (1, d2);
                prepareScreens();
                out = solvePP (g1 - vk, g2 - vk);
            }
            else
            {
                const double drv = seDriver (u);
                const double d1 = drvLP[0] += drvA * (drv - drvLP[0]);
                const double g1 = gridNode (0, d1);
                prepareScreens();
                out = solveSE (g1 - vk);
            }

            // alimentation : sag du B+
            const double iTot = lastCurrent + 0.004;
            bSag += dt / cfg.cFilter * ((cfg.bPlus - bSag) / cfg.rSupply - iTot);
            bSag = std::clamp (bSag, 0.4 * cfg.bPlus, 1.05 * cfg.bPlus);
            // polarisation automatique : la cathode suit le courant moyen (compression lente)
            if (cfg.cathodeBias)
                vk += dt / cfg.cCathode * (lastCurrent - vk / cfg.rK);

            // tension plaque -> normalisation, contre-reaction prise avant les fuites HF (stabilite)
            const double yN = plateLP.lp (out * invGain);
            fbPrev = yN;

            // transformateur de sortie
            double o = otHP1.process (yN);
            const double phi = flux.lp (o) * fluxScale;                // flux ~ integrale de la tension (grave)
            o -= cfg.coreSat * phi * phi * phi / (1.0 + std::fabs (phi * phi));   // saturation du noyau
            o = otLP.process (o);
            // haut-parleur (impedance qui monte a la resonance et dans l'aigu), reduit par la contre-reaction
            o = spkPeak.process (o);
            o = spkShelf.process (o);
            o = dcBlock.hp (o);

            iAvg.lp (lastCurrent);
            return o * (1.0 + cfg.nfb) * cfg.levelTrim;
        }

        // niveau de sortie en saturation franche (sinus 200 Hz, 3 V) : sert a egaliser les configurations
        double saturatedRms() const
        {
            PowerStage t (*this);
            t.setControls (0.5, 0.5);
            const int n = (int) (0.2 * fs);
            double acc = 0.0; int cnt = 0;
            for (int i = 0; i < n; ++i)
            {
                const double y = t.process (3.0 * std::sin (2 * 3.14159265358979 * 200.0 * i / fs));
                if (i > n / 2) { acc += y * y; ++cnt; }
            }
            return std::sqrt (acc / std::max (1, cnt));
        }
        void scaleLevel (double k) { cfg.levelTrim *= k; }

        double currentNow() const { return iAvg.z; }       // courant moyen (lueur des lampes)
        double quiescentCurrent() const { return qCurrent; }
        const Config& config() const { return cfg; }

    private:
        // noeud de grille : condensateur de liaison, resistance de grille, conduction de grille
        inline double gridNode (int k, double vd)
        {
            const double Vb = cfg.vBias;
            const double w = vd - vc[k];
            const double rs = cfg.rSource, rg = cfg.rGrid;
            // vg + rs * ((vg - Vb)/rg + ig(vg - vk)) = w + Vb (le noeud est vu depuis la masse)
            double vg = Vb + w * rg / (rg + rs);
            if (vg - vk > 0.0)                    // conduction de grille : Newton (sinon la solution lineaire est exacte)
            for (int it = 0; it < 3; ++it)
            {
                const double vgk = vg - vk;
                double ig = 0.0, dig = 0.0;
                if (vgk > 0.0) { const double s = std::sqrt (vgk); ig = gridK * vgk * s; dig = 1.5 * gridK * s; }
                const double f  = vg + rs * ((vg - Vb) / rg + ig) - (w + Vb);
                const double fp = 1.0 + rs * (1.0 / rg + dig);
                vg -= f / fp;
            }
            const double vgk = vg - vk;
            const double ig = vgk > 0.0 ? gridK * vgk * std::sqrt (vgk) : 0.0;
            const double i = (vg - Vb) / rg + ig;
            vc[k] += dt * i / cfg.cCouple;        // le condensateur se charge quand la grille conduit -> decalage de polarisation
            return vg;
        }

        inline void currents (double vg, double vp, int k, double& i, double& di)
        {
            const auto& ft = *fast;
            const double vpp = std::max (vp, 0.5);
            if (cfg.pentode)
            {
                const double ep = vsEx[k] * ft.S (vg * invVs[k]);
                const double i0 = ep * ft.invKg1;
                const double q = vpp / ft.kvb;
                const double at = fastAtanPos (q);
                i = i0 * at;
                di = i0 / (ft.kvb * (1.0 + q * q));
                screenNow[k] = ep * ft.invKg2 * (1.0 + 1.6 * (1.5707963267948966 - at));
            }
            else
            {
                auto cur = [&] (double v) { return powTab->operator() (v) * ft.S (vg / v) * (fastAtanPos (v / ft.kvb) * ft.invKg1 + ft.invKg2); };
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
                const double vs = std::max (bSag - cfg.rScreen * ig2Prev[k], 10.0);
                vsEx[k] = powTab->operator() (vs);
                invVs[k] = 1.0 / vs;
            }
        }

        // PP : d = Ia1 - Ia2, Vp1 = B - d Rq, Vp2 = B + d Rq
        inline double solvePP (double vg1, double vg2)
        {
            const double rq = cfg.rLoad * 0.25;
            double i1 = 0, i2 = 0, di1 = 0, di2 = 0;
            for (int it = 0; it < 4; ++it)
            {
                const double vp1 = bSag - d * rq, vp2 = bSag + d * rq;
                currents (vg1, vp1, 0, i1, di1);
                currents (vg2, vp2, 1, i2, di2);
                const double f = d - (i1 - i2);
                const double fp = 1.0 + rq * (di1 + di2);
                const double step = f / fp;
                d -= step;
                if (std::fabs (step) < 2e-6) break;
            }
            lastCurrent = i1 + i2 + screenNow[0] + screenNow[1];
            ig2Prev[0] = screenNow[0]; ig2Prev[1] = screenNow[1];
            return -d * rq;          // tension sur un demi-primaire
        }

        // SE : Vp = B - Rl (Ia - Iref), Iref = courant moyen (le primaire laisse passer le continu)
        inline double solveSE (double vg)
        {
            const double rl = cfg.rLoad;
            double ia = 0, dia = 0;
            for (int it = 0; it < 4; ++it)
            {
                const double vp = bSag - rl * (iSE - iRef);
                currents (vg, vp, 0, ia, dia);
                const double f = iSE - ia;
                const double fp = 1.0 + rl * dia;
                const double step = f / fp;
                iSE -= step;
                if (std::fabs (step) < 1e-6) break;
            }
            iRef += refA * (iSE - iRef);
            lastCurrent = iSE + screenNow[0];
            ig2Prev[0] = screenNow[0];
            return -(iSE - iRef) * rl;
        }

        // etage 12AX7 (SE) precalcule
        void buildSEDriver()
        {
            const double B = 300.0, Rl = 100e3, Rk = 1500.0;
            // repos : Vk = Rk Ia
            double vkq = 1.0;
            for (int i = 0; i < 60; ++i)
            {
                double ia = 0.001;
                for (int j = 0; j < 40; ++j) { const double vp = B - Rl * ia; ia += 0.5 * (koren12AX7 (-vkq, vp) - ia); }
                vkq = 0.5 * vkq + 0.5 * Rk * ia;
            }
            auto plateFor = [&] (double vin)
            {
                // grille limitee par la conduction (resistance de source 68k)
                double vg = vin;
                if (vg - vkq > 0.0) vg = vkq + (vg - vkq) / (1.0 + (vg - vkq) * 0.9);
                double lo = 0.0, hi = B;
                for (int j = 0; j < 60; ++j)
                {
                    const double vp = 0.5 * (lo + hi);
                    const double ia = koren12AX7 (vg - vkq, vp);
                    if (B - Rl * ia > vp) lo = vp; else hi = vp;
                }
                return 0.5 * (lo + hi);
            };
            const double vq = plateFor (0.0);
            for (int i = 0; i < seN; ++i)
            {
                const double vin = seMin + (seMax - seMin) * i / (seN - 1);
                seTable[i] = vq - plateFor (vin);       // etage inverseur -> signe repris pour garder la phase
            }
        }

        inline double seDriver (double u)
        {
            const double pos = (u - seMin) / (seMax - seMin) * (seN - 1);
            if (pos <= 0.0) return seTable[0];
            if (pos >= seN - 1) return seTable[seN - 1];
            const int i = (int) pos; const double f = pos - i;
            return seTable[i] + f * (seTable[i + 1] - seTable[i]);
        }

        // gain petit signal (sans contre-reaction) et echelle d'entree
        void calibrate()
        {
            const auto& c0 = cfg;
            gridK = 3.5e-4;
            drvA = 1.0 - std::exp (-2.0 * 3.14159265358979 * 22000.0 / fs);
            refA = 1.0 - std::exp (-2.0 * 3.14159265358979 * 12.0 / fs);
            // gain du driver
            double drvGain;
            if (c0.pushPull) drvGain = piGain;
            else drvGain = (seDriver (0.01) - seDriver (-0.01)) / 0.02;
            // l'entree est mise a l'echelle pour que la grille atteigne 0 V (attaque maxi) pour ~0,6 V
            // a Volume = 5, plus tot pour les lampes a faible marge (EL84), plus tard pour la KT88
            const double vClip = c0.cathodeBias ? std::fabs (biasMagnitudeCathode()) : std::fabs (c0.vBias);
            cfg.inScale = vClip / (drvGain * 0.6) * c0.headroom * (1.0 + c0.nfb);

            // repos et gain statique : on fait tourner l'etage sans signal puis avec un petit signal
            invGain = 1.0; cfg.levelTrim = 1.0;
            const double nfbSave = cfg.nfb; cfg.nfb = 0.0;
            quiescentB = cfg.bPlus; quiescentVk = cfg.cathodeBias ? -biasMagnitudeCathode() : 0.0;
            buildFilters();
            reset();
            for (int i = 0; i < (int) (0.35 * fs); ++i) process (0.0);
            quiescentB = bSag; quiescentVk = vk; qCurrent = iAvg.z; qIg2 = ig2Prev[0];
            // gain : sinus de 1 kHz de petite amplitude
            double num = 0.0, den = 0.0;
            const int n = (int) (0.05 * fs);
            const double amp = 0.002;
            for (int i = 0; i < n; ++i)
            {
                const double s = amp * std::sin (2 * 3.14159265358979 * 1000.0 * i / fs);
                const double o = fbPrevRaw (s);
                if (i > n / 2) { num += o * s; den += s * s; }
            }
            const double g = num / std::max (den, 1e-30);
            invGain = std::fabs (g) > 1e-9 ? 1.0 / g : 1.0;
            cfg.nfb = nfbSave;
            // le mode triode est un peu moins fort, comme sur un ampli reel
            cfg.levelTrim = cfg.pentode ? 1.0 : 0.84;
        }

        double fbPrevRaw (double x)
        {
            // un pas sans normalisation pour mesurer le gain (filtres de sortie ignores)
            process (x);
            return fbPrev;     // = plateLP(out * invGain) avec invGain = 1
        }

        double biasMagnitudeCathode() const
        {
            // tension grille-cathode du repos (Vk = Rk Ik)
            const auto& mm = *m;
            const double vs = cfg.bPlus * 0.98;
            double lo = -150.0, hi = 0.0;
            for (int i = 0; i < 80; ++i)
            {
                const double mid = 0.5 * (lo + hi);
                double ia, ig2, dd; pentodeCurrents (mm, mid, cfg.bPlus, vs, ia, ig2, dd);
                ((ia + ig2) > cfg.iq ? hi : lo) = mid;
            }
            return 0.5 * (lo + hi);
        }

        void buildFilters()
        {
            depthLP.setLP (170.0, fs);
            contourHP.setLP (2300.0, fs);
            plateLP.setLP (38000.0, fs);
            otHP1.highpass (cfg.otLow, cfg.pushPull ? 0.6 : 0.75, fs);
            otLP.lowpass (cfg.otHigh, cfg.otQ, fs);
            flux.setLP (9.0, fs);
            // flux normalise : pleine puissance a 40 Hz (PP) / 70 Hz (SE) -> ~1
            const double fRef = cfg.pushPull ? 40.0 : 70.0;
            fluxScale = (2 * 3.14159265358979 * fRef / 9.0) / 1.2;
            const double zOut = 1.0 / (1.0 + cfg.nfb * 0.8);
            spkPeak.peak (105.0, 1.6, 7.0 * cfg.spkBump * zOut, fs);
            spkShelf.highShelf (3200.0, 6.0 * cfg.spkTop * zOut, fs);
            dcBlock.setLP (6.0, fs);
            iAvg.setLP (4.0, fs);
        }

        Config cfg;
        const TubeModel* m = nullptr;
        const FastTube* fast = nullptr;
        const PowTable* powTab = nullptr;
        double vsEx[2] {}, invVs[2] {};
        int tube = 1;
        double fs = 192000.0, dt = 1.0 / 192000.0;
        double depth = 0.5, contour = 0.5;
        double invGain = 1.0, gridK = 3.5e-4, drvA = 0.5, refA = 0.001, fluxScale = 1.0;
        double bSag = 450.0, vk = 0.0, vc[2] {}, d = 0.0, iSE = 0.0, iRef = 0.0, ig2Prev[2] {}, screenNow[2] {};
        double lastCurrent = 0.0, qCurrent = 0.0, qIg2 = 0.0, quiescentB = 450.0, quiescentVk = 0.0;
        double fbPrev = 0.0, drvLP[2] {};
        static constexpr double piGain = 24.0, piSat = 88.0;
        static constexpr int seN = 2048;
        static constexpr double seMin = -12.0, seMax = 12.0;
        double seTable[seN] {};
        OnePole depthLP, contourHP, plateLP, flux, dcBlock, iAvg;
        Biquad otHP1, otLP, spkPeak, spkShelf;
    };
}
