// GLITCH WITCH — DSP engine (self-contained, no JUCE).
//
// One big button fires every ARMED effect at once:
//   Freeze -> Repeat -> Reverse -> Scatter -> Dive -> Crush -> Stutter -> [dry/wet] -> Tape Stop / Tape Start -> Output
// The output also runs into a capture ring: every time the button fires, a clip with pre-roll and post-roll
// is marked in the ring and an event is queued for a background thread to write it to disk.
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace gb
{
constexpr float kPi = 3.14159265358979f;

// Tempo-synced note lengths, in quarter-note beats
constexpr int kNumNotes = 10;
constexpr double kNoteBeats[kNumNotes] = { 2.0, 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0, 0.0625 };
inline const char* const* noteNames()
{
    static const char* const n[kNumNotes] = { "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T", "1/64" };
    return n;
}
// Snap and pre/post roll lengths. Negative = whole bars (uses the song's time signature).
constexpr int kNumSnaps = 6;
constexpr double kSnapBeats[kNumSnaps] = { 0.0, 0.25, 0.5, 1.0, 2.0, -1.0 };       // Off, 1/16, 1/8, 1/4, 1/2, 1 bar
constexpr int kNumRolls = 7;
constexpr double kRollBeats[kNumRolls] = { 0.0, 0.25, 0.5, 1.0, 2.0, -1.0, -2.0 }; // Off, 1/16, 1/8, 1/4, 1/2, 1 bar, 2 bars

inline float clampf (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); }

// Stereo circular buffer with fractional reads at absolute (wrapping) positions
struct StereoRing
{
    std::vector<float> l, r; int size = 4, w = 0;
    void prepare (int n) { size = std::max (4, n); l.assign ((size_t) size, 0.0f); r.assign ((size_t) size, 0.0f); w = 0; }
    void push (float a, float b) { l[(size_t) w] = a; r[(size_t) w] = b; if (++w >= size) w = 0; }
    int wrap (int i) const { i %= size; return i < 0 ? i + size : i; }
    void read (double pos, float& a, float& b) const
    {
        const double fl = std::floor (pos);
        const int i0 = wrap ((int) fl), i1 = wrap ((int) fl + 1);
        const float f = (float) (pos - fl);
        a = l[(size_t) i0] + (l[(size_t) i1] - l[(size_t) i0]) * f;
        b = r[(size_t) i0] + (r[(size_t) i1] - r[(size_t) i0]) * f;
    }
};

struct Params
{
    bool engage = false;     // THE BUTTON (automatable)
    int snap = 0;            // index into kSnapBeats
    float mix = 1.0f, outGain = 1.0f;

    bool stutterArm = true; int stutterSpeed = 5; int stutterSteps = 16;
    float stutterDepth = 1.0f, stutterSmooth = 3.0f; uint16_t stutterPattern = 0b1011011010110110;
    bool repeatArm = false; int repeatLength = 5; float repeatDrop = 0.0f;
    bool reverseArm = false; int reverseLength = 3;
    bool scatterArm = false; int scatterSlice = 5; float scatterChaos = 0.6f, scatterRange = 4.0f;
    bool crushArm = false; float crushBits = 6.0f, crushRate = 6000.0f;
    bool freezeArm = false; float freezeSize = 0.4f, freezeMix = 0.7f;
    bool diveArm = false; float diveAmount = -12.0f, diveTime = 1.0f;
    bool tapeStopArm = false; float tapeStopTime = 0.8f, tapeStopShape = 0.5f;
    bool tapeStartArm = false; float tapeStartTime = 0.8f, tapeStartShape = 0.5f;

    bool captureOn = true; int preRoll = 3, postRoll = 3; // indices into kRollBeats (default 1/4 each)
};

//==============================================================================
// Capture: the output is written into a big ring; clip boundaries go through a lock-free queue.
struct CaptureEvent
{
    enum Type { Start, End } type = Start;
    int64_t abs = 0;           // absolute sample index in the ring stream
    int64_t timeline = -1;     // host timeline sample of 'abs' (-1 if the transport was stopped)
    double ppq = 0, bpm = 120; // musical position of 'abs'
    int num = 4, den = 4;
    bool playing = false;
    bool truncated = false;    // End only: clip hit the length limit
};

struct CaptureRing
{
    std::vector<float> l, r;
    int64_t mask = 0;
    std::atomic<int64_t> written { 0 }; // samples published so far (audio thread writes, reader reads)

    void prepare (double sr)
    {
        int64_t n = 1; while ((double) n < sr * 32.0) n <<= 1; // ~32 s or more
        l.assign ((size_t) n, 0.0f); r.assign ((size_t) n, 0.0f); mask = n - 1;
        written.store (0);
        head.store (0); tail.store (0);
    }
    int64_t size() const { return mask + 1; }

    bool pushEvent (const CaptureEvent& e)
    {
        const int h = head.load (std::memory_order_relaxed), nh = (h + 1) & 63;
        if (nh == tail.load (std::memory_order_acquire)) return false;
        ev[(size_t) h] = e; head.store (nh, std::memory_order_release); return true;
    }
    bool popEvent (CaptureEvent& e)
    {
        const int t = tail.load (std::memory_order_relaxed);
        if (t == head.load (std::memory_order_acquire)) return false;
        e = ev[(size_t) t]; tail.store ((t + 1) & 63, std::memory_order_release); return true;
    }
    bool peekEvent (CaptureEvent& e) const
    {
        const int t = tail.load (std::memory_order_relaxed);
        if (t == head.load (std::memory_order_acquire)) return false;
        e = ev[(size_t) t]; return true;
    }

private:
    std::array<CaptureEvent, 64> ev;
    std::atomic<int> head { 0 }, tail { 0 };
};

//==============================================================================
class Engine
{
public:
    Params p;
    CaptureRing capture;

    // for the UI
    int currentStep = -1;
    bool active = false;   // the button's effects are firing
    bool busy = false;     // firing or still ringing out (tape spin-up, fades)
    bool pending = false;  // pressed, waiting for the snap grid
    bool capturing = false;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        history.prepare ((int) (sr * 1.2));
        repL.assign ((size_t) (sr * 4.2), 0.0f); repR.assign (repL.size(), 0.0f);
        revRing.prepare ((int) (sr * 8.5));
        scRing.prepare ((int) (sr * 12.5));
        dvRing.prepare ((int) (sr * 0.25));
        tape.prepare ((int) (sr * 8.0));
        frzL.assign ((size_t) sr, 0.0f); frzR.assign (frzL.size(), 0.0f);
        capture.prepare (sr);
        xfade = 1.0f - std::exp (-1.0f / (float) (sr * 0.006));
        slowXfade = 1.0f - std::exp (-1.0f / (float) (sr * 0.04));
        frzCoef = 1.0f - std::exp (-1.0f / (float) (sr * 0.06));
        reset();
    }

    void reset()
    {
        gate = 1.0f; repMix = revMix = scMix = dvMix = crMix = frzEnv = tapeMix = 0.0f;
        wasRepeat = wasFreeze = false; active = busy = pending = capturing = false; prevReq = false;
        tapeState = TapeIdle; diveSemis = 0.0f; freeBeat = 0.0; lastSnapIdx = INT64_MIN;
        for (auto& g : grains) g.active = false;
        sincePress = 0; postCount = 0; absPos = capture.written.load();
    }

    // Call once per block before process()
    void setTransport (double bpmIn, double ppqIn, bool playingIn, int numIn, int denIn, int64_t timeInSamplesIn)
    {
        bpm = bpmIn > 1.0 ? bpmIn : 120.0;
        num = std::max (1, numIn); den = std::max (1, denIn);
        if (playingIn && ppqIn > -1.0e9) { playing = true; ppq = ppqIn; timeline = timeInSamplesIn; }
        else { playing = false; ppq = freeBeat; timeline = -1; }
    }

    double beatsPerBar() const { return (double) num * 4.0 / (double) den; }
    double rollBeats (int idx) const
    {
        const double b = kRollBeats[std::clamp (idx, 0, kNumRolls - 1)];
        return b < 0 ? -b * beatsPerBar() : b;
    }
    double noteSamples (int idx) const { return kNoteBeats[std::clamp (idx, 0, kNumNotes - 1)] * 60.0 / bpm * sr; }

    void process (float* L, float* R, int n)
    {
        updateDive (n);
        const double beatPerSample = bpm / 60.0 / sr;
        double snapLen = kSnapBeats[std::clamp (p.snap, 0, kNumSnaps - 1)];
        if (snapLen < 0) snapLen = beatsPerBar();
        const bool useSnap = snapLen > 0.0 && playing;

        for (int i = 0; i < n; ++i)
        {
            const double beat = ppq + (double) i * beatPerSample;

            // ---------------- THE BUTTON (with optional snap to the song grid)
            const bool req = p.engage;
            if (req && ! prevReq) pending = true;
            prevReq = req;
            bool boundary = true;
            if (useSnap)
            {
                const int64_t idx = (int64_t) std::floor (beat / snapLen + 1e-9);
                boundary = idx != lastSnapIdx;
                lastSnapIdx = idx;
            }
            else lastSnapIdx = INT64_MIN;
            if (boundary)
            {
                if (! active && (pending || req)) { active = true; pending = false; onEngage(); }
                else if (active && ! req && ! pending) { active = false; onRelease(); }
                if (! req) pending = false;
            }
            if (! useSnap) { if (active && ! req && ! pending) { active = false; onRelease(); } }

            float a = L[i], b = R[i];
            const float da = a, db = b;
            history.push (a, b);

            processFreeze (a, b);
            processRepeat (a, b);
            processReverse (a, b);
            processScatter (a, b);
            processDive (a, b);
            processCrush (a, b);
            // stutter
            {
                const float g = stutterGain (beat);
                a *= g; b *= g;
            }
            // dry / wet
            const float m = clampf (p.mix, 0.0f, 1.0f);
            a = da + (a - da) * m; b = db + (b - db) * m;

            processTape (a, b);
            a *= p.outGain; b *= p.outGain;
            if (! std::isfinite (a) || ! std::isfinite (b)) { a = b = 0.0f; }
            L[i] = a; R[i] = b;

            busy = active || tapeState != TapeIdle || tapeMix > 1e-3f || repMix > 1e-3f || revMix > 1e-3f || scMix > 1e-3f
                || dvMix > 1e-3f || crMix > 1e-3f || frzEnv > 1e-3f || gate < 0.995f;

            // ---------------- capture
            const int64_t pos = absPos + i;
            capture.l[(size_t) (pos & capture.mask)] = a;
            capture.r[(size_t) (pos & capture.mask)] = b;
            updateCapture (pos, beat, i);
        }

        absPos += n;
        capture.written.store (absPos, std::memory_order_release);
        freeBeat = ppq + (double) n * beatPerSample;
        if (! playing) freeBeat = std::fmod (freeBeat, 4096.0);
    }

private:
    //==========================================================================
    void onEngage()
    {
        // fresh starts for the slice-based effects (they count from the moment of the press)
        repStarted = false; revK = 0; revLen = 0; scK = 0; scLen = 0;
        if (p.tapeStopArm) startTape (TapeStopping);
        else if (p.tapeStartArm) startTape (TapeStarting);
        sincePress = 0;
    }
    void onRelease()
    {
        if (p.tapeStopArm && p.tapeStartArm && tapeState == TapeStopping) startTape (TapeStarting); // spin back up on release
        else if (tapeState == TapeStopping) tapeState = TapeIdle;                                   // snap back
    }

    //==========================================================================
    float stutterGain (double beat)
    {
        const bool on = active && p.stutterArm;
        const double len = kNoteBeats[std::clamp (p.stutterSpeed, 0, kNumNotes - 1)];
        const int steps = std::clamp (p.stutterSteps, 1, 16);
        const int64_t idx = (int64_t) std::floor (beat / len + 1e-9);
        const int step = (int) (((idx % steps) + steps) % steps);
        currentStep = on ? step : -1;
        const bool cut = on && ((p.stutterPattern >> step) & 1u);
        const float target = cut ? 1.0f - clampf (p.stutterDepth, 0.0f, 1.0f) : 1.0f;
        const float c = 1.0f - std::exp (-1.0f / (float) (sr * std::max (0.3f, p.stutterSmooth) * 0.001));
        gate += (target - gate) * c;
        return gate;
    }

    //==========================================================================
    // Freeze: capture the last second on engage, then play drifting, detuned grains of it
    void processFreeze (float& a, float& b)
    {
        const bool on = active && p.freezeArm;
        if (on && ! wasFreeze) captureFreeze();
        wasFreeze = on;
        frzEnv += ((on ? 1.0f : 0.0f) - frzEnv) * frzCoef;
        if (frzEnv < 1e-4f) return;
        float fa = 0, fb = 0;
        renderGrains (fa, fb);
        const float m = clampf (p.freezeMix, 0.0f, 1.0f) * frzEnv;
        a = a * (1.0f - 0.7f * m) + fa * m;
        b = b * (1.0f - 0.7f * m) + fb * m;
    }

    void captureFreeze()
    {
        const int len = (int) frzL.size();
        for (int k = 0; k < len; ++k)
        {
            const int src = history.wrap (history.w - len + k);
            frzL[(size_t) k] = history.l[(size_t) src]; frzR[(size_t) k] = history.r[(size_t) src];
        }
        for (size_t g = 0; g < grains.size(); ++g) { grains[g].active = false; grains[g].phase = (float) g / (float) grains.size(); }
    }

    void renderGrains (float& outA, float& outB)
    {
        const float glen = (float) sr * (0.06f + 0.54f * clampf (p.freezeSize, 0.0f, 1.0f));
        const int capLen = (int) frzL.size();
        for (auto& g : grains)
        {
            if (! g.active || g.phase >= 1.0f)
            {
                g.active = true; g.len = glen;
                const float maxStart = std::max (1.0f, (float) capLen - glen * 1.1f - 2.0f);
                g.start = maxStart * (0.35f + 0.65f * rnd());
                g.rate = std::pow (2.0f, (rnd() * 2.0f - 1.0f) * 0.12f / 12.0f);
                g.pan = rnd() * 1.2f - 0.6f;
                if (g.phase >= 1.0f) g.phase -= 1.0f;
                g.pos = g.phase * g.len;
            }
            const float rp = g.start + g.pos * g.rate;
            const int i0 = std::clamp ((int) rp, 0, capLen - 2); const float f = rp - (float) i0;
            const float sa = frzL[(size_t) i0] + (frzL[(size_t) i0 + 1] - frzL[(size_t) i0]) * f;
            const float sb = frzR[(size_t) i0] + (frzR[(size_t) i0 + 1] - frzR[(size_t) i0]) * f;
            const float win = std::sin (kPi * g.phase); const float w2 = win * win;
            outA += sa * (1.0f - g.pan * 0.5f) * w2 * 0.8f;
            outB += sb * (1.0f + g.pan * 0.5f) * w2 * 0.8f;
            g.pos += 1.0f; g.phase = g.pos / g.len;
        }
    }

    //==========================================================================
    // Repeat: records one tempo-synced slice from the press (you hear it live the first time), then loops it,
    // optionally sinking or rising in pitch each pass.
    void processRepeat (float& a, float& b)
    {
        const bool on = active && p.repeatArm;
        if (on && ! wasRepeat)
        {
            repLen = std::clamp ((int) noteSamples (p.repeatLength), (int) (sr * 0.008), (int) repL.size() - 2);
            repRec = 0; repPos = 0.0f; repRate = 1.0f; repStarted = true;
        }
        wasRepeat = on;
        repMix += ((on ? 1.0f : 0.0f) - repMix) * xfade;
        if (repMix < 1e-4f || ! repStarted) return;
        float ra, rb;
        if (repRec < repLen)
        {
            if (on) { repL[(size_t) repRec] = a; repR[(size_t) repRec] = b; ++repRec; }
            ra = a; rb = b;
        }
        else
        {
            const int i0 = std::min ((int) repPos, repLen - 1); const float f = repPos - (float) i0;
            const int i1 = std::min (i0 + 1, repLen - 1);
            ra = repL[(size_t) i0] + (repL[(size_t) i1] - repL[(size_t) i0]) * f;
            rb = repR[(size_t) i0] + (repR[(size_t) i1] - repR[(size_t) i0]) * f;
            const float edge = std::max (1.0f, (float) sr * 0.002f);
            const float fade = std::min ({ 1.0f, (repPos + 1.0f) / edge, ((float) repLen - repPos) / edge });
            ra *= fade; rb *= fade;
            if (on)
            {
                repPos += repRate;
                if (repPos >= (float) repLen)
                {
                    repPos -= (float) repLen;
                    repRate = clampf (repRate * std::pow (2.0f, p.repeatDrop / 12.0f), 0.125f, 4.0f);
                    if (repPos >= (float) repLen) repPos = 0.0f;
                }
            }
        }
        a += (ra - a) * repMix; b += (rb - b) * repMix;
    }

    //==========================================================================
    // Reverse: every slice, the slice that just went by is played backwards.
    void processReverse (float& a, float& b)
    {
        revRing.push (a, b);
        const bool on = active && p.reverseArm;
        revMix += ((on ? 1.0f : 0.0f) - revMix) * xfade;
        if (revMix < 1e-4f) return;
        if (revLen <= 0 || revK >= revLen)
        {
            revLen = std::clamp ((int) noteSamples (p.reverseLength), (int) (sr * 0.01), revRing.size / 2 - 4);
            revBase = revRing.w - 1; revK = 0;
        }
        float ra, rb;
        revRing.read ((double) (revBase - revK), ra, rb);
        const float edge = std::max (1.0f, (float) sr * 0.003f);
        const float fade = std::min ({ 1.0f, ((float) revK + 1.0f) / edge, ((float) (revLen - revK)) / edge });
        ++revK;
        a += (ra * fade - a) * revMix; b += (rb * fade - b) * revMix;
    }

    //==========================================================================
    // Scatter: every slice, maybe jump to a random earlier slice (sometimes reversed or an octave off).
    void processScatter (float& a, float& b)
    {
        scRing.push (a, b);
        const bool on = active && p.scatterArm;
        scMix += ((on ? 1.0f : 0.0f) - scMix) * xfade;
        if (scMix < 1e-4f) return;
        if (scLen <= 0 || scK >= scLen)
        {
            scLen = std::clamp ((int) noteSamples (p.scatterSlice), (int) (sr * 0.01), scRing.size / 5);
            scK = 0;
            const float chaos = clampf (p.scatterChaos, 0.0f, 1.0f);
            const double rangeSamples = std::min ((double) scRing.size * 0.75, (double) p.scatterRange * 60.0 / bpm * sr);
            const int maxBack = std::max (1, (int) (rangeSamples / (double) scLen));
            scJump = rnd() < chaos;
            if (scJump)
            {
                const int back = 1 + (int) (rnd() * (float) maxBack) % maxBack;
                const int w0 = scRing.w;
                scRev = rnd() < 0.35f * chaos;
                const float r = rnd();
                scRate = 1.0f;
                if (! scRev && back >= 2 && r < 0.12f * chaos) scRate = 2.0f;
                else if (! scRev && r > 1.0f - 0.18f * chaos) scRate = 0.5f;
                scStart = (double) (w0 - back * scLen);
            }
        }
        float sa = a, sb = b;
        if (scJump)
        {
            const double posf = scRev ? scStart + (double) (scLen - 1 - scK) : scStart + (double) scK * scRate;
            scRing.read (posf, sa, sb);
            const float edge = std::max (1.0f, (float) sr * 0.003f);
            const float win = std::min ({ 1.0f, ((float) scK + 1.0f) / edge, ((float) (scLen - scK)) / edge });
            sa = a + (sa - a) * win; sb = b + (sb - b) * win;
        }
        ++scK;
        a += (sa - a) * scMix; b += (sb - b) * scMix;
    }

    //==========================================================================
    // Dive: pitch glides down (or up) while held, glides home on release. Two-tap delay-line shifter.
    void updateDive (int n)
    {
        const bool on = active && p.diveArm;
        const float target = on ? p.diveAmount : 0.0f;
        const float t = std::max (0.02f, on ? p.diveTime : p.diveTime * 0.35f);
        const float c = 1.0f - std::exp (-(float) n / (float) (sr * t * 0.35));
        diveSemis += (target - diveSemis) * c;
        if (std::abs (diveSemis) < 1e-3f && ! on) diveSemis = 0.0f;
        diveWanted = on || std::abs (diveSemis) > 0.02f;
    }
    void processDive (float& a, float& b)
    {
        dvRing.push (a, b);
        dvMix += ((diveWanted ? 1.0f : 0.0f) - dvMix) * xfade;
        if (dvMix < 1e-4f) { dvPhase = 0.0f; return; }
        const float ratio = std::pow (2.0f, diveSemis / 12.0f);
        const float W = (float) sr * 0.05f;
        dvPhase += (1.0f - ratio) / W;
        dvPhase -= std::floor (dvPhase);
        float out[2] = { 0, 0 };
        for (int t = 0; t < 2; ++t)
        {
            float ph = dvPhase + 0.5f * (float) t; ph -= std::floor (ph);
            const float d = 2.0f + ph * W;
            float x, y; dvRing.read ((double) dvRing.w - 1.0 - (double) d, x, y);
            const float g = std::sin (kPi * ph); const float g2 = g * g;
            out[0] += x * g2; out[1] += y * g2;
        }
        a += (out[0] - a) * dvMix; b += (out[1] - b) * dvMix;
    }

    //==========================================================================
    // Crush: bit depth + sample-rate reduction
    void processCrush (float& a, float& b)
    {
        const bool on = active && p.crushArm;
        crMix += ((on ? 1.0f : 0.0f) - crMix) * xfade;
        if (crMix < 1e-4f) return;
        crPhase += clampf (p.crushRate, 200.0f, (float) sr) / (float) sr;
        if (crPhase >= 1.0f)
        {
            crPhase -= std::floor (crPhase);
            const float q = std::pow (2.0f, clampf (p.crushBits, 1.0f, 16.0f) - 1.0f);
            crHoldA = std::round (clampf (a, -1.0f, 1.0f) * q) / q;
            crHoldB = std::round (clampf (b, -1.0f, 1.0f) * q) / q;
        }
        a += (crHoldA - a) * crMix; b += (crHoldB - b) * crMix;
    }

    //==========================================================================
    // Tape Stop / Tape Start (after the dry/wet blend, so the whole track grinds to a halt)
    enum TapeState { TapeIdle, TapeStopping, TapeStarting };
    void startTape (TapeState s) { tapeState = s; tapeRead = (double) tape.w - 1.0; tapeProg = 0.0f; tapeRate = s == TapeStopping ? 1.0f : 0.0f; }

    void processTape (float& a, float& b)
    {
        tape.push (a, b);
        const bool want = tapeState != TapeIdle;
        tapeMix += ((want ? 1.0f : 0.0f) - tapeMix) * (want ? xfade : (tapeFromStart ? slowXfade : xfade));
        if (tapeMix < 1e-4f) { tapeFromStart = false; return; }
        if (tapeState == TapeStopping)
        {
            tapeProg = std::min (1.0f, tapeProg + 1.0f / (std::max (0.05f, p.tapeStopTime) * (float) sr));
            tapeRate = std::pow (1.0f - tapeProg, shapeExp (p.tapeStopShape));
        }
        else if (tapeState == TapeStarting)
        {
            tapeProg = std::min (1.0f, tapeProg + 1.0f / (std::max (0.05f, p.tapeStartTime) * (float) sr));
            tapeRate = std::pow (tapeProg, shapeExp (p.tapeStartShape));
            if (tapeProg >= 1.0f) { tapeState = TapeIdle; tapeFromStart = true; }
        }
        float ta = 0, tb = 0;
        // keep the read head from running ahead of the tape or falling off its end
        tapeRead = std::clamp (tapeRead, (double) tape.w - (double) tape.size + 8.0, (double) tape.w - 1.0);
        tape.read (tapeRead, ta, tb);
        const float level = std::min (1.0f, tapeRate * 6.0f);
        ta *= level; tb *= level;
        a += (ta - a) * tapeMix; b += (tb - b) * tapeMix;
        if (tapeState != TapeIdle) tapeRead += tapeRate;
        else tapeRead += 1.0;
        // keep tapeRead in a sane numeric range relative to the write head
        if (tapeRead > 1.0e9) tapeRead -= (double) tape.size * std::floor (1.0e9 / (double) tape.size);
    }
    static float shapeExp (float shape) { return std::exp2 ((clampf (shape, 0.0f, 1.0f) - 0.5f) * 3.0f); } // 0.35 .. 2.8

    //==========================================================================
    void updateCapture (int64_t pos, double beat, int i)
    {
        const double samplesPerBeat = 60.0 / bpm * sr;
        if (! capturing)
        {
            if (p.captureOn && active && sincePress == 0)
            {
                const int64_t oldest = std::max<int64_t> (0, capture.written.load (std::memory_order_relaxed) - capture.size() + (int64_t) (sr * 2.0));
                const int64_t pre = (int64_t) std::llround (rollBeats (p.preRoll) * samplesPerBeat);
                const int64_t start = std::max (oldest, pos - pre);
                CaptureEvent e;
                e.type = CaptureEvent::Start; e.abs = start;
                e.playing = playing; e.bpm = bpm; e.num = num; e.den = den;
                e.ppq = beat - (double) (pos - start) / samplesPerBeat;
                e.timeline = playing && timeline >= 0 ? std::max<int64_t> (0, timeline + i - (pos - start)) : -1;
                if (capture.pushEvent (e)) { capturing = true; clipStart = start; postCount = 0; }
            }
        }
        else
        {
            if (busy) postCount = 0; else ++postCount;
            const int64_t post = (int64_t) std::llround (rollBeats (p.postRoll) * samplesPerBeat);
            const bool tooLong = pos - clipStart >= (int64_t) (sr * 600.0);
            if ((! busy && postCount > post) || tooLong)
            {
                CaptureEvent e; e.type = CaptureEvent::End; e.abs = pos + 1; e.truncated = tooLong;
                if (capture.pushEvent (e)) capturing = false;
            }
        }
        if (active) ++sincePress; else sincePress = 0;
    }

    float rnd() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return (float) (rs >> 8) * (1.0f / 16777216.0f); }

    struct Grain { bool active = false; float pos = 0, start = 0, len = 1, rate = 1, phase = 0, pan = 0; };

    double sr = 48000, bpm = 120, ppq = 0, freeBeat = 0; int num = 4, den = 4; bool playing = false; int64_t timeline = -1;
    bool prevReq = false; int64_t lastSnapIdx = INT64_MIN;
    float xfade = 0.01f, slowXfade = 0.001f, frzCoef = 0.001f;
    float gate = 1.0f;
    StereoRing history, revRing, scRing, dvRing, tape;
    // freeze
    std::vector<float> frzL, frzR; std::array<Grain, 4> grains; float frzEnv = 0; bool wasFreeze = false;
    // repeat
    std::vector<float> repL, repR; int repLen = 1, repRec = 0; float repPos = 0, repRate = 1, repMix = 0; bool wasRepeat = false, repStarted = false;
    // reverse
    int revLen = 0, revK = 0, revBase = 0; float revMix = 0;
    // scatter
    int scLen = 0, scK = 0; double scStart = 0; float scRate = 1, scMix = 0; bool scJump = false, scRev = false;
    // dive
    float diveSemis = 0, dvMix = 0, dvPhase = 0; bool diveWanted = false;
    // crush
    float crMix = 0, crPhase = 1, crHoldA = 0, crHoldB = 0;
    // tape
    TapeState tapeState = TapeIdle; double tapeRead = 0; float tapeRate = 1, tapeProg = 0, tapeMix = 0; bool tapeFromStart = false;
    // capture
    int64_t absPos = 0, clipStart = 0, postCount = 0, sincePress = 0;
    uint32_t rs = 0x5EEDu;
};
} // namespace gb
