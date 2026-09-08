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
 require(argc>=6,"usage: MoteFieldPromo output clip source-48k.wav bpm preset-indices [--audio-only|--audition|--wet-preview]");
 juce::ScopedJuceInitialiser_GUI gui;const juce::File output(argv[1]);require(output.createDirectory().wasOk(),"output folder");
 const auto kind=juce::String(argv[2]);const bool dark=!kind.startsWith("arp"),looping=kind=="looper",audioOnly=argc>6;
 demoBpm=juce::String(argv[4]).getDoubleValue();require(demoBpm>30. && demoBpm<300.,"invalid source tempo");
 juce::AudioFormatManager formats;formats.registerBasicFormats();
 std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(argv[3])));
 require(reader && reader->sampleRate==sampleRate && reader->lengthInSamples>0,"expected 48 kHz source WAV");
 juce::AudioBuffer<float> sourceAudio(2,int(reader->lengthInSamples));
 require(reader->read(&sourceAudio,0,sourceAudio.getNumSamples(),0,true,true),"source read failed");
 const int sourceSamples=sourceAudio.getNumSamples();
 std::vector<int> presets;
 for(const auto& token:juce::StringArray::fromTokens(argv[5],",",""))presets.push_back(token.getIntValue());
 require(!presets.empty(),"choose presets");
 const auto names=MoteFieldAudioProcessor::factoryPresetNames();
 const bool wetPreview=argc>6 && juce::String(argv[6])=="--wet-preview";
 if(wetPreview || (argc>6 && juce::String(argv[6])=="--audition")) {
  auto metrics=output.getChildFile(kind+"-audition.csv").createOutputStream();metrics->setPosition(0);metrics->truncate();
  *metrics<<"index,name,mode,mix,peak,rms,change_rms,stereo_rms,max_step\n";
  for(int id=0;id<names.size();++id) {
   if(wetPreview && std::find(presets.begin(),presets.end(),id)==presets.end())continue;
   Timeline clock;MoteFieldAudioProcessor test;test.setPlayHead(&clock);test.applyFactoryPreset(id);if(wetPreview)test.setParameterValue("mix",1.f);test.prepareToPlay(sampleRate,400);
   const int length=sourceSamples+sampleRate*2;juce::AudioBuffer<float> result(2,length),part(2,400);juce::MidiBuffer midi;
   double power=0,delta=0,stereo=0;float peak=0,step=0,prev[2]{};
   for(int offset=0;offset<length;) {
    const int count=std::min(400,length-offset);part.setSize(2,count,false,false,true);clock.sample=offset;
    for(int ch=0;ch<2;++ch)for(int i=0;i<count;++i)part.setSample(ch,i,offset+i<sourceSamples?sourceAudio.getSample(ch,offset+i)*.4f:0.f);
    test.processBlock(part,midi);
    for(int i=0;i<count;++i) {
     stereo+=std::pow(part.getSample(0,i)-part.getSample(1,i),2.);
     for(int ch=0;ch<2;++ch) {
      const float value=part.getSample(ch,i),input=offset+i<sourceSamples?sourceAudio.getSample(ch,offset+i)*.4f:0.f;
      require(std::isfinite(value),"non-finite audition");peak=std::max(peak,std::abs(value));power+=value*value;delta+=(value-input)*(value-input);
      step=std::max(step,std::abs(value-prev[ch]));prev[ch]=value;result.setSample(ch,offset+i,value);
     }
    }
    offset+=count;
   }
   wav(result,output.getChildFile(kind+"-audition-"+juce::String(id)+".wav"));
   *metrics<<id<<","<<names[id]<<","<<test.parameters.getRawParameterValue("mode")->load()<<","<<test.parameters.getRawParameterValue("mix")->load()<<","<<peak<<","<<std::sqrt(power/(length*2.))<<","<<std::sqrt(delta/(length*2.))<<","<<std::sqrt(stereo/length)<<","<<step<<"\n";
  }
  return 0;
 }
 const int sections=looping?4:int(presets.size())+1;
 std::vector<int> starts{0};
 for(int section=0;section<sections;++section)starts.push_back(starts.back()+sourceSamples+(looping?0:section==0?sampleRate/2:sampleRate*2));
 const int totalSamples=starts.back()+(looping?sampleRate*2:0),totalFrames=(totalSamples+samplesPerFrame-1)/samplesPerFrame;
 auto sectionStart=[&](int n){return starts[std::min(n,sections)];};
 const juce::String name="motefield-"+kind;
 const auto appearanceFile=output.getChildFile(name+"-appearance.settings");
 #if JUCE_WINDOWS
 _putenv_s("MOTEFIELD_APPEARANCE_FILE",appearanceFile.getFullPathName().toRawUTF8());
 #else
 setenv("MOTEFIELD_APPEARANCE_FILE",appearanceFile.getFullPathName().toRawUTF8(),1);
 #endif
 const std::array<juce::String,11> modeDescriptions{"swelling micro loops","sequenced slices","gliding fragments","granular cloud","orbiting grains","plucked grains","rhythmic cuts","fractured phrases","stepped pitch","stereo delay","diffused delay"};
 Timeline timeline;MoteFieldAudioProcessor processor;processor.setPlayHead(&timeline);processor.prepareToPlay(sampleRate,400);
 processor.setParameterValue("sync",1.f);processor.setParameterValue("bypassTrails",0.f);processor.setParameterValue("looperLevel",.80f);processor.setParameterValue("looperOrder",0.f);
 std::unique_ptr<MoteFieldAudioProcessorEditor> editor(static_cast<MoteFieldAudioProcessorEditor*>(processor.createEditor()));
 editor->setSize(1500,900);editor->setVisible(true);
 juce::AudioBuffer<float> audio(2,totalSamples),dry(2,totalSamples),block(2,400);juce::MidiBuffer midi;
 std::vector<unsigned char> bytes(1920*1080*3);float peak=0.f,presetMix=.5f;double sourceError=0.;
 auto log=output.getChildFile(name+"-events.txt").createOutputStream();log->setPosition(0);log->truncate();
 bool loopStopped=false;int activeSection=-1;std::vector<int> sourceCounts(sections,0);juce::String title,detail;
 auto enterSection=[&](int section) {
  const bool bypass=section==0;
  if(!looping || section==0) {
   processor.applyFactoryPreset(presets[section==0?0:section-1]);presetMix=processor.parameters.getRawParameterValue("mix")->load();
   processor.prepareToPlay(sampleRate,400);
  }
  processor.setParameterValue("bypass",bypass&&!looping?1.f:0.f);processor.setParameterValue("mix",bypass?0.f:presetMix);
  if(looping) {
   if(section==0) {
    processor.setParameterValue("looperOrder",1.f);processor.setParameterValue("looperLevel",1.f);
    processor.setParameterValue("loopQuantize",0.f);click(*editor,"record");
   }
   if(section==1)click(*editor,"play");
   if(section==2) {processor.applyFactoryPreset(presets[1]);presetMix=processor.parameters.getRawParameterValue("mix")->load();}
   const std::array<juce::String,4> titles{"RECORD / DRY","RESHAPE / INPUT OFF","NEW TEXTURE / INPUT OFF","PERFORM / INPUT OFF"};
   const std::array<juce::String,4> details{"Record the full piano phrase / PRE-FX","Soft Focus / the recording feeds the granulator","Moon Pool / same recording, new texture","Sculpt the recorded phrase / filter + space"};title=titles[section];detail=details[section];
  } else {
   title=bypass?"BEFORE / DRY":"AFTER / FX ON";
   detail=bypass?"The complete original loop":names[presets[section-1]]+" / "+modeDescriptions[juce::roundToInt(processor.parameters.getRawParameterValue("mode")->load())];
  }
  *log<<"SECTION "<<section<<" START_SAMPLE "<<starts[section]<<" SOURCE_SAMPLES "<<(looping&&section>=1?0:sourceSamples)<<" "<<title<<" | "<<detail<<"\n";
  std::cerr<<name<<" "<<starts[section]/double(sampleRate)<<"s "<<detail<<"\n";
 };
 for(int frame=0;frame<totalFrames;++frame) {
  const int frameEnd=std::min(totalSamples,(frame+1)*samplesPerFrame);
  for(int offset=frame*samplesPerFrame;offset<frameEnd;) {
   int section=0;while(section+1<sections && offset>=starts[section+1])++section;
   if(section!=activeSection){enterSection(section);activeSection=section;}
   const int local=offset-starts[section];const bool bypass=section==0;
   const int boundary=offset<starts.back()?std::min(starts[section+1],local<sourceSamples?starts[section]+sourceSamples:starts[section+1]):totalSamples;
   const int count=std::min({400,frameEnd-offset,boundary-offset});require(count>0,"empty render block");
   if(looping && offset==starts.back() && !loopStopped){click(*editor,"stop");loopStopped=true;title="RELEASE / TAIL";detail="The full phrase is complete / let the texture fade";}
   if(looping && section==3 && local<sourceSamples) {
    const float t=local/float(sourceSamples),swell=std::sin(juce::MathConstants<float>::pi*t);
    processor.setParameterValue("cutoff",6800.f-4500.f*swell);
    processor.setParameterValue("space",.43f+.22f*swell);
    processor.setParameterValue("shape",.59f+.24f*swell);
   }
   timeline.sample=looping?offset:local;block.setSize(2,count,false,false,true);
   for(int i=0;i<count;++i) {
    const bool hasInput=local+i<sourceSamples && !(looping&&section>=1);
    if(hasInput)++sourceCounts[section];
    for(int ch=0;ch<2;++ch) {
     const float input=hasInput?sourceAudio.getSample(ch,local+i)*.4f:0.f;
     block.setSample(ch,i,input);dry.setSample(ch,offset+i,input);
    }
   }
   processor.processBlock(block,midi);
   for(int i=0;i<count;++i)for(int ch=0;ch<2;++ch) {
    const float raw=block.getSample(ch,i),input=dry.getSample(ch,offset+i);
    if(bypass && local>sampleRate)sourceError=std::max(sourceError,double(std::abs(raw-input)));
    const float sample=bypass?input:raw;
    // Only fade the final tail; every source sample is played before the fade begins.
    const float delivery=juce::jlimit(0.f,1.f,(totalSamples-offset-i)/(sampleRate*.35f));
    require(std::isfinite(sample),"non-finite output");audio.setSample(ch,offset+i,sample*delivery);peak=std::max(peak,std::abs(sample));
   }
   offset+=count;
  }
  const int section=activeSection,localFrame=std::max(0,(frameEnd-1-starts[section])/samplesPerFrame);const bool bypass=section==0;
  editor->refreshDisplay();
  require(std::abs(processor.parameters.getRawParameterValue("mix")->load()-(bypass?0.f:presetMix))<.0001f,"Mix indicator mismatch");
  if(looping && localFrame==fps) {using S=motefield::LooperState;require(processor.getLooperState()==(section==0?S::recording:S::playing),"looper chapter state mismatch");}
  if(audioOnly)continue;
  const auto ui=editor->createComponentSnapshot(editor->getLocalBounds());juce::Image canvas(juce::Image::RGB,1920,1080,true);juce::Graphics g(canvas);
  const auto acid=juce::Colour(0xffd4f85b),white=juce::Colour(0xffeef2ed);
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff26332c),960,330,juce::Colour(0xff080e10),960,1050,true));g.fillAll();
  g.setColour(white);g.setFont(juce::FontOptions(25.f,juce::Font::bold));g.drawText("MOTEFIELD",210,14,420,40,juce::Justification::centredLeft);
  g.setFont(juce::FontOptions(14.f));g.setColour(acid);g.drawText("RANGO LABS  /  SOUND IN MOTION",440,23,600,24,juce::Justification::centredLeft);
  g.setColour(white.withAlpha(.65f));g.drawText(dark?"HOPE  /  PIANO LOOP  /  91 BPM":"CLOSURE  /  SYNTH ARP  /  133 BPM",1000,21,710,28,juce::Justification::centredRight);
  g.setOpacity(1.f);g.drawImageAt(ui,210,65);
  if(localFrame<fps && true)
   if(auto* control=find(*editor,"mix")) {
    auto bounds=editor->getLocalArea(control,control->getLocalBounds()).toFloat().expanded(5).translated(210,65);
    g.setColour((bypass?white:acid).withAlpha(.7f*(1.f-localFrame/float(fps))));g.drawRoundedRectangle(bounds,16,2);
   }
  const auto badge=juce::Rectangle<float>(210,981,300,52);
  g.setColour(bypass?white:acid);g.fillRoundedRectangle(badge,9);g.setColour(juce::Colour(0xff101715));g.setFont(juce::FontOptions(22.f,juce::Font::bold));
  g.drawText(looping?(section==0?"RECORD / DRY":loopStopped?"RELEASE / TAIL":section==1?"RESHAPE / INPUT OFF":section==2?"NEW TEXTURE":"PERFORM / INPUT OFF"):(bypass?"BEFORE / DRY":"AFTER / FX ON"),badge.toNearestInt(),juce::Justification::centred);
  g.setColour(white);g.setFont(juce::FontOptions(24.f,juce::Font::bold));g.drawText(detail,535,980,930,33,juce::Justification::centredLeft);
  g.setColour(white.withAlpha(.65f));g.setFont(juce::FontOptions(14.f));
  const auto mixText="MIX "+juce::String(juce::roundToInt(processor.parameters.getRawParameterValue("mix")->load()*100.f))+"%";
  g.drawText(mixText+(looping?"  /  PRE-FX  /  "+(section==0?juce::String("RECORDING THE FULL SOURCE"):juce::String("RECORDED AUDIO ONLY")):"  /  FULL LOOP  /  "+(section==0?juce::String("DRY REFERENCE"):juce::String(section)+" OF "+juce::String(int(presets.size()))+" PRESETS")),535,1016,1080,22,juce::Justification::centredLeft);
  if(!looping)for(int dot=0;dot<sections;++dot){g.setColour(dot==section?acid:juce::Colour(0xff48534b));g.fillRoundedRectangle(1460.f+dot*58.f,998,46,5,2);}
  g.setColour(juce::Colour(0xff303939));g.fillRect(210,1055,1500,3);g.setColour(acid);g.fillRect(210,1055,1500*(frame+1)/totalFrames,3);
  if(localFrame==fps){png(ui,output.getChildFile(name+"-section-"+juce::String(section)+"-ui.png"));png(canvas,output.getChildFile(name+"-section-"+juce::String(section)+".png"));}
  const juce::Image::BitmapData data(canvas,juce::Image::BitmapData::readOnly);
  for(int y=0;y<1080;++y)for(int x=0;x<1920;++x){const auto* p=data.getPixelPointer(x,y);const auto at=(y*1920+x)*3;bytes[at]=p[juce::PixelRGB::indexB];bytes[at+1]=p[juce::PixelRGB::indexG];bytes[at+2]=p[juce::PixelRGB::indexR];}
  require(std::fwrite(bytes.data(),1,bytes.size(),stdout)==bytes.size(),"video pipe closed");
 }
 for(int section=0;section<sections;++section) {
  require(sourceCounts[section]==(looping&&section>=1?0:sourceSamples),"source phrase was cut short or repeated within a chapter");
  *log<<"VERIFIED_SOURCE_COUNT "<<section<<" "<<sourceCounts[section]<<"\n";
 }
 require(peak<.98f,"render clipped");require(sourceError<.0001,"bypass did not match dry source");
 // A single constant trim for the entire comparison; no limiter or loudness pumping.
 const float gain=std::min(2.f,.80f/std::max(.01f,peak));audio.applyGain(gain);dry.applyGain(gain);
 wav(audio,output.getChildFile(name+".wav"));wav(dry,output.getChildFile(name+"-source.wav"));
 if(looping)require(processor.exportAudio(output.getChildFile("recorded-piano-phrase.wav")).wasOk(),"loop export failed");
 *log<<"Source: "<<juce::File(argv[3]).getFileName()<<"\nTempo: "<<demoBpm<<"\nSource input gain: 0.4\n";
 *log<<"Peak before trim: "<<peak<<"\nConstant delivery gain: "<<gain<<"\nDry bypass maximum error: "<<sourceError<<"\n";
 std::cerr<<"Done "<<name<<" peak="<<peak<<" gain="<<gain<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
