#include "DSP.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
constexpr double pi = 3.14159265358979323846;
void require (bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

void grainClicks(bool glass = false)
{
    constexpr int sr=48000, count=sr*6, block=128;
    motefield::Engine engine; engine.prepare(sr,block,2);
    motefield::EngineParameters p;
    p.mode=motefield::Mode::veil; p.variation=3; p.density=.84f; p.repeats=.63f; p.shape=.83f;
    if(glass) { p.mode=motefield::Mode::pluck; p.variation=3; p.density=.72f; p.repeats=.52f; p.shape=.74f; }
    p.cutoffHz=glass?10200.f:13500.f; p.mix=1; p.space=0; p.modulationDepth=0; p.resonance=.12f;
    std::vector<float> input(block), left(block), right(block);
    const float* ins[]{input.data(),input.data()}; float* outs[]{left.data(),right.data()};
    float previous[2]{}, maxJump=0, peak=0;
    double highFrequency=0; int measured=0;
    for(int pos=0;pos<count;pos+=block)
    {
        const int n=std::min(block,count-pos);
        for(int i=0;i<n;++i) input[i]=.12f*std::sin(2*pi*83*(pos+i)/sr);
        engine.process(ins,outs,2,n,p);
        for(int i=0;i<n;++i) for(int c=0;c<2;++c)
        {
            const float v=outs[c][i]; require(std::isfinite(v),"grain output not finite");
            if(pos+i>sr) { maxJump=std::max(maxJump,std::abs(v-previous[c])); highFrequency+=(v-previous[c])*(v-previous[c]); ++measured; }
            peak=std::max(peak,std::abs(v)); previous[c]=v;
        }
    }
    std::cout<<(glass?"Glass Seeds":"Dust Halo")<<" isolated grain test: peak="<<peak<<" max step="<<maxJump<<" derivative RMS="<<std::sqrt(highFrequency/measured)<<std::endl;
    require(maxJump<.005f,"Preset has abrupt grain/voice-boundary steps on low-level low-frequency input");
}
void loopClicks(double sr, float speed, bool reverse)
{
    constexpr int length=5003, block=128;
    motefield::Engine engine; engine.prepare(sr,block,2);
    motefield::EngineParameters p; p.mode=motefield::Mode::grid; p.mix=0; p.looperBeforeEffect=true; p.looperLevel=1;
    p.looperSpeed=speed; p.looperReverse=reverse;
    std::vector<float> in(block,0), left(block),right(block);
    const float* inputs[]{in.data(),in.data()}; float* outputs[]{left.data(),right.data()};
    for(int i=0;i<64;++i) engine.process(inputs,outputs,2,block,p);
    engine.requestLooperCommand(motefield::LooperCommand::record);
    for(int pos=0;pos<length;pos+=block)
    {
        const auto n=std::min(block,length-pos);
        for(int i=0;i<n;++i) in[i]=.15f+.3f*std::sin(2*pi*29*(pos+i)/sr);
        engine.process(inputs,outputs,2,n,p);
    }
    const auto recorded=engine.snapshotLoop();
    engine.requestLooperCommand(motefield::LooperCommand::play);
    std::fill(in.begin(),in.end(),0);
    float previous=0,maxJump=0; double energy=0;
    for(int pos=0;pos<int(length*4/speed)+4096;pos+=block)
    {
        engine.process(inputs,outputs,2,block,p);
        for(int i=0;i<block;++i)
        { if(pos+i>4096) { maxJump=std::max(maxJump,std::abs(left[i]-previous)); energy+=left[i]*left[i]; } previous=left[i]; }
    }
    require(engine.snapshotLoop().audio==recorded.audio,"seam repair destructively changed the recording or loop length");
    std::cout<<"Loop "<<sr<<" Hz "<<speed<<"x "<<(reverse?"reverse":"forward")<<": max step="<<maxJump<<std::endl;
    require(maxJump<.015f,"phrase-loop seam clicks on an intentionally non-zero-crossing recording");
    require(energy>5,"seam repair muted loop playback");
}
int main(int argc,char** argv)
{
    try {
        if(argc==1) { grainClicks(); grainClicks(true); }
        if(argc>1 && std::string(argv[1])=="--glass-only") { grainClicks(true); return 0; }
        for(double sr:{44100.,48000.}) for(float speed:{.5f,1.f,2.f,4.f}) for(bool reverse:{false,true}) loopClicks(sr,speed,reverse);
        std::cout<<"Click regressions passed.\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
