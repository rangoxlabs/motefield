#pragma once
#include <JuceHeader.h>
#include <cmath>

namespace appearance
{
constexpr int baseId = 0x2900000;
constexpr juce::uint32 acid = 0xffd4f85b;
struct Palette { juce::Colour paper, ink, muted, line, blue, cyan, coral, yellow, mint; };
inline Palette palette (bool dark, juce::Colour accent)
{
    return { juce::Colour (dark ? 0xff22262a : 0xffedf0ef), juce::Colour (dark ? 0xfff0f3f2 : 0xff202529),
             juce::Colour (dark ? 0xffadb7ba : 0xff566166), juce::Colour (dark ? 0xff505a60 : 0xffadb7bc),
             juce::Colour (0xff252b30), accent, juce::Colour (0xffe77967), accent, accent };
}
inline Palette read (const juce::Component& c)
{
    if (!c.getLookAndFeel().isColourSpecified (baseId)) return palette (false, juce::Colour (acid));
    return { c.findColour (baseId), c.findColour (baseId+1), c.findColour (baseId+2), c.findColour (baseId+3),
             c.findColour (baseId+4), c.findColour (baseId+5), c.findColour (baseId+6), c.findColour (baseId+7), c.findColour (baseId+8) };
}
inline juce::Colour onAccent (juce::Colour c)
{
    // Choose whichever neutral gives the stronger WCAG luminance contrast.
    const auto linear = [] (float v) { return v <= .04045f ? v / 12.92f : std::pow ((v + .055f) / 1.055f, 2.4f); };
    const auto l = .2126f * linear (c.getFloatRed()) + .7152f * linear (c.getFloatGreen()) + .0722f * linear (c.getFloatBlue());
    return l > .179f ? juce::Colour (0xff101416) : juce::Colour (0xffffffff);
}

// UI-only preferences: shared between instances in this process, separate from
// host state and sound presets. PropertiesFile handles the platform data path.
struct Preferences
{
    Preferences()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "MoteField Appearance"; options.filenameSuffix = "settings";
        options.folderName = "Rango Labs/MoteField"; options.osxLibrarySubFolder = "Application Support";
        options.millisecondsBeforeSaving = 750;
        const auto overridePath = juce::SystemStats::getEnvironmentVariable ("MOTEFIELD_APPEARANCE_FILE", {});
        file = overridePath.isEmpty() ? std::make_unique<juce::PropertiesFile> (options)
                                    : std::make_unique<juce::PropertiesFile> (juce::File (overridePath), options);
        dark = file->getBoolValue ("darkMode", false);
        const auto hex = file->getValue ("accent", "d4f85b");
        accent = hex.length() == 6 && hex.containsOnly ("0123456789abcdefABCDEF")
            ? juce::Colour::fromString ("ff" + hex) : juce::Colour (acid);
    }
    ~Preferences() { file->saveIfNeeded(); }
    void set (juce::Colour colour, bool night)
    {
        colour = colour.withAlpha (1.f);
        if (accent == colour && dark == night) return;
        accent = colour; dark = night; ++revision;
        file->setValue ("accent", accent.toDisplayString (false)); file->setValue ("darkMode", dark);
    }
    juce::Colour accent { acid };
    bool dark = false;
    int revision = 0;
    std::unique_ptr<juce::PropertiesFile> file;
};

class SettingsPanel final : public juce::Component, private juce::ChangeListener
{
public:
    SettingsPanel (Preferences& settings, std::function<void()> update) : preferences (settings), changed (std::move (update)),
        picker (juce::ColourSelector::showSliders | juce::ColourSelector::showColourspace)
    {
        setComponentID ("settings-panel");
        addAndMakeVisible (picker); picker.setComponentID ("accent-picker"); picker.setCurrentColour (preferences.accent, juce::dontSendNotification); picker.addChangeListener (this);
        addAndMakeVisible (hex); hex.setComponentID ("accent-hex"); hex.setInputRestrictions (7, "#0123456789abcdefABCDEF");
        hex.setText ("#" + preferences.accent.toDisplayString (false));
        hex.onReturnKey = [this] { commitHex(); }; hex.onFocusLost = [this] { commitHex(); };
        setup (dark, "DARK MODE", "dark-mode"); dark.setClickingTogglesState (true); dark.setToggleState (preferences.dark, juce::dontSendNotification);
        dark.onClick = [this] { preferences.set (preferences.accent, dark.getToggleState()); changed(); };
        setup (reset, "RESET TO ACID", "appearance-reset"); reset.onClick = [this] { picker.setCurrentColour (juce::Colour (acid), juce::dontSendNotification); changeListenerCallback (nullptr); };
        setup (close, "DONE", "settings-close"); close.onClick = [this] { if (!commitHex()) return; if (preferences.file->saveIfNeeded()) setVisible (false); else { error = "Could not save appearance. Check folder permissions."; repaint(); } };
        setWantsKeyboardFocus (true);
    }
    ~SettingsPanel() override { picker.removeChangeListener (this); }
    bool keyPressed (const juce::KeyPress& key) override
    { if (key == juce::KeyPress::escapeKey) { close.onClick(); return true; } return false; }
    void lookAndFeelChanged() override
    {
        const auto p = read (*this);
        picker.setColour (juce::ColourSelector::backgroundColourId, p.paper);
        picker.setColour (juce::ColourSelector::labelTextColourId, p.ink);
        hex.setColour (juce::TextEditor::backgroundColourId, p.paper);
        hex.setColour (juce::TextEditor::textColourId, p.ink);
        hex.applyColourToAllText (p.ink, true);
        hex.setColour (juce::TextEditor::outlineColourId, p.line);
    }
    void paint (juce::Graphics& g) override
    {
        const auto p = read (*this);
        g.setColour (juce::Colours::black.withAlpha (.68f)); g.fillAll();
        auto r = card().toFloat(); g.setColour (p.paper); g.fillRoundedRectangle (r, 16);
        g.setColour (p.line); g.drawRoundedRectangle (r.reduced (1), 16, 1);
        g.setColour (p.ink); g.setFont (22.f); g.drawText ("Appearance", r.reduced (22).withHeight (30), juce::Justification::centredLeft);
        g.setFont (13.f); g.setColour (p.muted);
        g.drawText ("Accent / LEDs / fluid reflections", r.withTrimmedTop (62).reduced (22,0).withHeight (22), juce::Justification::centredLeft);
        g.drawText (error.isEmpty() ? "Saved on this computer. Sound presets keep your appearance." : error,
                    r.withY (r.getBottom()-82).reduced (22,0).withHeight (24), juce::Justification::centredLeft);
    }
    void resized() override
    {
        auto r = card().reduced (22); r.removeFromTop (68);
        picker.setBounds (r.removeFromTop (juce::jmax (100, r.getHeight()-128)));
        auto row = r.removeFromTop (36); hex.setBounds (row.removeFromLeft (135)); row.removeFromLeft (12); dark.setBounds (row.removeFromLeft (130));
        auto bottom = card().reduced (22).removeFromBottom (34); close.setBounds (bottom.removeFromRight (90)); reset.setBounds (bottom.removeFromLeft (160));
    }
private:
    juce::Rectangle<int> card() const { return getLocalBounds().withSizeKeepingCentre (juce::jmin (500,getWidth()-24),juce::jmin (490,getHeight()-24)); }
    void setup (juce::TextButton& b, const char* name, const char* id) { addAndMakeVisible (b); b.setButtonText (name); b.setComponentID (id); }
    bool commitHex()
    {
        const auto value = hex.getText().trim().trimCharactersAtStart ("#");
        if (value.length() != 6 || ! value.containsOnly ("0123456789abcdefABCDEF")) { error = "Enter six hex digits, for example #D4F85B."; repaint(); return false; }
        error.clear(); picker.setCurrentColour (juce::Colour::fromString ("ff"+value), juce::dontSendNotification); changeListenerCallback (nullptr); repaint(); return true;
    }
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        preferences.set (picker.getCurrentColour(), dark.getToggleState());
        hex.setText ("#"+preferences.accent.toDisplayString (false), false); error.clear(); changed(); repaint();
    }
    Preferences& preferences; std::function<void()> changed;
    juce::ColourSelector picker; juce::TextEditor hex;
    juce::TextButton dark, reset, close; juce::String error;
};
}
