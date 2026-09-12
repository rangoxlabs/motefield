#pragma once
#include <algorithm>
#include <cmath>

namespace motefield
{
// Non-destructive playback geometry, shared by the looper and WAV export.
struct LoopRegion
{
    int start = 0, end = 2;
    int length() const noexcept { return end - start; }
    static LoopRegion from(int total, float first, float last) noexcept
    {
        total = std::max(2,total);
        const int a=std::clamp(static_cast<int>(std::llround(double(std::clamp(first,0.f,1.f))*total)),0,total-2);
        const int b=std::clamp(static_cast<int>(std::llround(double(std::clamp(last,0.f,1.f))*total)),a+2,total);
        return {a,b};
    }
};
inline double loopWrap(double position, double length) noexcept
{
    position=std::fmod(position,length);return position<0?position+length:position;
}
inline double loopSpliceFrames(int length, double rate, float seconds, float speed=1.f) noexcept
{
    return std::max(.5,std::min((length-1)*.5,rate*std::clamp(seconds,.005f,1.f)*std::clamp(speed,.25f,4.f)));
}
template<class Read>
float readLoopSplice(Read&& read, const LoopRegion& region, double position, double fade) noexcept
{
    const auto length=region.length();
    position=loopWrap(position,length);
    const auto at=[&](double x)
    {
        x=std::clamp(x,0.,double(length-1));const int a=static_cast<int>(x),b=std::min(a+1,length-1);
        return read(region.start+a)+static_cast<float>(x-a)*(read(region.start+b)-read(region.start+a));
    };
    // Crossfade corresponding tail/head windows across the existing join. The
    // local read heads slow through the overlap so the period stays exactly the
    // selected sample count: increasing Crossfade never shortens a bar loop.
    // Complementary gains avoid the correlated-signal boost of equal-power fades.
    if(position<fade || position>=length-fade)
    {
        const auto across=position<fade?position+fade:position-(length-fade);
        const auto t=std::clamp(across/(2*fade),0.,1.);
        const auto mix=static_cast<float>(t*t*(3-2*t));
        const auto tail=at(length-fade+t*(fade-1));
        const auto head=at(t*fade);
        return tail+(head-tail)*mix;
    }
    return at(position);
}
}
