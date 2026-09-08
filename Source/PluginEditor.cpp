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
juce::Image makeHardwarePanel (int height, const appearance::Palette& p)
{
    juce::Image image (juce::Image::RGB,1620,height,true);
    juce::Graphics g(image);
    g.fillAll(p.paper);
    const auto enclosure=juce::ImageCache::getFromMemory(BinaryData::AcidEnclosurev1_png,BinaryData::AcidEnclosurev1_pngSize);
    g.drawImage(enclosure,juce::Rectangle<float>(0,0,1620,972));
    // Dark finish keeps the same physical enclosure and panel geometry.
    if(p.paper.getBrightness()<.5f)
    {
        g.setColour(juce::Colour(0xff111a22).withAlpha(.65f));
        g.fillRoundedRectangle({31,25,1555,73},30);
        juce::Path rail;rail.startNewSubPath(328,116);rail.quadraticTo(339,80,391,86);rail.lineTo(1220,86);rail.quadraticTo(1310,103,1260,170);rail.lineTo(1155,215);rail.lineTo(477,218);rail.quadraticTo(369,212,328,151);rail.closeSubPath();g.fillPath(rail);
        g.fillRoundedRectangle({213,580,1290,120},50);
    }
    if(height>972)
    { g.setColour(p.paper);g.fillRoundedRectangle({32,984,1556,220},20);g.setColour(p.line);g.drawRoundedRectangle({36,988,1548,212},17,1); }
    return image;
}

juce::String secondsText (float seconds)
{
    const auto total = juce::jmax (0, static_cast<int> (seconds));
    return juce::String (total / 60).paddedLeft ('0', 2) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
}
}

MoteFieldLookAndFeel::MoteFieldLookAndFeel() { setAppearance (false, juce::Colour (appearance::acid)); }
void MoteFieldLookAndFeel::setAppearance (bool dark, juce::Colour accent)
{
    colours = appearance::palette (dark, accent);
    const auto [paper, ink, muted, line, blue, cyan, coral, yellow, mint] = colours;
    const std::array<juce::Colour,9> values { paper,ink,muted,line,blue,cyan,coral,yellow,mint };
    for (int i=0;i<9;++i) setColour (appearance::baseId+i,values[i]);
    setColour (juce::Slider::thumbColourId, cyan);
    setColour (juce::Slider::trackColourId, cyan);
    setColour (juce::TextEditor::textColourId, ink);
    setColour (juce::TextEditor::backgroundColourId, paper);
    setColour (juce::TextEditor::highlightColourId, cyan.withAlpha (.3f));
    setColour (juce::AlertWindow::backgroundColourId, paper);
    setColour (juce::AlertWindow::textColourId, ink);
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
    const auto [paper, ink, muted, line, blue, cyan, coral, yellow, mint] = colours;
    const auto size=static_cast<float>(juce::jmin(width,height));
    const juce::Point<float> centre(x+width*.5f,y+height*.5f);
    const bool selector=slider.getComponentID()=="mode-selector";
    const auto radius=size*(selector?.35f:.405f),angle=start+position*(end-start);
    if(selector)
    {
        for(int tick=0;tick<=30;++tick)
        {
            const auto a=start+(end-start)*tick/30.f;
            g.setColour(juce::Colours::white.withAlpha(tick%3==0?.75f:.30f));
            g.drawLine({centre.getPointOnCircumference(size*(tick%3==0?.435f:.465f),a),centre.getPointOnCircumference(size*.49f,a)},juce::jmax(.6f,size*.005f));
        }
        const auto point=centre.getPointOnCircumference(size*.432f,angle);
        g.setGradientFill(juce::ColourGradient(cyan.withAlpha(.55f),point.x,point.y,cyan.withAlpha(0.f),point.x+size*.05f,point.y,true));g.fillEllipse(juce::Rectangle<float>(size*.10f,size*.10f).withCentre(point));
        g.setColour(cyan);g.fillEllipse(juce::Rectangle<float>(size*.027f,size*.027f).withCentre(point));
    }
    else
    {
        const auto origin=centre.translated(0,size*.40f);
        g.setGradientFill(juce::ColourGradient(cyan.withAlpha(.42f),origin.x,origin.y,cyan.withAlpha(0.f),origin.x+size*.43f,origin.y,true));
        g.fillEllipse(juce::Rectangle<float>(size*.96f,size*.24f).withCentre(origin));
        juce::Path light;light.addCentredArc(centre.x,centre.y+size*.065f,radius,radius,0,pi*.30f,pi*1.70f,true);
        for(int glow=8;glow>0;--glow){g.setColour(cyan.withAlpha(.023f));g.strokePath(light,juce::PathStrokeType(glow*size*.015f));}
        g.setColour(cyan.brighter(.15f));g.strokePath(light,juce::PathStrokeType(size*.015f));
    }
    const auto knob=juce::ImageCache::getFromMemory(BinaryData::AcidKnobv1_png,BinaryData::AcidKnobv1_pngSize);
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(knob,juce::Rectangle<float>(size*(selector?.80f:.92f),size*(selector?.80f:.92f)).withCentre(centre.translated(0,size*(selector?.07f:.08f))));
    if(!selector)
    {
        juce::Path lip;lip.addCentredArc(centre.x,centre.y+size*.045f,size*.435f,size*.435f,0,pi*.42f,pi*1.58f,true);
        for(int halo=7;halo>0;--halo){g.setColour(cyan.withAlpha(.055f));g.strokePath(lip,juce::PathStrokeType(size*.012f*halo));}
        g.setColour(cyan.interpolatedWith(juce::Colours::white,.55f));g.strokePath(lip,juce::PathStrokeType(size*.008f));
    }
    juce::Path pointer;pointer.startNewSubPath(centre.getPointOnCircumference(radius*.58f,angle));pointer.lineTo(centre.getPointOnCircumference(radius*.79f,angle));
    g.setColour(juce::Colour(0xff202422));g.strokePath(pointer,juce::PathStrokeType(size*.026f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    if(slider.getComponentID()!="mode-selector"&&slider.isMouseOverOrDragging())
        text(g,slider.getTextFromValue(slider.getValue()),{centre.x-radius,centre.y+radius*.30f,radius*2,radius*.35f},size*.10f,juce::Colour(0xff202422),true,juce::Justification::centred);
}

void MoteFieldLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                const juce::Colour&, bool hover, bool down)
{
    const auto [paper, ink, muted, line, blue, cyan, coral, yellow, mint] = colours;
    const auto style = static_cast<int> (button.getProperties()["style"]);
    const auto active = button.getToggleState();
    const auto bounds = button.getLocalBounds().toFloat();
    const auto scale = juce::jmax (.7f, static_cast<float> (button.getHeight()) / (style == 2 ? 114.0f : 34.0f));
    if (style == 6 || button.getProperties()["utility"])
    {
        const auto face = bounds.reduced(3);
        auto colour = active ? cyan.darker(.75f) : juce::Colour(0xff0e1110);
        for(int halo=3;halo>0;--halo) if(active) {g.setColour(cyan.withAlpha(.12f));g.fillRoundedRectangle(face.expanded(halo),8);}
        g.setGradientFill(juce::ColourGradient(colour.brighter(.07f),0,0,colour,0,bounds.getBottom(),false));g.fillRoundedRectangle(face,7);
        g.setColour(active?cyan:juce::Colour(0xff555957));g.drawRoundedRectangle(face,7,1);
        g.setColour(active?cyan:juce::Colour(0xffe9ede9));
        const auto cx=bounds.getCentreX(), cy=bounds.getHeight()*.34f;
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
        else if(button.getProperties()["utility"])
        {
            const auto unit=5.f*scale;
            if(id=="settings")
            {for(int row=0;row<3;++row){const auto yy=cy+(row-1)*unit;g.drawLine(cx-unit,yy,cx+unit,yy,.9f*scale);const auto xx=cx+(row%2?-.35f:.35f)*unit;g.fillEllipse(xx-scale,yy-2*scale,2*scale,4*scale);}}
            else if(id=="details")
            {for(int dot=-1;dot<=1;++dot)g.drawEllipse(cx+dot*unit-scale,cy-scale,2*scale,2*scale,.8f*scale);}
            else
            {juce::Path arrows;arrows.startNewSubPath(cx-unit,cy-3*scale);arrows.lineTo(cx+unit,cy-3*scale);arrows.lineTo(cx+unit-3*scale,cy-6*scale);
             arrows.startNewSubPath(cx+unit,cy+3*scale);arrows.lineTo(cx-unit,cy+3*scale);arrows.lineTo(cx-unit+3*scale,cy+6*scale);g.strokePath(arrows,juce::PathStrokeType(scale));}
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
        if(hover) {g.setColour(juce::Colours::white.withAlpha(.08f));g.fillRoundedRectangle(bounds,5);}
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
    const bool dark=button.getProperties()["darkControl"] || paper.getBrightness()<.5f;
    auto fill=active?cyan:dark?juce::Colour(0xff191d1b):juce::Colour(0xffe8e9e4);
    if(hover)fill=fill.brighter(.04f);
    const auto edge=juce::jmax(.65f,scale*.65f);
    const auto pocket=bounds.reduced(2*edge);
    const auto radius=(style==2?13.f:8.f)*scale;
    if(active||down)for(int halo=6;halo>0;--halo){g.setColour(cyan.withAlpha(.045f));g.fillRoundedRectangle(pocket.expanded(halo*.5f*edge),radius+halo*.5f*edge);}
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff444942),0,pocket.getY(),juce::Colour(0xffc5c9c0),0,pocket.getBottom(),false));g.fillRoundedRectangle(pocket,radius);
    g.setColour(juce::Colours::black.withAlpha(.8f));g.fillRoundedRectangle(pocket.reduced(2*edge),radius-2*edge);
    const auto face=pocket.reduced(4*edge).translated(0,down?1.5f*edge:-edge);
    g.setGradientFill(juce::ColourGradient(fill.brighter(.16f),face.getX(),face.getY(),fill.darker(.1f),face.getRight(),face.getBottom(),false));g.fillRoundedRectangle(face,radius-3*edge);
    if(!dark&&!active)
    {
        juce::Graphics::ScopedSaveState texture(g);juce::Path clip;clip.addRoundedRectangle(face,radius-3*edge);g.reduceClipRegion(clip);g.setOpacity(.25f);
        const auto enamel=juce::ImageCache::getFromMemory(BinaryData::AcidKnobv1_png,BinaryData::AcidKnobv1_pngSize);g.drawImage(enamel.getClippedImage({400,300,400,400}),face);
    }
    g.setColour(juce::Colours::white.withAlpha(dark?.16f:.8f));g.drawRoundedRectangle(face.reduced(edge),radius-4*edge,edge);
    g.setColour(juce::Colours::black.withAlpha(.16f));g.drawRoundedRectangle(face,radius-3*edge,.7f*edge);
    if(!button.isEnabled()){g.setColour(paper.withAlpha(.35f));g.fillRoundedRectangle(face,radius-3*edge);}

}

void MoteFieldLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const auto [paper, ink, muted, line, blue, cyan, coral, yellow, mint] = colours;
    const auto style = static_cast<int> (button.getProperties()["style"]);
    auto bounds = button.getLocalBounds().toFloat();
    const auto scale = static_cast<float> (style == 2 ? juce::jmin (button.getWidth(), button.getHeight()) : button.getHeight()) / (style == 2 ? 83.0f : style == 1 ? 22.0f : style == 5 ? 45.0f : 34.0f);
    const bool utility=button.getProperties()["utility"];
    if (style == 6 || utility) bounds = bounds.withTrimmedTop(bounds.getHeight()*.61f).withTrimmedBottom(3);
    const auto colour = !button.isEnabled() ? muted.withAlpha (.65f)
        : style == 1 && button.getToggleState() ? cyan
        : (style==6||utility) && button.getToggleState() ? cyan
        : button.getToggleState() && style != 3 && style != 4 && style != 6 ? appearance::onAccent (cyan)
        : (style == 1 && button.getProperties()["glassDark"]) || button.getProperties()["darkControl"] ? juce::Colour (0xffeff3f3) : ink;

    text (g, button.getButtonText(), bounds, (button.getProperties()["panelButton"] ? 12.5f : style == 2 ? 14.0f : style == 5 ? 16.0f : style == 1 ? 12.5f : style == 6 || utility ? 7.5f : 11.0f) * scale,
          colour, style == 5 || (style == 1 && button.getToggleState()), juce::Justification::centred,
          style == 1 ? 0.0f : .04f);
}

void MoteFieldLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&)
{
    const auto [paper, ink, muted, line, blue, cyan, coral, yellow, mint] = colours;
    const auto bounds = juce::Rectangle<float> (static_cast<float> (width), static_cast<float> (height)).reduced (.8f);
    g.setColour (paper);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (line);
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
    const auto [paper, ink, muted, line, blue, cyan, coral, yellow, mint] = appearance::read (*this);
    const auto h = static_cast<float> (getHeight());
    text (g, title, { 0.0f, h * .83f, static_cast<float> (getWidth()), h * .17f }, h * .113f, ink, false, juce::Justification::centred, .01f);
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
        const auto destination = signalCurrent && measured > .00002f ? std::sqrt (std::tanh (measured * gain)) : 0.f;
        current += (destination - current) * (1.f - std::exp (-dt * 24.f));
        if (current < .00005f) current = 0.f;
    };
    followBand (lowDrive, next.spectralEnergy[0], 6.f);
    followBand (midDrive, next.spectralEnergy[1], 7.f);
    followBand (highDrive, next.spectralEnergy[2], 10.f);
    if (outputDrive < .0001f) outputDrive = 0.f;
    const auto energy = (lowDrive + midDrive + highDrive) / 3.f;
    transientDrive += (juce::jmax(0.f, energy - lastDrive) * 6.f - transientDrive) * (1.f - std::exp(-dt * 18.f));
    lastDrive = energy;
    if (advanced && !reducedMotion) motionPhase += dt * energy * 4.f;
    if (energy == 0.f) { motionPhase = 0.f; transientDrive = 0.f; }
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
    const auto targetX = 240.f + (reducedMotion ? 0.f : std::sin(motionPhase) * (energy * 15.f + transientDrive * 12.f)) + (weight > .001f && ! reducedMotion ? (meanX / weight - .5f) * 45.f : 0.f) + (reducedMotion ? 0.f : (next.fieldPosition * 2.f - next.fieldSplit) * 55.f);
    const auto targetY = juce::jlimit (65.f, 79.f, 72.f + (weight > .001f && ! reducedMotion ? (meanY / weight - .5f) * 22.f : 0.f) - (reducedMotion ? 0.f : next.fieldPitch * .6f));
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
        const auto tx = alive ? cluster.x / cluster.energy * liquidWidth + (reducedMotion ? 0.f : std::sin(motionPhase + static_cast<float>(i)*1.7f) * energy * 24.f) : centreX;
        const auto ty = alive ? cluster.y / cluster.energy * 144.f : centreY;
        const auto spread = ((.45f + .80f * amount) * (1.5f - next.cohesion) + next.fieldSplit * .35f) * next.width;
        const auto x = reducedMotion ? 95.f + i * 72.f : centreX + (tx - centreX) * spread;
        const auto y = reducedMotion ? 72.f : centreY + (ty - centreY) * spread;
        spring (body.x, body.vx, juce::jlimit (104.f, liquidWidth - 104.f, x), 30.f - next.viscosity * 20.f);
        spring (body.y, body.vy, juce::jlimit (43.f, 144.f - 43.f, y), 30.f - next.viscosity * 20.f);
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
    const auto accent=appearance::read(*this).cyan;
    const auto area=getLocalBounds().toFloat().reduced(1.f);
    const auto view=area.reduced(16.f*area.getWidth()/718.f,14.f*area.getWidth()/718.f);
    const float pixelAspect=(view.getWidth()*.69f*.95f/liquidWidth)/(view.getHeight()/liquidHeight);
    const float blend = std::pow (16.f + frame.cohesion * 12.f, 2.f);
    const auto deposit = [this, blend, pixelAspect] (float cx, float cy, float radius, float stretch, float strength = 1.f)
    {
        const auto ax = 1.f / stretch, ay = stretch / pixelAspect;
        // Retain the engine's original 480 x 144 motion space, while shading
        // round volumes in the actual display's aspect ratio.
        cx=240.f+(cx-240.f)*1.12f; cy=120.f+(cy-72.f)*1.8f;
        radius=juce::jmin(radius,(liquidHeight*.5f-24.f)*ay);
        cx=juce::jlimit(radius/ax+24.f,liquidWidth-radius/ax-24.f,cx);
        cy=juce::jlimit(radius/ay+24.f,liquidHeight-radius/ay-24.f,cy);
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
    float activeVolume=0.f;
    for(const auto& body:surfaceBodies) activeVolume+=body.radius;
    // Give active fragments room to reshape the core instead of hiding them
    // inside an oversized permanent sphere. The mass reunites when they decay.
    const auto radius = 54.f - juce::jmin(23.f,activeVolume*.22f) + lowDrive * 14.f + midDrive * 6.f + transientDrive * 6.f;
    deposit (centreX, centreY, radius, reducedMotion ? 1.f : juce::jlimit(.76f,1.38f,1.f + lowDrive * .30f - midDrive * .26f + std::sin(motionPhase)*outputDrive*.12f + (frame.width-1.f)*.15f*outputDrive));
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
            deposit (body.x, body.y, body.radius * 1.35f, juce::jlimit (.84f, 1.19f, body.stretch), juce::jmin (1.f, body.radius / 8.f));
    juce::Image::BitmapData pixels (liquidImage, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < liquidHeight; ++y)
        for (int x = 0; x < liquidWidth; ++x)
        {
            const auto i = static_cast<std::size_t> (y * liquidWidth + x);
            const auto field = liquidField[i];
            if (field < .78f) { pixels.setPixelColour (x, y, juce::Colours::transparentBlack);continue; }
            const auto height2 = blend * std::log (field);
            const auto gx = liquidHeat[i] / field, gy = liquidGradientY[i] / field * pixelAspect;
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
            const auto v = juce::jlimit (0.f, 1.f, .30f + light * .36f + key * .38f + fill * .30f + rim * rim * .22f);
            const auto film=std::exp(-square((rim-.65f-highDrive*.10f)/.14f));
            const auto sheen=juce::jlimit(0.f,.38f,film*(.16f+highDrive*.22f+midDrive*.10f));
            pixels.setPixelColour (x, y, juce::Colour::fromFloatRGBA(v,v,v,alpha).interpolatedWith(accent.withAlpha(alpha),sheen));
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
    const auto [paper, ink, muted, line, blue, cyan, coral, yellow, mint] = appearance::read (*this);
    const auto area = getLocalBounds().toFloat().reduced (1.0f);
    const auto s = area.getWidth() / 718.0f;
    const auto mode = juce::jlimit (0, 10, static_cast<int> (frame.mode));
    const auto title = juce::String (motefield::modeNames[static_cast<std::size_t> (mode)])
                       + " / " + juce::String::charToString (static_cast<juce::juce_wchar> ('A' + frame.variation));

    if (frame.held || frame.bypass)
    {
        const auto badge = juce::Rectangle<float> (146.0f * s, 14.0f * s, 66.0f * s, 21.0f * s);
        g.setColour (frame.bypass ? line : yellow);
        g.fillRoundedRectangle (badge, 9.0f * s);
        text (g, frame.bypass ? "BYPASS" : "HELD", badge, 10.0f * s, ink, true, juce::Justification::centred, .08f);
    }
    const auto view = area.reduced (16.0f * s, 14.0f * s);
    const auto stage = view.withWidth (view.getWidth() * .69f);
    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (view.toNearestInt());
    const auto colour = juce::Colour(0xffeff1e9);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    // Inset the entire material, including split satellites, to leave travel room.
    g.drawImage (liquidImage,stage.reduced(stage.getWidth()*.025f,0));

    const auto contour = view.withTrimmedLeft (view.getWidth() * .75f).withTrimmedBottom(78.f*s);
    g.setColour (line.withAlpha (.7f));

    if (frame.mode == motefield::Mode::grid)
    {
        text (g, "DELAY TAPS", contour.withHeight (12.0f * s), 8.0f * s, juce::Colour (0xffb4bec3), true);
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
        text (g, "SHAPE ENVELOPE", contour.withHeight (16.0f * s), 10.0f * s, juce::Colour (0xffeeeeea), false);
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
        g.setColour (colour.withAlpha (.0f));
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

void LooperTape::paint(juce::Graphics& g)
{
    const auto& frame=field.currentFrame();const auto p=appearance::read(*this);
    const auto s=getWidth()/1210.f;auto area=getLocalBounds().toFloat().reduced(2,5*s);
    const auto plot=area.withTrimmedTop(27*s).withTrimmedBottom(5*s);
    const auto seconds=frame.loopSeconds>0?frame.loopSeconds:60.f;
    for(int tick=0;tick<=4;++tick)
    {const auto x=plot.getX()+plot.getWidth()*tick/4.f;
     text(g,juce::String(seconds*tick/4.f,1)+" s",{x-(tick==4?66*s:0),area.getY(),66*s,22*s},14*s,juce::Colour(0xffeeeeea));
     g.setColour(juce::Colours::white.withAlpha(.18f));g.drawLine(x,plot.getY(),x,plot.getBottom(),.7f);}
    g.setColour(juce::Colours::white.withAlpha(.2f));g.drawHorizontalLine(juce::roundToInt(plot.getCentreY()),plot.getX(),plot.getRight());
    if(frame.loopState==motefield::LooperState::empty)return;
    juce::Path wave;
    const auto point=[&](int bin,bool upper){const auto amp=juce::jmin(1.f,std::pow(frame.loopWaveform[bin],.40f))*plot.getHeight()*.48f;
        return juce::Point<float>(plot.getX()+static_cast<float>(bin)/(frame.loopWaveform.size()-1)*plot.getWidth(),plot.getCentreY()+(upper?-amp:amp));};
    wave.startNewSubPath(point(0,true));for(int i=1;i<static_cast<int>(frame.loopWaveform.size());++i)wave.lineTo(point(i,true));for(int i=static_cast<int>(frame.loopWaveform.size())-1;i>=0;--i)wave.lineTo(point(i,false));wave.closeSubPath();
    g.setColour(juce::Colour(0xfff0f1e9));g.fillPath(wave);
    const auto playX=plot.getX()+frame.loopProgress*plot.getWidth();
    for(int glow=7;glow>0;--glow){g.setColour(p.cyan.withAlpha(.025f));g.drawLine(playX,plot.getY()-7*s,playX,plot.getBottom(),glow*2*s);}
    g.setColour(p.cyan);g.drawLine(playX,plot.getY()-7*s,playX,plot.getBottom(),2*s);
    auto badge=juce::Rectangle<float>(64*s,24*s).withCentre({juce::jlimit(area.getX()+32*s,area.getRight()-32*s,playX),area.getY()+11*s});
    g.setColour(juce::Colour(0xff101510));g.fillRoundedRectangle(badge,5*s);g.setColour(p.cyan);g.drawRoundedRectangle(badge,5*s,1);
    text(g,juce::String(frame.loopProgress*seconds,1)+" s",badge,14*s,p.cyan,false,juce::Justification::centred);
}

MoteFieldAudioProcessorEditor::MoteFieldAudioProcessorEditor (MoteFieldAudioProcessor& owner)
    : AudioProcessorEditor (&owner), processor (owner), looperTape (fieldDisplay), tooltips (this, 600)
{
    using namespace motefield::parameter;
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    rangoLogo = juce::ImageCache::getFromMemory (BinaryData::RangoLogo_png, BinaryData::RangoLogo_pngSize);
    modeSelector.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    modeSelector.setRotaryParameters (pi * 4.f / 3.f, pi * 3.f, true);
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
    looperTape.setComponentID ("loop-waveform");
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
    addKnob ("WIDTH", "width", "Stereo Width: 0% mono, 100% original width, 200% wider. Also spreads the reactor.");
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
    setupButton (wetSoloButton, "WET SOLO", "Hear the effect without the live dry blend. Keeps your Mix setting and recorded loop.", true);
    setupButton (levelMatchButton, "LEVEL MATCH", "Measure three seconds of input and output, then hold level compensation. Relearns after sound changes; waits for signal during silence.", true);
    attachButton ("wetSolo", wetSoloButton); attachButton ("levelMatch", levelMatchButton);
    addAndMakeVisible (matchStatus);
    matchStatus.setJustificationType (juce::Justification::centred);
    matchStatus.setColour (juce::Label::textColourId, juce::Colour (0xffc5cebd));
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
        button.getProperties().set ("darkControl",true);
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
    performanceButton.onClick = [this] { if (! performancePanel) { performancePanel = std::make_unique<PerformancePanel> (processor); addAndMakeVisible (*performancePanel); } performancePanel->setBounds (getLocalBounds().reduced (14)); performancePanel->sendLookAndFeelChange(); performancePanel->setVisible (true); performancePanel->toFront (true); };
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
    setupButton(randomPresetButton,"RANDOM","Create a new sound. Keeps your loop, transport, tempo and output level. Use SAVE to keep it.",false,0);
    randomPresetButton.setComponentID("random-preset");
    randomPresetButton.onClick=[this]{processor.randomizeSound(juce::Random::getSystemRandom().nextInt64());refreshDisplay();};
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
    setupButton (settingsButton, "SETTINGS", "Accent color and dark mode.");
    settingsButton.setComponentID ("settings");
    settingsButton.onClick = [this] {
        settingsPanel = std::make_unique<appearance::SettingsPanel> (*appearancePreferences, [this] { applyAppearance(); });
        addAndMakeVisible (*settingsPanel); settingsPanel->setBounds (getLocalBounds()); settingsPanel->sendLookAndFeelChange(); settingsPanel->toFront (true); settingsPanel->grabKeyboardFocus();
    };
    applyAppearance();
    updateTimeAttachment();
    setResizable (true, true);
    int initialWidth = 1000;
    if (const auto* screen = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        initialWidth = juce::jmin (initialWidth, juce::roundToInt (screen->userBounds.getWidth() * .80f),
                                 juce::roundToInt (screen->userBounds.getHeight() * .80f * 1620.f / 972.f));
    initialWidth = juce::jmax (640, initialWidth);
    const auto minimumWidth = juce::jmin (900, initialWidth);
    setResizeLimits (minimumWidth, juce::roundToInt (minimumWidth * 972.0 / 1620.0), 1860, 1200);
    getConstrainer()->setFixedAspectRatio (1620.0 / 972.0);
    setSize (initialWidth, juce::roundToInt (initialWidth * 972.0 / 1620.0));
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
    presetNameDialog = std::make_unique<juce::AlertWindow> ("Save user preset", "Name this sound. Your recorded phrase is saved too. Hold and Bypass are not stored.", juce::MessageBoxIconType::NoIcon);
    presetNameDialog->setLookAndFeel (&lookAndFeel);
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
    const auto height = detailsOpen ? 1220.0 : 972.0;
    // Opening the drawer must not grow beyond the window's existing height.
    // Also allow smaller sizes when the host window sits near a screen edge.
    auto width = open ? juce::jmin (getWidth(), static_cast<int> (previousHeight * 1620.0 / height)) : collapsedWidth;
    auto maximumWidth = 1860;
    const auto screen = getScreenBounds();
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (screen))
    {
        const auto available = display->userBounds.reduced (12.0f);
        const auto scaleX = static_cast<float> (screen.getWidth()) / static_cast<float> (juce::jmax (1, getWidth()));
        const auto scaleY = static_cast<float> (screen.getHeight()) / static_cast<float> (juce::jmax (1, getHeight()));
        const auto roomWidth = (available.getRight() - juce::jmax (available.getX(), static_cast<float> (screen.getX()))) / juce::jmax (.1f, scaleX);
        const auto roomHeight = (available.getBottom() - juce::jmax (available.getY(), static_cast<float> (screen.getY()))) / juce::jmax (.1f, scaleY);
        maximumWidth = juce::jmax (1, juce::jmin (maximumWidth, static_cast<int> (roomWidth), static_cast<int> (roomHeight * 1620.0 / height)));
    }
    width = juce::jlimit (1, maximumWidth, width);
    const auto minimumWidth = juce::jmin (1000, width);
    getConstrainer()->setFixedAspectRatio (1620.0 / height);
    setResizeLimits (minimumWidth, juce::roundToInt (minimumWidth * height / 1620.0), maximumWidth, juce::roundToInt (maximumWidth * height / 1620.0));
    setSize (width, juce::roundToInt (width * height / 1620.0));
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
    if (appearanceRevision != appearancePreferences->revision) applyAppearance();
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
    frame.width = value ("width");
    matchStatus.setText (value ("levelMatch") < .5f ? "OUTPUT MONITOR" : frame.matchLearning ? "LEVEL MATCH / LEARNING" : "LEVEL MATCH / " + juce::String (juce::Decibels::gainToDecibels(frame.matchGain), 1) + " dB", juce::dontSendNotification);
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

void MoteFieldAudioProcessorEditor::applyAppearance()
{
    appearanceRevision = appearancePreferences->revision;
    lookAndFeel.setAppearance (appearancePreferences->dark, appearancePreferences->accent);
    sendLookAndFeelChange();
    hardwarePanel = {};
    tempoLabel.setColour (juce::Label::textColourId, juce::Colour(0xffd5d8d4));
    timeValueLabel.setColour (juce::Label::textColourId, juce::Colour(0xffeff1ed));
    fieldDisplay.invalidateMaterial();
    resized(); repaint();
}

void MoteFieldAudioProcessorEditor::resized()
{
    const auto s=static_cast<float>(getWidth())/1620.f;
    const auto place=[s](juce::Component& c,float x,float y,float w,float h){c.setBounds(juce::Rectangle<float>(x*s,y*s,w*s,h*s).toNearestInt());};
    const auto height=detailsOpen?1220:972;
    if(!hardwarePanel.isValid()||hardwarePanel.getHeight()!=height)hardwarePanel=makeHardwarePanel(height,lookAndFeel.colours);
    for(auto& b:modeButtons)b.getProperties().set("glassDark",true);
    for(int i=0;i<4;++i)place(*knobs[i],443+i*201,79,116,133);
    for(int i=4;i<7;++i)place(*knobs[i],281+(i-4)*154,574,116,127);
    place(*knobs[12],743,574,116,127); place(*knobs[7],897,574,116,127);
    if(performancePanel)performancePanel->setBounds(getLocalBounds().reduced(14));
    if(settingsPanel)settingsPanel->setBounds(getLocalBounds());
    place(modeSelector,159,276,186,186);
    for(int i=0;i<11;++i)
    {const auto angle=(-120.f+i*30.f)*pi/180.f;place(modeButtons[i],252+std::sin(angle)*116-27,369-std::cos(angle)*116-13,54,26);}
    for(int i=0;i<4;++i)place(variationButtons[i],126+i*65,502,60,44);
    place(fieldDisplay,431,221,1090,321);
    place(matchStatus,1236,425,240,20); matchStatus.setFont(font(11*s));
    place(wetSoloButton,1236,450,114,40); place(levelMatchButton,1362,450,114,40);
    for(auto* b:{&wetSoloButton,&levelMatchButton}) { b->getProperties().set("darkControl",true); }
    place(reverseButton,1294,510,145,35);reverseButton.getProperties().set("darkControl",true);
    place(tapButton,1084,588,134,100);place(holdButton,1226,588,134,100);place(bypassButton,1366,588,134,100);
    place(looperTape,271,728,1210,121);
    place(recordButton,484,871,88,60);place(playButton,578,871,88,60);place(dubButton,674,871,88,60);
    place(stopButton,770,871,88,60);place(undoButton,866,871,88,60);place(eraseButton,962,871,88,60);
    place(preButton,1071,871,87,60);place(postButton,1167,871,87,60);place(loopReverseButton,1264,871,87,60);place(detailsButton,1455,871,90,60);
    for(auto* b:{&preButton,&postButton,&loopReverseButton,&detailsButton}){b->getProperties().set("style",0);b->getProperties().set("darkControl",true);b->getProperties().set("utility",true);}
    loopReverseButton.setButtonText("REVERSE");
    detailsButton.setButtonText(detailsOpen?"LESS":"DETAILS");
    place(settingsButton,1360,871,87,60);settingsButton.getProperties().set("darkControl",true);settingsButton.getProperties().set("utility",true);
    place(syncButton,78,878,92,45);place(timeValueLabel,175,878,65,45);timeValueLabel.setFont(font(15*s,true));
    place(tempoLabel,245,878,119,45);tempoLabel.setFont(font(13*s));place(performanceButton,363,878,89,45);
    place(randomPresetButton,806,34,100,48);
    place(presetBox,978,34,288,48);place(previousPreset,918,34,52,48);place(nextPreset,1277,34,46,48);place(savePresetButton,1333,34,74,48);
    for(int i=8;i<12;++i){knobs[i]->setVisible(detailsOpen);place(*knobs[i],60+(i-8)*210,1000,164,178);}
    for(auto* c:std::array<juce::Component*,4>{&roomBox,&speedBox,&divisionBox,&motionButton})c->setVisible(detailsOpen);
    place(roomBox,962,1020,234,42);place(speedBox,1240,1020,290,42);place(divisionBox,962,1112,234,42);place(motionButton,1240,1112,290,42);
}

void MoteFieldAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto p=appearance::read(*this);const auto s=static_cast<float>(getWidth())/1620.f;g.addTransform(juce::AffineTransform::scale(s));g.drawImageAt(hardwarePanel,0,0);
    {juce::Graphics::ScopedSaveState saved(g);const auto drive=motionButton.getToggleState()?0.f:logoDrive;
     g.addTransform(juce::AffineTransform::scale(1.f+drive*.045f,1.f-drive*.025f,132.f,56.f));g.setColour(p.ink);g.drawImage(rangoLogo,juce::Rectangle<float>(80,7,113,99),juce::RectanglePlacement::centred,true);}
    text(g,"MoteField",{192,27,350,40},33,p.ink,false);text(g,"by Rango Labs",{192,66,280,23},17,p.muted);
    text(g,"Out",{1411,43,44,30},13,p.ink);
    const auto& f=fieldDisplay.currentFrame();const auto level=juce::jlimit(0.f,1.f,(juce::Decibels::gainToDecibels(f.outputLevel,-60.f)+48.f)/48.f);
    for(int i=0;i<18;++i){g.setColour(i<level*18?p.ink:p.line.withAlpha(.4f));g.fillRect(1455.f+i*5.2f,73.f-(i+5)*1.2f,3.6f,(i+5)*1.2f);}
    text(g,"EFFECT MODE",{133,219,239,27},15,juce::Colours::white,false,juce::Justification::centred);
    text(g,"RECORDED LOOP",{86,729,177,27},16,juce::Colours::white);
    text(g,"Loop 01",{94,762,160,26},19,juce::Colours::white);
    text(g,secondsText(f.loopSeconds),{94,792,165,24},15,juce::Colours::white);
    const auto state=processor.getLooperState();const auto status=state==motefield::LooperState::empty?"READY":state==motefield::LooperState::recording?"RECORDING":state==motefield::LooperState::overdubbing?"OVERDUB":state==motefield::LooperState::stopped?"STOPPED":"PLAYING";
    text(g,status,{95,825,164,24},15,p.cyan);
    if(detailsOpen){text(g,"REVERB CHARACTER",{962,992,234,24},13,p.muted);text(g,"LOOP SPEED",{1240,992,260,24},13,p.muted);text(g,"SUBDIVISION",{962,1084,234,24},13,p.muted);text(g,"DISPLAY",{1240,1084,234,24},13,p.muted);}
}
