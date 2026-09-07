#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

namespace
{
const juce::Colour paper (0xffe8dfca), ink (0xff22261f), muted (0xff696653), line (0xffb5ae94);
const juce::Colour blue (0xff343c2b), cyan (0xffcbd797), coral (0xff8c4c36), yellow (0xffd3dd94), mint (0xff626d47);
constexpr float pi = juce::MathConstants<float>::pi;
juce::Font font (float size, bool bold = false, float spacing = 0.0f)
{
    static const auto regular = juce::Typeface::createSystemTypefaceFor (BinaryData::OpenSauceMedium_ttf, BinaryData::OpenSauceMedium_ttfSize);
    static const auto heavy = juce::Typeface::createSystemTypefaceFor (BinaryData::OpenSauceBold_ttf, BinaryData::OpenSauceBold_ttfSize);
    auto result = juce::Font (juce::FontOptions (bold ? heavy : regular)).withHeight (size);
    result.setExtraKerningFactor (spacing);
    return result;
}
juce::Colour familyColour (int mode) { (void) mode; return ink; }
juce::String modeDescription (int mode)
{
    static const juce::StringArray descriptions {
        "Overlapping phrases at related speeds", "Recent slices, rearranged into a rhythm",
        "Little loops that bend between pitches", "A soft cloud of overlapping grains",
        "Long circles of sound that build into drones", "New phrases from the notes you play",
        "Rhythmic cuts and changing playback speeds", "Intermittent bursts and fractured phrases",
        "Your notes, climbing through pitch patterns", "Stereo repeats arranged on a pulse",
        "A delay that dissolves into grains"
    };
    return descriptions[juce::jlimit (0, 10, mode)];
}
void text (juce::Graphics& g, const juce::String& value, juce::Rectangle<float> area,
           float size, juce::Colour colour = ink, bool bold = false,
           int justification = juce::Justification::centredLeft, float tracking = 0.0f)
{
    g.setColour (colour);
    g.setFont (font (size, bold, tracking));
    g.drawText (value, area, justification, false);
}
juce::Image makeHardwarePanel (int height)
{
    juce::Image image (juce::Image::RGB, 1240, height, true);
    juce::Graphics g (image);
    g.fillAll (ink);
    const juce::Rectangle<float> panel (5.f, 5.f, 1230.f, static_cast<float> (height) - 10.f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0e7d0), 35, 0, juce::Colour (0xffd6ceb7), 1220, static_cast<float> (height), false));
    g.fillRoundedRectangle (panel, 24.f);
    {
        juce::Graphics::ScopedSaveState clip (g);juce::Path shape;shape.addRoundedRectangle (panel, 24.f);g.reduceClipRegion (shape);
        if (const auto pattern = juce::Drawable::createFromImageData (BinaryData::HardwarePrint_svg, BinaryData::HardwarePrint_svgSize))
            pattern->drawWithin (g, { 5.f, 5.f, 1230.f, 790.f }, juce::RectanglePlacement::stretchToFit, 1.f);
        juce::Random grain (87102);
        for (int i = 0; i < 45000; ++i)
        {
            g.setColour ((i % 2 ? ink : paper.brighter (.25f)).withAlpha (.035f));
            g.fillRect (grain.nextFloat() * 1240.f, grain.nextFloat() * height, .65f, .65f);
        }
    }
    g.setColour (paper.brighter (.18f).withAlpha (.75f));g.drawRoundedRectangle (panel.reduced (4), 21.f, 2.f);
    g.setColour (muted.withAlpha (.55f));g.drawRoundedRectangle (panel.reduced (8), 18.f, 1.f);
    // Recessed control and transport plates leave the print on the enclosure.
    for (const auto plate : { juce::Rectangle<float> (793, 112, 405, 365), juce::Rectangle<float> (40, 690, 1160, 77) })
    {
        g.setColour (paper);g.fillRoundedRectangle (plate, 13.f);
        g.setColour (line.withAlpha (.85f));g.drawRoundedRectangle (plate.reduced (4), 10.f, 1.f);
    }
    g.setColour (line);g.drawLine (811, 434, 1180, 434, .8f);
    if (height > 800)
    {
        g.setColour (paper);g.fillRoundedRectangle ({ 40, 802, 1160, 151 }, 12.f);
        g.setColour (line);g.drawRoundedRectangle ({ 44, 806, 1152, 143 }, 9.f, .8f);
    }
    for (const juce::Point<float> c : { juce::Point<float> (24, 24), { 1216, 24 }, { 24, static_cast<float> (height - 24) }, { 1216, static_cast<float> (height - 24) } })
    {
        g.setColour (ink.withAlpha (.2f));g.fillEllipse (c.x-9,c.y-7,18,18);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe0e1c7), c.x-6,c.y-8,juce::Colour (0xff636e58),c.x+7,c.y+8,false));
        g.fillEllipse (c.x-8,c.y-8,16,16);g.setColour (ink);g.drawEllipse (c.x-8,c.y-8,16,16,1);
        g.drawLine (c.x-4,c.y-2,c.x+4,c.y+2,2.f);
    }
    return image;
}

juce::String secondsText (float seconds)
{
    const auto total = juce::jmax (0, static_cast<int> (seconds));
    return juce::String (total / 60).paddedLeft ('0', 2) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
}
}

MoteFieldLookAndFeel::MoteFieldLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, paper);
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, blue.withAlpha (.12f));
    setColour (juce::PopupMenu::highlightedTextColourId, ink);
    setColour (juce::ComboBox::textColourId, ink);
    setColour (juce::Label::textColourId, ink);
    setColour (juce::Slider::textBoxTextColourId, muted);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::backgroundColourId, ink);
    setColour (juce::TooltipWindow::textColourId, paper);
    setColour (juce::TooltipWindow::outlineColourId, ink);
}

void MoteFieldLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                            float position, float start, float end, juce::Slider& slider)
{
    const auto size = static_cast<float> (juce::jmin (width, height));
    const juce::Point<float> centre (static_cast<float> (x) + width * .5f, static_cast<float> (y) + height * .5f);
    const auto radius = size * .405f, angle = start + position * (end - start);
    const auto body = juce::Rectangle<float> (radius * 2, radius * 2).withCentre (centre);
    for (int i = 5; i > 0; --i)
    {
        g.setColour (ink.withAlpha (.035f));
        g.fillEllipse (body.expanded (static_cast<float> (i)).translated (0, size * .035f));
    }
    g.setColour (muted.withAlpha (.4f));g.drawEllipse (body.expanded (size * .045f), 1.f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff383b2e), body.getX(), body.getY(), ink, body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    for (int i = 0; i < 64; ++i)
    {
        const auto a = i * pi * 2.f / 64.f;
        g.setColour (juce::Colour (0xffa39e82).withAlpha (i % 2 ? .40f : .62f));
        g.drawLine ({ centre.getPointOnCircumference (radius * .84f, a), centre.getPointOnCircumference (radius * .97f, a) }, size * .010f);
    }
    const auto face = body.reduced (radius * .20f).translated (0, -size * .008f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff5efd7), face.getX(), face.getY(), juce::Colour (0xffcec5a8), face.getRight(), face.getBottom(), false));
    g.fillEllipse (face);g.setColour (juce::Colour (0xfffaf4da).withAlpha (.8f));g.drawEllipse (face.reduced (1.f), 1.3f);
    g.setColour (ink.withAlpha (.3f));g.drawEllipse (face, .7f);
    const auto c = face.getCentre();
    juce::Path pointer;pointer.startNewSubPath (c.getPointOnCircumference (radius * .44f, angle));pointer.lineTo (c.getPointOnCircumference (radius * .70f, angle));
    g.setColour (ink);g.strokePath (pointer, juce::PathStrokeType (size * .043f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (slider.getComponentID() != "mode-selector" && slider.isMouseOverOrDragging())
        text (g, slider.getTextFromValue (slider.getValue()), { body.getX(), c.y + radius * .32f, body.getWidth(), radius * .35f }, size * .10f, ink, true, juce::Justification::centred);
}

void MoteFieldLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                const juce::Colour&, bool hover, bool down)
{
    const auto style = static_cast<int> (button.getProperties()["style"]);
    const auto active = button.getToggleState();
    const auto bounds = button.getLocalBounds().toFloat();
    const auto scale = juce::jmax (.7f, static_cast<float> (button.getHeight()) / (style == 2 ? 114.0f : 34.0f));
    if (style == 6)
    {
        const auto face = juce::Rectangle<float> (26.0f * scale, 26.0f * scale).withCentre ({ 16.0f * scale, bounds.getCentreY() });
        auto colour = active ? button.findColour (juce::TextButton::buttonOnColourId) : paper;
        if (hover) colour = colour.darker (.04f);
        g.setColour (ink.withAlpha (.11f));
        g.fillEllipse (face.translated (0.0f, 1.5f * scale));
        g.setGradientFill (juce::ColourGradient (colour.brighter (.1f), face.getX(), face.getY(), colour.darker (.055f), face.getX(), face.getBottom(), false));
        g.fillEllipse (face);
        g.setColour (line);
        g.drawEllipse (face, .8f);
        g.setColour ((button.getComponentID() == "record" ? coral : ink).withAlpha (button.isEnabled() ? .9f : .22f));
        const auto cx = face.getCentreX(), cy = face.getCentreY();
        const auto id = button.getComponentID();
        if (id == "record") g.fillEllipse (cx - 4.0f * scale, cy - 4.0f * scale, 8.0f * scale, 8.0f * scale);
        else if (id == "play")
        {
            juce::Path triangle;
            triangle.addTriangle (cx - 3.0f * scale, cy - 5.0f * scale, cx - 3.0f * scale, cy + 5.0f * scale, cx + 5.0f * scale, cy);
            g.fillPath (triangle);
        }
        else if (id == "stop") g.fillRoundedRectangle (cx - 4.0f * scale, cy - 4.0f * scale, 8.0f * scale, 8.0f * scale, 1.0f);
        else if (id == "dub")
        {
            g.drawLine (cx - 5.0f * scale, cy, cx + 5.0f * scale, cy, 1.4f * scale);
            g.drawLine (cx, cy - 5.0f * scale, cx, cy + 5.0f * scale, 1.4f * scale);
        }
        else if (id == "undo")
        {
            juce::Path arrow;
            arrow.addCentredArc (cx, cy + scale, 5.0f * scale, 4.0f * scale, 0.0f, -pi * .5f, pi * .8f, true);
            arrow.startNewSubPath (cx - 2.0f * scale, cy - 5.0f * scale);
            arrow.lineTo (cx - 6.0f * scale, cy - scale);
            arrow.lineTo (cx - 1.0f * scale, cy + scale);
            g.strokePath (arrow, juce::PathStrokeType (1.1f * scale));
        }
        else
        {
            g.drawRoundedRectangle (cx - 3.0f * scale, cy - 2.0f * scale, 6.0f * scale, 7.0f * scale, scale, scale);
            g.drawLine (cx - 5.0f * scale, cy - 4.0f * scale, cx + 5.0f * scale, cy - 4.0f * scale, scale);
            g.drawLine (cx - scale, cy - 6.0f * scale, cx + scale, cy - 6.0f * scale, scale);
        }
        return;
    }
    if (style == 1)
    {
        if (active || hover)
        {
            g.setColour (active ? cyan : paper.darker (.04f));g.fillRoundedRectangle (bounds.reduced (.8f), 4.f);
            g.setColour (active ? mint : line);g.drawRoundedRectangle (bounds.reduced (.8f), 4.f, .8f);
        }
        return;
    }
    if (style == 3 || style == 4)
    {
        if (active || down || hover)
        {
            g.setColour (active ? button.findColour (juce::TextButton::buttonOnColourId).withAlpha (.20f) : ink.withAlpha (.05f));
            g.fillRoundedRectangle (bounds.reduced (1.0f), 6.0f * scale);
        }
        return;
    }
    auto fill = active ? button.findColour (juce::TextButton::buttonOnColourId) : button.findColour (juce::TextButton::buttonColourId);
    if (hover) fill = fill.brighter (.045f);
    if (down) fill = fill.darker (.055f);
    const auto face = bounds.reduced (3.0f * scale).withTrimmedBottom (down ? 0.0f : 2.0f * scale);
    const auto radius = (style == 2 ? 12.0f : 5.0f) * scale;
    if (style == 2 && active) { g.setColour (fill.withAlpha (.15f)); g.fillRoundedRectangle (bounds, radius + 2.0f); }
    g.setColour (ink.withAlpha (.15f));
    g.fillRoundedRectangle (face.translated (0.0f, 3.0f * scale), radius);
    g.setGradientFill (juce::ColourGradient (fill.brighter (.16f), face.getX(), face.getY(), fill.darker (.06f), face.getX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, radius);
    g.setColour (fill.darker (.22f));
    g.drawRoundedRectangle (face, radius, 1.0f);
    g.setColour (paper.brighter (.25f).withAlpha (.48f));
    g.drawRoundedRectangle (face.reduced (2.0f * scale), radius - 2.0f * scale, 1.0f);
    if (! button.isEnabled()) { g.setColour (paper.withAlpha (.6f)); g.fillRoundedRectangle (face, radius); }
}

void MoteFieldLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const auto style = static_cast<int> (button.getProperties()["style"]);
    auto bounds = button.getLocalBounds().toFloat();
    const auto scale = static_cast<float> (style == 2 ? juce::jmin (button.getWidth(), button.getHeight()) : button.getHeight()) / (style == 2 ? 114.0f : style == 1 ? 32.0f : style == 5 ? 45.0f : 34.0f);
    if (style == 6) bounds.removeFromLeft (32.0f * scale);
    const auto colour = button.isEnabled() ? (style == 2 && button.getComponentID() != "freeze" && ! button.getToggleState() ? paper : style == 5 && button.getToggleState() ? paper : ink) : muted.withAlpha (.52f);
    text (g, button.getButtonText(), bounds, (button.getProperties()["panelButton"] ? 12.5f : style == 2 || style == 5 ? 16.0f : style == 1 ? 13.0f : 11.0f) * scale,
          colour, style != 1 || button.getToggleState(), style == 6 ? juce::Justification::centredLeft : juce::Justification::centred,
          style == 1 ? 0.0f : .04f);
}

void MoteFieldLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (width), static_cast<float> (height)).reduced (.8f);
    g.setColour (juce::Colour (0xffdaddbf));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (mint);
    g.drawRoundedRectangle (bounds, 4.0f, 1.8f);
    const auto cx = bounds.getRight() - 13.0f, cy = bounds.getCentreY();
    juce::Path arrow;
    arrow.startNewSubPath (cx - 3.0f, cy - 1.5f);
    arrow.lineTo (cx, cy + 1.5f);
    arrow.lineTo (cx + 3.0f, cy - 1.5f);
    g.setColour (muted);
    g.strokePath (arrow, juce::PathStrokeType (1.2f));
}
juce::Font MoteFieldLookAndFeel::getComboBoxFont (juce::ComboBox& box) { return font (static_cast<float> (box.getHeight()) * .40f); }
juce::Font MoteFieldLookAndFeel::getLabelFont (juce::Label& label) { return label.getFont(); }

ParameterKnob::ParameterKnob (const juce::String& name, const juce::String& description) : title (name)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setRotaryParameters (pi * 1.23f, pi * 2.77f, true);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.setMouseDragSensitivity (180);
    slider.setTooltip (description + "  Double-click to reset. Shift-drag for fine adjustment.");
    slider.setTitle (name);
    addAndMakeVisible (slider);
}
void ParameterKnob::resized() { slider.setBounds (getLocalBounds().withTrimmedBottom (juce::roundToInt (static_cast<float> (getHeight()) * .17f))); }
void ParameterKnob::paint (juce::Graphics& g)
{
    const auto h = static_cast<float> (getHeight());
    text (g, title, { 0.0f, h * .83f, static_cast<float> (getWidth()), h * .17f }, h * .097f, ink, true, juce::Justification::centred, .055f);
}

void FieldDisplay::update (const motefield::VisualFrame& next)
{
    const auto reset = next.sampleTime < frame.sampleTime;
    if (reset) { liquidVoices = {};surfaceBodies = {};staleSeconds = 0.f; }
    const auto advanced = next.sampleTime != frame.sampleTime;
    const auto dt = advanced && ! reset
        ? juce::jlimit (.001f, .1f, static_cast<float> (static_cast<double> (next.sampleTime - frame.sampleTime) / sampleRate))
        : 1.0f / 60.0f;
    const auto follow = 1.0f - std::exp (-dt * 22.0f);
    const auto fade = std::exp (-dt * 16.0f);
    for (auto& mote : liquidVoices) mote.seen = false;
    const auto mode = static_cast<int> (next.mode);
    float longestDelay = .1f;
    for (int i = 0; i < next.voiceCount; ++i)
        if (next.voices[static_cast<std::size_t> (i)].delayTap)
            longestDelay = juce::jmax (longestDelay, next.voices[static_cast<std::size_t> (i)].duration);

    for (int i = 0; i < next.voiceCount; ++i)
    {
        const auto& voice = next.voices[static_cast<std::size_t> (i)];
        LiquidVoice* mote = nullptr;
        for (auto& candidate : liquidVoices)
            if (candidate.id == voice.id && candidate.energy > .001f) { mote = &candidate; break; }
        const auto existing = mote != nullptr;
        if (! existing)
            for (auto& candidate : liquidVoices)
                if (candidate.energy < .004f) { mote = &candidate; break; }
        if (mote == nullptr) continue;
        const auto level = (advanced || staleSeconds < .10f) ? std::tanh (voice.level * 12.0f) : 0.f;
        const auto phase = voice.rate < 0.0f || (voice.delayTap && next.reverse) ? 1.0f - voice.phase : voice.phase;
        const auto seed = static_cast<float> (voice.id % 17u) / 17.0f;
        auto x = .08f + phase * .84f;
        auto y = .5f + voice.pan * .23f + std::sin (phase * pi * 2.0f + seed * pi * 2.0f) * .10f;
        if (next.mode == motefield::Mode::orbit)
        {
            x = .5f + std::cos ((phase + seed) * pi * 2.0f) * .34f;
            y = .5f + std::sin ((phase + seed) * pi * 2.0f) * .24f;
        }
        if (voice.delayTap)
        {
            x = .10f + voice.duration / longestDelay * .78f;
            y = .5f + voice.pan * .19f;
        }
        if (reducedMotion)
        {
            x = .10f + seed * .80f;
            y = .5f + voice.pan * .22f;
        }
        const auto granules = mode >= 3 && mode < 6;
        const auto contour = 1.0f / std::sqrt (voice.envelopePower);
        const auto stretch = juce::jlimit (.75f, 1.55f, .8f + std::abs (voice.rate) * .24f);
        const auto radiusX = juce::jlimit (.022f, .11f,
            (granules ? .025f : .040f) * (.65f + std::sqrt (level) * .9f) * stretch * (.75f + contour * .25f));
        const auto radiusY = (.12f + std::sqrt (level) * .27f) * (.65f + voice.envelope * .35f) / std::sqrt (stretch);
        const auto heat = voice.delayTap ? -.1f : juce::jlimit (-1.0f, 1.0f, std::log2 (juce::jmax (.125f, std::abs (voice.rate))));
        // The sample window deforms the body; it is never drawn as a static card.
        const auto bend = std::tanh ((voice.waveform[4] - voice.waveform[18]) * 7.0f) * .35f;
        if (! existing) *mote = { voice.id, x, y, radiusX, radiusY, .0f, heat, bend, true };
        mote->seen = true;
        const auto amount = reducedMotion ? 1.0f : follow;
        mote->x += (x - mote->x) * amount;
        mote->y += (y - mote->y) * amount;
        mote->radiusX += (radiusX - mote->radiusX) * follow;
        mote->radiusY += (radiusY - mote->radiusY) * follow;
        mote->energy += (level - mote->energy) * follow;
        mote->heat += (heat - mote->heat) * follow;
        mote->bend += (bend - mote->bend) * follow;
    }
    for (auto& mote : liquidVoices)
        if (! mote.seen) mote.energy *= fade;
    staleSeconds = advanced ? 0.f : staleSeconds + dt;
    const bool signalCurrent = staleSeconds < .10f;
    const auto target = signalCurrent ? std::tanh (next.outputLevel * 3.f) : 0.f;
    outputDrive += (target - outputDrive) * (1.f - std::exp (-dt * (target > outputDrive ? 28.f : 10.f)));
    const auto followBand = [dt, signalCurrent] (float& current, float measured, float gain)
    {
        const auto destination = signalCurrent ? std::tanh (measured * gain) : 0.f;
        current += (destination - current) * (1.f - std::exp (-dt * 24.f));
        if (current < .00005f) current = 0.f;
    };
    followBand (lowDrive, next.spectralEnergy[0], 6.f);
    followBand (midDrive, next.spectralEnergy[1], 7.f);
    followBand (highDrive, next.spectralEnergy[2], 10.f);
    if (outputDrive < .0001f) outputDrive = 0.f;
    struct Cluster { float energy = 0.f, x = 0.f, y = 0.f, bend = 0.f; };
    std::array<Cluster, 5> clusters {};
    float weight = 0.f, meanX = 0.f, meanY = 0.f;
    for (const auto& voice : liquidVoices)
    {
        if (next.bypass || voice.energy < .00001f) continue;
        auto& cluster = clusters[voice.id % clusters.size()];
        cluster.energy += voice.energy;cluster.x += voice.x * voice.energy;
        cluster.y += voice.y * voice.energy;cluster.bend += voice.bend * voice.energy;
        weight += voice.energy;meanX += voice.x * voice.energy;meanY += voice.y * voice.energy;
    }
    const auto targetX = 240.f + (weight > .001f && ! reducedMotion ? (meanX / weight - .5f) * 45.f : 0.f) + (reducedMotion ? 0.f : (next.fieldPosition * 2.f - next.fieldSplit) * 55.f);
    const auto targetY = 72.f + (weight > .001f && ! reducedMotion ? (meanY / weight - .5f) * 22.f : 0.f) - (reducedMotion ? 0.f : next.fieldPitch * .6f);
    centreX += (targetX - centreX) * (1.f - std::exp (-dt * 14.f));
    centreY += (targetY - centreY) * (1.f - std::exp (-dt * 14.f));
    const auto spring = [dt] (float& position, float& velocity, float destination, float speed)
    {
        const auto steps = juce::jmax (1, static_cast<int> (std::ceil (dt / .006f)));
        const auto h = dt / static_cast<float> (steps);
        for (int i = 0; i < steps; ++i)
        {
            velocity += ((destination - position) * speed * speed - 2.f * speed * velocity) * h;
            position += velocity * h;
        }
        if (std::abs (position - destination) < .001f && std::abs (velocity) < .001f) { position = destination;velocity = 0.f; }
    };
    for (std::size_t i = 0; i < surfaceBodies.size(); ++i)
    {
        auto& body = surfaceBodies[i];const auto& cluster = clusters[i];
        const auto amount = 1.f - std::exp (-cluster.energy * 1.6f);
        const auto alive = cluster.energy > .0001f;
        const auto tx = alive ? cluster.x / cluster.energy * liquidWidth : centreX;
        const auto ty = alive ? cluster.y / cluster.energy * liquidHeight : centreY;
        const auto spread = (.35f + .65f * amount) * (1.5f - next.cohesion) + next.fieldSplit * .35f;
        const auto x = reducedMotion ? 95.f + i * 72.f : centreX + (tx - centreX) * spread;
        const auto y = reducedMotion ? 72.f : centreY + (ty - centreY) * spread;
        spring (body.x, body.vx, juce::jlimit (55.f, liquidWidth - 55.f, x), 30.f - next.viscosity * 20.f);
        spring (body.y, body.vy, juce::jlimit (43.f, liquidHeight - 43.f, y), 30.f - next.viscosity * 20.f);
        spring (body.radius, body.vr, 27.f * std::sqrt (amount), 28.f);
        spring (body.stretch, body.vs, alive && ! reducedMotion ? juce::jlimit (.7f,1.4f, std::sqrt (next.fieldStretch)) + cluster.bend / cluster.energy * .4f : 1.f, 24.f);
    }
    frame = next;
    const auto moving = std::any_of (liquidVoices.begin(), liquidVoices.end(), [] (const auto& voice) { return voice.energy >= .0001f; });
    const auto settling = std::any_of (surfaceBodies.begin(), surfaceBodies.end(), [] (const auto& body) { return body.radius > .002f; });
    if (! materialRendered || materialActive || advanced || outputDrive > 0.f || lowDrive + midDrive + highDrive > 0.f || moving || settling)
    {
        renderLiquid();materialRendered = true;materialActive = moving || settling || outputDrive > 0.f || lowDrive + midDrive + highDrive > 0.f;
    }
    repaint();
}

void FieldDisplay::renderLiquid()
{
    // All bodies contribute to one implicit surface. Shared normals remove
    // draw-order seams when grains join, split or move through one another.
    liquidField.fill (0.f);liquidHeat.fill (0.f);liquidGradientY.fill (0.f);
    const float blend = std::pow (12.f + frame.cohesion * 12.f, 2.f);
    const auto deposit = [this, blend] (float cx, float cy, float radius, float stretch, float strength = 1.f)
    {
        const auto ax = 1.f / stretch, ay = stretch;
        const auto reach = std::sqrt (radius * radius + blend * 8.f);
        const auto left = juce::jmax (0, static_cast<int> (cx - reach / ax));
        const auto right = juce::jmin (liquidWidth - 1, static_cast<int> (cx + reach / ax) + 1);
        const auto top = juce::jmax (0, static_cast<int> (cy - reach / ay));
        const auto bottom = juce::jmin (liquidHeight - 1, static_cast<int> (cy + reach / ay) + 1);
        std::array<float, liquidWidth> horizontal {};
        for (int x = left; x <= right; ++x)
        {
            const auto dx = (x + .5f - cx) * ax;
            horizontal[static_cast<std::size_t> (x)] = std::exp ((radius * radius - dx * dx) / blend);
        }
        for (int y = top; y <= bottom; ++y)
        {
            const auto dy = (y + .5f - cy) * ay;
            const auto vertical = std::exp (-dy * dy / blend);
            for (int x = left; x <= right; ++x)
            {
                const auto index = static_cast<std::size_t> (y * liquidWidth + x);
                const auto weight = horizontal[static_cast<std::size_t> (x)] * vertical * strength;
                liquidField[index] += weight;
                liquidHeat[index] += weight * (x + .5f - cx) * ax * ax;
                liquidGradientY[index] += weight * dy * ay;
            }
        }
    };
    // The main mass responds to frequency content, not only a slow peak meter.
    const auto radius = 32.f + lowDrive * 8.f + midDrive * 3.f;
    deposit (centreX, centreY, radius, reducedMotion ? 1.f : 1.f + lowDrive * .24f - midDrive * .22f);
    const auto tension = reducedMotion ? 0.f : (midDrive * .65f + highDrive * .85f) * (.25f + frame.tension * 1.5f) + frame.magnet * .5f;
    if (tension > .0001f)
        for (int pole = 0; pole < 5; ++pole)
        {
            const auto angle = (-155.f + static_cast<float> (pole) * 65.f) * pi / 180.f;
            const auto pull = radius * (.76f + highDrive * .17f);
            const auto emphasis = pole % 2 ? highDrive : tension;
            deposit (centreX + std::cos (angle) * pull, centreY + std::sin (angle) * pull,
                     7.f + midDrive * 6.f + highDrive * 6.f, 1.f, emphasis * .85f);
        }
    for (const auto& body : surfaceBodies)
        if (body.radius > .05f)
            deposit (body.x, body.y, body.radius, juce::jlimit (.84f, 1.19f, body.stretch), juce::jmin (1.f, body.radius / 8.f));
    juce::Image::BitmapData pixels (liquidImage, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < liquidHeight; ++y)
        for (int x = 0; x < liquidWidth; ++x)
        {
            const auto i = static_cast<std::size_t> (y * liquidWidth + x);
            const auto field = liquidField[i];
            if (field < .78f) { pixels.setPixelColour (x, y, juce::Colours::transparentBlack);continue; }
            const auto height2 = blend * std::log (field);
            const auto gx = liquidHeat[i] / field, gy = liquidGradientY[i] / field;
            const auto edge = height2 / (2.f * std::sqrt (gx * gx + gy * gy) + 1.f);
            const auto alpha = juce::jlimit (0.f, 1.f, edge + .5f);
            if (alpha <= 0.f) { pixels.setPixelColour (x, y, juce::Colours::transparentBlack);continue; }
            const auto z = std::sqrt (juce::jmax (.01f, height2));
            const auto length = std::sqrt (gx * gx + gy * gy + z * z);
            const auto nx = gx / length, ny = gy / length, nz = z / length;
            const auto rx = 2.f * nx * nz, ry = 2.f * ny * nz, rz = 2.f * nz * nz - 1.f;
            const auto square = [] (float v) { return v * v; };
            const auto key = std::exp (-square ((rx + .47f) / .34f) - square ((ry + .45f) / .62f)) * juce::jmax (0.f, rz * .55f + .45f);
            const auto fill = std::exp (-square ((rx - .72f) / .21f) - square (square ((ry - .08f) / .85f))) * .2f;
            const auto light = juce::jmax (0.f, nx * -.42f + ny * -.52f + nz * .74f);
            const auto rim = 1.f - nz;
            const auto v = juce::jlimit (0.f, 1.f, (8.f + light * light * light * 15.f + key * 146.f + fill * 100.f + rim * rim * rim * 12.f) / 255.f);
            pixels.setPixelColour (x, y, juce::Colour::fromFloatRGBA (v * .96f, v, v * .91f, alpha));
        }
}

void FieldDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (! gesture || e.position.x > getWidth() * .72f || e.position.y < getHeight() * .22f) return;
    dragging = true; dragStart = e.position; gestureMode = e.mods.isAltDown() ? 2 : e.mods.isShiftDown() ? 1 : 0;
    startX = gestureMode == 1 ? frame.fieldStretch : gestureMode == 2 ? frame.fieldSplit : frame.fieldPosition;
    startY = frame.fieldPitch;
    gesture (gestureMode == 1 ? "fieldStretch" : gestureMode == 2 ? "fieldSplit" : "fieldPosition", startX, 0);
    if (gestureMode == 0) gesture ("fieldPitch", startY, 0);
}
void FieldDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging || ! gesture) return;
    const auto delta = e.position - dragStart;
    if (gestureMode == 1) gesture ("fieldStretch", juce::jlimit (.25f, 4.f, startX * std::pow (2.f, -delta.y / 60.f)), 1);
    else if (gestureMode == 2) gesture ("fieldSplit", juce::jlimit (0.f,1.f,startX + delta.x / 160.f),1);
    else { gesture ("fieldPosition", juce::jlimit (0.f,1.f,startX + delta.x / (getWidth() * .6f)),1); gesture ("fieldPitch", juce::jlimit (-24.f,24.f,startY - delta.y / 4.f),1); }
}
void FieldDisplay::mouseUp (const juce::MouseEvent&)
{
    if (! dragging || ! gesture) return;
    dragging = false; gesture (gestureMode == 1 ? "fieldStretch" : gestureMode == 2 ? "fieldSplit" : "fieldPosition", 0.f, 2);
    if (gestureMode == 0) gesture ("fieldPitch",0.f,2);
}
void FieldDisplay::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (! gesture) return;
    if (dragging) mouseUp (event);
    for (const auto* id : { "fieldPosition", "fieldPitch", "fieldStretch", "fieldSplit" })
    { gesture (id,0.f,0); gesture (id,juce::String (id) == "fieldStretch" ? 1.f : 0.f,1); gesture (id,0.f,2); }
}

void FieldDisplay::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat().reduced (1.0f);
    const auto s = area.getWidth() / 718.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc6ccb0), area.getX(), area.getY(),
                                             juce::Colour (0xffe0e1c5), area.getX(), area.getBottom(), false));
    g.fillRoundedRectangle (area, 20.0f * s);
    g.setColour (juce::Colours::white.withAlpha (.9f));
    g.drawRoundedRectangle (area.reduced (2.0f), 18.0f * s, 1.0f);
    g.setColour (line.withAlpha (.7f));
    g.setColour (ink);g.drawRoundedRectangle (area.reduced (2.f * s), 15.f * s, 6.f * s);
    g.setColour (mint);g.drawRoundedRectangle (area.reduced (6.f * s), 11.f * s, 1.8f * s);
    const auto mode = juce::jlimit (0, 10, static_cast<int> (frame.mode));
    const auto title = juce::String (motefield::modeNames[static_cast<std::size_t> (mode)])
                       + " / " + juce::String::charToString (static_cast<juce::juce_wchar> ('A' + frame.variation));
    text (g, title, { 23.0f * s, 12.0f * s, 150.0f * s, 24.0f * s }, 12.0f * s, ink, true, juce::Justification::centredLeft, .03f);
    if (frame.held || frame.bypass)
    {
        const auto badge = juce::Rectangle<float> (146.0f * s, 14.0f * s, 66.0f * s, 21.0f * s);
        g.setColour (frame.bypass ? line : yellow);
        g.fillRoundedRectangle (badge, 9.0f * s);
        text (g, frame.bypass ? "BYPASS" : "HELD", badge, 10.0f * s, ink, true, juce::Justification::centred, .08f);
    }
    const auto view = area.reduced (27.0f * s, 14.0f * s).withTrimmedTop (32.0f * s);
    const auto stage = view.withWidth (view.getWidth() * .69f);
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (view.toNearestInt());
    const auto colour = familyColour (mode);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (liquidImage, stage);

    const auto contour = view.withTrimmedLeft (view.getWidth() * .75f);
    g.setColour (line.withAlpha (.7f));
    g.drawVerticalLine (juce::roundToInt (view.getX() + view.getWidth() * .72f), view.getY(), view.getBottom());
    if (frame.mode == motefield::Mode::grid)
    {
        text (g, "DELAY TAPS", contour.withHeight (12.0f * s), 8.0f * s, muted, true);
        float longest = .1f;
        for (int i = 0; i < frame.voiceCount; ++i)
            longest = juce::jmax (longest, frame.voices[static_cast<std::size_t> (i)].duration);
        for (int i = 0; i < frame.voiceCount; ++i)
        {
            const auto& voice = frame.voices[static_cast<std::size_t> (i)];
            const auto x = contour.getX() + voice.duration / longest * (contour.getWidth() - 3.0f * s);
            const auto bottom = contour.getBottom() - 17.0f * s;
            g.setColour (mint.withAlpha (.8f));
            g.drawLine (x, bottom, x, bottom - std::tanh (voice.level * 3.0f) * 42.0f * s, 2.0f * s);
        }
        text (g, "Shape is not used by Grid", contour.withY (contour.getBottom() - 12.0f * s).withHeight (12.0f * s), 7.5f * s, muted);
    }
    else
    {
        text (g, "SHAPE / GRAIN ENVELOPE", contour.withHeight (12.0f * s), 8.0f * s, muted, true);
        const auto plot = contour.withTrimmedTop (19.0f * s).withTrimmedBottom (3.0f * s);
        const auto envelopePoint = [&] (float phase, float power)
        {
            const auto level = std::pow (juce::jmax (0.0f, std::sin (pi * phase)), power);
            return juce::Point<float> (plot.getX() + phase * plot.getWidth(), plot.getBottom() - level * plot.getHeight());
        };
        juce::Path envelope;
        envelope.startNewSubPath (envelopePoint (0.0f, juce::jlimit (.2f, 6.f, motefield::shapeEnvelopePower (shape) + (frame.tension - .5f) * 3.f)));
        for (int point = 1; point <= 80; ++point)
            envelope.lineTo (envelopePoint (static_cast<float> (point) / 80.0f, juce::jlimit (.2f, 6.f, motefield::shapeEnvelopePower (shape) + (frame.tension - .5f) * 3.f)));
        g.setColour (colour.withAlpha (.7f));
        g.strokePath (envelope, juce::PathStrokeType (1.5f * s));
        envelope.closeSubPath();
        g.setColour (colour.withAlpha (.10f));
        g.fillPath (envelope);
        if (! reducedMotion)
            for (int i = 0; i < frame.voiceCount; ++i)
            {
                const auto& voice = frame.voices[static_cast<std::size_t> (i)];
                if (voice.delayTap) continue;
                const auto point = envelopePoint (voice.phase, voice.envelopePower);
                const auto radius = (1.2f + 1.8f * std::tanh (voice.level * 4.0f)) * s;
                g.setColour (colour.darker (.2f).withAlpha (juce::jlimit (.1f, .8f, voice.level * 4.0f)));
                g.fillEllipse (point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);
            }
    }
}

void LooperTape::paint (juce::Graphics& g)
{
    const auto& frame = field.currentFrame();
    const auto area = getLocalBounds().toFloat().reduced (2.0f, 5.0f);
    const auto mid = area.getCentreY();
    g.setColour (line.withAlpha (.65f));
    g.fillRoundedRectangle (area.withHeight (4.0f).withCentre (area.getCentre()), 2.0f);
    if (frame.loopState == motefield::LooperState::empty) return;
    for (std::size_t bin = 0; bin < frame.loopWaveform.size(); ++bin)
    {
        const auto x = area.getX() + static_cast<float> (bin) / 127.0f * area.getWidth();
        const auto amplitude = juce::jmin (1.0f, std::sqrt (frame.loopWaveform[bin])) * area.getHeight() * .47f;
        g.setColour (ink.withAlpha (.43f));
        g.drawLine (x, mid - amplitude, x, mid + amplitude, 1.4f);
    }
    const auto playX = area.getX() + frame.loopProgress * area.getWidth();
    const auto colour = frame.loopState == motefield::LooperState::recording || frame.loopState == motefield::LooperState::overdubbing ? coral : yellow;
    g.setColour (colour.withAlpha (.45f));
    g.fillRoundedRectangle ({ area.getX(), mid - 2.0f, juce::jmax (1.0f, playX - area.getX()), 4.0f }, 2.0f);
    g.setColour (colour);
    g.fillEllipse (playX - 5.0f, mid - 5.0f, 10.0f, 10.0f);
    g.setColour (juce::Colours::white.withAlpha (.8f));
    g.drawEllipse (playX - 5.0f, mid - 5.0f, 10.0f, 10.0f, 1.0f);
}

MoteFieldAudioProcessorEditor::MoteFieldAudioProcessorEditor (MoteFieldAudioProcessor& owner)
    : AudioProcessorEditor (&owner), processor (owner), looperTape (fieldDisplay), tooltips (this, 600)
{
    using namespace motefield::parameter;
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    rangoLogo = juce::ImageCache::getFromMemory (BinaryData::RangoLogo_png, BinaryData::RangoLogo_pngSize);
    modeSelector.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    modeSelector.setRotaryParameters (pi * 7.f / 6.f, pi * 17.f / 6.f, true);
    modeSelector.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    modeSelector.setPopupDisplayEnabled (false, false, nullptr);
    modeSelector.setComponentID ("mode-selector");
    modeSelector.setTitle ("Effect mode");
    modeSelector.setTooltip ("Effect mode. Click a name or turn the selector.");
    addAndMakeVisible (modeSelector);
    modeAttachment = std::make_unique<SliderAttachment> (processor.parameters, mode, modeSelector);
    fieldDisplay.setComponentID ("field");
    addAndMakeVisible (fieldDisplay);
    fieldDisplay.gesture = [this] (const char* id, float v, int stage)
    { if (auto* p = processor.parameters.getParameter (id)) { if (stage == 0) p->beginChangeGesture(); else if (stage == 2) p->endChangeGesture(); else p->setValueNotifyingHost (p->convertTo0to1 (v)); } };
    addAndMakeVisible (looperTape);
    addKnob ("ACTIVITY", density, "Activity: changes how often fragments appear and overlap.");
    addKnob ("SHAPE", shape, "Shape: changes the volume contour of each fragment.");
    addKnob ("FILTER", cutoff, "Filter: rolls off high frequencies.");
    addKnob ("MIX", mix, "Mix: blends the original signal with the effect.");
    addKnob ("TIME", nullptr, "Time: subdivision with sync on; manual tempo with sync off.");
    addKnob ("REPEATS", repeats, "Repeats: extends fragments and feeds sound back into the effect.");
    addKnob ("SPACE", space, "Space: adds the selected reverb.");
    addKnob ("LOOP LEVEL", looperLevel, "Loop Level: balances the recorded phrase against live playing.");
    addKnob ("DRIFT", modDepth, "Drift: pitch modulation depth.");
    addKnob ("DRIFT RATE", modRate, "Drift Rate: pitch modulation speed.");
    addKnob ("RESONANCE", resonance, "Resonance: emphasizes frequencies around the filter cutoff.");
    addKnob ("OUTPUT", output, "Output: trims the effect output in decibels.");
    for (std::size_t i = 0; i < modeButtons.size(); ++i)
    {
        auto& button = modeButtons[i];
        setupButton (button, motefield::modeNames[i], modeDescription (static_cast<int> (i)), false, 1);
        button.setColour (juce::TextButton::buttonOnColourId, familyColour (static_cast<int> (i)));
        button.setComponentID ("mode-" + juce::String (static_cast<int> (i)));
        button.onClick = [this, i] { processor.setParameterValue (motefield::parameter::mode, static_cast<float> (i)); };
    }
    for (std::size_t i = 0; i < variationButtons.size(); ++i)
    {
        auto& button = variationButtons[i];
        setupButton (button, juce::String::charToString (static_cast<juce::juce_wchar> ('A' + i)), "Choose a variation of the selected effect.", false, 5);
        button.setColour (juce::TextButton::buttonColourId, paper);
        button.setColour (juce::TextButton::buttonOnColourId, ink);
        button.setComponentID ("variation-" + juce::String (static_cast<int> (i)));
        button.onClick = [this, i] { processor.setParameterValue (motefield::parameter::variation, static_cast<float> (i)); };
    }
    setupButton (reverseButton, "FX REVERSE", "Reverse the effect playback.", true);
    setupButton (syncButton, "SUBDIV", "Switch Time between host-synced subdivisions and manual tempo.", true);
    syncButton.setColour (juce::TextButton::buttonOnColourId, cyan);
    setupButton (tapButton, "TAP", "Tap twice or more to set the manual tempo. This turns host sync off.", false, 2);
    tapButton.setComponentID ("tap");
    tapButton.setColour (juce::TextButton::buttonColourId, blue.brighter (.18f));
    tapButton.setColour (juce::TextButton::buttonOnColourId, cyan);
    tapButton.onClick = [this] { tapTempo(); };
    setupButton (holdButton, "HOLD", "Hold captures the recent sound. Click again to resume live capture.", true, 2);
    holdButton.setColour (juce::TextButton::buttonColourId, yellow);
    holdButton.setColour (juce::TextButton::buttonOnColourId, yellow);
    setupButton (bypassButton, "BYPASS", "Listen to the unprocessed input. Playback continues in the background.", true, 2);
    bypassButton.setColour (juce::TextButton::buttonColourId, blue);
    bypassButton.setColour (juce::TextButton::buttonOnColourId, cyan);
    const auto attachButton = [this] (const char* id, juce::TextButton& button)
    {
        button.setComponentID (id);
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (processor.parameters, id, button));
    };
    attachButton (reverse, reverseButton);
    attachButton (motefield::parameter::sync, syncButton);
    holdButton.setComponentID ("freeze");
    holdButton.setClickingTogglesState (false);
    holdButton.onClick = [this] { if (value ("holdStyle") < .5f) processor.setParameterValue ("freeze", value ("freeze") > .5f ? 0.f : 1.f); };
    holdButton.onStateChange = [this] { if (value ("holdStyle") > .5f) { const bool down = holdButton.isDown(); if (down != momentaryHoldDown) { momentaryHoldDown = down; processor.setParameterValue ("freeze", down ? 1.f : 0.f); } } };
    attachButton (bypass, bypassButton);
    setupButton (recordButton, "REC", "Start a new phrase, up to 60 seconds.", false, 3);
    setupButton (playButton, "PLAY", "Close a recording, finish overdubbing, or resume a stopped phrase.", false, 3);
    setupButton (dubButton, "DUB", "Add another layer. Click again to finish the layer.", false, 3);
    setupButton (stopButton, "STOP", "Stop phrase playback; keep the recording.", false, 3);
    setupButton (undoButton, "UNDO", "Remove the newest overdub pass, keeping older layers.", false, 3);
    setupButton (eraseButton, "ERASE", "Erase the phrase and all overdubs.", false, 3);
    recordButton.setColour (juce::TextButton::buttonOnColourId, coral);
    dubButton.setColour (juce::TextButton::buttonOnColourId, coral);
    const auto command = [this] (juce::TextButton& button, motefield::LooperCommand action, const char* id)
    {
        button.getProperties().set ("style", 6);
        button.setComponentID (id);
        button.onClick = [this, action] { processor.requestLooperCommand (action); };
    };
    command (recordButton, motefield::LooperCommand::record, "record");
    command (playButton, motefield::LooperCommand::play, "play");
    command (dubButton, motefield::LooperCommand::dub, "dub");
    command (stopButton, motefield::LooperCommand::stop, "stop");
    command (undoButton, motefield::LooperCommand::undo, "undo");
    command (eraseButton, motefield::LooperCommand::clear, "erase");
    setupButton (preButton, "PRE", "Place the looper before the effects.");
    setupButton (postButton, "POST", "Record the processed sound after the effects.");
    preButton.onClick = [this] { processor.setParameterValue (motefield::parameter::looperOrder, 1.0f); };
    postButton.onClick = [this] { processor.setParameterValue (motefield::parameter::looperOrder, 0.0f); };
    setupButton (loopReverseButton, "LOOP REVERSE", "Reverse the recorded phrase.", true);
    attachButton (looperReverse, loopReverseButton);
    setupButton (detailsButton, "DETAILS +", "Show drift, resonance, output, reverb character, and loop speed.", true, 4);
    detailsButton.setComponentID ("details");
    detailsButton.onClick = [this] { setDetailsOpen (detailsButton.getToggleState()); };
    setupButton (performanceButton, "PERFORM", "Loop, material, sidechain, pattern and MIDI controls.");
    performanceButton.setComponentID ("perform");
    performanceButton.onClick = [this] { if (! performancePanel) { performancePanel = std::make_unique<PerformancePanel> (processor); addAndMakeVisible (*performancePanel); } performancePanel->setBounds (getLocalBounds().reduced (14)); performancePanel->setVisible (true); performancePanel->toFront (true); };
    setupButton (motionButton, "LESS MOTION", "Keep the liquid voices in fixed positions while their size follows the sound.", true);
    motionButton.onClick = [this] { fieldDisplay.setReducedMotion (motionButton.getToggleState()); };
    setupCombo (presetBox, {}, "Factory and User presets. Loop audio, Hold and Bypass are retained.");
    presetBox.setTextWhenNothingSelected ("Current sound");
    presetBox.setComponentID ("presets");
    refreshPresetMenu();
    presetBox.onChange = [this] { selectPresetItem (presetBox.getSelectedId()); };
    setupButton (previousPreset, "<", "Previous starting point.", false, 4);
    setupButton (nextPreset, ">", "Next starting point.", false, 4);
    previousPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    previousPreset.getProperties().set ("style", 0);nextPreset.getProperties().set ("style", 0);
    setupButton (savePresetButton, "SAVE", "Save this sound to your User presets.", false, 4);
    savePresetButton.setComponentID ("save-preset");
    savePresetButton.getProperties().set ("style", 0);
    savePresetButton.onClick = [this] { showSavePresetDialog(); };
    setupCombo (roomBox, { "Bright", "Dark", "Hall", "Infinite" }, "Reverb character.");
    setupCombo (speedBox, { "1/2x", "1x", "2x" }, "Phrase playback speed.");
    setupCombo (divisionBox, { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 bar" }, "Rhythmic subdivision. Also applies to the manual tempo.");
    comboAttachments.push_back (std::make_unique<ComboAttachment> (processor.parameters, reverbStyle, roomBox));
    comboAttachments.push_back (std::make_unique<ComboAttachment> (processor.parameters, looperSpeed, speedBox));
    comboAttachments.push_back (std::make_unique<ComboAttachment> (processor.parameters, division, divisionBox));
    tempoLabel.setJustificationType (juce::Justification::centred);
    tempoLabel.setColour (juce::Label::textColourId, muted);
    addAndMakeVisible (tempoLabel);
    timeValueLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (timeValueLabel);
    updateTimeAttachment();
    setResizable (true, true);
    int initialWidth = 1000;
    if (const auto* screen = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        initialWidth = juce::jmin (initialWidth, juce::roundToInt (screen->userBounds.getWidth() * .80f),
                                 juce::roundToInt (screen->userBounds.getHeight() * .80f * 1240.f / 800.f));
    initialWidth = juce::jmax (640, initialWidth);
    const auto minimumWidth = juce::jmin (900, initialWidth);
    setResizeLimits (minimumWidth, juce::roundToInt (minimumWidth * 800.0 / 1240.0), 1860, 1200);
    getConstrainer()->setFixedAspectRatio (1240.0 / 800.0);
    setSize (initialWidth, juce::roundToInt (initialWidth * 800.0 / 1240.0));
    refreshDisplay();
    startTimerHz (60);
}

MoteFieldAudioProcessorEditor::~MoteFieldAudioProcessorEditor() { stopTimer(); if (momentaryHoldDown) processor.setParameterValue ("freeze",0.f); setLookAndFeel (nullptr); }
void MoteFieldAudioProcessorEditor::setupButton (juce::TextButton& button, const juce::String& name,
                                               const juce::String& help, bool toggles, int style)
{
    button.setButtonText (name);
    button.setTitle (name);
    button.setTooltip (help);
    button.setClickingTogglesState (toggles);
    button.getProperties().set ("style", style);
    button.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffeae9e3));
    button.setColour (juce::TextButton::buttonOnColourId, cyan);
    addAndMakeVisible (button);
}
void MoteFieldAudioProcessorEditor::setupCombo (juce::ComboBox& box, const juce::StringArray& items, const juce::String& help)
{
    box.addItemList (items, 1);
    box.setJustificationType (juce::Justification::centred);
    box.setTooltip (help);
    addAndMakeVisible (box);
}
void MoteFieldAudioProcessorEditor::addKnob (const juce::String& title, const char* id, const juce::String& help)
{
    auto knob = std::make_unique<ParameterKnob> (title, help);
    addAndMakeVisible (*knob);
    if (id != nullptr)
    {
        knob->slider.setComponentID (id);
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (processor.parameters, id, knob->slider));
        const auto* parameter = processor.parameters.getParameter (id);
        knob->slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
        if (juce::String (id) == motefield::parameter::cutoff || juce::String (id) == motefield::parameter::modRate)
            knob->slider.textFromValueFunction = [] (double v) { return v >= 1000.0 ? juce::String (v / 1000.0, 1) + " kHz" : (v < 10.0 ? juce::String (v, 2) : juce::String (juce::roundToInt (v))) + " Hz"; };
        else if (juce::String (id) == motefield::parameter::output)
            knob->slider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
        else knob->slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    }
    knobs.push_back (std::move (knob));
}
float MoteFieldAudioProcessorEditor::value (const char* id) const { return processor.parameters.getRawParameterValue (id)->load(); }
void MoteFieldAudioProcessorEditor::updateTimeAttachment()
{
    lastSync = value (motefield::parameter::sync) > .5f;
    auto& slider = knobs[4]->slider;
    timeAttachment.reset();
    slider.textFromValueFunction = nullptr;
    slider.valueFromTextFunction = nullptr;
    const auto* id = lastSync ? motefield::parameter::division : motefield::parameter::tempo;
    slider.setComponentID ("time");
    timeAttachment = std::make_unique<SliderAttachment> (processor.parameters, id, slider);
    const auto* parameter = processor.parameters.getParameter (id);
    slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
    if (! lastSync) slider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " BPM"; };
    syncButton.setButtonText (lastSync ? "SUBDIV" : "TEMPO");
}
void MoteFieldAudioProcessorEditor::tapTempo()
{
    const auto now = juce::Time::getMillisecondCounterHiRes(), interval = now - lastTap;
    if (lastTap > 0.0 && interval >= 250.0 && interval <= 1500.0)
    {
        tapInterval = tapCount <= 1 ? interval : tapInterval * .6 + interval * .4;
        processor.setParameterValue (motefield::parameter::tempo, static_cast<float> (60000.0 / tapInterval));
        processor.setParameterValue (motefield::parameter::sync, 0.0f);
        ++tapCount;
    }
    else tapCount = 1;
    lastTap = now;
    tapFlash = 9;
}
void MoteFieldAudioProcessorEditor::loadPreset (int index)
{
    processor.applyFactoryPreset (index);
    presetBox.setText (processor.currentPresetName(), juce::dontSendNotification);
}

void MoteFieldAudioProcessorEditor::refreshPresetMenu()
{
    userPresets = MoteFieldAudioProcessor::userPresetFiles();
    auto* menu = presetBox.getRootMenu();
    menu->clear();
    juce::PopupMenu factory, user;
    const auto names = MoteFieldAudioProcessor::factoryPresetNames();
    for (int i = 0; i < names.size(); ++i) factory.addItem (i + 1, names[i]);
    for (int i = 0; i < userPresets.size(); ++i) user.addItem (1001 + i, userPresets[i].getFileNameWithoutExtension());
    if (userPresets.isEmpty()) user.addItem (19999, "No saved presets yet", false);
    menu->addSubMenu ("Factory", factory);
    menu->addSubMenu ("User", user);
    menu->addSeparator();
    menu->addItem (20000, "Save preset...");
    menu->addItem (20001, "Refresh user presets");
    presetBox.setText (processor.currentPresetName(), juce::dontSendNotification);
}

void MoteFieldAudioProcessorEditor::selectPresetItem (int id)
{
    if (id >= 1 && id <= MoteFieldAudioProcessor::factoryPresetNames().size()) loadPreset (id - 1);
    else if (id >= 1001 && id < 1001 + userPresets.size())
    {
        const auto result = processor.loadUserPreset (userPresets[id - 1001]);
        if (result.failed()) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset could not be loaded", result.getErrorMessage());
    }
    else if (id == 20000) showSavePresetDialog();
    else if (id == 20001) refreshPresetMenu();
    presetBox.setText (processor.currentPresetName(), juce::dontSendNotification);
}

void MoteFieldAudioProcessorEditor::stepPreset (int direction)
{
    const auto names = MoteFieldAudioProcessor::factoryPresetNames();
    int current = -1;
    if (processor.currentPresetSource() == "factory") current = names.indexOf (processor.currentPresetName());
    else for (int i = 0; i < userPresets.size(); ++i)
        if (userPresets[i].getFileNameWithoutExtension() == processor.currentPresetName()) current = names.size() + i;
    const int total = names.size() + userPresets.size();
    const int next = current < 0 ? (direction > 0 ? 0 : total - 1) : (current + direction + total) % total;
    selectPresetItem (next < names.size() ? next + 1 : 1001 + next - names.size());
}

void MoteFieldAudioProcessorEditor::showSavePresetDialog()
{
    if (presetNameDialog != nullptr) return;
    presetNameDialog = std::make_unique<juce::AlertWindow> ("Save user preset", "Name this sound. Loop audio, Hold and Bypass are not stored in sound presets.", juce::MessageBoxIconType::NoIcon);
    presetNameDialog->addTextEditor ("name", processor.currentPresetSource().isEmpty() ? "My preset" : processor.currentPresetName(), "Preset name");
    presetNameDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    presetNameDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    const juce::Component::SafePointer<MoteFieldAudioProcessorEditor> safe (this);
    presetNameDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->presetNameDialog == nullptr) return;
        const auto name = safe->presetNameDialog->getTextEditorContents ("name");
        safe->presetNameDialog.reset();
        if (result == 1) safe->savePresetNamed (name);
    }));
}

void MoteFieldAudioProcessorEditor::savePresetNamed (const juce::String& name, bool overwrite)
{
    const auto result = processor.saveUserPreset (name, overwrite);
    if (result.wasOk()) { refreshPresetMenu(); return; }
    if (! overwrite && result.getErrorMessage() == "A user preset with this name already exists.")
    {
        const juce::Component::SafePointer<MoteFieldAudioProcessorEditor> safe (this);
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Replace user preset?",
            "Replace \"" + name.trim() + "\" with the current sound?", "Replace", "Cancel", this,
            juce::ModalCallbackFunction::create ([safe, name] (int answer) { if (safe != nullptr && answer == 1) safe->savePresetNamed (name, true); }));
    }
    else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Preset could not be saved", result.getErrorMessage());
}
void MoteFieldAudioProcessorEditor::setDetailsOpen (bool open)
{
    if (detailsOpen == open) return;
    if (open) collapsedWidth = getWidth();
    const auto previousHeight = getHeight();
    detailsOpen = open;
    const auto height = detailsOpen ? 990.0 : 800.0;
    // Opening the drawer must not grow beyond the window's existing height.
    // Also allow smaller sizes when the host window sits near a screen edge.
    auto width = open ? juce::jmin (getWidth(), static_cast<int> (previousHeight * 1240.0 / height)) : collapsedWidth;
    auto maximumWidth = 1860;
    const auto screen = getScreenBounds();
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (screen))
    {
        const auto available = display->userBounds.reduced (12.0f);
        const auto scaleX = static_cast<float> (screen.getWidth()) / static_cast<float> (juce::jmax (1, getWidth()));
        const auto scaleY = static_cast<float> (screen.getHeight()) / static_cast<float> (juce::jmax (1, getHeight()));
        const auto roomWidth = (available.getRight() - juce::jmax (available.getX(), static_cast<float> (screen.getX()))) / juce::jmax (.1f, scaleX);
        const auto roomHeight = (available.getBottom() - juce::jmax (available.getY(), static_cast<float> (screen.getY()))) / juce::jmax (.1f, scaleY);
        maximumWidth = juce::jmax (1, juce::jmin (maximumWidth, static_cast<int> (roomWidth), static_cast<int> (roomHeight * 1240.0 / height)));
    }
    width = juce::jlimit (1, maximumWidth, width);
    const auto minimumWidth = juce::jmin (1000, width);
    getConstrainer()->setFixedAspectRatio (1240.0 / height);
    setResizeLimits (minimumWidth, juce::roundToInt (minimumWidth * height / 1240.0), maximumWidth, juce::roundToInt (maximumWidth * height / 1240.0));
    setSize (width, juce::roundToInt (width * height / 1240.0));
    detailsButton.setButtonText (detailsOpen ? "DETAILS -" : "DETAILS +");
    resized();
}
juce::String MoteFieldAudioProcessorEditor::currentHelp() const
{
    auto* hovered = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
    if (hovered != nullptr && (hovered == this || isParentOf (hovered)))
        if (auto* tooltip = dynamic_cast<juce::TooltipClient*> (hovered))
            if (tooltip->getTooltip().isNotEmpty()) return tooltip->getTooltip().upToFirstOccurrenceOf ("  Double-click", false, false);
    return modeDescription (static_cast<int> (value (motefield::parameter::mode))) + "  /  "
           + (value (motefield::parameter::reverse) > .5f ? "Reverse" : "Forward");
}
void MoteFieldAudioProcessorEditor::refreshDisplay()
{
    using namespace motefield::parameter;
    if (lastSync != (value (motefield::parameter::sync) > .5f) && ! knobs[4]->slider.isMouseButtonDown()) updateTimeAttachment();
    motefield::VisualFrame frame = fieldDisplay.currentFrame();
    const auto previousAudioTime = frame.sampleTime;
    processor.readVisualFrame (frame);
    const auto freshAudio = frame.sampleTime != previousAudioTime;
    frame.mode = static_cast<motefield::Mode> (static_cast<int> (value (mode)));
    frame.variation = static_cast<int> (value (variation));
    frame.held = value (freeze) > .5f;
    holdButton.setToggleState (frame.held, juce::dontSendNotification);
    frame.reverse = value (reverse) > .5f;
    frame.bypass = value (bypass) > .5f;
    fieldDisplay.setSampleRate (processor.getSampleRate());
    fieldDisplay.setShape (value (shape));
    fieldDisplay.update (frame);
    const auto targetLogo = freshAudio ? std::tanh (juce::jmax (frame.inputLevel, frame.outputLevel) * 3.f) : 0.f;
    logoDrive += (targetLogo - logoDrive) * (targetLogo > logoDrive ? .30f : .11f);
    if (logoDrive < .0001f) logoDrive = 0.f;
    for (std::size_t i = 0; i < modeButtons.size(); ++i) modeButtons[i].setToggleState (i == static_cast<std::size_t> (frame.mode), juce::dontSendNotification);
    for (std::size_t i = 0; i < variationButtons.size(); ++i) variationButtons[i].setToggleState (i == static_cast<std::size_t> (frame.variation), juce::dontSendNotification);
    const auto state = processor.getLooperState();
    const auto empty = state == motefield::LooperState::empty, recording = state == motefield::LooperState::recording;
    const auto playing = state == motefield::LooperState::playing, dubbing = state == motefield::LooperState::overdubbing;
    recordButton.setEnabled (empty || recording);
    recordButton.setButtonText (frame.loopPending ? "ARMED" : "REC");
    recordButton.setToggleState (recording, juce::dontSendNotification);
    playButton.setEnabled (! empty);
    playButton.setToggleState (playing, juce::dontSendNotification);
    dubButton.setEnabled (playing || dubbing);
    dubButton.setToggleState (dubbing, juce::dontSendNotification);
    stopButton.setEnabled (recording || playing || dubbing);
    undoButton.setEnabled (frame.canUndo);
    eraseButton.setEnabled (! empty);
    preButton.setToggleState (value (looperOrder) > .5f, juce::dontSendNotification);
    postButton.setToggleState (value (looperOrder) <= .5f, juce::dontSendNotification);
    if (tapFlash > 0) --tapFlash;
    tapButton.setToggleState (tapFlash > 0, juce::dontSendNotification);
    const auto source = processor.isReceivingHostTempo() && lastSync ? "HOST " : lastSync ? "INTERNAL " : "MANUAL ";
    const auto bpm = processor.isReceivingHostTempo() && lastSync ? processor.getEffectiveBpm() : static_cast<double> (value (tempo));
    tempoLabel.setText (juce::String (source) + juce::String (juce::roundToInt (bpm)) + " BPM", juce::dontSendNotification);
    timeValueLabel.setText (lastSync ? divisionBox.getText() : juce::String (juce::roundToInt (value (tempo))), juce::dontSendNotification);
    const auto presetName = processor.currentPresetName() + (processor.isPresetModified() ? " *" : "");
    if (presetBox.getText() != presetName) presetBox.setText (presetName, juce::dontSendNotification);
    helpText = currentHelp();
    looperTape.repaint();
    repaint();
}

void MoteFieldAudioProcessorEditor::resized()
{
    const auto s = static_cast<float> (getWidth()) / 1240.0f;
    const auto place = [s] (juce::Component& component, int x, int y, int width, int height)
    {
        component.setBounds ((juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y), static_cast<float> (width), static_cast<float> (height)) * s).toNearestInt());
    };
    if (! hardwarePanel.isValid() || hardwarePanel.getHeight() != (detailsOpen ? 990 : 800)) hardwarePanel = makeHardwarePanel (detailsOpen ? 990 : 800);
    for (int i = 0; i < 8; ++i) place (*knobs[static_cast<std::size_t> (i)], 50 + (i % 4) * 183, i < 4 ? 106 : 265, 153, 143);
    if (performancePanel) performancePanel->setBounds (getLocalBounds().reduced (14));
    place (performanceButton, 628, 420, 110, 30);
    place (syncButton, 62, 420, 94, 30);
    place (timeValueLabel, 157, 420, 62, 30);
    timeValueLabel.setFont (font (13.0f * s, true));
    place (tempoLabel, 230, 425, 180, 18);
    tempoLabel.setFont (font (9.5f * s, false, .065f));
    place (modeSelector, 941, 226, 116, 116);
    for (int i = 0; i < 11; ++i)
    {
        const auto a = (-150.f + i * 30.f) * pi / 180.f;
        const auto cx = 999.f + std::sin (a) * 118.f, cy = 284.f - std::cos (a) * 118.f;
        place (modeButtons[static_cast<std::size_t> (i)], juce::roundToInt (cx - 29), juce::roundToInt (cy - 15), 58, 30);
    }
    place (reverseButton, 634, 486, 112, 30);
    for (int i = 0; i < 4; ++i) place (variationButtons[static_cast<std::size_t> (i)], 903 + i * 75, 438, 65, 35);
    place (fieldDisplay, 44, 480, 718, 198);
    place (tapButton, 804, 488, 114, 181);
    place (holdButton, 942, 488, 114, 181);
    place (bypassButton, 1080, 488, 114, 181);
    place (looperTape, 218, 691, 404, 30);
    place (recordButton, 44, 729, 79, 34);
    place (playButton, 136, 729, 79, 34);
    place (dubButton, 228, 729, 79, 34);
    place (stopButton, 340, 729, 79, 34);
    place (undoButton, 432, 729, 79, 34);
    place (eraseButton, 524, 729, 79, 34);
    place (preButton, 803, 729, 55, 34);
    place (postButton, 862, 729, 55, 34);
    place (loopReverseButton, 944, 729, 95, 34);
    place (detailsButton, 1080, 729, 114, 34);
    place (presetBox, 798, 51, 180, 36);
    place (savePresetButton, 979, 51, 48, 36);
    place (previousPreset, 764, 52, 30, 34);
    place (nextPreset, 1030, 52, 30, 34);
    for (int i = 8; i < 12; ++i)
    {
        knobs[static_cast<std::size_t> (i)]->setVisible (detailsOpen);
        place (*knobs[static_cast<std::size_t> (i)], 48 + (i - 8) * 175, 812, 145, 140);
    }
    for (auto* component : std::array<juce::Component*, 4> { &roomBox, &speedBox, &divisionBox, &motionButton }) component->setVisible (detailsOpen);
    place (roomBox, 804, 830, 157, 34);
    place (speedBox, 990, 830, 204, 34);
    place (divisionBox, 804, 902, 157, 34);
    place (motionButton, 990, 902, 204, 34);
}

void MoteFieldAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto scale = static_cast<float> (getWidth()) / 1240.f;
    g.addTransform (juce::AffineTransform::scale (scale));
    g.drawImageAt (hardwarePanel, 0, 0);
    const auto& frame = fieldDisplay.currentFrame();
    if (rangoLogo.isValid())
    {
        juce::Graphics::ScopedSaveState save (g);
        const auto drive = motionButton.getToggleState() ? 0.f : logoDrive;
        const auto transform = juce::AffineTransform::scale (1.f + drive * .045f, 1.f - drive * .025f, 93.f, 64.f)
                                  .rotated (drive * .025f, 93.f, 64.f);
        g.addTransform (transform);g.setColour (ink);
        g.drawImage (rangoLogo, { 32, 7, 122, 122 }, juce::RectanglePlacement::centred, true);
    }
    text (g, "MoteField", { 151, 27, 470, 51 }, 42, ink, true);
    text (g, "by Rango Labs", { 154, 81, 300, 24 }, 15, muted);
    text (g, "Out", { 1072, 51, 38, 36 }, 11, muted);
    const auto meter = juce::jlimit (0.f, 1.f, (juce::Decibels::gainToDecibels (frame.outputLevel, -60.f) + 48.f) / 48.f);
    for (int i = 0; i < 14; ++i)
    {
        g.setColour (meter > static_cast<float> (i) / 14.f ? ink : line.withAlpha (.6f));
        const float h = 9.f + i * 1.15f;
        g.fillRect (1117.f + i * 5.5f, 84.f-h, 3.f, h);
    }
    text (g, "EFFECT MODE", { 810, 123, 375, 25 }, 11, muted, true, juce::Justification::centred, .07f);
    const auto mode = juce::jlimit (0, 10, static_cast<int> (frame.mode));
    const auto family = mode < 3 ? "Micro loops" : mode < 6 ? "Granules" : mode < 9 ? "Glitch" : "Multi delay";
    text (g, family, { 810, 405, 375, 22 }, 11, muted, false, juce::Justification::centred);
    text (g, "VARIATION", { 811, 439, 85, 32 }, 9, muted);
    g.setColour (line.withAlpha (.7f));g.drawLine (50, 462, 760, 462, .8f);
    text (g, "PHRASE LOOPER", { 49, 694, 160, 23 }, 10, ink, true);
    const auto recording = frame.loopState == motefield::LooperState::recording;
    const auto time = frame.loopState == motefield::LooperState::empty ? "READY / 60 SEC"
        : secondsText (recording ? frame.loopSeconds : frame.loopSeconds * frame.loopProgress) + " / " + (recording ? "01:00" : secondsText (frame.loopSeconds));
    text (g, time, { 632, 694, 135, 23 }, 10, muted, false, juce::Justification::centredRight);
    text (g, "LOOPER POSITION", { 809, 696, 144, 19 }, 8, muted);
    text (g, helpText, { 49, 773, 965, 17 }, 9.5f, muted);
    text (g, "RANGO LABS / " + juce::String (JucePlugin_VersionString), { 1060, 773, 132, 17 }, 8, muted, false, juce::Justification::centredRight);
    if (detailsOpen)
    {
        text (g, "REVERB CHARACTER", { 805, 810, 160, 18 }, 8, muted, true);
        text (g, "LOOP SPEED", { 992, 810, 160, 18 }, 8, muted, true);
        text (g, "SUBDIVISION", { 805, 881, 160, 18 }, 8, muted, true);
        text (g, "DISPLAY", { 992, 881, 160, 18 }, 8, muted, true);
        text (g, "Loops save with your project. Open Perform for capture, export, material and MIDI controls.", { 50, 961, 1140, 18 }, 10, muted);
    }
}
