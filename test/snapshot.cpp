// Dev tool: runs the real plugin offline with a fake song playing, fires THE BUTTON a few times,
// waits for the capture files, then saves editor screenshots.   snapshot <out_dir>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

struct FakePlayHead : juce::AudioPlayHead
{
    double sr = 48000, bpm = 120; juce::int64 t = 0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm); p.setIsPlaying (playing); p.setTimeInSamples (t);
        p.setPpqPosition ((double) t / sr * bpm / 60.0);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (argc > 1 ? argv[1] : "/tmp");
    GlitchBitchProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000, 512);
    proc.prepareToPlay (48000, 512);
    FakePlayHead ph; proc.setPlayHead (&ph);
    auto* glitch = proc.apvts.getParameter ("glitch");
    auto setP = [&] (const char* id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    setP ("snap", 3); setP ("repeatArm", 1);

    juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
    juce::Random r (3);
    auto runFor = [&] (double seconds, bool on)
    {
        glitch->setValueNotifyingHost (on ? 1.0f : 0.0f);
        const int blocks = (int) (seconds * 48000 / 512);
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                const double tt = (double) (ph.t + i) / 48000.0, beat = tt * 2.0, in = beat - std::floor (beat);
                const float s = (float) (0.6 * std::sin (2 * 3.14159 * 60 * in * 0.5) * std::exp (-in * 6) + 0.1 * (r.nextFloat() - 0.5) * std::exp (-std::fmod (beat * 2, 1.0) * 8));
                buf.setSample (0, i, s); buf.setSample (1, i, s);
            }
            proc.processBlock (buf, midi);
            ph.t += 512;
        }
    };
    runFor (4.1, false); runFor (1.0, true); runFor (2.0, false);   // glitch 1 near bar 3
    setP ("tapeStopArm", 1); setP ("tapeStartArm", 1);
    runFor (1.5, true); runFor (2.5, false);                        // glitch 2
    setP ("crushArm", 1);
    runFor (0.7, true); runFor (2.0, false);                        // glitch 3
    juce::Thread::sleep (800);

    {
        const juce::ScopedLock sl (proc.clipLock);
        std::printf ("clips: %d  errors: %d\n", (int) proc.clips.size(), proc.captureErrors.load());
        for (auto& c : proc.clips)
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatReader> rd (wav.createReaderFor (new juce::FileInputStream (c.file), true));
            std::printf ("  %s  %.3f s  %s  timeRef=%s  len=%lld  peaks=%d\n", c.file.getFileName().toRawUTF8(), c.seconds,
                         c.positionText().toRawUTF8(), rd ? rd->metadataValues["TimeReference"].toRawUTF8() : "?",
                         rd ? (long long) rd->lengthInSamples : -1LL, (int) c.peaks.size());
        }
    }

    // screenshots: idle, then firing
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setVisible (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
    juce::PNGImageFormat png;
    auto save = [&] (const char* name)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f = out.getChildFile (name); f.deleteFile();
        juce::FileOutputStream os (f); png.writeImageToStream (img, os);
    };
    save ("gb_idle.png");
    setP ("buttonMode", 1);
    glitch->setValueNotifyingHost (1.0f);
    for (int k = 0; k < 40; ++k) { runFor (0.02, true); juce::MessageManager::getInstance()->runDispatchLoopUntil (20); }
    save ("gb_firing.png");
    glitch->setValueNotifyingHost (0.0f);
    runFor (2.0, false);
    ed.reset();
    proc.releaseResources();
    return 0;
}
