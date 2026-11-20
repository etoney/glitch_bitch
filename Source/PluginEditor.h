#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class GlitchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GlitchLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hi, bool down) override;
    juce::Image rustDisc;
};

// The porthole screen: the output waveform scrolling on a dirty CRT. It tears apart while the glitch fires.
class GlitchScreen : public juce::Component
{
public:
    explicit GlitchScreen (GlitchWitchProcessor& p) : proc (p) {}
    void update (float dt);
    void paint (juce::Graphics&) override;
private:
    GlitchWitchProcessor& proc;
    std::array<float, GlitchWitchProcessor::kScopeSize> vals {};
    float time = 0, tear = 0, rgb = 0;
    bool active = false, pending = false;
    juce::Random rnd;
};

// THE BUTTON: a big rusty mushroom button. Hold mode: fires while held. Latch mode: click on, click off.
class TheButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit TheButton (GlitchWitchProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override;
    void update (float dt);
private:
    bool isLatch() const;
    void setGlitch (bool on);
    GlitchWitchProcessor& proc;
    juce::RangedAudioParameter* param = nullptr;
    bool down = false, lit = false, pending = false;
    float glow = 0, pulse = 0;
};

// Glowing switch (ARM / ON / HOLD / LATCH)
class PowerButton : public juce::ToggleButton
{
public:
    explicit PowerButton (const juce::String& label = "ARM") : juce::ToggleButton (label) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// One step of the stutter sequencer: lit = volume is cut on this step
class StepButton : public juce::ToggleButton
{
public:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void setState (bool isCurrent, bool inRange, bool rackOn)
    {
        if (isCurrent != current || inRange != active || rackOn != running)
        { current = isCurrent; active = inRange; running = rackOn; repaint(); }
    }
private:
    bool current = false, active = true, running = false;
};

// A captured glitch in the tray. Drag it straight onto a track in your DAW.
class ClipTile : public juce::Component, public juce::SettableTooltipClient
{
public:
    ClipTile();
    void setClip (const CapturedClip* c, int number);
    void setRecording (bool r) { if (r != recording) { recording = r; repaint(); } }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    std::function<void (const juce::File&)> onReveal, onRemove, onDelete;
    float blink = 0;
private:
    bool hasClip = false, recording = false, dragging = false;
    CapturedClip clip;
    int number = 0;
};

// In-window modal dialog (message, confirm, or text entry), drawn over the editor
class PresetDialog : public juce::Component
{
public:
    PresetDialog (const juce::String& title, const juce::String& message, bool withTextField,
                  const juce::String& initialText, juce::StringArray buttons,
                  std::function<void (int buttonIndex, const juce::String& text)> onClose);
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void parentHierarchyChanged() override;
    void mouseDown (const juce::MouseEvent&) override {}
private:
    void finish (int index);
    juce::String title, message;
    juce::TextEditor field;
    bool hasField;
    juce::OwnedArray<juce::TextButton> buttons;
    std::function<void (int, const juce::String&)> onClose;
    juce::Rectangle<int> panel;
};

class GlitchWitchEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GlitchWitchEditor (GlitchWitchProcessor&);
    ~GlitchWitchEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct FxPlate
    {
        juce::String title, armParam;
        juce::Rectangle<int> bounds;
        PowerButton arm { "ARM" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
        std::vector<Knob*> knobs;
        bool wasArmed = false, wasFiring = false;
    };

    Knob* makeKnob (const juce::String& id, const juce::String& text);
    void placeKnobs (const std::vector<Knob*>& ks, juce::Rectangle<int> area);
    void timerCallback() override;
    void buildBackground();
    void refreshTray();
    void refreshModeButtons();

    GlitchWitchProcessor& proc;
    GlitchLookAndFeel lnf;
    std::vector<std::unique_ptr<Knob>> knobs;

    // header
    GlitchScreen screen;
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, deleteButton { "DELETE" };
    juce::Array<juce::File> userFiles;
    int lastPresetVersion = -1;
    std::unique_ptr<PresetDialog> dialog;
    static constexpr int kUserIdBase = 1001;
    void refreshPresetBox();
    void stepPreset (int delta);
    void promptSave();
    void confirmDelete();
    void showDialog (const juce::String& title, const juce::String& message, bool withTextField, const juce::String& initialText,
                     juce::StringArray buttons, std::function<void (int, const juce::String&)> onResult);
    void showMessage (const juce::String& title, const juce::String& text);

    // the button
    TheButton theButton;
    PowerButton holdButton { "HOLD" }, latchButton { "LATCH" };
    juce::ComboBox snapBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> snapAtt;
    juce::Rectangle<int> buttonPlate, statusArea, snapLabelArea, modeLabelArea;
    juce::String armedText;

    // effects
    std::array<FxPlate, 9> plates;   // stutter, repeat, reverse, scatter, crush, freeze, dive, tape stop, tape start
    std::array<StepButton, 16> steps;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 16> stepAtts;
    juce::Rectangle<int> stepArea, ioPlate, meterArea;
    std::vector<Knob*> ioKnobs;

    // capture tray
    juce::Rectangle<int> trayPlate, trayLabelArea, tilesArea, trayHint;
    PowerButton captureButton { "CAPTURE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> captureAtt;
    juce::ComboBox preBox, postBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> preAtt, postAtt;
    juce::Rectangle<int> preLabel, postLabel;
    juce::TextButton folderButton { "FOLDER" }, clearButton { "CLEAR" }, pageLeft { "<" }, pageRight { ">" };
    static constexpr int kTiles = 6;
    std::array<ClipTile, kTiles> tiles;
    int page = 0, lastClipsVersion = -1, lastErrors = 0;
    bool wasCapturing = false, lastFiring = false;

    juce::Image background, logoImage;
    juce::Rectangle<int> screenWell, logoArea, badgeArea, crestArea, presetArea, stripeArea;
    float flicker = 0.0f, inMeter = 0.0f, outMeter = 0.0f, logoGlitch = 0.0f;
    int logoSeed = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchWitchEditor)
};
