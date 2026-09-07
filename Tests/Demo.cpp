#include "PluginEditor.h"
#include <cstdio>
#include <iostream>
#include <stdexcept>

namespace
{
constexpr int sampleRate = 48000, fps = 30, samplesPerFrame = sampleRate / fps;
constexpr double bpm = 96., noteDuration = 60. / bpm / 4.;
void require (bool condition, const char* message) { if (! condition) throw std::runtime_error (message); }
struct Timeline final : juce::AudioPlayHead
{
    int sample = 0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info; info.setBpm (bpm); info.setIsPlaying (true);
        info.setTimeInSamples (sample); info.setTimeInSeconds (sample / double (sampleRate));
        info.setPpqPosition (sample / double (sampleRate) * bpm / 60.);
        info.setTimeSignature (juce::AudioPlayHead::TimeSignature {4, 4}); return info;
    }
};
// Original four-bar figure: Dmaj9, Bm9, Gmaj9, A6sus. No sampled recordings.
constexpr int chords[4][5] {{62,66,69,73,76},{59,62,66,69,73},{55,59,62,66,69},{57,62,64,66,71}};
constexpr int pattern[16] {0,2,1,3,2,4,3,2,4,2,3,1,2,1,3,2};
int noteAt (int step) { return chords[(step / 16) % 4][pattern[step % 16]] + (step % 16 == 8 ? 12 : 0); }
float tone (double time, int channel, bool counter = false)
{
    const double spacing = counter ? noteDuration * 4 : noteDuration;
    const int current = static_cast<int> (time / spacing);
    double result = 0;
    for (int back = 0; back < (counter ? 3 : 7); ++back)
    {
        const int step = current - back; if (step < 0) continue;
        const double age = time - step * spacing;
        const int midi = counter ? chords[(step / 4 + 2) % 4][(step * 3) % 5] + 12 : noteAt (step);
        const double frequency = 440. * std::pow (2., (midi - 69.) / 12.);
        const double phase = juce::MathConstants<double>::twoPi * frequency * age;
        const double attack = 1. - std::exp (-age * 650.);
        const double envelope = attack * std::exp (-age * (counter ? 3.3 : 7.2));
        const double pan = .17 * std::sin (step * 1.7);
        const double velocity = counter ? .105 : (step % 4 == 0 ? .17 : .13);
        result += velocity * envelope * (std::sin (phase) + .20 * std::exp (-age * 6.) * std::sin (2. * phase)
                     + .11 * std::exp (-age * 12.) * std::sin (3. * phase) + .06 * std::exp (-age * 15.) * std::sin (5. * phase))
                  * (channel == 0 ? 1. - pan : 1. + pan);
    }
    return static_cast<float> (result);
}
juce::Component* find (juce::Component& p, const juce::String& id)
{
    if (p.getComponentID() == id) return &p;
    for (auto* c : p.getChildren()) if (auto* result = find (*c, id)) return result;
    return nullptr;
}
void click (MoteFieldAudioProcessorEditor& editor, const juce::String& id)
{
    auto* button = dynamic_cast<juce::Button*> (find (editor, id)); require (button != nullptr, "missing control");
    if (button->getClickingTogglesState()) button->setToggleState (! button->getToggleState(), juce::sendNotificationSync);
    else if (button->onClick) button->onClick();
}
void png (const juce::Image& image, const juce::File& file)
{
    auto stream = file.createOutputStream(); require (stream != nullptr, "cannot write screenshot");
    stream->setPosition (0); stream->truncate();
    require (juce::PNGImageFormat().writeImageToStream (image, *stream), "PNG failed");
}
void wav (const juce::AudioBuffer<float>& audio, const juce::File& file)
{
    auto stream = file.createOutputStream(); require (stream != nullptr, "cannot write WAV"); stream->setPosition (0); stream->truncate();
    std::unique_ptr<juce::OutputStream> ownedStream (stream.release());
    auto writer = juce::WavAudioFormat().createWriterFor (ownedStream, juce::AudioFormatWriterOptions().withSampleRate (sampleRate).withNumChannels (2).withBitsPerSample (24));
    require (writer != nullptr && writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()), "WAV failed");
}
struct Chapter { int second; const char* title; const char* detail; const char* focus; };
const std::vector<Chapter> presetChapters {
    {0,"THE SOURCE", "An original glass-harp arpeggio. Hear the dry input first.", "bypass"},
    {5,"FIRST LIGHT", "Bloom / B - overlapping micro loops and half-speed undertones.", "presets"},
    {15,"NEAR ORBIT", "Orbit / A - longer grains turn the same notes into a floating texture.", "presets"},
    {25,"GLASS SEEDS", "Pluck / D - articulate grains with scattered octave-up voices.", "presets"},
    {35,"POCKET CUTS", "Chop / A - short fragments reassemble the arpeggio into rhythmic cuts.", "presets"},
    {45,"AFTER HOURS", "Grid / B - tempo-related stereo repeats. Watch the delay guide.", "presets"},
    {55,"HOLD THE MOMENT", "Input stops. Hold keeps the captured delay phrase moving.", "freeze"}
};
const std::vector<Chapter> loopChapters {
    {0,"A PHRASE TO KEEP", "First Light / Bloom B. A four-bar arpeggio at 96 BPM.", "presets"},
    {5,"RECORD / FOUR BARS", "Capture the processed phrase into the POST looper.", "record"},
    {15,"PLAY / INPUT OFF", "The source is silent. Everything you hear is the captured loop and tails.", "play"},
    {25,"DUB / A SECOND VOICE", "Add a high counterline over the existing phrase.", "dub"},
    {35,"PLAY / TWO LAYERS", "Input is silent again. Both recorded layers continue.", "play"},
    {40,"REVERSE THE LOOP", "The phrase changes direction; the effect reverse stays independent.", "looperReverse"},
    {45,"HALF SPEED", "Forward again at 0.5x. Varispeed lowers pitch by an octave.", "details"},
    {50,"UNDO / ORIGINAL PHRASE", "Return to normal speed and remove the newest overdub.", "undo"},
    {55,"STOP / FADE OUT", "The recorded phrase stops with a one-second fade.", "stop"}
};
}
int main (int argc, char** argv)
{
    try
    {
        require (argc >= 3, "usage: MoteFieldDemo absolute-output-folder presets|looper [--audio-only|--short]");
        juce::ScopedJuceInitialiser_GUI gui;
        const juce::File output (argv[1]); require (output.createDirectory().wasOk(), "cannot create output folder");
        const bool looping = juce::String (argv[2]) == "looper";
        const bool audioOnly = argc > 3 && juce::String (argv[3]) == "--audio-only";
        const int seconds = argc > 3 && juce::String (argv[3]) == "--short" ? 2 : 60;
        const juce::String name = looping ? "motefield-looper" : "motefield-presets";
        const auto& chapters = looping ? loopChapters : presetChapters;
        Timeline timeline;
        MoteFieldAudioProcessor processor; processor.setPlayHead (&timeline); processor.prepareToPlay (sampleRate, 400);
        processor.setParameterValue ("sync", 1.f);
        processor.setParameterValue ("looperLevel", .85f);
        processor.setParameterValue ("looperOrder", 0.f);
        processor.setParameterValue ("loopFade", 1.f);
        processor.setParameterValue ("loopFadeMode", 2.f);
        processor.applyFactoryPreset (0);
        if (! looping) processor.setParameterValue ("bypass", 1.f);
        std::unique_ptr<MoteFieldAudioProcessorEditor> editor (static_cast<MoteFieldAudioProcessorEditor*> (processor.createEditor()));
        editor->setSize (1364,880); editor->setVisible (true);
        juce::AudioBuffer<float> audio (2, seconds * sampleRate), dry (2, seconds * sampleRate), block (2,400);
        juce::MidiBuffer midi;
        auto eventLog = output.getChildFile (name + "-events.txt").createOutputStream();
        eventLog->setPosition (0); eventLog->truncate();
        std::vector<unsigned char> pixelsOut (1920 * 1080 * 3);
        int chapter = 0;
        float peak = 0;
        for (int frame = 0; frame < seconds * fps; ++frame)
        {
            const int second = frame / fps;
            if (frame % fps == 0)
            {
                while (chapter + 1 < static_cast<int> (chapters.size()) && second >= chapters[chapter + 1].second) ++chapter;
                if (second == chapters[chapter].second) { *eventLog << second << "s " << chapters[chapter].title << "\n"; std::cerr << name << " " << second << "s: " << chapters[chapter].title << "\n"; }
                if (! looping)
                {
                    if (second == 5) { processor.setParameterValue ("bypass",0.f); processor.applyFactoryPreset (0); }
                    if (second == 15) processor.applyFactoryPreset (17);
                    if (second == 25) processor.applyFactoryPreset (20);
                    if (second == 35) processor.applyFactoryPreset (21);
                    if (second == 45) processor.applyFactoryPreset (5);
                    if (second == 55) click (*editor,"freeze");
                }
                else
                {
                    if (second == 5) click (*editor,"record");
                    if (second == 15 || second == 35) click (*editor,"play");
                    if (second == 25) click (*editor,"dub");
                    if (second == 40) click (*editor,"looperReverse");
                    if (second == 45) { click (*editor,"looperReverse"); processor.setParameterValue ("looperSpeed",0.f); click (*editor,"details"); }
                    if (second == 50) { processor.setParameterValue ("looperSpeed",1.f); click (*editor,"details"); editor->setSize (1364,880); click (*editor,"undo"); }
                    if (second == 55) click (*editor,"stop");
                }
            }
            for (int chunk = 0; chunk < 4; ++chunk)
            {
                const int offset = frame * samplesPerFrame + chunk * 400;
                timeline.sample = offset;
                for (int i = 0; i < 400; ++i)
                {
                    const double time = (offset + i) / double (sampleRate);
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const float input = looping ? (time < 15. ? tone (time,ch) : time >= 25. && time < 35. ? tone (time - 25.,ch,true) : 0.f)
                                                    : time < 55. ? tone (time,ch) : 0.f;
                        block.setSample (ch,i,input); dry.setSample (ch,offset+i,input);
                    }
                }
                processor.processBlock (block,midi);
                for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 400; ++i)
                {
                    const float fade = looping ? 1.f : juce::jlimit (0.f,1.f,(seconds * sampleRate - offset - i) / float (sampleRate));
                    const float sample = block.getSample (ch,i) * fade;
                    require (std::isfinite (sample), "non-finite audio"); peak = std::max (peak,std::abs (sample));
                    audio.setSample (ch,offset+i,sample);
                }
            }
            editor->refreshDisplay();
            if (looping && frame % fps == 0)
            {
                using S = motefield::LooperState;
                const auto state = processor.getLooperState();
                if (second == 5) require (state == S::recording,"record did not start");
                if (second == 15 || second == 35) require (state == S::playing,"loop did not play");
                if (second == 25) require (state == S::overdubbing,"overdub did not start");
            }
            if (audioOnly) continue;
            const auto ui = editor->createComponentSnapshot (editor->getLocalBounds());
            juce::Image canvas (juce::Image::RGB,1920,1080,true);
            juce::Graphics g (canvas); g.fillAll (juce::Colour (0xff20251d));
            g.setColour (juce::Colour (0xffe9e3cf)); g.setFont (juce::FontOptions (22.f));
            g.drawText ("RANGO LABS   /   MOTEFIELD " + juce::String (JucePlugin_VersionString),278,24,760,30,juce::Justification::centredLeft);
            g.setColour (juce::Colour (0xffbdc993)); g.setFont (juce::FontOptions (17.f));
            g.drawText (looping ? "02   /   PHRASE LOOPER" : "01   /   PRESET EXPLORATION",1050,24,592,30,juce::Justification::centredRight);
            g.setColour (juce::Colour (0xff899471)); g.fillRect (278,69,1364,1);
            const int uiX = (1920-editor->getWidth())/2, uiY = 94;
            g.drawImageAt (ui,uiX,uiY);
            const double elapsed = frame / double (fps) - chapters[chapter].second;
            if (elapsed < 1.6)
                if (auto* control = find (*editor,chapters[chapter].focus))
                {
                    auto bounds = editor->getLocalArea (control,control->getLocalBounds()).toFloat().expanded (5).translated (float (uiX),float (uiY));
                    g.setColour (juce::Colour (0xffd0dfa0).withAlpha (static_cast<float> (.85 * (1. - elapsed / 1.6))));
                    g.drawRoundedRectangle (bounds,7.f,3.f);
                }
            g.setColour (juce::Colour (0xffd0dfa0)); g.setFont (juce::FontOptions (20.f,juce::Font::bold));
            g.drawText (chapters[chapter].title,278,989,560,30,juce::Justification::centredLeft);
            g.setColour (juce::Colour (0xffdedbc9)); g.setFont (juce::FontOptions (18.f));
            g.drawText (chapters[chapter].detail,278,1023,1300,28,juce::Justification::centredLeft);
            g.setColour (juce::Colour (0xffbdc993)); g.setFont (juce::FontOptions (16.f));
            g.drawText (juce::String (second).paddedLeft ('0',2) + " / 60",1510,991,132,28,juce::Justification::centredRight);
            g.setColour (juce::Colour (0xff515a43)); g.fillRect (278,1068,1364,3);
            g.setColour (juce::Colour (0xffbdc993)); g.fillRect (278,1068,juce::roundToInt (1364.f*(frame+1)/(seconds*fps)),3);
            if (frame % fps == 0 && second == chapters[chapter].second + 2)
            {
                png (ui, output.getChildFile (name + "-" + juce::String(second) + "-ui.png"));
                png (canvas, output.getChildFile (name + "-" + juce::String(second) + "-1080.png"));
            }
            if (frame == 30) png (canvas,output.getChildFile (name + "-poster.png"));
            const juce::Image::BitmapData data (canvas,juce::Image::BitmapData::readOnly);
            for (int y = 0; y < 1080; ++y) for (int x = 0; x < 1920; ++x)
            {
                const auto* p = data.getPixelPointer (x,y); const auto at = static_cast<std::size_t>((y*1920+x)*3);
                pixelsOut[at]=p[juce::PixelRGB::indexB]; pixelsOut[at+1]=p[juce::PixelRGB::indexG]; pixelsOut[at+2]=p[juce::PixelRGB::indexR];
            }
            require (std::fwrite (pixelsOut.data(),1,pixelsOut.size(),stdout) == pixelsOut.size(),"video stream interrupted");
        }
        require (peak < .99f,"audio peak exceeded delivery headroom");
        wav (audio,output.getChildFile (name + ".wav")); wav (dry,output.getChildFile (name + "-source.wav"));
        if (looping) require (processor.exportAudio (output.getChildFile ("recorded-phrase.wav")).wasOk(),"phrase export failed");
        *eventLog << "Audio peak: " << peak << "\nNative processor and editor capture; no simulated fluid frames.\n";
        std::cerr << name << " complete; peak " << peak << "\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
