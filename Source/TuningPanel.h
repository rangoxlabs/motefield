#pragma once
#include "PluginProcessor.h"
#include "Appearance.h"
#include <BinaryData.h>

// A second page of the existing envelope area, with no popup or scroll view.
class TuningPanel final : public juce::Component
{
    class Readout final : public juce::Button
    {
    public:
        explicit Readout (TuningPanel& owner) : Button ("Tuning"), panel (owner) {}
        void paintButton (juce::Graphics& g, bool over, bool) override
        {
            const auto s = getWidth() / 240.f;
            g.addTransform (juce::AffineTransform::scale (s));
            g.setColour (juce::Colour (over ? 0xff30392e : 0xff232a23)); g.fillRoundedRectangle (0, 0, 240, 48, 6);
            g.setColour (juce::Colour (0xff778568)); g.drawRoundedRectangle (0.5f, .5f, 239, 47, 6, 1);
            panel.label (g, "TRANSPOSE", 8, 4, 125, 18, 10);
            panel.label (g, panel.pitchText() + " st", 128, 4, 82, 18, 13, true);
            panel.label (g, panel.interval(), 8, 25, 137, 18, 10);
            panel.label (g, "A4 " + juce::String (panel.value ("tuningReference"), 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " Hz", 142, 25, 88, 18, 10);
        }
        TuningPanel& panel;
    };
public:
    explicit TuningPanel (MoteFieldAudioProcessor& owner) : processor (owner), readout (*this)
    {
        setComponentID ("tuning-panel"); setWantsKeyboardFocus (true);
        readout.setComponentID ("tuning-open"); addAndMakeVisible (readout);
        readout.onClick = [this] { setOpen (true); };
        const auto button = [this] (juce::TextButton& b, const char* id, const juce::String& name)
        { addChildComponent (b); b.setComponentID (id); b.setButtonText (name); b.getProperties().set ("darkControl", true); };
        button (reset, "tuning-reset", juce::String::fromUTF8 ("\xe2\x86\xba")); reset.setVisible (true); reset.onClick = [this] { processor.setParameterValue ("fieldPitch", 0); refresh(); };
        button (back, "tuning-back", "<"); button (minus, "tuning-minus", "-"); button (plus, "tuning-plus", "+");
        button (a432, "tuning-432", "432"); button (a440, "tuning-440", "440");
        back.onClick = [this] { setOpen (false); };
        minus.onClick = [this] { setPitch (coarse() - 1, fine()); };
        plus.onClick = [this] { setPitch (coarse() + 1, fine()); };
        a432.onClick = [this] { processor.setParameterValue ("tuningReference", 432); refresh(); };
        a440.onClick = [this] { processor.setParameterValue ("tuningReference", 440); refresh(); };
        for (auto* entry : { &transpose, &cents, &reference })
        {
            addChildComponent (*entry); entry->setMultiLine (false); entry->setJustification (juce::Justification::centred);
            entry->setInputRestrictions (9, "0123456789-+."); entry->setSelectAllWhenFocused (true);
            entry->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff11170f));
            entry->setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff6c7861));
            entry->onEscapeKey = [this] { setOpen (false); };
        }
        transpose.setComponentID ("tuning-transpose"); cents.setComponentID ("tuning-fine"); reference.setComponentID ("tuning-reference");
        transpose.onReturnKey = [this] { commitPitch(); grabKeyboardFocus(); };
        cents.onReturnKey = [this] { commitPitch(); grabKeyboardFocus(); };
        reference.onReturnKey = [this] { commitReference(); grabKeyboardFocus(); };
        transpose.onFocusLost = cents.onFocusLost = [this] { if (! refreshing) commitPitch(); };
        reference.onFocusLost = [this] { if (! refreshing) commitReference(); };
        addChildComponent (range); range.setComponentID ("tuning-range");
        range.setSliderStyle (juce::Slider::LinearHorizontal); range.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0); range.setRange (-24, 24, 1);
        range.onDragStart = [this] { sliding = true; slideFine = fine(); processor.parameters.getParameter ("fieldPitch")->beginChangeGesture(); };
        range.onValueChange = [this]
        {
            if (refreshing) return;
            const auto pitch = juce::jlimit (-24.f, 24.f, static_cast<float> (range.getValue()) + (sliding ? slideFine : fine()) / 100.f);
            auto* p = processor.parameters.getParameter ("fieldPitch");
            if (sliding) p->setValueNotifyingHost (p->convertTo0to1 (pitch)); else processor.setParameterValue ("fieldPitch", pitch);
            refresh();
        };
        range.onDragEnd = [this] { processor.parameters.getParameter ("fieldPitch")->endChangeGesture(); sliding = false; };
        refresh();
    }
    ~TuningPanel() override
    {
        refreshing = true;
        transpose.onFocusLost = cents.onFocusLost = reference.onFocusLost = nullptr;
        if (sliding) processor.parameters.getParameter ("fieldPitch")->endChangeGesture();
    }
    std::function<void(bool)> onPageChanged;
    bool isOpen() const { return opened; }
    void setOpen (bool next)
    {
        if (opened == next) return;
        if (opened) { commitPitch(); commitReference(); }
        opened = next;
        for (auto* c : getChildren()) c->setVisible (opened);
        readout.setVisible (! opened); reset.setVisible (! opened);
        if (onPageChanged) onPageChanged (opened);
        refresh(); resized(); repaint();
        if (opened && isShowing()) grabKeyboardFocus(); // Entry stays idle until deliberately clicked.
    }
    bool keyPressed (const juce::KeyPress& key) override
    { if (key == juce::KeyPress::escapeKey && opened) { setOpen (false); return true; } return false; }
    void refresh()
    {
        refreshing = true;
        const auto update = [] (juce::TextEditor& e, const juce::String& t) { if (! e.hasKeyboardFocus (false)) e.setText (t, false); };
        update (transpose, juce::String (coarse())); update (cents, juce::String (juce::roundToInt (fine())));
        update (reference, juce::String (value ("tuningReference"), 1));
        if (! sliding) range.setValue (coarse(), juce::dontSendNotification);
        const auto accent = appearance::read (*this).cyan;
        for (auto* e : { &transpose, &cents, &reference }) { e->setColour (juce::TextEditor::textColourId, accent); e->setColour (juce::TextEditor::focusedOutlineColourId, accent); }
        range.setColour (juce::Slider::thumbColourId, accent); range.setColour (juce::Slider::trackColourId, accent);
        a432.setToggleState (std::abs (value ("tuningReference") - 432.f) < .05f, juce::dontSendNotification);
        a440.setToggleState (std::abs (value ("tuningReference") - 440.f) < .05f, juce::dontSendNotification);
        refreshing = false; repaint(); readout.repaint();
    }
    void resized() override
    {
        if (! opened) { auto area = getLocalBounds(); reset.setBounds (area.removeFromRight (juce::roundToInt (getWidth() * .15f))); readout.setBounds (area); return; }
        const auto s = getWidth() / 240.f, sy = getHeight() / 305.f;
        const auto place = [s, sy] (juce::Component& c, float x, float y, float w, float h) { c.setBounds (juce::Rectangle<float> (x*s,y*sy,w*s,h*sy).toNearestInt()); };
        place (back,0,0,27,27); place (minus,107,38,27,30); place (transpose,139,38,62,30); place (plus,206,38,27,30);
        place (range,0,73,240,21); place (cents,144,141,65,30);
        place (reference,0,209,79,31); place (a432,128,209,45,31); place (a440,194,209,45,31);
        for (auto* e : { &transpose, &cents, &reference }) e->setFont (face (19*s));
    }
    void paint (juce::Graphics& g) override
    {
        if (! opened) return;
        g.addTransform (juce::AffineTransform::scale (getWidth()/240.f, getHeight()/305.f));
        label (g,"TUNING",44,0,154,27,16);
        label (g,"Transpose",0,40,103,27,12); label (g,"-2 octaves",0,96,82,17,9);
        label (g,"Unison",103,96,66,17,9); label (g,"+2 octaves",184,96,56,17,9);
        label (g,interval(),0,115,240,20,12,true);
        rule (g,136); label (g,"Fine tuning",0,142,129,30,12); label (g,"cents",214,144,26,26,9);
        rule (g,179); label (g,"A4 reference",0,183,240,22,12); label (g,"Hz",85,211,30,27,11);
        label (g,"Source A4 = 440 Hz",0,250,240,19,10);
        const float offset = 1200.f * std::log2 (value ("tuningReference") / 440.f);
        label (g,"Reference offset: " + juce::String (offset,1) + " cents",0,270,240,19,10);
    }
private:
    static juce::Font face (float size)
    { static auto type = juce::Typeface::createSystemTypefaceFor (BinaryData::OpenSauceMedium_ttf, BinaryData::OpenSauceMedium_ttfSize); return juce::Font (juce::FontOptions (type)).withHeight (size); }
    void label (juce::Graphics& g, const juce::String& text, float x, float y, float w, float h, float size, bool accent = false)
    { g.setFont (face (size * 1.15f)); g.setColour (accent ? appearance::read (*this).cyan : juce::Colour (0xffd4dccf)); g.drawText (text, juce::Rectangle<float> (x,y,w,h), juce::Justification::centredLeft); }
    static void rule (juce::Graphics& g, float y) { g.setColour (juce::Colour (0xff414c3e)); g.drawHorizontalLine (juce::roundToInt (y), 0,240); }
    float value (const char* id) const { return processor.parameters.getRawParameterValue (id)->load(); }
    int coarse() const { const auto p = value ("fieldPitch"); return static_cast<int> (std::trunc (p + (p >= 0 ? .0001f : -.0001f))); }
    float fine() const { return std::round ((value ("fieldPitch") - coarse()) * 100.f); }
    juce::String pitchText() const { const auto p = std::round (value ("fieldPitch") * 100.f) / 100.f; return (p > 0 ? "+" : "") + (std::abs (fine()) > .1f ? juce::String (p, 2) : juce::String (juce::roundToInt (p))); }
    juce::String interval() const
    {
        static const char* names[] { "Unison", "Minor second", "Major second", "Minor third", "Major third", "Perfect fourth", "Tritone", "Perfect fifth", "Minor sixth", "Major sixth", "Minor seventh", "Major seventh", "Octave", "Minor ninth", "Major ninth", "Minor tenth", "Major tenth", "Perfect eleventh", "Augmented eleventh", "Perfect twelfth", "Minor thirteenth", "Major thirteenth", "Minor fourteenth", "Major fourteenth", "Two octaves" };
        return juce::String (names[juce::jlimit (0,24,std::abs (coarse()))]) + (coarse() < 0 ? " down" : "") + (fine() != 0 ? " / " + juce::String (juce::roundToInt (fine())) + " ct" : "");
    }
    void setPitch (int st, float ct) { processor.setParameterValue ("fieldPitch", juce::jlimit (-24.f,24.f,juce::jlimit(-24,24,st) + juce::jlimit(-99.f,99.f,ct)/100.f)); refresh(); }
    void commitPitch() { if (! refreshing && opened) setPitch (transpose.getText().getIntValue(), cents.getText().getFloatValue()); }
    void commitReference() { if (! refreshing && opened) { processor.setParameterValue ("tuningReference", juce::jlimit (1.f,20000.f,reference.getText().getFloatValue())); refresh(); } }
    MoteFieldAudioProcessor& processor;
    Readout readout;
    juce::TextButton reset, back, minus, plus, a432, a440;
    juce::TextEditor transpose, cents, reference;
    juce::Slider range;
    bool opened = false, refreshing = false, sliding = false;
    float slideFine = 0;
};
