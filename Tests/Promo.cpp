#include "PluginEditor.h"
#include <cstdio>
#include <iostream>
#include <stdexcept>
namespace {
constexpr int sampleRate=48000,fps=30,samplesPerFrame=1600;
double demoBpm=120.;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Timeline final:juce::AudioPlayHead {
 int sample=0;
 juce::Optional<PositionInfo> getPosition() const override {
  PositionInfo p;p.setBpm(demoBpm);p.setIsPlaying(true);p.setTimeInSamples(sample);
  p.setTimeInSeconds(sample/double(sampleRate));p.setPpqPosition(sample/double(sampleRate)*demoBpm/60.);
  p.setTimeSignature(juce::AudioPlayHead::TimeSignature{4,4});return p;
 }
};
constexpr int fantasyChords[4][5]{{62,66,69,73,76},{59,62,66,69,73},{55,59,62,66,69},{57,61,64,69,71}};
constexpr int fantasyMelody[32]{71,-1,67,66,64,-1,66,69,67,-1,64,62,64,-1,67,71,74,-1,71,69,67,-1,66,67,69,-1,66,64,62,-1,66,69};
constexpr int darkChords[4][5]{{51,54,58,61,65},{47,51,54,58,61},{44,47,51,54,58},{46,50,53,56,59}};
float tone(double t,int channel,bool dark,bool counter=false) {
 const double beat=60./demoBpm,bar=4.*beat,cycle=4.*bar;
 t=std::fmod(t+cycle,cycle);double result=0.;
 const auto tau=juce::MathConstants<double>::twoPi;
 if(!dark || counter) {
  const double spacing=counter?beat:beat*.5;
  const int current=int(t/spacing),count=counter?16:32;
  for(int back=0;back<8;++back) {
   const int raw=current-back,step=(raw%count+count)%count;
   const double age=t-raw*spacing;
   const int note=counter?darkChords[step/4][(step*2+1)%5]+12:
       fantasyMelody[step];
   if(note<0)continue;
   const double f=440.*std::pow(2.,(note-69.)/12.),ph=tau*f*age;
   const double env=(1.-std::exp(-age*500.))*std::exp(-age*(counter?3.5:3.2));
   const double pan=.22*std::sin(step*1.7),velocity=counter?.075:(step%8==0?.125:.093);
   result+=velocity*env*(std::sin(ph)+.22*std::exp(-age*6.)*std::sin(2.*ph)+.09*std::exp(-age*10.)*std::sin(5.*ph))*(channel==0?1.-pan:1.+pan);
  }
 } else {
  const int current=int(t/bar);
  for(int back=0;back<3;++back) {
   const int raw=current-back,chord=(raw%4+4)%4;
   const double age=t-raw*bar;
   // Slightly rolled, extended electric-piano voicings with a soft tine transient.
   for(int strike=0;strike<2;++strike)for(int n=0;n<5;++n) {
    const double hit=strike==0?1.:.56;
    const double a=age-strike*beat*2.5-n*.009;if(a<0.)continue;
    const double f=440.*std::pow(2.,(darkChords[chord][n]-69.)/12.),ph=tau*f*a;
    const double env=(1.-std::exp(-a*180.))*std::exp(-a*.95);
    const double tine=std::sin(ph+(.85*std::exp(-a*2.5))*std::sin(2.*ph));
    const double pan=(n-2)*.10;
    result+=hit*.047*env*(tine+.13*std::sin(2.*ph))*(channel==0?1.-pan:1.+pan);
    // Quiet sustained fundamental supplies the dark chord bed, without reverb.
    result+=hit*.007*(1.-std::exp(-a*6.))*std::exp(-a*.60)*std::sin(ph*(channel==0?.9998:1.0002));
   }
   const double f=440.*std::pow(2.,(darkChords[chord][0]-12-69.)/12.);
   result+=.058*(1.-std::exp(-age*70.))*std::exp(-age*1.4)*std::sin(tau*f*age);
  }
 }
 return float(result);
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
}
int main(int argc,char** argv) {try {
 require(argc>=3,"usage: MoteFieldPromo output fantasy|rnb|looper [--audio-only]");
 juce::ScopedJuceInitialiser_GUI gui;const juce::File output(argv[1]);require(output.createDirectory().wasOk(),"output folder");
 const auto kind=juce::String(argv[2]);const bool dark=kind!="fantasy",looping=kind=="looper",audioOnly=argc>3;
 demoBpm=dark?100.:96.;
 const int phraseFrames=juce::roundToInt((looping?16.:4.)*60./demoBpm*fps);
 const int sections=looping?5:24,totalFrames=phraseFrames*sections,totalSamples=totalFrames*samplesPerFrame;
 const double seconds=totalFrames/double(fps);
 const juce::String name="motefield-"+kind;
 const auto appearanceFile=output.getChildFile(name+"-appearance.settings");
 #if JUCE_WINDOWS
 _putenv_s("MOTEFIELD_APPEARANCE_FILE",appearanceFile.getFullPathName().toRawUTF8());
 #else
 setenv("MOTEFIELD_APPEARANCE_FILE",appearanceFile.getFullPathName().toRawUTF8(),1);
 #endif
 const std::array<int,8> presets=dark?std::array<int,8>{15,18,30,11,23,9,28,14}:std::array<int,8>{0,17,20,21,13,29,25,5};
 const std::array<juce::String,8> descriptions=dark?std::array<juce::String,8>{"Velvet Veil / grain cloud","Moon Pool / floating layers","Night Tide / diffuse repeats","Copper Chain / sequenced slices","Fault Lines / fractured phrases","Petal Drift / micro loops","Cross Streets / stereo repeats","Sideways Rain / pitch bends"}:
 std::array<juce::String,8>{"First Light / micro loops","Near Orbit / long grains","Glass Seeds / pitched fragments","Pocket Cuts / rhythmic cuts","Soft Landing / pitch bends","Ink Wash / diffuse repeats","Half Steps / pitch sequence","After Hours / stereo delay"};
 Timeline timeline;MoteFieldAudioProcessor processor;processor.setPlayHead(&timeline);processor.prepareToPlay(sampleRate,400);
 processor.setParameterValue("sync",1.f);processor.setParameterValue("trails",0.f);processor.setParameterValue("looperLevel",.80f);processor.setParameterValue("looperOrder",0.f);
 std::unique_ptr<MoteFieldAudioProcessorEditor> editor(static_cast<MoteFieldAudioProcessorEditor*>(processor.createEditor()));
 editor->setSize(1500,900);editor->setVisible(true);
 juce::AudioBuffer<float> audio(2,totalSamples),dry(2,totalSamples),block(2,400);juce::MidiBuffer midi;
 std::vector<unsigned char> bytes(1920*1080*3);float peak=0.f,presetMix=.5f;double sourceError=0.;
 auto log=output.getChildFile(name+"-events.txt").createOutputStream();log->setPosition(0);log->truncate();
 for(int frame=0;frame<totalFrames;++frame) {
  const int section=frame/phraseFrames,localFrame=frame%phraseFrames;const bool bypass=looping?section==0:section%3==0;
  juce::String title,detail;
  if(!looping) {title=bypass?"BEFORE / DRY":"AFTER / MOTEFIELD ON";detail=descriptions[section/3]+"   |   Same musical excerpt";}
  else {
   const std::array<juce::String,5> titles{"BEFORE / DRY","AFTER / RECORD","LOOP PLAYING / INPUT OFF","OVERDUB / ADD A MELODY","TWO LAYERS / INPUT OFF"};
   const std::array<juce::String,5> details{"Rhythmic electric-piano chords / 100 BPM","Velvet Veil into the POST looper / four bars","The recorded phrase continues without the source","A new high counterline joins the recorded chords","Chords and melody now live inside the looper"};
   title=titles[section];detail=details[section];
  }
  if(localFrame==0) {
   if(!looping && bypass) {processor.applyFactoryPreset(presets[section/3]);presetMix=processor.parameters.getRawParameterValue("mix")->load();}
   if(looping && section==0) {processor.applyFactoryPreset(15);presetMix=processor.parameters.getRawParameterValue("mix")->load();}
   processor.setParameterValue("bypass",bypass?1.f:0.f);
   processor.setParameterValue("mix",bypass?0.f:presetMix);
   if(looping) {if(section==1)click(*editor,"record");if(section==2||section==4)click(*editor,"play");if(section==3)click(*editor,"dub");}
   *log<<frame/fps<<"s "<<title<<" | "<<detail<<"\n";std::cerr<<name<<" "<<frame/fps<<"s "<<title<<"\n";
  }
  for(int chunk=0;chunk<4;++chunk) {
   const int offset=frame*samplesPerFrame+chunk*400;timeline.sample=offset;
   for(int i=0;i<400;++i)for(int ch=0;ch<2;++ch) {
    const double absolute=(offset+i)/double(sampleRate);
    const double bar=4.*60./demoBpm;
    const double t=looping?absolute:(section/3%4)*bar+std::fmod((localFrame*samplesPerFrame+chunk*400+i)/double(sampleRate),bar);
    const float source=looping&&section>=2?(section==3?tone(t,ch,true,true):0.f):tone(t,ch,dark);
    const double within=std::fmod((localFrame*samplesPerFrame+chunk*400+i)/double(sampleRate),bar);
    const float edge=looping?1.f:float(std::sin(juce::MathConstants<double>::halfPi*juce::jlimit(0.,1.,std::min(within,bar-within)/.007)));
    const float input=source*edge;
    block.setSample(ch,i,input);dry.setSample(ch,offset+i,input);
   }
   processor.processBlock(block,midi);
   for(int i=0;i<400;++i)for(int ch=0;ch<2;++ch) {
    const float raw=block.getSample(ch,i),input=dry.getSample(ch,offset+i);
    if(bypass && localFrame>fps)sourceError=std::max(sourceError,double(std::abs(raw-input)));
    // Bypass sections use the unprocessed input, with no per-section gain matching.
    const float sample=bypass?input:raw;
    const float delivery=juce::jlimit(0.f,1.f,(totalSamples-offset-i)/(sampleRate*.35f));
    require(std::isfinite(sample),"non-finite output");audio.setSample(ch,offset+i,sample*delivery);peak=std::max(peak,std::abs(sample));
   }
  }
  editor->refreshDisplay();
  require(std::abs(processor.parameters.getRawParameterValue("mix")->load()-(bypass?0.f:presetMix))<.0001f,"Mix indicator mismatch");
  if(looping && localFrame==fps) {using S=motefield::LooperState;require(processor.getLooperState()==(section==1?S::recording:section==3?S::overdubbing:section==0?S::empty:S::playing),"looper chapter state mismatch");}
  if(audioOnly)continue;
  const auto ui=editor->createComponentSnapshot(editor->getLocalBounds());juce::Image canvas(juce::Image::RGB,1920,1080,true);juce::Graphics g(canvas);
  const auto acid=juce::Colour(0xffd4f85b),white=juce::Colour(0xffeef2ed);
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff26332c),960,330,juce::Colour(0xff080e10),960,1050,true));g.fillAll();
  g.setColour(white);g.setFont(juce::FontOptions(25.f,juce::Font::bold));g.drawText("MOTEFIELD",210,14,420,40,juce::Justification::centredLeft);
  g.setFont(juce::FontOptions(14.f));g.setColour(acid);g.drawText("RANGO LABS  /  SOUND IN MOTION",440,23,600,24,juce::Justification::centredLeft);
  g.setColour(white.withAlpha(.65f));g.drawText(dark?"AFTER THE LAST TRAIN  /  100 BPM":"LANTERNS OVER THE WATER  /  96 BPM",1000,21,710,28,juce::Justification::centredRight);
  g.setOpacity(1.f);g.drawImageAt(ui,210,65);
  if(localFrame<fps && (looping?section<2:section%3<2))
   if(auto* control=find(*editor,"mix")) {
    auto bounds=editor->getLocalArea(control,control->getLocalBounds()).toFloat().expanded(5).translated(210,65);
    g.setColour((bypass?white:acid).withAlpha(.7f*(1.f-localFrame/float(fps))));g.drawRoundedRectangle(bounds,16,2);
   }
  const auto badge=juce::Rectangle<float>(210,981,300,52);
  g.setColour(bypass?white:acid);g.fillRoundedRectangle(badge,9);g.setColour(juce::Colour(0xff101715));g.setFont(juce::FontOptions(22.f,juce::Font::bold));
  g.drawText(looping?(section==0?"BEFORE / DRY":section==1?"RECORD":section==3?"OVERDUB":"PLAY / INPUT OFF"):(bypass?"BEFORE / DRY":"AFTER / FX ON"),badge.toNearestInt(),juce::Justification::centred);
  g.setColour(white);g.setFont(juce::FontOptions(24.f,juce::Font::bold));g.drawText(looping?detail:descriptions[section/3],535,980,930,33,juce::Justification::centredLeft);
  g.setColour(white.withAlpha(.65f));g.setFont(juce::FontOptions(14.f));
  const auto mixText="MIX "+juce::String(juce::roundToInt(processor.parameters.getRawParameterValue("mix")->load()*100.f))+"%";
  g.drawText(mixText+(looping?"  /  LIVE LOOP CAPTURE":"  /  SAME EXCERPT  /  "+juce::String(section/3+1)+" OF 8 PRESETS"),535,1016,1080,22,juce::Justification::centredLeft);
  if(!looping)for(int dot=0;dot<8;++dot){g.setColour(dot==section/3?acid:juce::Colour(0xff48534b));g.fillRoundedRectangle(1460.f+dot*31.f,998,23,5,2);}
  g.setColour(juce::Colour(0xff303939));g.fillRect(210,1055,1500,3);g.setColour(acid);g.fillRect(210,1055,1500*(frame+1)/totalFrames,3);
  if(localFrame==fps){png(ui,output.getChildFile(name+"-section-"+juce::String(section)+"-ui.png"));png(canvas,output.getChildFile(name+"-section-"+juce::String(section)+".png"));}
  const juce::Image::BitmapData data(canvas,juce::Image::BitmapData::readOnly);
  for(int y=0;y<1080;++y)for(int x=0;x<1920;++x){const auto* p=data.getPixelPointer(x,y);const auto at=(y*1920+x)*3;bytes[at]=p[juce::PixelRGB::indexB];bytes[at+1]=p[juce::PixelRGB::indexG];bytes[at+2]=p[juce::PixelRGB::indexR];}
  require(std::fwrite(bytes.data(),1,bytes.size(),stdout)==bytes.size(),"video pipe closed");
 }
 require(peak<.98f,"render clipped");require(sourceError<.0001,"bypass did not match dry source");
 // A single constant trim for the entire comparison; no limiter or loudness pumping.
 const float gain=std::min(2.f,.80f/std::max(.01f,peak));audio.applyGain(gain);dry.applyGain(gain);
 wav(audio,output.getChildFile(name+".wav"));wav(dry,output.getChildFile(name+"-source.wav"));
 if(looping)require(processor.exportAudio(output.getChildFile("recorded-rnb-phrase.wav")).wasOk(),"loop export failed");
 *log<<"Peak before trim: "<<peak<<"\nConstant delivery gain: "<<gain<<"\nDry bypass maximum error: "<<sourceError<<"\n";
 std::cerr<<"Done "<<name<<" peak="<<peak<<" gain="<<gain<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
