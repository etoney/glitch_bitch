#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "GlitchEngine.h"
#include "Presets.h"

// One captured glitch, written to disk as a WAV file
struct CapturedClip
{
    juce::File file;
    double seconds = 0;
    bool playing = false;         // the transport was running (so the position below is real)
    double ppq = 0;               // song position of the clip start, in quarter notes
    int num = 4, den = 4;
    juce::int64 timeline = -1;    // host timeline sample of the clip start
    std::vector<float> peaks;     // max |sample| per kPeakBlock samples, for the tray thumbnails
    bool truncated = false;
    static constexpr int kPeakBlock = 256;

    juce::String positionText() const;   // "BAR 33 . 2" or "TRANSPORT STOPPED"
};

class GlitchWitchProcessor;

// Background thread that turns the capture ring into WAV files
class CaptureWriter : public juce::Thread
{
public:
    explicit CaptureWriter (GlitchWitchProcessor& p) : juce::Thread ("Glitch Witch capture"), owner (p) {}
    void run() override;
    void setSampleRate (double s) { sr = s; }
    void queueLoad (const juce::File& f) { const juce::ScopedLock sl (loadLock); toLoad.add (f); notify(); }

private:
    void startClip (const gb::CaptureEvent&);
    void finishClip (bool truncated, bool failed);
    void loadExisting();
    juce::File makeFileFor (const gb::CaptureEvent&);

    GlitchWitchProcessor& owner;
    double sr = 48000;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    CapturedClip current;
    juce::int64 cursor = 0, startAbs = 0, endAbs = -1;
    bool open = false;
    float peakAcc = 0; int peakCount = 0;
    juce::CriticalSection loadLock;
    juce::Array<juce::File> toLoad;
    std::vector<float> scratchL, scratchR;
};

class GlitchWitchProcessor : public juce::AudioProcessor
{
public:
    GlitchWitchProcessor();
    ~GlitchWitchProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Glitch Witch"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    void updateTrackProperties (const TrackProperties& props) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // ---- UI feeds
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f };
    std::atomic<int> currentStep { -1 };
    std::atomic<bool> uiActive { false }, uiPending { false }, uiCapturing { false };
    static constexpr int kScopeSize = 512;
    std::array<std::atomic<float>, kScopeSize> scope {};   // recent output peaks, for the screen
    std::atomic<int> scopeWrite { 0 };

    // ---- presets
    static juce::File getUserPresetFolder();
    static juce::Array<juce::File> getUserPresets();
    static juce::File fileForPresetName (const juce::String& name);
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);
    juce::String getCurrentPresetName() const { return currentPresetName; }
    std::atomic<int> presetVersion { 0 };
    static bool isKeptOnPresetLoad (const juce::String& id);

    // ---- captured clips (message thread + writer thread, guarded by clipLock)
    juce::CriticalSection clipLock;
    std::vector<CapturedClip> clips;          // newest last
    std::atomic<int> clipsVersion { 0 };
    std::atomic<int> captureErrors { 0 };
    void addClip (CapturedClip&&);
    void removeClip (const juce::File& f, bool deleteFile);
    void clearClips (bool deleteFiles);
    static juce::File getCaptureRoot();
    juce::String getTrackName() const { const juce::ScopedLock sl (nameLock); return trackName; }
    int nextClipNumber() { return ++clipCounter; }

    gb::Engine engine;
    static constexpr int kMaxClips = 48;

private:
    void pullParams();
    std::array<std::atomic<float>*, 16> stepPtrs {};
    CaptureWriter writer { *this };
    int currentProgram = 0;
    juce::String currentPresetName { "Init (Stutter 1/16)" };
    juce::CriticalSection nameLock;
    juce::String trackName;
    std::atomic<int> clipCounter { 0 };
    float scopeAcc = 0; int scopeCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchWitchProcessor)
};
