#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// Classic Steel从0906G的P2独立保留；仅微增低中频传播吸收，保持双半环颗粒与轮廓。
namespace sanmu_steel
{
constexpr double pi=3.14159265358979323846;
inline constexpr const char* revision="Steel-S1";
struct Settings
{
    float decay=2.39f,predelay=15,highHz=6000,highDb=-18,bassHz=300,bassMult=1;
    float size=100,attack=30,early=100,late=100,rate=.8f,depth=20,lowCut=20,highCut=9000;
};
class Delay
{
public:
    void prepare(int maximum)
    {
        limit=std::max(1,maximum);std::size_t capacity=1;
        while(capacity<static_cast<std::size_t>(limit+4))capacity<<=1;
        data.assign(capacity,0);mask=capacity-1;cursor=0;
    }
    void reset(){std::fill(data.begin(),data.end(),0);cursor=0;}
    void write(float value) noexcept {data[cursor]=value;cursor=(cursor+1)&mask;}
    float read(float samples) const noexcept
    {
        samples=std::clamp(samples,0.f,static_cast<float>(limit));
        const auto integer=static_cast<std::size_t>(samples);
        const float fraction=samples-static_cast<float>(integer);
        const auto index=(cursor-1-integer)&mask;
        return data[index]+fraction*(data[(index-1)&mask]-data[index]);
    }
private:
    std::vector<float> data;
    std::size_t mask=0,cursor=0;
    int limit=1;
};
class Pole
{
public:
    void set(double rate,float hz) noexcept
    {
        const double g=std::tan(pi*std::clamp(static_cast<double>(hz),1.,rate*.45)/rate);
        coefficient=static_cast<float>(g/(1+g));
    }
    float low(float x) noexcept
    {
        const float v=(x-state)*coefficient,y=state+v;state=y+v;return y;
    }
    float high(float x) noexcept {return x-low(x);}
    void reset() noexcept {state=0;}
private:
    float coefficient=0,state=0;
};
// 按传播时长增加低频吸收，260Hz以上平缓回到单位增益。
// 只改变固定的频率损耗，不跟随音频电平；避免将压缩或限幅误作共振治理。
class LowBodyLoss
{
public:
    void set(double rate,float seconds,float decay) noexcept
    {
        gain=std::exp(-6.90775527898f*seconds/decay*.32f);
        lowBand.set(rate,260);
    }
    float process(float x) noexcept {return x+(gain-1)*lowBand.low(x);}
    void reset() noexcept {lowBand.reset();}
private:
    Pole lowBand;
    float gain=1;
};
// 每个延迟存储段各自承担传播损耗，全通内部的多次循环也按时长衰减。
class Loss
{
public:
    void set(double rate,float seconds,const Settings& p) noexcept
    {
        mid=std::exp(-6.90775527898f*seconds/p.decay);
        bassGain=std::exp(-6.90775527898f*seconds/(p.decay*p.bassMult));
        const float top=std::exp(-6.90775527898f*seconds/(p.decay*std::pow(10.f,p.highDb/40.f)));
        topRatio=top/mid;bass.set(rate,p.bassHz);treble.set(rate,p.highHz);
        body.set(rate,seconds,p.decay);
    }
    float process(float x) noexcept
    {
        x=body.process(x);
        const float shelf=mid*x+(bassGain-mid)*bass.low(x);
        return topRatio*shelf+(1-topRatio)*treble.low(shelf);
    }
    void reset() noexcept {bass.reset();treble.reset();body.reset();}
private:
    LowBodyLoss body;
    Pole bass,treble;
    float mid=0,bassGain=0,topRatio=0;
};
class Segment
{
public:
    void prepare(double rate,double maximumSeconds){memory.prepare(static_cast<int>(rate*maximumSeconds+16));}
    void configure(double rate,float seconds,const Settings& p)
    {
        samples=std::max(1.f,std::round(static_cast<float>(rate)*seconds));loss.set(rate,samples/static_cast<float>(rate),p);
    }
    void reset(){memory.reset();loss.reset();}
    float read(float modulation=0) noexcept {return loss.process(memory.read(samples+modulation-1));}
    void write(float x) noexcept {memory.write(x);}
    float tap(float fraction) const noexcept {return memory.read(samples*fraction);}
private:
    Delay memory;
    Loss loss;
    float samples=1;
};
// F为有损延迟，H=(-g+F)/(1-gF)。|g|<1且F无增益时保持被动性。
class Allpass
{
public:
    void prepare(double rate,double maximumSeconds){segment.prepare(rate,maximumSeconds);}
    void configure(double rate,float seconds,float amount,const Settings& p)
    {segment.configure(rate,seconds,p);gain=amount;}
    void reset(){segment.reset();}
    float process(float x,float modulation=0) noexcept
    {
        const float delayed=segment.read(modulation);
        const float state=x+gain*delayed;
        const float output=delayed-gain*state;
        segment.write(state);return output;
    }
    float tap(float fraction) const noexcept {return segment.tap(fraction);}
private:
    Segment segment;
    float gain=0;
};
class Field
{
public:
    void prepare(double rate)
    {
        sampleRate=rate;
        for(auto& d:predelay)d.prepare(static_cast<int>(rate*.26));
        for(auto& branch:inputDiffusion)for(auto& ap:branch)ap.prepare(rate,.07);
        for(auto& branch:tankDiffusion)for(auto& ap:branch)ap.prepare(rate,.04);
        for(auto& branch:propagation)for(auto& d:branch)d.prepare(rate,.065);
        reset();
    }
    void reset()
    {
        for(auto& d:predelay)d.reset();
        for(auto& branch:inputDiffusion)for(auto& ap:branch)ap.reset();
        for(auto& branch:tankDiffusion)for(auto& ap:branch)ap.reset();
        for(auto& branch:propagation)for(auto& d:branch)d.reset();
        for(auto* filters:{&inputLow,&inputHigh,&outputLow,&outputHigh})for(auto& f:*filters)f.reset();
        phase={0,1.31};
    }
    void configure(const Settings& p) noexcept
    {
        const float space=.55f+.0055f*p.size;
        const float early=p.early*.01f,attack=p.attack*.01f;
        const float late=std::pow(p.late*.01f,1.4f);
        preSamples=static_cast<float>(sampleRate)*p.predelay*.001f;
        constexpr std::array<float,6> inputMs{1.17f,1.87f,3.13f,4.37f,6.79f,9.71f};
        constexpr float apMs[2][2]={{9.7f,17.9f},{14.3f,23.9f}};
        constexpr float delayMs[2][2]={{41.3f,33.1f},{47.9f,39.7f}};
        for(std::size_t c=0;c<2;++c)
        {
            for(std::size_t i=0;i<6;++i)
                inputDiffusion[c][i].configure(sampleRate,inputMs[i]*.001f*(.30f+.70f*early)*(1+3.2f*attack*attack),
                    .15f+.60f*early,p);
            for(std::size_t i=0;i<2;++i)
            {
                tankDiffusion[c][i].configure(sampleRate,apMs[c][i]*.001f*space,
                    i==0?.10f+.66f*late:.08f+.60f*late,p);
                propagation[c][i].configure(sampleRate,delayMs[c][i]*.001f*space,p);
            }
            inputLow[c].set(sampleRate,10500);inputHigh[c].set(sampleRate,55);
            outputLow[c].set(sampleRate,p.highCut);outputHigh[c].set(sampleRate,p.lowCut);
        }
        modSamples=static_cast<float>(sampleRate)*p.depth*.0000025f;
        modRate=p.rate;
    }
    std::array<float,2> process(float left,float right) noexcept
    {
        std::array<float,2> input{left,right};
        for(std::size_t c=0;c<2;++c)
        {
            predelay[c].write(input[c]);
            input[c]=inputLow[c].low(inputHigh[c].high(predelay[c].read(preSamples)));
            for(auto& ap:inputDiffusion[c])input[c]=ap.process(input[c]);
        }
        // 两个半环都先取出旧传播状态，再写入当前值，避免左右计算顺序造成额外一采样偏差。
        std::array<float,2> tail{},middle{};
        for(std::size_t c=0;c<2;++c)
        {tail[c]=propagation[c][1].read();middle[c]=propagation[c][0].read();}
        for(std::size_t c=0;c<2;++c)
        {
            phase[c]+=2*pi*modRate*(c==0?.83:1.07)/sampleRate;
            if(phase[c]>=2*pi)phase[c]-=2*pi;
            const float modulation=static_cast<float>(std::sin(phase[c]))*modSamples;
            propagation[c][0].write(tankDiffusion[c][0].process(input[c]+tail[1-c],modulation));
            propagation[c][1].write(tankDiffusion[c][1].process(middle[c],-.71f*modulation));
        }
        const std::array<float,7> pickups{
            propagation[0][0].tap(.071f),propagation[1][0].tap(.066f),
            propagation[0][0].tap(.317f),propagation[1][0].tap(.419f),
            propagation[0][1].tap(.237f),tankDiffusion[1][1].tap(.613f),propagation[1][1].tap(.733f)
        };
        constexpr float weights[2][7]={{1,1,1,-1,1,-1,1},{1,1,-1,1,1,1,-1}};
        std::array<float,2> output{};
        constexpr float gain=.80f*.3779644730092272f;
        for(std::size_t c=0;c<2;++c)
        {
            for(std::size_t i=0;i<7;++i)output[c]+=gain*weights[c][i]*pickups[i];
            output[c]=outputLow[c].low(outputHigh[c].high(output[c]));
        }
        return output;
    }
private:
    double sampleRate=48000;
    std::array<Delay,2> predelay;
    std::array<std::array<Allpass,6>,2> inputDiffusion;
    std::array<std::array<Allpass,2>,2> tankDiffusion;
    std::array<std::array<Segment,2>,2> propagation;
    std::array<Pole,2> inputLow,inputHigh,outputLow,outputHigh;
    std::array<double,2> phase{};
    float preSamples=0,modSamples=0,modRate=0;
};
} // namespace sanmu_steel
