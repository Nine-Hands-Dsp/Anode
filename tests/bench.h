// banc de mesure commun (ancien et nouveau moteur)
#include <cstdio>
#include <vector>
#include <complex>
#include <cmath>
#include <chrono>
#include <functional>
static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2 * M_PI / len; const std::complex<double> wl (cos (ang), sin (ang));
        for (size_t i = 0; i < n; i += len) { std::complex<double> w (1); for (size_t j = 0; j < len / 2; ++j) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } }
    }
}
// spectre (puissance) fenetre Blackman-Harris
static std::vector<double> spectrum (const std::vector<double>& y)
{
    const size_t n = y.size(); std::vector<std::complex<double>> a (n);
    for (size_t i = 0; i < n; ++i) { const double t = 2 * M_PI * i / (n - 1);
        const double w = 0.35875 - 0.48829 * cos (t) + 0.14128 * cos (2 * t) - 0.01168 * cos (3 * t); a[i] = y[i] * w; }
    fft (a); std::vector<double> p (n / 2); for (size_t i = 0; i < n / 2; ++i) p[i] = std::norm (a[i]); return p;
}
// repliement : energie non harmonique sous 20 kHz / energie totale sous 20 kHz (dB)
static double aliasDb (const std::vector<double>& y, double fs, double f0)
{
    auto p = spectrum (y); const double bin = fs / (2.0 * p.size()) * 1.0; (void) bin;
    const double df = fs / (double) y.size();
    double tot = 0, bad = 0;
    for (size_t k = 2; k < p.size(); ++k)
    {
        const double f = k * df; if (f > 20000) break;
        tot += p[k];
        const double h = f / f0; const double dist = std::fabs (h - std::round (h)) * f0;
        if (dist > 6 * df) bad += p[k];
    }
    return 10 * log10 (bad / tot + 1e-30);
}
static std::vector<double> harmonicsDb (const std::vector<double>& y, double fs, double f0, int nh)
{
    std::vector<double> r; double h1 = 0;
    for (int k = 1; k <= nh; ++k)
    {
        std::complex<double> s = 0; for (size_t i = 0; i < y.size(); ++i) s += y[i] * std::polar (1.0, -2 * M_PI * f0 * k * i / fs);
        const double m = std::abs (s); if (k == 1) h1 = m; else r.push_back (20 * log10 (m / h1 + 1e-12));
    }
    r.insert (r.begin(), 20 * log10 (h1 * 2 / y.size()));
    return r;
}
