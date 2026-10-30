#include "RoomEngine.h"
#include <cmath>
namespace dsp_engines
{
void RoomEngine::prepare(double sampleRate,int)
{
    const double rate=std::isfinite(sampleRate)&&sampleRate>=8000?sampleRate:48000;
    field.prepare(rate);for(auto& p:parameters)p.reset(rate,.05);reset();
}
void RoomEngine::reset(){field.reset();initialized=false;tick=0;activeStyle=-1;}
void RoomEngine::process(const float* inL,const float* inR,float* outL,float* outR,int numSamples,const ReverbParameters& p)
{
    const int style=p.mode==0?0:1;
    if(activeStyle!=style)
    {
        field.reset();
        initialized=false;tick=0;activeStyle=style;
    }
    const std::array<float,14> requested{p.decay,p.predelay,p.highFreq,p.highShelf,p.bassFreq,p.bassMult,
        p.size,p.attack,p.earlyDiff,p.lateDiff,p.modRate,p.modDepth,p.lowCut,p.highCut};
    constexpr std::array<float,14> lows{.2f,0,1000,-24,50,.25f,0,0,0,0,.1f,0,10,1000};
    constexpr std::array<float,14> highs{20,250,20000,0,1000,4,100,100,100,100,10,100,500,20000};
    constexpr std::array<float,14> defaults{.8f,5,6000,-12,300,1,100,25,100,100,.5f,15,20,10000};
    for(std::size_t i=0;i<parameters.size();++i)
    {
        const float v=std::isfinite(requested[i])?std::clamp(requested[i],lows[i],highs[i]):defaults[i];
        if(initialized)parameters[i].setTargetValue(v);else parameters[i].setCurrentAndTargetValue(v);
    }
    initialized=true;
    for(int n=0;n<numSamples;++n)
    {
        std::array<float,14> v{};
        for(std::size_t i=0;i<v.size();++i)v[i]=parameters[i].getNextValue();
        if(tick==0)
        {
            field.configure({v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10],v[11],v[12],v[13]},style);
        }
        tick=(tick+1)&31u;
        const float left=std::isfinite(inL[n])?inL[n]:0,right=std::isfinite(inR[n])?inR[n]:0;
        const auto y=field.process(left,right);
        outL[n]=y[0];outR[n]=y[1];
    }
}
}
