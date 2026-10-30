#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// Plate自包含内核，不引用Hall或尚待审计的旧共享原语。
namespace sanmu_plate
{
constexpr double pi=3.14159265358979323846;
inline constexpr const char* revision="Plate-P3";
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
        gain=std::exp(-6.90775527898f*seconds/decay*.22f);
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
        // Plate让高频余韵温和延续；该映射独立于Hall的更深高频吸收。
        const float top=std::exp(-6.90775527898f*seconds/(p.decay*std::pow(10.f,p.highDb/80.f)));
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
// 八路正交散射：多个循环路线共享能量，避免双半环串成单一主循环。
inline constexpr std::size_t branchCount=8;
using Frame=std::array<float,branchCount>;
inline Frame scatter(Frame x) noexcept
{
    for(std::size_t stride=1;stride<branchCount;stride*=2)
        for(std::size_t base=0;base<branchCount;base+=stride*2)
            for(std::size_t i=0;i<stride;++i)
            {
                const float a=x[base+i],b=x[base+i+stride];
                x[base+i]=a+b;x[base+i+stride]=a-b;
            }
    for(auto& value:x)value*=.3535533905932738f;
    return x;
}
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
        phase={0,1.31,2.51,4.09,.71,2.09,3.61,5.33};
    }
    void configure(const Settings& p) noexcept
    {
        const float space=.55f+.0055f*p.size;
        const float early=p.early*.01f,attack=p.attack*.01f;
        const float late=std::pow(p.late*.01f,1.4f);
        preSamples=static_cast<float>(sampleRate)*p.predelay*.001f;
        constexpr std::array<float,6> inputMs{1.17f,1.87f,3.13f,4.37f,6.79f,9.71f};
        constexpr float apMs[branchCount][2]={{9.7f,17.9f},{14.3f,23.9f},{7.7f,19.1f},{11.3f,27.7f},
            {13.1f,21.7f},{8.9f,25.3f},{16.3f,18.7f},{10.9f,29.3f}};
        constexpr float delayMs[branchCount][2]={{41.3f,33.1f},{47.9f,39.7f},{53.9f,29.3f},{37.1f,43.9f},
            {44.9f,31.7f},{51.1f,36.7f},{39.1f,46.3f},{49.3f,34.9f}};
        constexpr float signs[branchCount][2]={{-1,1},{1,1},{-1,-1},{1,-1},{1,1},{-1,1},{1,-1},{-1,-1}};
        // 初始扩散保留频谱；尾声的高频阻尼在反馈传播时生效。
        auto injection=p;injection.highDb=0;injection.bassMult=1;
        for(std::size_t c=0;c<2;++c)
        {
            for(std::size_t i=0;i<6;++i)
                inputDiffusion[c][i].configure(sampleRate,inputMs[i]*.001f*(.30f+.70f*early)*(1+3.2f*attack*attack),
                    .15f+.60f*early,injection);
            inputLow[c].set(sampleRate,14500);inputHigh[c].set(sampleRate,55);
            outputLow[c].set(sampleRate,p.highCut);outputHigh[c].set(sampleRate,p.lowCut);
        }
        for(std::size_t c=0;c<branchCount;++c)
            for(std::size_t i=0;i<2;++i)
            {
                tankDiffusion[c][i].configure(sampleRate,apMs[c][i]*.001f*space,
                    signs[c][i]*(i==0?.10f+.66f*late:.08f+.60f*late),p);
                propagation[c][i].configure(sampleRate,delayMs[c][i]*.001f*space,p);
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
        // 各路同时读出上个传播状态，再散射并写入，保证更新不依赖分支计算顺序。
        Frame tail{},middle{};
        for(std::size_t c=0;c<branchCount;++c)
        {tail[c]=propagation[c][1].read();middle[c]=propagation[c][0].read();}
        const auto feedback=scatter(tail);
        constexpr std::array<double,branchCount> rates{.83,1.07,.67,1.31,.93,1.17,.73,1.43};
        for(std::size_t c=0;c<branchCount;++c)
        {
            phase[c]+=2*pi*modRate*rates[c]/sampleRate;
            if(phase[c]>=2*pi)phase[c]-=2*pi;
            const float modulation=static_cast<float>(std::sin(phase[c]))*modSamples;
            const float excitation=.5f*input[c&1u];
            propagation[c][0].write(tankDiffusion[c][0].process(excitation+feedback[c],modulation));
            propagation[c][1].write(tankDiffusion[c][1].process(middle[c],-.71f*modulation));
        }
        const std::array<float,16> pickups{
            propagation[0][0].tap(.071f),propagation[1][0].tap(.066f),
            propagation[2][0].tap(.35f),propagation[3][0].tap(.44f),
            propagation[4][0].tap(.29f),propagation[5][0].tap(.37f),
            propagation[6][0].tap(.17f),propagation[7][0].tap(.23f),
            propagation[0][1].tap(.41f),propagation[1][1].tap(.67f),
            propagation[2][1].tap(.29f),propagation[3][1].tap(.53f),
            propagation[4][1].tap(.31f),propagation[5][1].tap(.59f),
            propagation[6][1].tap(.71f),propagation[7][1].tap(.47f)
        };
        constexpr float weights[2][16]={{1,1,1,1,-1,-1,1,1,1,1,-1,-1,1,1,-1,-1},
            {1,1,-1,-1,1,1,1,1,1,1,1,1,-1,-1,-1,-1}};
        std::array<float,2> output{};
        constexpr float gain=1.6f*.25f;
        for(std::size_t c=0;c<2;++c)
        {
            for(std::size_t i=0;i<16;++i)output[c]+=gain*weights[c][i]*pickups[i];
            output[c]=outputLow[c].low(outputHigh[c].high(output[c]));
        }
        return output;
    }
private:
    double sampleRate=48000;
    std::array<Delay,2> predelay;
    std::array<std::array<Allpass,6>,2> inputDiffusion;
    std::array<std::array<Allpass,2>,branchCount> tankDiffusion;
    std::array<std::array<Segment,2>,branchCount> propagation;
    std::array<Pole,2> inputLow,inputHigh,outputLow,outputHigh;
    std::array<double,branchCount> phase{};
    float preSamples=0,modSamples=0,modRate=0;
};
} // namespace sanmu_plate
