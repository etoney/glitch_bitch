// Offline test for the Glitch Bitch engine: behaviour checks + demo renders (no JUCE).
//   render_glitch [out_dir]
#include "GlitchEngine.h"
#include <cstdio>
#include <functional>
#include <string>

using namespace gb;
static const int SR = 48000, BLOCK = 256;
static const double BPM = 120.0;

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R)
{
    FILE* f = std::fopen (path.c_str(), "wb"); if (! f) return;
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 4, riff = 36 + dataBytes;
    const uint16_t fmt = 1, ch = 2, bits = 16, align = 4; const uint32_t rate = SR, bps = SR * 4, fmtLen = 16;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtLen, 4, 1, f); std::fwrite (&fmt, 2, 1, f); std::fwrite (&ch, 2, 1, f);
    std::fwrite (&rate, 4, 1, f); std::fwrite (&bps, 4, 1, f); std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i) { int16_t s[2] = { (int16_t) (clampf (L[i], -1, 1) * 32767), (int16_t) (clampf (R[i], -1, 1) * 32767) }; std::fwrite (s, 2, 2, f); }
    std::fclose (f);
}

// synthetic drum loop + bass note at 120 bpm
static std::vector<float> makeBeat (double seconds)
{
    std::vector<float> y ((size_t) (SR * seconds));
    uint32_t r = 1;
    auto noise = [&] { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return (float) (r >> 8) / 8388608.0f - 1.0f; };
    const double spb = 60.0 / BPM;
    for (size_t i = 0; i < y.size(); ++i)
    {
        const double t = (double) i / SR, beat = t / spb, inBeat = beat - std::floor (beat), in8 = beat * 2 - std::floor (beat * 2);
        const int b = (int) std::floor (beat) % 4;
        float s = 0;
        const double tk = inBeat * spb;
        if (b == 0 || b == 2) s += 0.7f * (float) std::sin (2 * kPi * (50 + 120 * std::exp (-tk * 30)) * tk) * (float) std::exp (-tk * 8);
        if (b == 1 || b == 3) s += 0.4f * noise() * (float) std::exp (-tk * 18) + 0.3f * (float) std::sin (2 * kPi * 190 * tk) * (float) std::exp (-tk * 20);
        s += 0.12f * noise() * (float) std::exp (-in8 * spb * 0.5 * 60);
        s += 0.15f * (float) std::sin (2 * kPi * 55.0 * t);
        y[i] = s * 0.8f;
    }
    return y;
}

struct Run { std::vector<float> L, R; std::vector<CaptureEvent> events; };

static Run run (const std::function<void (Params&)>& setup, double pressAt, double releaseAt, double seconds = 8.0)
{
    Engine e; e.prepare (SR);
    setup (e.p);
    Run out; out.L = makeBeat (seconds); out.R = out.L;
    for (size_t b = 0; b + BLOCK <= out.L.size(); b += BLOCK)
    {
        const double t = (double) b / SR;
        e.p.engage = t >= pressAt && t < releaseAt;
        e.setTransport (BPM, t * BPM / 60.0, true, 4, 4, (int64_t) b);
        e.process (&out.L[b], &out.R[b], BLOCK);
        CaptureEvent ev; while (e.capture.popEvent (ev)) out.events.push_back (ev);
    }
    return out;
}

static double rmsOf (const std::vector<float>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += v[i] * (double) v[i]; return std::sqrt (s / (double) (b - a)); }
static bool allFinite (const std::vector<float>& v) { for (auto x : v) if (! std::isfinite (x) || std::abs (x) > 4.0f) return false; return true; }
static double maxJump (const std::vector<float>& v, size_t a, size_t b) { double m = 0; for (size_t i = a + 1; i < b; ++i) m = std::max (m, (double) std::abs (v[i] - v[i - 1])); return m; }

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    bool ok = true;
    auto check = [&] (bool c, const std::string& what) { std::printf ("  %-64s %s\n", what.c_str(), c ? "ok" : "FAIL"); ok &= c; };
    const auto dry = makeBeat (8.0);
    const double dryJump = maxJump (dry, 0, dry.size());

    struct Case { const char* name; std::function<void (Params&)> setup; };
    std::vector<Case> cases = {
        { "stutter",    [] (Params& p) { p.stutterArm = true; } },
        { "stutter32",  [] (Params& p) { p.stutterArm = true; p.stutterSpeed = 7; } },
        { "repeat",     [] (Params& p) { p.stutterArm = false; p.repeatArm = true; p.repeatLength = 3; } },
        { "repeatdrop", [] (Params& p) { p.stutterArm = false; p.repeatArm = true; p.repeatLength = 5; p.repeatDrop = -2; } },
        { "reverse",    [] (Params& p) { p.stutterArm = false; p.reverseArm = true; p.reverseLength = 1; } },
        { "scatter",    [] (Params& p) { p.stutterArm = false; p.scatterArm = true; p.scatterChaos = 0.9f; } },
        { "crush",      [] (Params& p) { p.stutterArm = false; p.crushArm = true; p.crushBits = 4; p.crushRate = 3000; } },
        { "freeze",     [] (Params& p) { p.stutterArm = false; p.freezeArm = true; } },
        { "dive",       [] (Params& p) { p.stutterArm = false; p.diveArm = true; } },
        { "tapestop",   [] (Params& p) { p.stutterArm = false; p.tapeStopArm = true; } },
        { "tapestart",  [] (Params& p) { p.stutterArm = false; p.tapeStartArm = true; } },
        { "stopstart",  [] (Params& p) { p.stutterArm = false; p.tapeStopArm = true; p.tapeStartArm = true; } },
        { "everything", [] (Params& p) { p.repeatArm = p.reverseArm = p.scatterArm = p.crushArm = p.freezeArm = p.diveArm = true; p.tapeStopArm = p.tapeStartArm = true; } },
    };

    for (auto& c : cases)
    {
        std::printf ("%s\n", c.name);
        auto r = run (c.setup, 2.0, 4.0);
        writeWav (dir + "/gb_" + c.name + ".wav", r.L, r.R);
        check (allFinite (r.L) && allFinite (r.R), "finite and bounded");
        check (rmsOf (r.L, 0, SR * 2) > 0.05 && std::abs (rmsOf (r.L, 0, SR * 2) - rmsOf (dry, 0, SR * 2)) < 1e-6, "untouched before the press");
        // after release + 1.5 s the signal must be back to dry (tape start done etc.)
        double diff = 0; for (size_t i = SR * 6; i < SR * 8 - BLOCK; ++i) diff = std::max (diff, (double) std::abs (r.L[i] - dry[i]));
        check (diff < 1e-3, "back to dry after release (max diff " + std::to_string (diff) + ")");
        double d2 = 0; for (size_t i = SR * 2 + SR / 4; i < SR * 4; ++i) d2 = std::max (d2, (double) std::abs (r.L[i] - dry[i]));
        check (d2 > 0.05, "audibly changed while held");
        check (maxJump (r.L, 0, r.L.size() - BLOCK) < dryJump * 2.5 + 0.2, "no big clicks (max step " + std::to_string (maxJump (r.L, 0, r.L.size() - BLOCK)) + ")");
        // capture: one clip, starts 1/4 note (0.5 s) before the press, ends after the busy tail + 1/4
        check (r.events.size() == 2 && r.events[0].type == CaptureEvent::Start && r.events[1].type == CaptureEvent::End, "one capture clip");
        if (r.events.size() == 2)
        {
            const double s = (double) r.events[0].abs / SR, e = (double) r.events[1].abs / SR;
            check (std::abs (s - 1.5) < 0.01, "clip starts 1/4 before the press (" + std::to_string (s) + " s)");
            check (std::abs (r.events[0].ppq - 3.0) < 0.01 && r.events[0].timeline == (int64_t) (1.5 * SR), "clip start ppq/timeline");
            check (e > 4.49 && e < 6.0, "clip ends after release + post-roll (" + std::to_string (e) + " s)");
        }
    }

    // ---- Click check on a pure sine (everything except Crush, which is stepped on purpose)
    {
        std::printf ("clicks (sine)\n");
        for (auto& c : cases)
        {
            if (std::string (c.name) == "crush" || std::string (c.name) == "everything") continue;
            Engine e; e.prepare (SR); c.setup (e.p);
            std::vector<float> L ((size_t) SR * 8), R;
            for (size_t i = 0; i < L.size(); ++i) L[i] = 0.3f * std::sin (2.0f * kPi * 220.0f * (float) i / SR);
            R = L;
            for (size_t b = 0; b + BLOCK <= L.size(); b += BLOCK)
            {
                const double t = (double) b / SR;
                e.p.engage = t >= 2.0 && t < 4.0;
                e.setTransport (BPM, t * BPM / 60.0, true, 4, 4, (int64_t) b);
                e.process (&L[b], &R[b], BLOCK);
            }
            const double natural = 0.3 * 2 * kPi * 220.0 / SR;
            const double mj = maxJump (L, 0, L.size() - BLOCK);
            check (mj < natural * 4.0, std::string (c.name) + ": max step " + std::to_string (mj / natural) + "x natural");
        }
    }
    // ---- Snap: press at 2.1 s with 1/4 snap -> fires at beat 5 (2.5 s)
    {
        std::printf ("snap\n");
        auto r = run ([] (Params& p) { p.snap = 3; p.stutterArm = false; p.tapeStopArm = true; }, 2.1, 3.3);
        double d = 0; for (size_t i = SR * 2; i < (size_t) (SR * 2.49); ++i) d = std::max (d, (double) std::abs (r.L[i] - dry[i]));
        check (d < 1e-6, "nothing before the next 1/4");
        check (r.events.size() == 2 && std::abs ((double) r.events[0].abs / SR - 2.0) < 0.01, "clip starts on the grid (2.0 s = 1/4 before 2.5)");
    }
    // ---- Quick tap with snap still fires for one grid unit
    {
        std::printf ("tap\n");
        auto r = run ([] (Params& p) { p.snap = 3; p.stutterArm = false; p.crushArm = true; p.crushBits = 2; }, 2.1, 2.2);
        double d = 0; for (size_t i = (size_t) (SR * 2.55); i < (size_t) (SR * 2.95); ++i) d = std::max (d, (double) std::abs (r.L[i] - dry[i]));
        check (d > 0.05, "tap fires for one 1/4");
    }
    // ---- Capture ring contents match the output
    {
        std::printf ("ring\n");
        Engine e; e.prepare (SR); e.p.stutterArm = true;
        auto L = makeBeat (4.0); auto R = L; std::vector<float> out;
        for (size_t b = 0; b + BLOCK <= L.size(); b += BLOCK)
        {
            e.p.engage = b > SR;
            e.setTransport (BPM, (double) b / SR * 2.0, true, 4, 4, (int64_t) b);
            e.process (&L[b], &R[b], BLOCK);
        }
        bool same = true; const int64_t w = e.capture.written.load();
        for (int64_t k = 0; k < w; ++k) if (e.capture.l[(size_t) (k & e.capture.mask)] != L[(size_t) k]) { same = false; break; }
        check (same, "ring holds exactly the output");
    }

    std::printf (ok ? "\nALL OK\n" : "\nSOME CHECKS FAILED\n");
    return ok ? 0 : 1;
}
