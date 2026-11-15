#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
juce::String CapturedClip::positionText() const
{
    if (! playing) return "TRANSPORT STOPPED";
    if (ppq < -1.0e-6) return "BEFORE BAR 1";
    const double bpb = (double) num * 4.0 / (double) juce::jmax (1, den);
    const double beatLen = 4.0 / (double) juce::jmax (1, den);
    const int bar = (int) std::floor (ppq / bpb + 1e-6) + 1;
    const double inBar = ppq - (double) (bar - 1) * bpb;
    const int beat = juce::jlimit (1, num, (int) std::floor (inBar / beatLen + 1e-6) + 1);
    const double frac = inBar / beatLen - (double) (beat - 1);
    juce::String s = "BAR " + juce::String (bar) + " . " + juce::String (beat);
    if (frac > 0.02) s << " +" << juce::String (juce::roundToInt (frac * 4.0)) << "/16";  // off the beat (snap was off)
    return s;
}

//==============================================================================
APVTS::ParameterLayout GlitchBitchProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto add = [&] (const char* id, const char* name, float lo, float hi, float def, float skewCentre = -1.0f, const char* suffix = "", float interval = 0.0f)
    {
        NormalisableRange<float> r (lo, hi, interval);
        if (skewCentre > 0) r.setSkewForCentre (skewCentre);
        const String sfx (suffix);
        auto toText = [sfx] (float v, int)
        {
            if (sfx == "st") return (v > 0.04f ? "+" : "") + String (v, 1) + " st";
            if (sfx == "Hz") return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz";
            if (sfx == "dB") return String (v, 1) + " dB";
            if (sfx == "s")  return String (v, 2) + " s";
            if (sfx == "ms") return v < 10.0f ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms";
            if (sfx == "bits") return String (v, 1) + " bit";
            if (sfx == "beats") return String (v, 1) + " beats";
            return String (roundToInt (v * 100.0f)) + "%";
        };
        auto fromText = [sfx] (const String& t)
        {
            const float v = t.retainCharacters ("-+.0123456789").getFloatValue();
            if (sfx == "Hz" && t.containsIgnoreCase ("k")) return v * 1000.0f;
            if (sfx.isEmpty()) return v / 100.0f;
            return v;
        };
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                             AudioParameterFloatAttributes().withLabel (suffix)
                                                                 .withStringFromValueFunction (toText)
                                                                 .withValueFromStringFunction (fromText)));
    };
    auto choice = [&] (const char* id, const char* name, StringArray items, int def, bool automatable = true)
    {
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def,
                                                              AudioParameterChoiceAttributes().withAutomatable (automatable)));
    };
    auto toggle = [&] (const char* id, const char* name, bool def)
    { ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    StringArray notes; for (int i = 0; i < gb::kNumNotes; ++i) notes.add (gb::noteNames()[i]);

    // ---- THE BUTTON
    toggle ("glitch", "Glitch (The Button)", false);
    choice ("buttonMode", "Button Mode", { "Hold", "Latch" }, 0, false);
    choice ("snap", "Snap", { "Off", "1/16", "1/8", "1/4", "1/2", "1 Bar" }, 0);
    add ("mix", "Dry/Wet", 0, 1, 1);
    add ("outputDb", "Output", -24, 12, 0, -1, "dB");

    // ---- STUTTER
    toggle ("stutterArm", "Stutter Arm", true);
    choice ("stutterSpeed", "Stutter Speed", notes, 5);
    ps.push_back (std::make_unique<AudioParameterInt> (ParameterID { "stutterSteps", 1 }, "Stutter Steps", 1, 16, 16));
    add ("stutterDepth", "Stutter Depth", 0, 1, 1);
    add ("stutterSmooth", "Stutter Smooth", 0.3f, 30, 3, 5, "ms");
    const uint16_t defPattern = gb::Params().stutterPattern;
    for (int i = 0; i < 16; ++i)
        toggle (("step" + String (i)).toRawUTF8(), ("Stutter Step " + String (i + 1)).toRawUTF8(), (defPattern >> i) & 1);

    // ---- REPEAT / REVERSE / SCATTER
    toggle ("repeatArm", "Repeat Arm", false);
    choice ("repeatLength", "Repeat Length", notes, 5);
    add ("repeatDrop", "Repeat Drop", -12, 12, 0, -1, "st", 0.1f);
    toggle ("reverseArm", "Reverse Arm", false);
    choice ("reverseLength", "Reverse Length", notes, 3);
    toggle ("scatterArm", "Scatter Arm", false);
    choice ("scatterSlice", "Scatter Slice", notes, 5);
    add ("scatterChaos", "Scatter Chaos", 0, 1, 0.6f);
    add ("scatterRange", "Scatter Range", 1, 8, 4, -1, "beats", 0.5f);

    // ---- CRUSH / FREEZE / DIVE
    toggle ("crushArm", "Crush Arm", false);
    add ("crushBits", "Crush Bits", 2, 16, 6, 7, "bits", 0.1f);
    add ("crushRate", "Crush Rate", 400, 24000, 6000, 4000, "Hz");
    toggle ("freezeArm", "Freeze Arm", false);
    add ("freezeSize", "Freeze Size", 0, 1, 0.4f);
    add ("freezeMix", "Freeze Mix", 0, 1, 0.7f);
    toggle ("diveArm", "Dive Arm", false);
    add ("diveAmount", "Dive Amount", -24, 24, -12, -1, "st", 0.1f);
    add ("diveTime", "Dive Time", 0.05f, 4, 1, 0.8f, "s");

    // ---- TAPE
    toggle ("tapeStopArm", "Tape Stop Arm", false);
    add ("tapeStopTime", "Tape Stop Time", 0.1f, 4, 0.8f, 0.8f, "s");
    add ("tapeStopShape", "Tape Stop Curve", 0, 1, 0.5f);
    toggle ("tapeStartArm", "Tape Start Arm", false);
    add ("tapeStartTime", "Tape Start Time", 0.1f, 4, 0.8f, 0.8f, "s");
    add ("tapeStartShape", "Tape Start Curve", 0, 1, 0.5f);

    // ---- CAPTURE
    const StringArray rolls { "Off", "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bars" };
    toggle ("captureOn", "Capture", true);
    choice ("preRoll", "Pre-Roll", rolls, 3, false);
    choice ("postRoll", "Post-Roll", rolls, 3, false);
    return { ps.begin(), ps.end() };
}

GlitchBitchProcessor::GlitchBitchProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GLITCHBITCH", createLayout())
{
    for (int i = 0; i < 16; ++i) stepPtrs[(size_t) i] = apvts.getRawParameterValue ("step" + juce::String (i));
    for (auto& s : scope) s.store (0.0f);
    engine.prepare (48000.0);
    writer.setSampleRate (48000.0);
    writer.startThread (juce::Thread::Priority::low);
}

GlitchBitchProcessor::~GlitchBitchProcessor()
{
    writer.stopThread (4000);
}

bool GlitchBitchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

bool GlitchBitchProcessor::isKeptOnPresetLoad (const juce::String& id)
{
    return id == "glitch" || id == "buttonMode" || id == "outputDb" || id == "captureOn" || id == "preRoll" || id == "postRoll";
}

void GlitchBitchProcessor::pullParams()
{
    auto& p = engine.p;
    auto on  = [this] (const char* id) { return apvts.getRawParameterValue (id)->load() > 0.5f; };
    auto val = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto idx = [this] (const char* id) { return juce::roundToInt (apvts.getRawParameterValue (id)->load()); };
    p.engage = on ("glitch"); p.snap = idx ("snap");
    p.mix = val ("mix"); p.outGain = juce::Decibels::decibelsToGain (val ("outputDb"), -100.0f);
    p.stutterArm = on ("stutterArm"); p.stutterSpeed = idx ("stutterSpeed"); p.stutterSteps = idx ("stutterSteps");
    p.stutterDepth = val ("stutterDepth"); p.stutterSmooth = val ("stutterSmooth");
    uint16_t pattern = 0;
    for (int i = 0; i < 16; ++i) if (stepPtrs[(size_t) i]->load() > 0.5f) pattern |= (uint16_t) (1u << i);
    p.stutterPattern = pattern;
    p.repeatArm = on ("repeatArm"); p.repeatLength = idx ("repeatLength"); p.repeatDrop = val ("repeatDrop");
    p.reverseArm = on ("reverseArm"); p.reverseLength = idx ("reverseLength");
    p.scatterArm = on ("scatterArm"); p.scatterSlice = idx ("scatterSlice"); p.scatterChaos = val ("scatterChaos"); p.scatterRange = val ("scatterRange");
    p.crushArm = on ("crushArm"); p.crushBits = val ("crushBits"); p.crushRate = val ("crushRate");
    p.freezeArm = on ("freezeArm"); p.freezeSize = val ("freezeSize"); p.freezeMix = val ("freezeMix");
    p.diveArm = on ("diveArm"); p.diveAmount = val ("diveAmount"); p.diveTime = val ("diveTime");
    p.tapeStopArm = on ("tapeStopArm"); p.tapeStopTime = val ("tapeStopTime"); p.tapeStopShape = val ("tapeStopShape");
    p.tapeStartArm = on ("tapeStartArm"); p.tapeStartTime = val ("tapeStartTime"); p.tapeStartShape = val ("tapeStartShape");
    p.captureOn = on ("captureOn"); p.preRoll = idx ("preRoll"); p.postRoll = idx ("postRoll");
}

void GlitchBitchProcessor::prepareToPlay (double sampleRate, int)
{
    // the writer reads the capture ring, so it has to stop while the ring is rebuilt
    writer.stopThread (4000);
    pullParams();
    engine.prepare (sampleRate);
    writer.setSampleRate (sampleRate);
    writer.startThread (juce::Thread::Priority::low);
    setLatencySamples (0);
}

void GlitchBitchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    pullParams();

    double bpm = 120.0, ppq = -1.0e10; bool playing = false; int num = 4, den = 4; juce::int64 t = -1;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            playing = pos->getIsPlaying() || pos->getIsRecording();
            if (auto q = pos->getPpqPosition()) ppq = *q;
            if (auto ts = pos->getTimeSignature()) { num = ts->numerator; den = ts->denominator; }
            if (auto s = pos->getTimeInSamples()) t = *s;
        }
    engine.setTransport (bpm, ppq, playing, num, den, t);

    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();
    if (inCh == 1 && buffer.getNumChannels() > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n); // mono in -> stereo out

    inLevel.store (std::max (inLevel.load() * 0.8f, buffer.getMagnitude (0, n)));
    float* L = buffer.getWritePointer (0);
    if (buffer.getNumChannels() > 1)
        engine.process (L, buffer.getWritePointer (1), n);
    else
    {
        // mono track: run the right channel on a scratch copy in small chunks
        float tmp[256];
        for (int s = 0; s < n; s += 256)
        {
            const int c = std::min (256, n - s);
            std::copy (L + s, L + s + c, tmp);
            if (s > 0) engine.setTransport (bpm, playing ? ppq + (double) s * bpm / 60.0 / getSampleRate() : -1.0e10, playing, num, den, t >= 0 ? t + s : -1);
            engine.process (L + s, tmp, c);
        }
    }
    outLevel.store (std::max (outLevel.load() * 0.8f, buffer.getMagnitude (0, n)));
    currentStep.store (engine.currentStep);
    uiActive.store (engine.active); uiPending.store (engine.pending); uiCapturing.store (engine.capturing);

    // scope feed: one peak per 128 samples
    for (int i = 0; i < n; ++i)
    {
        scopeAcc = std::max (scopeAcc, std::abs (L[i]));
        if (++scopeCount >= 128)
        {
            const int w = scopeWrite.load (std::memory_order_relaxed);
            scope[(size_t) w].store (scopeAcc, std::memory_order_relaxed);
            scopeWrite.store ((w + 1) % kScopeSize, std::memory_order_relaxed);
            scopeAcc = 0; scopeCount = 0;
        }
    }
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
}

//==============================================================================
int GlitchBitchProcessor::getNumPrograms() { return (int) gb::factoryPresets().size(); }

const juce::String GlitchBitchProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? juce::String (gb::factoryPresets()[(size_t) index].name) : juce::String();
}

void GlitchBitchProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    currentPresetName = gb::factoryPresets()[(size_t) index].name;
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! isKeptOnPresetLoad (rp->getParameterID()))
                rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto& [id, v] : gb::factoryPresets()[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
    ++presetVersion;
}

juce::File GlitchBitchProcessor::getUserPresetFolder()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory); // ~/Library on macOS, %APPDATA% on Windows
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    return base.getChildFile ("Castle Danger").getChildFile ("Glitch Bitch").getChildFile ("Presets");
}

juce::Array<juce::File> GlitchBitchProcessor::getUserPresets()
{
    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.glitchbitch");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });
    return files;
}

juce::File GlitchBitchProcessor::fileForPresetName (const juce::String& name)
{
    return getUserPresetFolder().getChildFile (juce::File::createLegalFileName (name.trim()) + ".glitchbitch");
}

bool GlitchBitchProcessor::saveUserPreset (const juce::String& rawName)
{
    const auto name = rawName.trim();
    if (name.isEmpty()) return false;
    const auto file = fileForPresetName (name);
    if (! file.getParentDirectory().createDirectory()) return false;
    juce::XmlElement xml ("GlitchBitchPreset");
    xml.setAttribute ("name", name);
    xml.setAttribute ("pluginVersion", JucePlugin_VersionString);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (isKeptOnPresetLoad (rp->getParameterID())) continue;
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", rp->getParameterID());
            e->setAttribute ("value", (double) rp->convertFrom0to1 (rp->getValue()));
        }
    if (! xml.writeTo (file)) return false;
    currentPresetName = name;
    ++presetVersion;
    return true;
}

bool GlitchBitchProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("GlitchBitchPreset")) return false;
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! isKeptOnPresetLoad (rp->getParameterID()))
                rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
    {
        const auto id = e->getStringAttribute ("id");
        if (isKeptOnPresetLoad (id)) continue;
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 ((float) e->getDoubleAttribute ("value")));
    }
    currentPresetName = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
    ++presetVersion;
    return true;
}

//==============================================================================
juce::File GlitchBitchProcessor::getCaptureRoot()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Glitch Bitch Captures");
}

void GlitchBitchProcessor::addClip (CapturedClip&& c)
{
    {
        const juce::ScopedLock sl (clipLock);
        clips.push_back (std::move (c));
        while ((int) clips.size() > kMaxClips) clips.erase (clips.begin()); // the file stays on disk
    }
    ++clipsVersion;
}

void GlitchBitchProcessor::removeClip (const juce::File& f, bool deleteFile)
{
    {
        const juce::ScopedLock sl (clipLock);
        clips.erase (std::remove_if (clips.begin(), clips.end(), [&] (const CapturedClip& c) { return c.file == f; }), clips.end());
    }
    if (deleteFile) f.deleteFile();
    ++clipsVersion;
}

void GlitchBitchProcessor::clearClips (bool deleteFiles)
{
    std::vector<CapturedClip> old;
    { const juce::ScopedLock sl (clipLock); old.swap (clips); }
    if (deleteFiles) for (auto& c : old) c.file.deleteFile();
    ++clipsVersion;
}

void GlitchBitchProcessor::updateTrackProperties (const TrackProperties& props)
{
    const juce::ScopedLock sl (nameLock);
    trackName = props.name.value_or (juce::String());
}

void GlitchBitchProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    state.setProperty ("clipCounter", clipCounter.load(), nullptr);
    juce::ValueTree list ("CLIPS");
    {
        const juce::ScopedLock sl (clipLock);
        for (auto& c : clips)
        {
            juce::ValueTree t ("CLIP");
            t.setProperty ("file", c.file.getFullPathName(), nullptr);
            t.setProperty ("seconds", c.seconds, nullptr);
            t.setProperty ("playing", c.playing, nullptr);
            t.setProperty ("ppq", c.ppq, nullptr);
            t.setProperty ("num", c.num, nullptr);
            t.setProperty ("den", c.den, nullptr);
            t.setProperty ("timeline", (juce::int64) c.timeline, nullptr);
            list.appendChild (t, nullptr);
        }
    }
    state.appendChild (list, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void GlitchBitchProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto vt = juce::ValueTree::fromXml (*xml);
    currentProgram = vt.getProperty ("program", 0);
    currentPresetName = vt.getProperty ("presetName", getProgramName (currentProgram)).toString();
    clipCounter.store ((int) vt.getProperty ("clipCounter", 0));

    std::vector<CapturedClip> restored;
    if (auto list = vt.getChildWithName ("CLIPS"); list.isValid())
    {
        for (auto t : list)
        {
            CapturedClip c;
            c.file = juce::File (t.getProperty ("file").toString());
            if (! c.file.existsAsFile()) continue;
            c.seconds = t.getProperty ("seconds", 0.0);
            c.playing = t.getProperty ("playing", false);
            c.ppq = t.getProperty ("ppq", 0.0);
            c.num = t.getProperty ("num", 4);
            c.den = t.getProperty ("den", 4);
            c.timeline = (juce::int64) t.getProperty ("timeline", -1);
            restored.push_back (std::move (c));
        }
        vt.removeChild (list, nullptr);
    }
    apvts.replaceState (vt);
    if (auto* g = apvts.getParameter ("glitch")) g->setValueNotifyingHost (0.0f); // never reopen a session mid-glitch
    {
        const juce::ScopedLock sl (clipLock);
        clips = std::move (restored);
        for (auto& c : clips) writer.queueLoad (c.file);   // thumbnails are rebuilt in the background
    }
    ++clipsVersion;
    ++presetVersion;
}

//==============================================================================
void CaptureWriter::run()
{
    auto& cap = owner.engine.capture;
    while (! threadShouldExit())
    {
        loadExisting();
        for (int guard = 0; guard < 64; ++guard)
        {
            gb::CaptureEvent e;
            if (! open)
            {
                if (! cap.popEvent (e)) break;
                if (e.type == gb::CaptureEvent::Start) startClip (e);
                continue; // stray End events (after a failed clip) are dropped
            }
            if (endAbs < 0 && cap.peekEvent (e) && e.type == gb::CaptureEvent::End)
            {
                cap.popEvent (e); endAbs = e.abs; current.truncated = e.truncated;
            }
            const juce::int64 w = cap.written.load (std::memory_order_acquire);
            if (w - cursor > cap.size() - (juce::int64) (sr * 1.5)) { finishClip (false, true); continue; } // fell too far behind
            const juce::int64 limit = endAbs >= 0 ? std::min (w, endAbs) : w;
            while (cursor < limit)
            {
                const int chunk = (int) std::min<juce::int64> (limit - cursor, (juce::int64) scratchL.size());
                for (int k = 0; k < chunk; ++k)
                {
                    const auto idx = (size_t) ((cursor + k) & cap.mask);
                    scratchL[(size_t) k] = cap.l[idx]; scratchR[(size_t) k] = cap.r[idx];
                    peakAcc = std::max ({ peakAcc, std::abs (scratchL[(size_t) k]), std::abs (scratchR[(size_t) k]) });
                    if (++peakCount >= CapturedClip::kPeakBlock) { current.peaks.push_back (peakAcc); peakAcc = 0; peakCount = 0; }
                }
                const float* chans[2] = { scratchL.data(), scratchR.data() };
                if (writer == nullptr || ! writer->writeFromFloatArrays (chans, 2, chunk)) { finishClip (false, true); break; }
                cursor += chunk;
            }
            if (open && endAbs >= 0 && cursor >= endAbs) { finishClip (current.truncated, false); continue; }
            break;
        }
        wait (10);
    }
    if (open) finishClip (false, false); // plugin closing or sample rate changing: keep what we have
}

juce::File CaptureWriter::makeFileFor (const gb::CaptureEvent& e)
{
    auto folder = GlitchBitchProcessor::getCaptureRoot().getChildFile (juce::Time::getCurrentTime().formatted ("%Y-%m-%d"));
    folder.createDirectory();
    auto track = owner.getTrackName().trim();
    if (track.isEmpty()) track = "Glitch Bitch";
    CapturedClip tmp; tmp.playing = e.playing; tmp.ppq = e.ppq; tmp.num = e.num; tmp.den = e.den;
    juce::String pos = tmp.positionText().replace ("BAR ", "Bar ").replace (" . ", ".").replace (" +", " ").replace ("/16", "-16");
    if (! e.playing) pos = juce::Time::getCurrentTime().formatted ("%H%M%S");
    const auto name = juce::File::createLegalFileName (track + " - Glitch " + juce::String (owner.nextClipNumber()).paddedLeft ('0', 3) + " - " + pos);
    return folder.getChildFile (name + ".wav").getNonexistentSibling (false);
}

void CaptureWriter::startClip (const gb::CaptureEvent& e)
{
    current = CapturedClip();
    current.playing = e.playing; current.ppq = e.ppq; current.num = e.num; current.den = e.den; current.timeline = e.timeline;
    current.file = makeFileFor (e);
    cursor = e.abs; startAbs = e.abs; endAbs = -1; peakAcc = 0; peakCount = 0;
    scratchL.resize (8192); scratchR.resize (8192);

    std::unique_ptr<juce::OutputStream> out = std::make_unique<juce::FileOutputStream> (current.file);
    if (! static_cast<juce::FileOutputStream*> (out.get())->openedOk()) { ++owner.captureErrors; open = false; return; }

    // Broadcast WAV timestamp: DAWs that read it (Pro Tools Spot, Reaper, Cubase/Nuendo...) can put the clip back where it came from
    const auto bwav = juce::WavAudioFormat::createBWAVMetadata ("Glitch Bitch capture", "Castle Danger Glitch Bitch",
                                                               current.file.getFileNameWithoutExtension().substring (0, 32),
                                                               juce::Time::getCurrentTime(), (juce::int64) std::max<int64_t> (0, e.timeline), {});
    auto opts = juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24);
    for (auto& k : bwav.getAllKeys()) opts = opts.withMetadata (k, bwav[k]);
    juce::WavAudioFormat wav;
    writer = wav.createWriterFor (out, opts);
    if (writer == nullptr) { ++owner.captureErrors; current.file.deleteFile(); open = false; return; }
    open = true;
}

void CaptureWriter::finishClip (bool truncated, bool failed)
{
    writer.reset();   // flushes + closes the file
    open = false;
    if (peakCount > 0) { current.peaks.push_back (peakAcc); peakAcc = 0; peakCount = 0; }
    if (failed) { ++owner.captureErrors; current.file.deleteFile(); return; }
    current.truncated = truncated;
    current.seconds = (double) (cursor - startAbs) / sr;
    if (current.peaks.empty()) { current.file.deleteFile(); return; }
    owner.addClip (std::move (current));
    current = CapturedClip();
}

void CaptureWriter::loadExisting()
{
    juce::Array<juce::File> jobs;
    { const juce::ScopedLock sl (loadLock); jobs.swapWith (toLoad); }
    if (jobs.isEmpty()) return;
    juce::WavAudioFormat wav;
    for (auto& f : jobs)
    {
        if (threadShouldExit()) return;
        auto in = std::make_unique<juce::FileInputStream> (f);
        if (! in->openedOk()) continue;
        std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (in.release(), true));
        if (reader == nullptr) continue;
        std::vector<float> peaks;
        juce::AudioBuffer<float> buf ((int) reader->numChannels, 8192);
        float acc = 0; int cnt = 0;
        for (juce::int64 pos = 0; pos < reader->lengthInSamples; pos += 8192)
        {
            const int n = (int) std::min<juce::int64> (8192, reader->lengthInSamples - pos);
            reader->read (&buf, 0, n, pos, true, true);
            for (int i = 0; i < n; ++i)
            {
                for (int ch = 0; ch < buf.getNumChannels(); ++ch) acc = std::max (acc, std::abs (buf.getSample (ch, i)));
                if (++cnt >= CapturedClip::kPeakBlock) { peaks.push_back (acc); acc = 0; cnt = 0; }
            }
        }
        if (cnt > 0) peaks.push_back (acc);
        {
            const juce::ScopedLock sl (owner.clipLock);
            for (auto& c : owner.clips) if (c.file == f) { c.peaks = peaks; c.seconds = (double) reader->lengthInSamples / reader->sampleRate; }
        }
        ++owner.clipsVersion;
    }
}

juce::AudioProcessorEditor* GlitchBitchProcessor::createEditor() { return new GlitchBitchEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GlitchBitchProcessor(); }
