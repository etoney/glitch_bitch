// GLITCH BITCH — editor. Same rusty Castle Danger hardware as Possessed, Chop Shop and Dominatrix.
#include "PluginEditor.h"

namespace pal
{
const juce::Colour ironDark  { 0xff1d1815 };
const juce::Colour iron      { 0xff3a322c };
const juce::Colour rustDeep  { 0xff4a1d0c };
const juce::Colour rust      { 0xff9a4518 };
const juce::Colour rustLight { 0xffc0702a };
const juce::Colour paint     { 0xffd6c7a6 };   // dirty cream stencil paint
const juce::Colour paintDim  { 0xffa8997c };
const juce::Colour signal    { 0xffd2452a };   // rusty red for active things
const juce::Colour signalHot { 0xffff6a3d };
const juce::Colour hazard    { 0xffc99a22 };   // worn safety yellow
const juce::Colour screen    { 0xff0c0a08 };
const juce::Colour leather   { 0xff4a1a16 };   // worn oxblood leather
const juce::Colour leatherDark { 0xff1c0a08 };
}

//==============================================================================
namespace tex
{
inline float hash (int x, int y, int seed)
{
    uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u + (uint32_t) seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float) (h & 0xffffffu) / 16777215.0f;
}
inline float smooth (float t) { return t * t * (3.0f - 2.0f * t); }
inline float smoothstep (float a, float b, float x) { return smooth (juce::jlimit (0.0f, 1.0f, (x - a) / (b - a))); }
inline float vnoise (float x, float y, int seed)
{
    const int xi = (int) std::floor (x), yi = (int) std::floor (y);
    const float fx = x - (float) xi, fy = y - (float) yi;
    const float a = hash (xi, yi, seed), b = hash (xi + 1, yi, seed), c = hash (xi, yi + 1, seed), d = hash (xi + 1, yi + 1, seed);
    const float u = smooth (fx), v = smooth (fy);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
inline float fbm (float x, float y, int seed, int octaves)
{
    float s = 0, amp = 0.5f, f = 1, norm = 0;
    for (int i = 0; i < octaves; ++i) { s += amp * vnoise (x * f, y * f, seed + i * 31); norm += amp; f *= 2.03f; amp *= 0.5f; }
    return s / norm;
}
struct RGB { float r, g, b; };
inline RGB rgb (juce::Colour c) { return { c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue() }; }
inline RGB mix (RGB a, RGB b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }
inline RGB mul (RGB a, float k) { return { a.r * k, a.g * k, a.b * k }; }

inline RGB rustAt (float x, float y, float rustBias, int seed)
{
    const float n      = fbm (x / 170.0f, y / 170.0f, seed, 4);
    const float detail = fbm (x / 36.0f, y / 36.0f, seed + 7, 3);
    const float fine   = vnoise (x / 7.0f, y / 7.0f, seed + 13);
    const float mask   = smoothstep (0.45f, 0.62f, n + (detail - 0.5f) * 0.35f + rustBias);
    RGB base = mix (rgb (pal::ironDark), rgb (pal::iron), detail);
    RGB r    = mix (rgb (pal::rustDeep), rgb (pal::rust), fbm (x / 20.0f, y / 20.0f, seed + 3, 2));
    if (fine > 0.72f) r = mix (r, rgb (pal::rustLight), (fine - 0.72f) * 2.5f);
    return mix (base, r, mask);
}

// Paint-chip a rendered image: knock out chunks of alpha with noise, plus grain.
inline void distress (juce::Image& img, float scale, float wear, int seed)
{
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < bd.height; ++y)
        for (int x = 0; x < bd.width; ++x)
        {
            auto c = bd.getPixelColour (x, y);
            if (c.getAlpha() == 0) continue;
            const float n = fbm ((float) x / (7.0f * scale), (float) y / (7.0f * scale), seed, 3);
            const float chip = n < wear ? 0.08f : 1.0f;
            const float grain = 0.7f + 0.3f * hash (x, y, seed + 99);
            bd.setPixelColour (x, y, c.withMultipliedAlpha (chip * grain));
        }
}

inline juce::Font stencil (float size) { return juce::Font (juce::FontOptions ("Impact", size, juce::Font::plain)); }
inline juce::Font plainBold (float size) { return juce::Font (juce::FontOptions (size, juce::Font::bold)); }

inline juce::Image distressedText (const juce::String& text, int w, int h, juce::Font font, juce::Colour col,
                                   juce::Justification just, float scale, float wear, int seed, float kerning = 0.0f)
{
    juce::Image img (juce::Image::ARGB, juce::roundToInt ((float) w * scale), juce::roundToInt ((float) h * scale), true);
    {
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (scale));
        g.setColour (col);
        g.setFont (font.withExtraKerningFactor (kerning));
        g.drawFittedText (text, 0, 0, w, h, just, 1, 0.55f);   // squeeze rather than clip if the font runs wide
    }
    distress (img, scale, wear, seed);
    return img;
}
} // namespace tex

//==============================================================================
// The Castle Danger crest: a castle silhouette under a blood moon, on a riveted iron medallion.
static void drawCastleCrest (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto c = r.getCentre();
    const float R = std::min (r.getWidth(), r.getHeight()) * 0.5f;

    // iron ring
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (r.translated (1.5f, 2.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a5a4c), c.x - R * 0.6f, c.y - R * 0.8f, juce::Colour (0xff1a1410), c.x + R, c.y + R, true));
    g.fillEllipse (r);
    g.setColour (pal::rust.withAlpha (0.55f)); g.drawEllipse (r.reduced (1.0f), 1.6f);
    for (int i = 0; i < 8; ++i)
    {
        const float a = (float) i / 8.0f * juce::MathConstants<float>::twoPi + 0.39f;
        const float rx = c.x + std::cos (a) * R * 0.88f, ry = c.y + std::sin (a) * R * 0.88f;
        g.setColour (juce::Colour (0xff8a7a68)); g.fillEllipse (rx - 1.8f, ry - 1.8f, 3.6f, 3.6f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (rx - 1.8f, ry - 1.8f, 3.6f, 3.6f, 0.6f);
    }

    // night sky + blood moon
    const auto inner = r.reduced (R * 0.2f);
    const float ir = inner.getWidth() * 0.5f;
    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip; clip.addEllipse (inner);
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a0c08), c.x, inner.getY(), juce::Colour (0xff080505), c.x, inner.getBottom(), false));
    g.fillEllipse (inner);
    const juce::Rectangle<float> moon (c.x - ir * 0.55f, inner.getY() + ir * 0.12f, ir * 1.1f, ir * 1.1f);
    juce::DropShadow (pal::signalHot.withAlpha (0.55f), (int) (ir * 0.45f), {}).drawForPath (g, [&] { juce::Path p; p.addEllipse (moon); return p; }());
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff7a4a), moon.getX() + moon.getWidth() * 0.35f, moon.getY() + moon.getHeight() * 0.3f,
                                             juce::Colour (0xff8a1408), moon.getRight(), moon.getBottom(), true));
    g.fillEllipse (moon);
    g.setColour (juce::Colour (0x40400000));
    g.fillEllipse (moon.getX() + moon.getWidth() * 0.55f, moon.getY() + moon.getHeight() * 0.25f, moon.getWidth() * 0.16f, moon.getWidth() * 0.16f);
    g.fillEllipse (moon.getX() + moon.getWidth() * 0.25f, moon.getY() + moon.getHeight() * 0.55f, moon.getWidth() * 0.12f, moon.getWidth() * 0.12f);

    // castle silhouette
    const float x0 = inner.getX(), y0 = inner.getY(), w = inner.getWidth(), h = inner.getHeight();
    auto X = [&] (float f) { return x0 + f * w; };
    auto Y = [&] (float f) { return y0 + f * h; };
    juce::Path castle;
    auto tower = [&] (float l, float rgt, float top, int merlons)
    {
        castle.addRectangle (X (l), Y (top), X (rgt) - X (l), Y (1.05f) - Y (top));
        const float mw = (rgt - l) / (float) (merlons * 2 - 1);
        for (int i = 0; i < merlons; ++i)
            castle.addRectangle (X (l + (float) i * 2.0f * mw), Y (top - 0.06f), X (l + mw) - X (l), Y (0.06f) - Y (0.0f) + 0.5f);
    };
    tower (0.08f, 0.30f, 0.50f, 3);           // left tower
    tower (0.70f, 0.92f, 0.50f, 3);           // right tower
    tower (0.38f, 0.62f, 0.36f, 3);           // keep
    tower (0.30f, 0.70f, 0.64f, 5);           // curtain wall
    castle.addTriangle (X (0.08f), Y (0.44f), X (0.19f), Y (0.27f), X (0.30f), Y (0.44f));   // spires
    castle.addTriangle (X (0.70f), Y (0.44f), X (0.81f), Y (0.27f), X (0.92f), Y (0.44f));
    castle.setUsingNonZeroWinding (true);
    g.setColour (juce::Colour (0xff070505)); g.fillPath (castle);
    // gate + lit windows
    juce::Path gate;
    gate.addRectangle (X (0.44f), Y (0.84f), X (0.56f) - X (0.44f), Y (0.2f) - Y (0.0f));
    gate.addEllipse (X (0.44f), Y (0.79f), X (0.56f) - X (0.44f), Y (0.1f) - Y (0.0f));
    g.setColour (juce::Colour (0xff2a0c06)); g.fillPath (gate);
    g.setColour (pal::signalHot.withAlpha (0.9f));
    for (auto p : { juce::Point<float> (0.19f, 0.62f), { 0.81f, 0.62f }, { 0.5f, 0.5f } })
        g.fillRect (X (p.x) - w * 0.018f, Y (p.y), w * 0.036f, h * 0.07f);
}

//==============================================================================
GlitchLookAndFeel::GlitchLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, pal::paintDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, pal::rust.withAlpha (0.6f));
    setColour (juce::Label::textColourId, pal::paintDim);
    setColour (juce::ComboBox::textColourId, pal::paint);
    setColour (juce::ComboBox::arrowColourId, pal::signal);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1411));
    setColour (juce::PopupMenu::textColourId, pal::paint);
    setColour (juce::PopupMenu::headerTextColourId, pal::rustLight);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, pal::rust);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::textColourOffId, pal::paint);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0f0b09));
    setColour (juce::TextEditor::textColourId, pal::paint);
    setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff5a3420));
    setColour (juce::TextEditor::focusedOutlineColourId, pal::rustLight);
    setColour (juce::TextEditor::highlightColourId, pal::rust.withAlpha (0.6f));
    setColour (juce::CaretComponent::caretColourId, pal::signalHot);

    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff1a1411));
    setColour (juce::AlertWindow::textColourId, pal::paint);
    setColour (juce::AlertWindow::outlineColourId, pal::rust);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0f0b09));
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff6a3a20));

    rustDisc = juce::Image (juce::Image::ARGB, 128, 128, true);
    juce::Image::BitmapData bd (rustDisc, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < 128; ++y)
        for (int x = 0; x < 128; ++x)
        {
            const float n = tex::fbm ((float) x / 22.0f, (float) y / 22.0f, 501, 4);
            const float a = tex::smoothstep (0.5f, 0.7f, n);
            const auto c = tex::rustAt ((float) x * 2.0f, (float) y * 2.0f, 0.3f, 777);
            bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (c.r, c.g, c.b, a * 0.85f));
        }
}

juce::Font GlitchLookAndFeel::getComboBoxFont (juce::ComboBox&) { return tex::plainBold (13.0f); }
juce::Font GlitchLookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return tex::stencil (juce::jmin (15.0f, (float) h * 0.62f)).withExtraKerningFactor (0.08f); }

void GlitchLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 3.0f);
    g.setGradientFill (juce::ColourGradient (down ? pal::rustDeep : juce::Colour (0xff3a2a20), 0, r.getY(),
                                             down ? juce::Colour (0xff2a120a) : juce::Colour (0xff17110e), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (hi ? pal::rustLight : juce::Colour (0xff6a3a20)); g.drawRoundedRectangle (r, 3.0f, 1.2f);
}

void GlitchLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colour (0xff0f0b09)); g.fillRoundedRectangle (r, 3.0f);
    g.setColour (box.isMouseOver (true) ? pal::rustLight : juce::Colour (0xff6a3a20)); g.drawRoundedRectangle (r, 3.0f, 1.2f);
    juce::Path arrow; const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 5, ay - 3, ax + 5, ay - 3, ax, ay + 4);
    g.setColour (pal::signal); g.fillPath (arrow);
}

void GlitchLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& s)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float thick = juce::jmax (2.5f, r * 0.11f);
    const float alpha = s.isEnabled() ? 1.0f : 0.3f;

    // tick marks stamped round the dial
    g.setColour (pal::paintDim.withAlpha (0.45f * alpha));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle) - juce::MathConstants<float>::halfPi;
        const float r1 = r + 1.0f, r0 = r - (i % 5 == 0 ? 5.0f : 3.0f);
        g.drawLine (c.x + std::cos (a) * r0, c.y + std::sin (a) * r0, c.x + std::cos (a) * r1, c.y + std::sin (a) * r1, i % 5 == 0 ? 1.4f : 0.9f);
    }

    const float ar = r - thick - 4.0f;
    juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff120e0c).withAlpha (alpha)); g.strokePath (track, juce::PathStrokeType (thick + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xff3a2418).withAlpha (alpha)); g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path val; val.addCentredArc (c.x, c.y, ar, ar, 0, startAngle, angle, true);
    const auto arcCol = (s.isMouseOverOrDragging() ? pal::signalHot : pal::signal).withAlpha (alpha);
    g.setColour (arcCol.withAlpha (0.25f * alpha));
    g.strokePath (val, juce::PathStrokeType (thick + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (arcCol);
    g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float br = ar - thick * 1.4f;
    if (br < 4.0f) return;
    const juce::Rectangle<float> body (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillEllipse (body.translated (1.5f, 2.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a5a4c).withAlpha (alpha), c.x - br * 0.5f, c.y - br * 0.7f,
                                             juce::Colour (0xff17120f), c.x + br * 0.6f, c.y + br, true));
    g.fillEllipse (body);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addEllipse (body);
        g.reduceClipRegion (clip);
        const float off = (float) (((juce::uint64) (juce::pointer_sized_int) &s >> 4) % 64);
        g.setOpacity (0.9f * alpha);
        g.drawImage (rustDisc, body.expanded (br * 0.4f).translated (-off * 0.3f, off * 0.2f), juce::RectanglePlacement::stretchToFit);
    }
    g.setColour (juce::Colour (0xff0f0b09).withAlpha (0.8f));
    for (int i = 0; i < 32; ++i)
    {
        const float a = (float) i / 32.0f * juce::MathConstants<float>::twoPi;
        g.drawLine (c.x + std::cos (a) * br * 0.87f, c.y + std::sin (a) * br * 0.87f,
                    c.x + std::cos (a) * br, c.y + std::sin (a) * br, 1.0f);
    }
    g.setColour (juce::Colour (0xff0b0807)); g.drawEllipse (body, 1.2f);

    juce::Path p; p.addRoundedRectangle (-1.8f, -br * 0.92f, 3.6f, br * 0.6f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c.x + 0.8f, c.y + 1.0f));
    g.setColour (pal::paint.withAlpha (alpha));
    g.fillPath (p, juce::AffineTransform::rotation (angle).translated (c));
}

namespace colours   // eyes, dialogs and accents (rusty-family values)
{
const juce::Colour blood    { 0xffb3141c };
const juce::Colour bloodHot { 0xffff3b22 };
}

//==============================================================================  (shared with Possessed)
//==============================================================================
void GlitchBitchEditor::refreshPresetBox()
{
    lastPresetVersion = proc.presetVersion.load();
    userFiles = GlitchBitchProcessor::getUserPresets();
    const auto current = proc.getCurrentPresetName();

    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Factory");
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.addSeparator();
    presetBox.addSectionHeading ("User Presets");
    if (userFiles.isEmpty()) { presetBox.addItem ("(none saved yet)", 999); presetBox.setItemEnabled (999, false); }
    for (int i = 0; i < userFiles.size(); ++i) presetBox.addItem (userFiles[i].getFileNameWithoutExtension(), kUserIdBase + i);

    int selected = 0;
    for (int i = 0; i < userFiles.size() && selected == 0; ++i)
        if (userFiles[i] == GlitchBitchProcessor::fileForPresetName (current)) selected = kUserIdBase + i;
    for (int i = 0; i < proc.getNumPrograms() && selected == 0; ++i)
        if (proc.getProgramName (i) == current) selected = i + 1;
    if (selected != 0) presetBox.setSelectedId (selected, juce::dontSendNotification);
    else presetBox.setText (current, juce::dontSendNotification);
    deleteButton.setEnabled (selected >= kUserIdBase);
}

void GlitchBitchEditor::stepPreset (int delta)
{
    std::vector<int> ids;
    for (int i = 0; i < proc.getNumPrograms(); ++i) ids.push_back (i + 1);
    for (int i = 0; i < userFiles.size(); ++i) ids.push_back (kUserIdBase + i);
    auto it = std::find (ids.begin(), ids.end(), presetBox.getSelectedId());
    const int pos = it == ids.end() ? 0 : (int) (it - ids.begin());
    const int n = (int) ids.size();
    presetBox.setSelectedId (ids[(size_t) (((pos + delta) % n + n) % n)], juce::sendNotificationSync);
}

void GlitchBitchEditor::showDialog (const juce::String& title, const juce::String& message, bool withTextField, const juce::String& initialText,
                                  juce::StringArray buttons, std::function<void (int, const juce::String&)> onResult)
{
    juce::Component::SafePointer<GlitchBitchEditor> safe (this);
    dialog = std::make_unique<PresetDialog> (title, message, withTextField, initialText, buttons,
        [safe, onResult] (int index, const juce::String& text)
        {
            // close first, then act (the result may open another dialog)
            juce::MessageManager::callAsync ([safe, onResult, index, text]
            {
                if (safe == nullptr) return;
                safe->dialog.reset();
                if (onResult) onResult (index, text);
            });
        });
    addAndMakeVisible (*dialog);
    dialog->setBounds (getLocalBounds());
}

void GlitchBitchEditor::showMessage (const juce::String& title, const juce::String& text)
{
    showDialog (title, text, false, {}, { "OK" }, {});
}

void GlitchBitchEditor::promptSave()
{
    const bool onUser = presetBox.getSelectedId() >= kUserIdBase;
    juce::Component::SafePointer<GlitchBitchEditor> safe (this);
    auto doSave = [safe] (const juce::String& name)
    {
        if (safe == nullptr) return;
        if (! safe->proc.saveUserPreset (name))
            safe->showMessage ("Couldn't save preset", "Glitch Bitch couldn't write the preset file to:\n"
                                                         + GlitchBitchProcessor::getUserPresetFolder().getFullPathName());
        else safe->refreshPresetBox();
    };

    showDialog ("SAVE PRESET", "Name your preset:", true, onUser ? proc.getCurrentPresetName() : juce::String(), { "Save", "Cancel" },
        [safe, doSave] (int button, const juce::String& text)
        {
            if (button != 0 || safe == nullptr) return;
            const auto name = text.trim();
            if (name.isEmpty()) { safe->showMessage ("SAVE PRESET", "Please enter a name for the preset."); return; }
            for (int i = 0; i < safe->proc.getNumPrograms(); ++i)
                if (safe->proc.getProgramName (i).equalsIgnoreCase (name))
                {
                    safe->showMessage ("SAVE PRESET", "\"" + name + "\" is a factory preset name. Please choose another name.");
                    return;
                }
            if (GlitchBitchProcessor::fileForPresetName (name).existsAsFile())
            {
                safe->showDialog ("REPLACE PRESET?", "A preset named \"" + name + "\" already exists. Replace it with the current settings?",
                                  false, {}, { "Replace", "Cancel" },
                                  [doSave, name] (int b, const juce::String&) { if (b == 0) doSave (name); });
                return;
            }
            doSave (name);
        });
}

void GlitchBitchEditor::confirmDelete()
{
    const int id = presetBox.getSelectedId();
    if (id < kUserIdBase || id - kUserIdBase >= userFiles.size()) return;
    const auto file = userFiles[id - kUserIdBase];
    juce::Component::SafePointer<GlitchBitchEditor> safe (this);
    showDialog ("DELETE PRESET?", "Delete \"" + file.getFileNameWithoutExtension() + "\"? This can't be undone.", false, {},
                { "Delete", "Cancel" }, [safe, file] (int b, const juce::String&)
                {
                    if (b != 0 || safe == nullptr) return;
                    file.deleteFile();
                    safe->refreshPresetBox();
                });
}
//==============================================================================
PresetDialog::PresetDialog (const juce::String& t, const juce::String& m, bool withTextField, const juce::String& initialText,
                            juce::StringArray names, std::function<void (int, const juce::String&)> cb)
    : title (t), message (m), hasField (withTextField), onClose (std::move (cb))
{
    setWantsKeyboardFocus (true);
    if (hasField)
    {
        field.setText (initialText, false);
        field.setInputRestrictions (60);
        field.setSelectAllWhenFocused (true);
        field.setFont (juce::Font (juce::FontOptions (15.0f)));
        field.setIndents (8, 6);
        field.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0f0b09));
        field.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff5a3420));
        field.setColour (juce::TextEditor::focusedOutlineColourId, pal::rustLight);
        field.setColour (juce::TextEditor::textColourId, pal::paint);
        field.setColour (juce::TextEditor::highlightColourId, pal::rust.withAlpha (0.6f));
        field.setColour (juce::CaretComponent::caretColourId, pal::signalHot);
        field.onReturnKey = [this] { finish (0); };
        field.onEscapeKey = [this] { finish ((int) buttons.size() - 1); };
        addAndMakeVisible (field);
    }
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (names[i]));
        b->onClick = [this, i] { finish (i); };
        addAndMakeVisible (b);
    }
}

void PresetDialog::parentHierarchyChanged()
{
    if (isShowing())
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PresetDialog> (this)]
        {
            if (sp == nullptr) return;
            if (sp->hasField) sp->field.grabKeyboardFocus(); else sp->grabKeyboardFocus();
        });
}

void PresetDialog::finish (int index)
{
    if (onClose == nullptr) return;
    auto cb = std::move (onClose);
    onClose = nullptr;
    cb (index, field.getText());
}

bool PresetDialog::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::returnKey) { finish (0); return true; }
    if (k == juce::KeyPress::escapeKey) { finish ((int) buttons.size() - 1); return true; }
    return true; // keep keys from leaking to the host while the dialog is open
}

void PresetDialog::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre (380, hasField ? 190 : 160);
    auto r = panel.reduced (20);
    r.removeFromTop (30); // title
    r.removeFromTop (hasField ? 22 : 50); // message
    if (hasField) { field.setBounds (r.removeFromTop (32)); r.removeFromTop (16); }
    auto row = r.removeFromBottom (28);
    for (int i = (int) buttons.size() - 1; i >= 0; --i) { buttons[i]->setBounds (row.removeFromRight (96)); row.removeFromRight (8); }
}

void PresetDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.72f));
    auto p = panel.toFloat();
    g.setColour (juce::Colour (0xff1a1411)); g.fillRect (p);
    g.setGradientFill (juce::ColourGradient (pal::rustDeep.withAlpha (0.5f), p.getX(), p.getY(), juce::Colours::transparentBlack, p.getX(), p.getY() + p.getHeight() * 0.6f, false));
    g.fillRect (p);
    g.setColour (pal::rust); g.drawRect (p, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRect (p.reduced (2.0f), 1.0f);
    for (auto pt : { juce::Point<float> (p.getX() + 8, p.getY() + 8), { p.getRight() - 8, p.getY() + 8 }, { p.getX() + 8, p.getBottom() - 8 }, { p.getRight() - 8, p.getBottom() - 8 } })
    {
        g.setColour (juce::Colour (0xff6a5a4c)); g.fillEllipse (pt.x - 3, pt.y - 3, 6, 6);
        g.setColour (pal::rust.withAlpha (0.6f)); g.drawEllipse (pt.x - 3, pt.y - 3, 6, 6, 1.0f);
    }
    auto r = panel.reduced (22);
    g.setColour (pal::signalHot); g.setFont (tex::stencil (19.0f).withExtraKerningFactor (0.1f));
    g.drawText (title, r.removeFromTop (30), juce::Justification::centredLeft);
    g.setColour (pal::paint); g.setFont (tex::plainBold (13.0f));
    g.drawFittedText (message, r.removeFromTop (hasField ? 22 : 50), juce::Justification::topLeft, 3);
}

//==============================================================================
void PowerButton::paintButton (juce::Graphics& g, bool hi, bool)
{
    // indicator lamp + stencilled label on a rusty plate (Dominatrix / Chop Shop style)
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    const auto lit = pal::signalHot;
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 3.0f);
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff4a1a10) : juce::Colour (0xff2a221d), 0, r.getY(),
                                             juce::Colour (0xff120d0b), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (on ? lit.withAlpha (0.85f) : (hi ? pal::rustLight : juce::Colour (0xff5a3420))); g.drawRoundedRectangle (r, 3.0f, 1.2f);
    const float d = r.getHeight() * 0.42f;
    auto led = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + r.getHeight() * 0.55f, r.getCentreY() });
    if (on)
    {
        juce::Path lp; lp.addEllipse (led);
        juce::DropShadow (lit.withAlpha (0.9f), 10, {}).drawForPath (g, lp);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff0c0), led.getCentreX(), led.getCentreY(), lit, led.getX(), led.getY(), true));
    }
    else g.setColour (juce::Colour (0xff2a1c16));
    g.fillEllipse (led);
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawEllipse (led, 1.0f);
    g.setColour (on ? pal::paint : pal::paintDim);
    g.setFont (tex::stencil (13.5f).withExtraKerningFactor (0.1f));
    g.drawFittedText (getButtonText(), r.withTrimmedLeft (r.getHeight() * 0.9f).withTrimmedRight (4).toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void StepButton::paintButton (juce::Graphics& g, bool hi, bool)
{
    // chunky rusty pad; lit (cut) pads glow like a hot coil
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const bool cut = getToggleState();
    const float alpha = active ? 1.0f : 0.3f;
    g.setColour (juce::Colours::black.withAlpha (0.6f * alpha)); g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 3.0f);
    if (cut)
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb2361a).withAlpha (alpha), r.getCentreX(), r.getY(),
                                                 juce::Colour (0xff4a0e06).withAlpha (alpha), r.getCentreX(), r.getBottom(), false));
    else
        g.setGradientFill (juce::ColourGradient ((hi ? juce::Colour (0xff43332a) : juce::Colour (0xff302520)).withAlpha (alpha), r.getCentreX(), r.getY(),
                                                 juce::Colour (0xff120d0b).withAlpha (alpha), r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour ((cut ? pal::signalHot : (hi ? pal::rustLight : juce::Colour (0xff5a3420))).withAlpha (alpha));
    g.drawRoundedRectangle (r, 3.0f, cut ? 1.4f : 1.1f);
    g.setColour (juce::Colours::black.withAlpha (0.25f * alpha));
    g.drawLine (r.getRight() - 12, r.getBottom() - 3, r.getRight() - 3, r.getBottom() - 8, 1.0f);
    if (current && running)
    {
        juce::Path p; p.addRoundedRectangle (r, 3.0f);
        juce::DropShadow (pal::hazard.withAlpha (0.9f), 9, {}).drawForPath (g, p);
        g.setColour (pal::hazard); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 2.0f);
    }
}

//==============================================================================
// THE GLITCH SCREEN
void GlitchScreen::update (float dt)
{
    time += dt;
    const int w = proc.scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < GlitchBitchProcessor::kScopeSize; ++i)
        vals[(size_t) i] = proc.scope[(size_t) ((w + i) % GlitchBitchProcessor::kScopeSize)].load (std::memory_order_relaxed);
    active = proc.uiActive.load(); pending = proc.uiPending.load();
    tear += ((active ? 1.0f : 0.0f) - tear) * std::min (1.0f, dt * (active ? 18.0f : 4.0f));
    rgb = (rnd.nextFloat() < 0.02f + tear * 0.5f) ? rnd.nextFloat() : rgb * 0.6f;
    repaint();
}

void GlitchScreen::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float W = b.getWidth(), H = b.getHeight(), mid = b.getCentreY();
    juce::Random fr ((juce::int64) (time * (active ? 24.0f : 3.0f)));   // tearing changes at frame rate while firing

    auto drawWave = [&] (float dx, juce::Colour col, float thick)
    {
        const int bands = 7;
        for (int band = 0; band < bands; ++band)
        {
            const float y0 = H * (float) band / (float) bands, y1 = H * (float) (band + 1) / (float) bands;
            const float shift = tear > 0.05f && fr.nextFloat() < 0.6f ? (fr.nextFloat() * 2.0f - 1.0f) * 26.0f * tear : 0.0f;
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (juce::Rectangle<float> (0, y0, W, y1 - y0).toNearestInt());
            g.setColour (col);
            const int n = (int) vals.size();
            for (int x = 0; x < (int) W; x += 2)
            {
                const int idx = juce::jlimit (0, n - 1, (int) ((float) x / W * (float) n));
                const float v = std::min (1.0f, std::sqrt (vals[(size_t) idx]) * 0.95f);
                const float hh = std::max (1.0f, v * H * 0.42f);
                g.fillRect ((float) x + dx + shift, mid - hh, thick, hh * 2.0f);
            }
        }
    };
    if (tear > 0.02f || rgb > 0.05f)
    {
        const float off = 2.0f + 5.0f * std::max (tear, rgb);
        drawWave (off, juce::Colour (0xffff2a1a).withAlpha (0.55f), 1.4f);
        drawWave (-off, juce::Colour (0xff3ad0c8).withAlpha (0.35f * std::max (tear, rgb)), 1.4f);
    }
    drawWave (0.0f, (active ? pal::signalHot : pal::paint).withAlpha (0.9f), 1.5f);

    // scanlines + glass
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    for (float y = 0; y < H; y += 3.0f) g.fillRect (0.0f, y, W, 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), 0, 0, juce::Colours::transparentWhite, 0, H * 0.5f, false));
    g.fillRect (b);
    if (active && fr.nextFloat() < 0.25f) { g.setColour (juce::Colours::white.withAlpha (0.05f + 0.1f * fr.nextFloat())); g.fillRect (0.0f, fr.nextFloat() * H, W, 2.0f + fr.nextFloat() * 8.0f); }

    g.setFont (tex::stencil (12.0f).withExtraKerningFactor (0.15f));
    if (active)       { g.setColour (pal::signalHot.withAlpha (std::fmod (time, 0.5f) < 0.3f ? 1.0f : 0.35f)); g.drawText ("GLITCHING", b.reduced (8, 5), juce::Justification::topLeft); }
    else if (pending) { g.setColour (pal::hazard.withAlpha (std::fmod (time, 0.3f) < 0.15f ? 1.0f : 0.4f)); g.drawText ("WAITING FOR THE GRID", b.reduced (8, 5), juce::Justification::topLeft); }
    else              { g.setColour (pal::paintDim.withAlpha (0.5f)); g.drawText ("OUTPUT", b.reduced (8, 5), juce::Justification::topLeft); }
}

//==============================================================================
// THE BUTTON
TheButton::TheButton (GlitchBitchProcessor& p) : proc (p)
{
    param = proc.apvts.getParameter ("glitch");
    setTooltip ("THE BUTTON. Fires every ARMED effect. Hold mode: fires while held. Latch mode: click on, click off. "
                "It's an automatable parameter, so you can record it or map it to a pad with your DAW.");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

bool TheButton::isLatch() const { return proc.apvts.getRawParameterValue ("buttonMode")->load() > 0.5f; }

void TheButton::setGlitch (bool on)
{
    if (param == nullptr) return;
    param->setValueNotifyingHost (on ? 1.0f : 0.0f);
}

bool TheButton::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float R = (float) std::min (getWidth(), getHeight()) * 0.5f;
    return juce::Point<float> ((float) x, (float) y).getDistanceFrom (c) < R * 0.9f;
}

void TheButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || param == nullptr) return;
    down = true;
    param->beginChangeGesture();
    if (isLatch()) { setGlitch (param->getValue() < 0.5f); param->endChangeGesture(); }
    else setGlitch (true);   // the gesture stays open while held, so touch automation records the whole hold
    repaint();
}

void TheButton::mouseUp (const juce::MouseEvent&)
{
    if (! down) return;
    down = false;
    if (! isLatch() && param != nullptr) { setGlitch (false); param->endChangeGesture(); }
    repaint();
}

void TheButton::update (float dt)
{
    const bool on = proc.uiActive.load();
    const bool wait = proc.uiPending.load() || (param != nullptr && param->getValue() > 0.5f && ! on);
    const float target = on ? 1.0f : (wait ? 0.45f : 0.0f);
    const float old = glow;
    glow += (target - glow) * std::min (1.0f, dt * (target > glow ? 20.0f : 5.0f));
    pulse += dt;
    if (on != lit || wait != pending || std::abs (glow - old) > 0.003f || wait || on) { lit = on; pending = wait; repaint(); }
}

void TheButton::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float R = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f;
    const auto c = bounds.getCentre();
    auto circle = [&] (float r, juce::Point<float> at) { return juce::Rectangle<float> (at.x - r, at.y - r, r * 2, r * 2); };
    const float pend = pending && ! lit ? 0.5f + 0.5f * std::sin (pulse * 14.0f) : 0.0f;
    const float heat = std::max (glow, pend * 0.6f);

    // drop shadow + iron bezel
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (circle (R, c.translated (3.0f, 5.0f)));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6a5a4c), c.x - R * 0.6f, c.y - R * 0.8f, juce::Colour (0xff120e0c), c.x + R, c.y + R, true));
    g.fillEllipse (circle (R, c));
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addEllipse (circle (R, c)); g.reduceClipRegion (clip);
        if (auto* lf = dynamic_cast<GlitchLookAndFeel*> (&getLookAndFeel()))
        { g.setOpacity (0.85f); g.drawImage (lf->rustDisc, circle (R * 1.3f, c.translated (-R * 0.2f, R * 0.1f)), juce::RectanglePlacement::stretchToFit); g.setOpacity (1.0f); }
    }
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawEllipse (circle (R, c), 1.5f);
    for (int i = 0; i < 8; ++i)   // bolts
    {
        const float a = (float) i / 8.0f * juce::MathConstants<float>::twoPi + 0.39f;
        const auto bp = c + juce::Point<float> (std::cos (a), std::sin (a)) * R * 0.92f;
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (circle (4.2f, bp.translated (1.0f, 1.5f)));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9a8a78), bp.x - 2, bp.y - 2, juce::Colour (0xff2a201a), bp.x + 4, bp.y + 4, true));
        g.fillEllipse (circle (4.2f, bp));
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawLine (bp.x - 2.5f, bp.y, bp.x + 2.5f, bp.y, 1.0f);
    }

    // worn hazard ring
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path ring; ring.addEllipse (circle (R * 0.84f, c)); ring.addEllipse (circle (R * 0.72f, c)); ring.setUsingNonZeroWinding (false);
        g.reduceClipRegion (ring);
        g.setColour (juce::Colour (0xff141010)); g.fillEllipse (circle (R * 0.84f, c));
        g.setColour (pal::hazard.withAlpha (0.85f));
        for (float x = -R * 2.0f; x < R * 2.0f; x += 16.0f)
        {
            juce::Path s; s.addQuadrilateral (c.x + x, c.y - R, c.x + x + 8.0f, c.y - R, c.x + x + 8.0f - R * 2.0f, c.y + R, c.x + x - R * 2.0f, c.y + R);
            g.fillPath (s);
        }
        juce::Random wr (7);
        g.setColour (juce::Colour (0xff141010).withAlpha (0.7f));
        for (int i = 0; i < 40; ++i)   // paint chips
        {
            const float a = wr.nextFloat() * juce::MathConstants<float>::twoPi, rr = R * (0.72f + wr.nextFloat() * 0.12f);
            g.fillEllipse (circle (1.0f + wr.nextFloat() * 3.5f, c + juce::Point<float> (std::cos (a), std::sin (a)) * rr));
        }
    }
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (circle (R * 0.84f, c), 1.5f); g.drawEllipse (circle (R * 0.72f, c), 2.0f);

    // the dome (sinks when pressed)
    const float dr = R * (down ? 0.64f : 0.66f);
    const auto dc = c.translated (0.0f, down ? 2.5f : 0.0f);
    juce::Path dome; dome.addEllipse (circle (dr, dc));
    if (heat > 0.01f)
    {
        juce::DropShadow (pal::signalHot.withAlpha (0.85f * heat), (int) (R * 0.5f), {}).drawForPath (g, dome);
        juce::DropShadow (colours::blood.withAlpha (0.9f * heat), (int) (R * 0.2f), {}).drawForPath (g, dome);
    }
    {
        const auto hot1 = juce::Colour (0xffff7a4a), hot2 = juce::Colour (0xffc41a10), hot3 = juce::Colour (0xff4a0404);
        const auto cold1 = juce::Colour (0xff9a3222), cold2 = juce::Colour (0xff5a140c), cold3 = juce::Colour (0xff1a0604);
        juce::ColourGradient grad (cold1.interpolatedWith (hot1, heat), dc.x - dr * 0.35f, dc.y - dr * 0.4f,
                                   cold3.interpolatedWith (hot3, heat), dc.x + dr * 0.9f, dc.y + dr, true);
        grad.addColour (0.45, cold2.interpolatedWith (hot2, heat));
        g.setGradientFill (grad);
        g.fillPath (dome);
    }
    {
        juce::Graphics::ScopedSaveState ss (g);   // grime on the dome
        g.reduceClipRegion (dome);
        juce::Random gr (99);
        for (int i = 0; i < 90; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.05f + gr.nextFloat() * 0.12f));
            g.fillEllipse (circle (1.0f + gr.nextFloat() * 6.0f, dc + juce::Point<float> (gr.nextFloat() * 2 - 1, gr.nextFloat() * 2 - 1) * dr));
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (down ? 0.12f : 0.28f), dc.x - dr * 0.3f, dc.y - dr * 0.75f,
                                                 juce::Colours::transparentWhite, dc.x - dr * 0.1f, dc.y - dr * 0.05f, false));
        g.fillEllipse (dc.x - dr * 0.72f, dc.y - dr * 0.9f, dr * 1.3f, dr * 0.8f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.85f)); g.strokePath (dome, juce::PathStrokeType (2.0f));

    // stencilled label
    g.setFont (tex::stencil (dr * 0.42f).withExtraKerningFactor (0.06f));
    const auto txt = juce::Rectangle<float> (dc.x - dr, dc.y - dr * 0.32f, dr * 2.0f, dr * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawText ("GLITCH", txt.translated (1.5f, 2.0f), juce::Justification::centred);
    g.setColour (pal::paint.withAlpha (0.75f + 0.25f * heat)); g.drawText ("GLITCH", txt, juce::Justification::centred);
    g.setFont (tex::stencil (dr * 0.14f).withExtraKerningFactor (0.3f));
    g.setColour (pal::paint.withAlpha (0.55f));
    g.drawText (isLatch() ? "LATCH" : "HOLD", juce::Rectangle<float> (dc.x - dr, dc.y + dr * 0.22f, dr * 2.0f, dr * 0.2f), juce::Justification::centred);
}

//==============================================================================
// CAPTURE TRAY TILES
ClipTile::ClipTile()
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void ClipTile::setClip (const CapturedClip* c, int n)
{
    hasClip = c != nullptr;
    if (hasClip) clip = *c; else clip = CapturedClip();
    number = n;
    setTooltip (hasClip ? clip.file.getFileName() + "\nDrag onto a track. Right-click for more." : juce::String());
    setMouseCursor (hasClip ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

void ClipTile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const bool hover = isMouseOver (true) && hasClip;
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (1.5f, 2.0f), 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff100c0a), r.getX(), r.getY(), juce::Colour (0xff1e1410), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);

    if (recording)
    {
        const float a = 0.55f + 0.45f * std::sin (blink * 9.0f);
        g.setColour (colours::bloodHot.withAlpha (a)); g.fillEllipse (r.getCentreX() - 9, r.getCentreY() - 26, 18, 18);
        g.setFont (tex::stencil (16.0f).withExtraKerningFactor (0.15f));
        g.setColour (pal::signalHot.withAlpha (a)); g.drawText ("CAPTURING", r.withTrimmedTop (r.getHeight() * 0.5f).withHeight (22), juce::Justification::centred);
        g.setColour (pal::signalHot.withAlpha (0.8f)); g.drawRoundedRectangle (r, 4.0f, 1.6f);
        return;
    }
    if (! hasClip)
    {
        g.setColour (juce::Colour (0xff3a2418)); g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (pal::paintDim.withAlpha (0.18f)); g.setFont (tex::stencil (13.0f).withExtraKerningFactor (0.2f));
        g.drawText ("EMPTY", r, juce::Justification::centred);
        return;
    }

    auto in = r.reduced (8.0f, 6.0f);
    auto top = in.removeFromTop (16);
    g.setFont (tex::stencil (14.0f).withExtraKerningFactor (0.08f));
    g.setColour (pal::signalHot); g.drawText ("#" + juce::String (number).paddedLeft ('0', 3), top, juce::Justification::centredLeft);
    g.setColour (pal::paintDim); g.setFont (tex::plainBold (11.0f));
    g.drawText (juce::String (clip.seconds, 2) + " s" + (clip.truncated ? " (MAX)" : ""), top, juce::Justification::centredRight);

    auto foot = in.removeFromBottom (14);
    auto posRow = in.removeFromBottom (18);
    g.setFont (tex::stencil (13.0f).withExtraKerningFactor (0.1f));
    g.setColour (pal::paint); g.drawText (clip.positionText(), posRow, juce::Justification::centredLeft);
    g.setFont (tex::plainBold (9.5f));
    g.setColour (hover ? pal::hazard : pal::paintDim.withAlpha (0.6f));
    g.drawText (dragging ? "DROP IT ON A TRACK" : "DRAG TO A TRACK", foot, juce::Justification::centredLeft);

    // waveform
    auto wf = in.reduced (0, 4);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRect (wf);
    if (! clip.peaks.empty())
    {
        const float mid = wf.getCentreY(), hh = wf.getHeight() * 0.48f;
        const int cols = std::max (1, (int) wf.getWidth());
        g.setColour (hover ? pal::signalHot : pal::paint.withAlpha (0.85f));
        for (int x = 0; x < cols; ++x)
        {
            const size_t a = (size_t) ((double) x / cols * (double) clip.peaks.size());
            const size_t b = std::max (a + 1, (size_t) ((double) (x + 1) / cols * (double) clip.peaks.size()));
            float v = 0; for (size_t k = a; k < std::min (b, clip.peaks.size()); ++k) v = std::max (v, clip.peaks[k]);
            const float h = std::max (0.5f, std::min (1.0f, v) * hh);
            g.fillRect (wf.getX() + (float) x, mid - h, 1.0f, h * 2.0f);
        }
    }
    else { g.setColour (pal::paintDim.withAlpha (0.4f)); g.setFont (tex::plainBold (10.0f)); g.drawText ("loading...", wf, juce::Justification::centred); }

    g.setColour (hover ? pal::hazard : juce::Colour (0xff5a3420)); g.drawRoundedRectangle (r, 4.0f, hover ? 1.8f : 1.1f);
}

void ClipTile::mouseDown (const juce::MouseEvent& e)
{
    if (! hasClip || ! e.mods.isPopupMenu()) return;
    juce::PopupMenu m;
   #if JUCE_MAC
    m.addItem (1, "Show in Finder");
   #else
    m.addItem (1, "Show in Explorer");
   #endif
    m.addItem (2, "Remove from tray (keep the file)");
    m.addItem (3, "Delete the file...");
    const auto f = clip.file;
    juce::Component::SafePointer<ClipTile> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, f] (int r)
    {
        if (safe == nullptr) return;
        if (r == 1 && safe->onReveal) safe->onReveal (f);
        if (r == 2 && safe->onRemove) safe->onRemove (f);
        if (r == 3 && safe->onDelete) safe->onDelete (f);
    });
}

void ClipTile::mouseDrag (const juce::MouseEvent& e)
{
    if (! hasClip || dragging || e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 5 || ! clip.file.existsAsFile()) return;
    dragging = true; repaint();
    juce::Component::SafePointer<ClipTile> safe (this);
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ clip.file.getFullPathName() }, false, this,
        [safe] { if (safe != nullptr) { safe->dragging = false; safe->repaint(); } });
}

//==============================================================================
// EDITOR
GlitchBitchEditor::GlitchBitchEditor (GlitchBitchProcessor& p)
    : AudioProcessorEditor (&p), proc (p), screen (p), theButton (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (screen);
    screen.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (theButton);

    // HOLD / LATCH
    auto setMode = [this] (int m)
    {
        if (auto* mp = proc.apvts.getParameter ("buttonMode")) mp->setValueNotifyingHost (mp->convertTo0to1 ((float) m));
        if (m == 0)   // leaving latch with the glitch latched on: let go of it
            if (auto* gp = proc.apvts.getParameter ("glitch"); gp != nullptr && gp->getValue() > 0.5f)
            { gp->beginChangeGesture(); gp->setValueNotifyingHost (0.0f); gp->endChangeGesture(); }
        refreshModeButtons();
        theButton.repaint();
    };
    holdButton.setClickingTogglesState (false); latchButton.setClickingTogglesState (false);
    holdButton.onClick = [setMode] { setMode (0); };
    latchButton.onClick = [setMode] { setMode (1); };
    holdButton.setTooltip ("HOLD: the glitch fires while you hold THE BUTTON down");
    latchButton.setTooltip ("LATCH: click THE BUTTON once to start, again to stop");
    addAndMakeVisible (holdButton); addAndMakeVisible (latchButton);
    refreshModeButtons();
    snapBox.addItemList ({ "Off", "1/16", "1/8", "1/4", "1/2", "1 Bar" }, 1);
    snapBox.setTooltip ("SNAP: wait for the next grid line before firing and before letting go, so glitches (and their captured clips) land on the beat");
    addAndMakeVisible (snapBox);
    snapAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "snap", snapBox);

    // effect plates
    const char* titles[9] = { "STUTTER", "REPEAT", "REVERSE", "SCATTER", "CRUSH", "FREEZE", "DIVE", "TAPE STOP", "TAPE START" };
    const char* arms[9] = { "stutterArm", "repeatArm", "reverseArm", "scatterArm", "crushArm", "freezeArm", "diveArm", "tapeStopArm", "tapeStartArm" };
    const char* tips[9] = {
        "STUTTER: tempo-synced chopper. Lit steps cut the volume. SPEED sets the step size.",
        "REPEAT: grabs a slice the moment the glitch fires and loops it (s-s-s-stutter). DROP bends each repeat down or up.",
        "REVERSE: plays each slice backwards as it goes by.",
        "SCATTER: jumps to random earlier slices (sometimes backwards or an octave off). CHAOS = how often, RANGE = how far back.",
        "CRUSH: bit depth and sample-rate reduction.",
        "FREEZE: holds the moment as a drifting drone under the sound.",
        "DIVE: pitch slides down (or up) while the glitch is held, and slides home after.",
        "TAPE STOP: everything slows down and grinds to a halt. With TAPE START also armed, it spins back up when you let go.",
        "TAPE START: spins up from a dead stop to full speed. With TAPE STOP also armed, this is what happens when you let go." };
    for (size_t i = 0; i < 9; ++i)
    {
        auto& pl = plates[i];
        pl.title = titles[i]; pl.armParam = arms[i];
        pl.arm.setTooltip ("Arm " + pl.title + ": it fires when you hit THE BUTTON");
        addAndMakeVisible (pl.arm);
        pl.att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, pl.armParam, pl.arm);
        juce::ignoreUnused (tips);
    }
    plates[0].knobs = { makeKnob ("stutterSpeed", "Speed"), makeKnob ("stutterSteps", "Steps"), makeKnob ("stutterDepth", "Depth"), makeKnob ("stutterSmooth", "Smooth") };
    plates[1].knobs = { makeKnob ("repeatLength", "Length"), makeKnob ("repeatDrop", "Drop") };
    plates[2].knobs = { makeKnob ("reverseLength", "Length") };
    plates[3].knobs = { makeKnob ("scatterSlice", "Slice"), makeKnob ("scatterChaos", "Chaos"), makeKnob ("scatterRange", "Range") };
    plates[4].knobs = { makeKnob ("crushBits", "Bits"), makeKnob ("crushRate", "Rate") };
    plates[5].knobs = { makeKnob ("freezeSize", "Size"), makeKnob ("freezeMix", "Mix") };
    plates[6].knobs = { makeKnob ("diveAmount", "Amount"), makeKnob ("diveTime", "Time") };
    plates[7].knobs = { makeKnob ("tapeStopTime", "Time"), makeKnob ("tapeStopShape", "Curve") };
    plates[8].knobs = { makeKnob ("tapeStartTime", "Time"), makeKnob ("tapeStartShape", "Curve") };
    for (size_t i = 0; i < 9; ++i)
        for (auto* k : plates[i].knobs) k->slider.setTooltip (tips[i]);
    plates[0].knobs[0]->slider.setTooltip ("SPEED: stutter step size, from 1/2 notes to 1/64 notes (T = triplet)");
    ioKnobs = { makeKnob ("mix", "Dry/Wet"), makeKnob ("outputDb", "Output") };
    for (int i = 0; i < 16; ++i)
    {
        steps[(size_t) i].setTooltip ("Step " + juce::String (i + 1) + ": lit = volume cut");
        addAndMakeVisible (steps[(size_t) i]);
        stepAtts[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "step" + juce::String (i), steps[(size_t) i]);
    }

    // presets
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= kUserIdBase && id - kUserIdBase < userFiles.size())
        {
            if (! proc.loadUserPreset (userFiles[id - kUserIdBase]))
                showMessage ("Couldn't load preset", "That preset file is missing or damaged.");
        }
        else if (id >= 1 && id <= proc.getNumPrograms())
            proc.setCurrentProgram (id - 1);
    };
    addAndMakeVisible (presetBox);
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { promptSave(); };
    deleteButton.onClick = [this] { confirmDelete(); };
    for (auto* b : { &prevButton, &nextButton, &saveButton, &deleteButton }) addAndMakeVisible (*b);
    refreshPresetBox();

    // capture tray
    captureButton.setTooltip ("CAPTURE: every glitch is recorded as a WAV file, with pre-roll and post-roll");
    addAndMakeVisible (captureButton);
    captureAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "captureOn", captureButton);
    for (auto* b : { &preBox, &postBox }) { b->addItemList ({ "Off", "1/16", "1/8", "1/4", "1/2", "1 Bar", "2 Bars" }, 1); addAndMakeVisible (*b); }
    preBox.setTooltip ("PRE: clean audio kept before the glitch starts");
    postBox.setTooltip ("POST: audio kept after the glitch (and its tails) end");
    preAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "preRoll", preBox);
    postAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "postRoll", postBox);
    folderButton.setTooltip ("Open the folder the captures are saved in");
    folderButton.onClick = []
    {
        auto root = GlitchBitchProcessor::getCaptureRoot();
        root.createDirectory();
        root.startAsProcess();
    };
    clearButton.setTooltip ("Empty the tray");
    clearButton.onClick = [this]
    {
        juce::Component::SafePointer<GlitchBitchEditor> safe (this);
        showDialog ("CLEAR THE TRAY?", "Remove every clip from the tray. Keep the WAV files on disk, or delete them too?", false, {},
                    { "Keep Files", "Delete Files", "Cancel" }, [safe] (int b, const juce::String&)
                    {
                        if (safe == nullptr || b == 2) return;
                        safe->proc.clearClips (b == 1);
                        safe->page = 0;
                    });
    };
    pageLeft.onClick = [this] { page = std::max (0, page - 1); refreshTray(); };
    pageRight.onClick = [this] { ++page; refreshTray(); };
    for (auto* b : { &folderButton, &clearButton, &pageLeft, &pageRight }) addAndMakeVisible (*b);
    for (auto& t : tiles)
    {
        addAndMakeVisible (t);
        t.onReveal = [] (const juce::File& f) { f.revealToUser(); };
        t.onRemove = [this] (const juce::File& f) { proc.removeClip (f, false); };
        t.onDelete = [this] (const juce::File& f)
        {
            juce::Component::SafePointer<GlitchBitchEditor> safe (this);
            showDialog ("DELETE CLIP?", "Delete \"" + f.getFileName() + "\" from disk? This can't be undone.", false, {}, { "Delete", "Cancel" },
                        [safe, f] (int b, const juce::String&) { if (safe != nullptr && b == 0) safe->proc.removeClip (f, true); });
        };
    }

    lastErrors = proc.captureErrors.load();
    setSize (1180, 884);
    refreshTray();
    startTimerHz (30);
}

GlitchBitchEditor::~GlitchBitchEditor() { setLookAndFeel (nullptr); }

GlitchBitchEditor::Knob* GlitchBitchEditor::makeKnob (const juce::String& id, const juce::String& text)
{
    auto k = std::make_unique<Knob>();
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 17);
    k->slider.setColour (juce::Slider::textBoxTextColourId, pal::paint.withAlpha (0.88f));
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff3a2418));
    k->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x90000000));
    k->slider.setMouseDragSensitivity (220);
    k->label.setText (text.toUpperCase(), juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (tex::stencil (14.0f).withExtraKerningFactor (0.1f));
    k->label.setColour (juce::Label::textColourId, pal::paintDim);
    k->label.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (k->slider); addAndMakeVisible (k->label);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    if (auto* prm = proc.apvts.getParameter (id))
        k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    knobs.push_back (std::move (k));
    return knobs.back().get();
}

void GlitchBitchEditor::placeKnobs (const std::vector<Knob*>& ks, juce::Rectangle<int> area)
{
    if (ks.empty()) return;
    const int w = area.getWidth() / (int) ks.size();
    for (auto* k : ks)
    {
        auto cell = area.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell);
    }
}

void GlitchBitchEditor::refreshModeButtons()
{
    const bool latch = proc.apvts.getRawParameterValue ("buttonMode")->load() > 0.5f;
    holdButton.setToggleState (! latch, juce::dontSendNotification);
    latchButton.setToggleState (latch, juce::dontSendNotification);
}

void GlitchBitchEditor::resized()
{
    const int W = getWidth(), H = getHeight();
    screenWell = { 14, 10, 262, 100 };
    screen.setBounds (screenWell.reduced (4, 3));
    logoArea   = { 292, 8, 336, 70 };
    badgeArea  = { 640, 14, 258, 88 };
    crestArea  = { 648, 20, 76, 76 };
    presetArea = { 912, 14, W - 926, 88 };
    presetBox.setBounds (presetArea.getX(), 36, presetArea.getWidth(), 28);
    {
        auto row = juce::Rectangle<int> (presetArea.getX(), 72, presetArea.getWidth(), 28);
        prevButton.setBounds (row.removeFromLeft (36)); row.removeFromLeft (6);
        nextButton.setBounds (row.removeFromLeft (36)); row.removeFromLeft (6);
        deleteButton.setBounds (row.removeFromRight (78)); row.removeFromRight (6);
        saveButton.setBounds (row);
    }
    stripeArea = { 0, 118, W, 12 };

    const int top = 140, gap = 8, bodyH = 466;

    // ---- THE BUTTON column
    buttonPlate = { 12, top, 268, bodyH };
    {
        auto bp = buttonPlate.reduced (14).withTrimmedTop (24);
        theButton.setBounds (bp.removeFromTop (232).withSizeKeepingCentre (232, 232));
        bp.removeFromTop (6);
        modeLabelArea = bp.removeFromTop (16);
        auto modeRow = bp.removeFromTop (28);
        holdButton.setBounds (modeRow.removeFromLeft (modeRow.getWidth() / 2 - 4)); modeRow.removeFromLeft (8);
        latchButton.setBounds (modeRow);
        bp.removeFromTop (10);
        snapLabelArea = bp.removeFromTop (16);
        snapBox.setBounds (bp.removeFromTop (26));
        bp.removeFromTop (8);
        statusArea = bp;
    }

    // ---- effects
    auto split = [gap] (juce::Rectangle<int> row, std::vector<float> weights)
    {
        std::vector<juce::Rectangle<int>> out; float total = 0; for (auto w : weights) total += w;
        const int avail = row.getWidth() - gap * (int) (weights.size() - 1);
        for (size_t i = 0; i < weights.size(); ++i)
        {
            const int w = i + 1 == weights.size() ? row.getWidth() : (int) ((float) avail * weights[i] / total);
            out.push_back (row.removeFromLeft (w));
            row.removeFromLeft (gap);
        }
        return out;
    };
    juce::Rectangle<int> fx (buttonPlate.getRight() + gap, top, W - 12 - (buttonPlate.getRight() + gap), bodyH);
    const int rowH = (bodyH - 2 * gap) / 3;
    auto row1 = fx.removeFromTop (rowH); fx.removeFromTop (gap);
    auto row2 = fx.removeFromTop (rowH); fx.removeFromTop (gap);
    auto row3 = fx;
    auto a = split (row1, { 4.7f, 1.7f, 1.65f });
    auto b = split (row2, { 2.6f, 2.0f, 2.0f, 2.0f });
    auto c = split (row3, { 2.25f, 2.25f, 3.9f });
    const std::vector<juce::Rectangle<int>> boxes { a[0], a[1], a[2], b[0], b[1], b[2], b[3], c[0], c[1] };
    for (size_t i = 0; i < 9; ++i)
    {
        auto& pl = plates[i];
        pl.bounds = boxes[i];
        pl.arm.setBounds (pl.bounds.getRight() - 72, pl.bounds.getY() + 5, 62, 22);
        auto inner = pl.bounds.reduced (10).withTrimmedTop (22);
        if (i == 0)
        {
            placeKnobs (pl.knobs, inner.removeFromLeft (262));
            inner.removeFromLeft (10);
            stepArea = inner.withSizeKeepingCentre (inner.getWidth(), std::min (inner.getHeight(), 92));
            const int cw = stepArea.getWidth() / 8, ch = stepArea.getHeight() / 2;
            for (int s = 0; s < 16; ++s)
                steps[(size_t) s].setBounds (stepArea.getX() + (s % 8) * cw + (s % 8 >= 4 ? 3 : 0) - 2, stepArea.getY() + (s / 8) * ch, cw - 2, ch - 2);
        }
        else placeKnobs (pl.knobs, inner);
    }
    ioPlate = c[2];
    {
        auto inner = ioPlate.reduced (10).withTrimmedTop (22);
        meterArea = inner.removeFromRight (60); inner.removeFromRight (10);
        placeKnobs (ioKnobs, inner);
    }

    // ---- capture tray
    trayPlate = { 12, top + bodyH + gap + 8, W - 24, H - (top + bodyH + gap + 8) - 12 };
    {
        auto hdr = juce::Rectangle<int> (trayPlate.getX(), trayPlate.getY() + 5, trayPlate.getWidth() - 10, 24);
        clearButton.setBounds (hdr.removeFromRight (70)); hdr.removeFromRight (6);
        folderButton.setBounds (hdr.removeFromRight (82)); hdr.removeFromRight (16);
        postBox.setBounds (hdr.removeFromRight (86)); postLabel = hdr.removeFromRight (46); hdr.removeFromRight (8);
        preBox.setBounds (hdr.removeFromRight (86)); preLabel = hdr.removeFromRight (38); hdr.removeFromRight (16);
        captureButton.setBounds (hdr.removeFromRight (100));
        auto inner = trayPlate.reduced (12).withTrimmedTop (26);
        trayHint = inner.removeFromBottom (16);
        inner.removeFromBottom (4);
        tilesArea = inner;
        pageLeft.setBounds (inner.removeFromLeft (28).withSizeKeepingCentre (28, 60)); inner.removeFromLeft (6);
        pageRight.setBounds (inner.removeFromRight (28).withSizeKeepingCentre (28, 60)); inner.removeFromRight (6);
        const int tw = inner.getWidth() / kTiles;
        for (int i = 0; i < kTiles; ++i) tiles[(size_t) i].setBounds (inner.getX() + i * tw, inner.getY(), tw - 6, inner.getHeight());
    }
    if (dialog != nullptr) dialog->setBounds (getLocalBounds());
    buildBackground();
}

void GlitchBitchEditor::buildBackground()
{
    const float scale = 2.0f;
    const int W = getWidth(), H = getHeight();
    if (W <= 0 || H <= 0) return;
    juce::Image base (juce::Image::RGB, W, H, false);

    struct PlateInfo { juce::Rectangle<float> r; juce::String title; bool hot; };
    std::vector<PlateInfo> plateInfo;
    plateInfo.push_back ({ buttonPlate.toFloat(), "THE BUTTON", true });
    for (auto& pl : plates) plateInfo.push_back ({ pl.bounds.toFloat(), pl.title, false });
    plateInfo.push_back ({ ioPlate.toFloat(), "IN / OUT", false });
    plateInfo.push_back ({ trayPlate.toFloat(), "CAPTURE TRAY", true });
    plateInfo.push_back ({ badgeArea.toFloat(), {}, false });
    const auto stripe = stripeArea.toFloat();
    const auto well = screenWell.toFloat();

    {
        juce::Image::BitmapData bd (base, juce::Image::BitmapData::writeOnly);
        for (int py = 0; py < H; ++py)
        {
            const float y = (float) py;
            for (int px = 0; px < W; ++px)
            {
                const float x = (float) px;
                tex::RGB col;
                int inside = -1; float edge = 1e9f; bool shadow = false;
                for (int i = 0; i < (int) plateInfo.size(); ++i)
                {
                    const auto& pr = plateInfo[(size_t) i].r;
                    if (pr.contains (x, y))
                    {
                        inside = i;
                        edge = std::min ({ x - pr.getX(), pr.getRight() - x, y - pr.getY(), pr.getBottom() - y });
                        break;
                    }
                    if (pr.translated (3.0f, 4.0f).contains (x, y)) shadow = true;
                }
                if (well.translated (3.0f, 4.0f).contains (x, y) && ! well.contains (x, y)) shadow = true;

                if (stripe.contains (x, y))
                {
                    const bool red = std::fmod (x + y + 1000.0f, 28.0f) < 14.0f;
                    const float wear = tex::fbm (x / 12.0f, y / 4.0f, 2024, 3);
                    col = red ? tex::rgb (juce::Colour (0xff8a1810)) : tex::rgb (juce::Colour (0xff141010));
                    col = tex::mix (col, tex::rustAt (x, y, 0.3f, 99), tex::smoothstep (0.5f, 0.68f, wear));
                    col = tex::mul (col, 0.85f + 0.15f * tex::hash (px, py, 3));
                }
                else if (well.contains (x, y))
                {
                    const float cx = (x - well.getCentreX()) / (well.getWidth() * 0.5f), cy = (y - well.getCentreY()) / (well.getHeight() * 0.5f);
                    const float haze = juce::jlimit (0.0f, 1.0f, 1.0f - std::sqrt (cx * cx * 0.7f + cy * cy));
                    const float smoke = tex::fbm (x / 26.0f, y / 18.0f, 666, 4);
                    col = tex::mix (tex::rgb (juce::Colour (0xff040303)), tex::rgb (juce::Colour (0xff1e0806)), haze * (0.45f + 0.55f * smoke));
                }
                else if (inside >= 0)
                {
                    const float d = tex::fbm (x / 30.0f, y / 30.0f, 211, 3);
                    tex::RGB steel = tex::mix (tex::rgb (juce::Colour (0xff1e1a17)), tex::rgb (juce::Colour (0xff332c27)), d);
                    const float u = std::fmod (x + y + 2000.0f, 16.0f) / 16.0f, v = std::fmod (x - y + 2000.0f, 16.0f) / 16.0f;
                    const float tread = (std::abs (u - 0.5f) < 0.08f && std::abs (v - 0.25f) < 0.18f) || (std::abs (v - 0.5f) < 0.08f && std::abs (u - 0.75f) < 0.18f) ? 1.0f : 0.0f;
                    steel = tex::mix (steel, tex::rgb (juce::Colour (0xff4a4038)), tread * 0.55f);
                    const float edgeRust = (1.0f - juce::jlimit (0.0f, 1.0f, edge / 14.0f)) * tex::smoothstep (0.35f, 0.6f, tex::vnoise (x / 6.0f, y / 6.0f, 17));
                    const float bloom = tex::smoothstep (0.6f, 0.75f, tex::fbm (x / 55.0f, y / 55.0f, 404, 3)) * 0.7f;
                    col = tex::mix (steel, tex::rustAt (x, y, 0.15f, 1337), juce::jlimit (0.0f, 1.0f, edgeRust + bloom));
                    col = tex::mul (col, 0.88f);
                }
                else
                {
                    col = tex::rustAt (x, y, 0.1f, 42);
                    if (shadow) col = tex::mul (col, 0.45f);
                }

                if (! well.contains (x, y))
                {
                    const float streak = tex::fbm (x / 9.0f, y / 240.0f, 909, 3);
                    const float drip = tex::smoothstep (0.58f, 0.78f, streak) * (0.25f + 0.5f * y / (float) H);
                    col = tex::mul (col, 1.0f - drip * 0.6f);
                    const float runoff = tex::smoothstep (0.66f, 0.82f, tex::fbm (x / 5.0f, y / 120.0f, 31337, 2));
                    col = tex::mix (col, tex::rgb (pal::rustDeep), runoff * 0.35f);
                    const float oil = tex::smoothstep (0.7f, 0.8f, tex::fbm (x / 70.0f, y / 50.0f, 555, 3));
                    col = tex::mul (col, 1.0f - oil * 0.45f);
                    if (tex::hash (px, py, 5) > 0.988f) col = tex::mul (col, 0.45f);
                }
                const float grime = 0.72f + 0.28f * tex::fbm (x / 260.0f, y / 260.0f, 77, 2);
                const float dx = (x / (float) W - 0.5f) * 2.0f, dy = (y / (float) H - 0.5f) * 2.0f;
                const float vig = 1.0f - 0.45f * std::min (1.0f, (dx * dx + dy * dy) * 0.6f);
                col = tex::mul (col, grime * vig);
                bd.setPixelColour (px, py, juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, col.r), juce::jlimit (0.0f, 1.0f, col.g),
                                                                        juce::jlimit (0.0f, 1.0f, col.b), 1.0f));
            }
        }
    }

    const int PW = (int) ((float) W * scale), PH = (int) ((float) H * scale);
    background = juce::Image (juce::Image::RGB, PW, PH, false);
    juce::Graphics g (background);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (base, 0, 0, PW, PH, 0, 0, W, H);
    g.addTransform (juce::AffineTransform::scale (scale));
    juce::Random rnd (1991);

    auto rivet = [&] (float cx, float cy, float rad)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (cx - rad + 1, cy - rad + 1.5f, rad * 2, rad * 2);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a7a68), cx - rad * 0.4f, cy - rad * 0.5f, juce::Colour (0xff2a201a), cx + rad, cy + rad, true));
        g.fillEllipse (cx - rad, cy - rad, rad * 2, rad * 2);
        g.setColour (pal::rust.withAlpha (0.55f)); g.drawEllipse (cx - rad, cy - rad, rad * 2, rad * 2, 1.2f);
        g.setColour (pal::rustDeep.withAlpha (0.35f)); g.fillRect (cx - 0.8f, cy + rad, 1.6f, 3.0f + rnd.nextFloat() * 9.0f);
    };
    auto plateFrame = [&] (juce::Rectangle<float> pr, bool hot)
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawLine (pr.getX() + 2, pr.getY() + 1, pr.getRight() - 2, pr.getY() + 1, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawRect (pr, 1.0f);
        if (hot) { g.setColour (pal::signal.withAlpha (0.45f)); g.drawRect (pr.reduced (2.0f), 1.5f); }
        rivet (pr.getX() + 6, pr.getY() + 6, 3.0f); rivet (pr.getRight() - 6, pr.getY() + 6, 3.0f);
        rivet (pr.getX() + 6, pr.getBottom() - 6, 3.0f); rivet (pr.getRight() - 6, pr.getBottom() - 6, 3.0f);
    };

    for (int i = 0; i < 420; ++i)   // scratches
    {
        const float x = rnd.nextFloat() * (float) W, y = rnd.nextFloat() * (float) H;
        if (well.contains (x, y)) continue;
        const float len = 6.0f + rnd.nextFloat() * 70.0f, ang = rnd.nextFloat() * juce::MathConstants<float>::pi;
        g.setColour ((rnd.nextBool() ? juce::Colour (0xffd0c0a8) : juce::Colours::black).withAlpha (0.04f + rnd.nextFloat() * 0.1f));
        g.drawLine (x, y, x + std::cos (ang) * len, y + std::sin (ang) * len, 0.5f + rnd.nextFloat() * 0.6f);
    }

    int seed = 10;
    for (auto& pl : plateInfo)
    {
        plateFrame (pl.r, pl.hot);
        if (pl.title.isEmpty()) continue;
        auto title = tex::distressedText (pl.title, (int) pl.r.getWidth() - 40, 18, tex::stencil (15.0f), pl.hot ? pal::signalHot : pal::paint,
                                          juce::Justification::centredLeft, scale, 0.2f, seed++, 0.12f);
        g.setOpacity (1.0f);
        g.drawImage (title, juce::Rectangle<float> (pl.r.getX() + 14, pl.r.getY() + 4, pl.r.getWidth() - 40, 18.0f));
    }
    for (auto rr : { stepArea, meterArea, tilesArea.withTrimmedLeft (34).withTrimmedRight (34) })
        if (! rr.isEmpty()) { g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (rr.toFloat().expanded (4.0f), 4.0f); }

    // small stencilled labels
    auto stamp = [&] (const juce::String& text, juce::Rectangle<int> area, juce::Justification j, int sd)
    {
        auto im = tex::distressedText (text, area.getWidth(), area.getHeight(), tex::stencil (13.0f), pal::paintDim, j, scale, 0.18f, sd, 0.15f);
        g.setOpacity (1.0f); g.drawImage (im, area.toFloat());
    };
    stamp ("MODE", modeLabelArea, juce::Justification::centredLeft, 501);
    stamp ("SNAP TO GRID", snapLabelArea, juce::Justification::centredLeft, 502);
    stamp ("PRE", preLabel, juce::Justification::centredRight, 503);
    stamp ("POST", postLabel, juce::Justification::centredRight, 504);

    // screen porthole frame
    {
        g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawRect (well, 3.0f);
        g.setColour (juce::Colour (0xff3a2a20)); g.drawRect (well.expanded (3.0f), 3.0f);
        g.setColour (pal::rust.withAlpha (0.5f)); g.drawRect (well.expanded (4.5f), 1.0f);
        for (int ix = 0; ix < 3; ++ix)
            for (int iy = 0; iy < 2; ++iy)
                rivet (well.getX() + (float) ix * 0.5f * well.getWidth() + (float) (ix - 1) * 1.5f,
                       well.getY() + (float) iy * well.getHeight() + (iy == 0 ? -1.5f : 1.5f), 3.2f);
    }

    g.setColour (juce::Colours::black.withAlpha (0.75f));
    g.drawLine (0, stripe.getY(), (float) W, stripe.getY(), 1.5f);
    g.drawLine (0, stripe.getBottom(), (float) W, stripe.getBottom(), 1.5f);

    // CASTLE DANGER MUSIC COMPANY badge
    const auto badge = badgeArea.toFloat();
    g.setColour (pal::paint.withAlpha (0.22f)); g.drawRect (badge.reduced (5.0f), 1.0f);
    drawCastleCrest (g, crestArea.toFloat());
    const float tx = (float) crestArea.getRight() + 8.0f, tw = badge.getRight() - tx - 10.0f;
    auto l1 = tex::distressedText ("CASTLE DANGER", (int) tw, 28, tex::stencil (25.0f), juce::Colour (0xffeadfc4), juce::Justification::centredLeft, scale, 0.24f, 4242, 0.08f);
    auto l2 = tex::distressedText ("MUSIC COMPANY", (int) tw, 20, tex::stencil (17.0f), pal::rustLight, juce::Justification::centredLeft, scale, 0.24f, 4343, 0.2f);
    auto l3 = tex::distressedText ("WE MAKE MUSIC", (int) tw, 14, tex::plainBold (9.0f), pal::paintDim, juce::Justification::centredLeft, scale, 0.15f, 4444, 0.3f);
    g.setColour (juce::Colours::black);
    g.setOpacity (0.7f); g.drawImage (l1, juce::Rectangle<float> (tx + 1.5f, badge.getY() + 13.0f, tw, 28.0f));
    g.setOpacity (1.0f); g.drawImage (l1, juce::Rectangle<float> (tx, badge.getY() + 11.0f, tw, 28.0f));
    g.setOpacity (0.7f); g.drawImage (l2, juce::Rectangle<float> (tx + 1.5f, badge.getY() + 40.0f, tw, 20.0f));
    g.setOpacity (1.0f); g.drawImage (l2, juce::Rectangle<float> (tx, badge.getY() + 38.0f, tw, 20.0f));
    g.drawImage (l3, juce::Rectangle<float> (tx + 1.0f, badge.getY() + 60.0f, tw, 14.0f));

    auto sub = tex::distressedText ("PRESS. HOLD. DESTROY.  //  V" + juce::String (JucePlugin_VersionString),
                                    336, 14, tex::plainBold (10.5f), pal::paintDim, juce::Justification::centredLeft, scale, 0.15f, 99, 0.16f);
    g.setOpacity (1.0f);
    g.drawImage (sub, juce::Rectangle<float> ((float) logoArea.getX() + 4, 90.0f, 336.0f, 14.0f));
    auto plab = tex::distressedText ("PRESET", 120, 16, tex::stencil (14.0f), pal::paint, juce::Justification::centredLeft, scale, 0.2f, 7, 0.15f);
    g.drawImage (plab, juce::Rectangle<float> ((float) presetArea.getX() + 2, (float) presetArea.getY(), 120.0f, 16.0f));

    // the logo: GLITCH BITCH in rusty blood red, dripping (drawn live so it can flicker and glitch)
    {
        const juce::String word ("GLITCH BITCH");
        const int lw = logoArea.getWidth(), lh = logoArea.getHeight() + 14;
        logoImage = juce::Image (juce::Image::ARGB, (int) ((float) lw * scale), (int) ((float) lh * scale), true);
        juce::Graphics lg (logoImage);
        lg.addTransform (juce::AffineTransform::scale (scale));
        auto f = tex::stencil (62.0f).withExtraKerningFactor (0.02f);
        {
            juce::GlyphArrangement all; all.addLineOfText (f, word, 0, 0);
            const float wAll = all.getBoundingBox (0, -1, true).getWidth();
            if (wAll > (float) lw - 8.0f) f = f.withHorizontalScale (((float) lw - 8.0f) / wAll);
        }
        const float textH = 62.0f;
        lg.setGradientFill (juce::ColourGradient (pal::signalHot, 0, 8.0f, juce::Colour (0xff7a1008), 0, textH, false));
        lg.setFont (f);
        lg.drawText (word, 2, 0, lw - 4, (int) textH, juce::Justification::centredLeft, false);
        float dripTop = textH * 0.8f;
        {
            juce::Image::BitmapData bd (logoImage, juce::Image::BitmapData::readOnly);
            for (int y = bd.height - 1; y >= 0; --y)
            {
                int solid = 0;
                for (int x = 0; x < bd.width; x += 2) if (bd.getPixelColour (x, y).getAlpha() > 160) ++solid;
                if (solid > 6) { dripTop = (float) y / scale; break; }
            }
        }
        tex::distress (logoImage, scale, 0.3f, 666);
        juce::Graphics dg (logoImage);
        dg.addTransform (juce::AffineTransform::scale (scale));
        juce::Random drip (13);
        for (int i = 0; i < 12; ++i)
        {
            const float x = 10.0f + drip.nextFloat() * ((float) lw - 24.0f);
            const float len = 5.0f + drip.nextFloat() * 14.0f, wdt = 2.0f + drip.nextFloat() * 2.2f;
            dg.setColour (juce::Colour (0xffa0160c));
            dg.fillRoundedRectangle (x - wdt * 0.5f, dripTop - 5, wdt, len, wdt * 0.5f);
            dg.fillEllipse (x - wdt * 0.8f, dripTop - 5 + len - wdt, wdt * 1.6f, wdt * 1.9f);
        }
    }
}

void GlitchBitchEditor::paint (juce::Graphics& g)
{
    if (background.isValid()) g.drawImage (background, getLocalBounds().toFloat());
    else g.fillAll (pal::ironDark);

    if (logoImage.isValid())
    {
        const float alpha = juce::jlimit (0.45f, 1.0f, 1.0f - flicker * 0.6f);
        const auto la = logoArea.toFloat().withHeight ((float) logoArea.getHeight() + 14.0f);
        g.setColour (juce::Colours::black);
        g.setOpacity (0.65f); g.drawImage (logoImage, la.translated (2.5f, 3.5f));
        if (logoGlitch > 0.02f)
        {
            // torn into horizontal bands, with a red / cyan ghost
            juce::Random r (logoSeed);
            const int iw = logoImage.getWidth(), ih = logoImage.getHeight();
            const int bands = 6;
            g.setOpacity (0.35f * logoGlitch);
            g.drawImage (logoImage, la.translated (-4.0f * logoGlitch, 0.0f));
            for (int k = 0; k < bands; ++k)
            {
                const int sy = ih * k / bands, sh = ih / bands + 1;
                const float shift = r.nextFloat() < 0.55f ? (r.nextFloat() * 2 - 1) * 16.0f * logoGlitch : 0.0f;
                const float dy = la.getY() + la.getHeight() * (float) sy / (float) ih, dh = la.getHeight() * (float) sh / (float) ih;
                g.setOpacity (alpha);
                g.drawImage (logoImage, juce::roundToInt (la.getX() + shift), juce::roundToInt (dy), juce::roundToInt (la.getWidth()), juce::roundToInt (dh) + 1,
                             0, sy, iw, sh);
            }
        }
        else { g.setOpacity (alpha); g.drawImage (logoImage, la); }
        g.setOpacity (1.0f);
    }

    // plates: dim red trim when armed, full glow while firing
    const bool firing = proc.uiActive.load();
    auto glowPlate = [&] (juce::Rectangle<int> b, const juce::String& title, bool hot)
    {
        auto rb = b.toFloat();
        if (hot)
        {
            for (int i = 1; i <= 6; ++i) { g.setColour (pal::signalHot.withAlpha (0.16f * (1.0f - (float) i / 7.0f))); g.drawRect (rb.expanded ((float) i * 1.5f), 1.5f); }
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a1408).withAlpha (0.16f), rb.getX(), rb.getY(), juce::Colours::transparentBlack, rb.getX(), rb.getY() + rb.getHeight() * 0.6f, false));
            g.fillRect (rb);
            g.setColour (pal::signalHot); g.drawRect (rb.reduced (1.0f), 2.0f);
            g.setFont (tex::stencil (15.0f).withExtraKerningFactor (0.12f));
            g.drawText (title, juce::Rectangle<float> (rb.getX() + 14, rb.getY() + 4, rb.getWidth() - 40, 18.0f), juce::Justification::centredLeft);
        }
        else { g.setColour (pal::signal.withAlpha (0.5f)); g.drawRect (rb.reduced (1.0f), 1.2f); }
    };
    for (auto& pl : plates)
        if (pl.arm.getToggleState()) glowPlate (pl.bounds, pl.title, firing);
    if (firing) glowPlate (buttonPlate, "THE BUTTON", true);

    // status under the snap menu
    {
        auto r = statusArea.toFloat();
        g.setFont (tex::stencil (13.0f).withExtraKerningFactor (0.1f));
        g.setColour (pal::paintDim); g.drawText ("ARMED", r.removeFromTop (16), juce::Justification::centredLeft);
        g.setFont (tex::plainBold (12.0f));
        g.setColour (armedText.isEmpty() ? pal::hazard : pal::paint);
        g.drawFittedText (armedText.isEmpty() ? "Nothing armed. Arm an effect." : armedText, r.removeFromTop (34).toNearestInt(), juce::Justification::topLeft, 2);
    }
    // tray hint
    g.setFont (tex::plainBold (11.0f));
    g.setColour (pal::paintDim.withAlpha (0.8f));
    g.drawText ("Glitch while the song plays: each glitch lands here as a WAV. Drag it onto a new track at the bar shown (turn on snap in your DAW). "
                "Right-click a clip for more.", trayHint, juce::Justification::centredLeft, true);
}

void GlitchBitchEditor::paintOverChildren (juce::Graphics& g)
{
    if (meterArea.isEmpty()) return;
    auto m = meterArea.toFloat().reduced (2.0f, 4.0f);
    const float bw = (m.getWidth() - 6) * 0.5f;
    auto drawBar = [&] (juce::Rectangle<float> bar, float lvl, const char* lbl)
    {
        auto lab = bar.removeFromBottom (14);
        g.setColour (juce::Colour (0xff0c0a08)); g.fillRect (bar);
        const float db = juce::Decibels::gainToDecibels (lvl, -60.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        const int segs = 14;
        const float sh = (bar.getHeight() - 4.0f) / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            const float t = (float) (i + 1) / (float) segs;
            const auto cell = juce::Rectangle<float> (bar.getX() + 3, bar.getBottom() - 2 - (float) (i + 1) * sh, bar.getWidth() - 6, sh - 1.5f);
            const bool on = t <= frac + 1e-4f;
            const auto c = t > 0.94f ? pal::signalHot : (t > 0.8f ? pal::hazard : pal::paint.darker (0.15f));
            g.setColour (on ? c : c.withAlpha (0.08f)); g.fillRect (cell);
        }
        g.setColour (pal::paintDim); g.setFont (tex::stencil (12.0f).withExtraKerningFactor (0.1f));
        g.drawText (lbl, lab.expanded (8, 0), juce::Justification::centred, false);
    };
    drawBar (m.removeFromLeft (bw), inMeter, "IN");
    m.removeFromLeft (6);
    drawBar (m.removeFromLeft (bw), outMeter, "OUT");
}

void GlitchBitchEditor::refreshTray()
{
    lastClipsVersion = proc.clipsVersion.load();
    std::vector<CapturedClip> list;
    { const juce::ScopedLock sl (proc.clipLock); list = proc.clips; }
    std::reverse (list.begin(), list.end());   // newest first
    const bool rec = proc.uiCapturing.load();
    wasCapturing = rec;
    const int total = (int) list.size() + (rec ? 1 : 0);
    const int pages = std::max (1, (total + kTiles - 1) / kTiles);
    page = juce::jlimit (0, pages - 1, page);
    pageLeft.setEnabled (page > 0);
    pageRight.setEnabled (page < pages - 1);
    for (int i = 0; i < kTiles; ++i)
    {
        const int idx = page * kTiles + i - (rec ? 1 : 0);
        auto& t = tiles[(size_t) i];
        const bool isRec = rec && page == 0 && i == 0;
        t.setRecording (isRec);
        if (! isRec && idx >= 0 && idx < (int) list.size())
        {
            const auto& c = list[(size_t) idx];
            auto name = c.file.getFileNameWithoutExtension();
            int num = name.fromFirstOccurrenceOf ("Glitch ", false, false).upToFirstOccurrenceOf (" ", false, false).getIntValue();
            if (num <= 0) num = (int) list.size() - idx;
            t.setClip (&c, num);
        }
        else t.setClip (nullptr, 0);
    }
}

void GlitchBitchEditor::timerCallback()
{
    const float dt = 1.0f / 30.0f;
    const float in = proc.inLevel.exchange (0.0f), out = proc.outLevel.exchange (0.0f);
    inMeter = std::max (in, inMeter * 0.85f);
    outMeter = std::max (out, outMeter * 0.85f);
    screen.update (dt);
    theButton.update (dt);

    const bool firing = proc.uiActive.load();
    auto& rnd = juce::Random::getSystemRandom();
    flicker = (rnd.nextFloat() < 0.04f + outMeter * 0.3f) ? rnd.nextFloat() : flicker * 0.5f;
    const float target = firing ? 1.0f : (rnd.nextFloat() < 0.015f ? 0.6f : 0.0f);
    logoGlitch = std::max (target, logoGlitch * 0.7f);
    if (firing || logoGlitch > 0.02f) logoSeed = rnd.nextInt();
    repaint (logoArea.expanded (24).withHeight (logoArea.getHeight() + 30));

    if (proc.presetVersion.load() != lastPresetVersion) { refreshPresetBox(); refreshModeButtons(); }

    const int cur = proc.currentStep.load();
    const int len = (int) proc.apvts.getRawParameterValue ("stutterSteps")->load();
    const bool stutOn = plates[0].arm.getToggleState();
    for (int i = 0; i < 16; ++i) steps[(size_t) i].setState (i == cur, i < len, stutOn && firing);

    juce::StringArray armed;
    for (auto& pl : plates)
    {
        const bool a = pl.arm.getToggleState(), f = a && firing;
        if (a) armed.add (pl.title);
        if (a != pl.wasArmed || f != pl.wasFiring) { pl.wasArmed = a; pl.wasFiring = f; repaint (pl.bounds.expanded (16)); }
    }
    const auto at = armed.joinIntoString (" + ");
    if (at != armedText) { armedText = at; repaint (statusArea); }
    if (firing != lastFiring) { lastFiring = firing; repaint (buttonPlate.expanded (16)); }
    refreshModeButtons();

    if (proc.clipsVersion.load() != lastClipsVersion || proc.uiCapturing.load() != wasCapturing) refreshTray();
    for (auto& t : tiles) { t.blink += dt; }
    if (wasCapturing) tiles[0].repaint();

    if (const int e = proc.captureErrors.load(); e != lastErrors)
    {
        lastErrors = e;
        if (dialog == nullptr)
            showMessage ("CAPTURE PROBLEM", "Glitch Bitch couldn't write a capture file. Check that this folder exists and has free space:\n"
                                            + GlitchBitchProcessor::getCaptureRoot().getFullPathName());
    }
    if (! meterArea.isEmpty()) repaint (meterArea);
}
