#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// 独立的几何早期场与统计晚期场；不依赖其他混响类别。
namespace sanmu_room
{
constexpr double pi=3.14159265358979323846;
inline constexpr const char* revision="Room-R1";
struct Settings
{
    float decay=.8f,predelay=5,highHz=6000,highDb=-12,bassHz=300,bassMult=1;
    float size=100,attack=25,early=100,late=100,rate=.5f,depth=15,lowCut=20,highCut=10000;
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
    void write(float x) noexcept {data[cursor]=x;cursor=(cursor+1)&mask;}
    float read(float samples) const noexcept
    {
        samples=std::clamp(samples,0.f,static_cast<float>(limit));
        const auto whole=static_cast<std::size_t>(samples);const float f=samples-static_cast<float>(whole);
        const auto i=(cursor-1-whole)&mask;return data[i]+f*(data[(i-1)&mask]-data[i]);
    }
private:
    std::vector<float> data;std::size_t mask=0,cursor=0;int limit=1;
};
class Pole
{
public:
    void set(double sr,float hz) noexcept
    {const double g=std::tan(pi*std::clamp(static_cast<double>(hz),1.,sr*.45)/sr);a=static_cast<float>(g/(1+g));}
    float low(float x) noexcept {const float v=(x-z)*a,y=z+v;z=y+v;return y;}
    float high(float x) noexcept {return x-low(x);}
    void reset() noexcept {z=0;}
private:
    float a=0,z=0;
};
// 三频带损耗均不超过单位增益；全通内部同样按传播时间计算损耗。
class Loss
{
public:
    void set(double sr,float seconds,const Settings& p) noexcept
    {
        mid=std::exp(-6.9077553f*seconds/p.decay);
        lowGain=std::exp(-6.9077553f*seconds/(p.decay*p.bassMult));
        const float high=std::exp(-6.9077553f*seconds/(p.decay*std::pow(10.f,p.highDb/65.f)));
        topRatio=high/mid;bass.set(sr,p.bassHz);treble.set(sr,p.highHz);
    }
    float process(float x) noexcept
    {const float y=mid*x+(lowGain-mid)*bass.low(x);return topRatio*y+(1-topRatio)*treble.low(y);}
    void reset() noexcept {bass.reset();treble.reset();}
private:
    Pole bass,treble;float mid=0,lowGain=0,topRatio=1;
};
class Allpass
{
public:
    void prepare(double sr){memory.prepare(static_cast<int>(sr*.08+16));}
    void set(double sr,float seconds,float g,const Settings& p)
    {samples=std::max(1.f,std::round(static_cast<float>(sr)*seconds));gain=g;loss.set(sr,samples/static_cast<float>(sr),p);}
    float process(float x) noexcept
    {const float d=loss.process(memory.read(samples-1));const float v=x+gain*d;memory.write(v);return d-gain*v;}
    void reset(){memory.reset();loss.reset();}
private:
    Delay memory;Loss loss;float samples=1,gain=0;
};
using Frame=std::array<float,16>;
inline Frame scatter(Frame x) noexcept
{
    for(std::size_t step=1;step<16;step*=2)
        for(std::size_t base=0;base<16;base+=2*step)
            for(std::size_t j=0;j<step;++j)
            {const float a=x[base+j],b=x[base+j+step];x[base+j]=a+b;x[base+j+step]=a-b;}
    for(auto& v:x)v*=.25f;return x;
}
struct Path {float samples=1,gain=0;std::size_t input=0;};
class Field
{
public:
    void prepare(double sampleRate)
    {
        sr=sampleRate;
        for(auto& d:pre)d.prepare(static_cast<int>(sr*.26+16));
        for(auto& d:source)d.prepare(static_cast<int>(sr*.22+16));
        for(auto& d:spread)d.prepare(static_cast<int>(sr*.16+16));
        for(auto& d:lines)d.prepare(static_cast<int>(sr*.12+16));
        for(auto& a:earlyAP)for(auto& b:a)b.prepare(sr);
        for(auto& a:inputAP)for(auto& b:a)b.prepare(sr);
        for(auto& a:loopAP)a.prepare(sr);
        for(auto& f:inputHP)f.set(sr,35);
        reset();
    }
    void reset()
    {
        for(auto& d:pre)d.reset();for(auto& d:source)d.reset();for(auto& d:spread)d.reset();
        for(auto& d:lines)d.reset();for(auto& a:earlyAP)for(auto& b:a)b.reset();
        for(auto& a:inputAP)for(auto& b:a)b.reset();for(auto& a:loopAP)a.reset();
        for(auto& f:loss)f.reset();for(auto& f:inputHP)f.reset();for(auto& f:lowCut)f.reset();
        for(auto& f:highCut)f.reset();for(auto& f:earlyTone)f.reset();
        phase=0;geometrySize=-1;geometryStyle=-1;
    }
    void configure(const Settings& p,int selectedStyle)
    {
        const int style=selectedStyle==0?0:1;
        if(std::abs(geometrySize-p.size)>0||geometryStyle!=style){buildGeometry(p.size,style);geometrySize=p.size;geometryStyle=style;}
        const float scale=.35f+.65f*p.size*.01f;
        const float attack=p.attack*.01f,early=p.early*.01f,late=p.late*.01f;
        preSamples=p.predelay*.001f*static_cast<float>(sr);
        spreadSamples=(.002f+attack*(style==0?.024f:.043f))*scale*static_cast<float>(sr);
        // 共同默认白脉冲较已认可Plate低约6dB，固定标定2倍；无自动增益。
        erGain=2*(style==0?.60f:.52f)*(1-.65f*attack);
        tailGain=2*(style==0?1.35f:1.48f)*(1+.12f*attack);
        phaseStep=2*pi*p.rate/sr;modSamples=p.depth*.01f*static_cast<float>(sr)*.000045f;
        constexpr std::array<float,16> studio{.0083f,.0097f,.0113f,.0131f,.0149f,.0167f,.0187f,.0209f,.0233f,.0257f,.0281f,.0307f,.0331f,.0359f,.0383f,.0413f};
        constexpr std::array<float,16> live{.0137f,.0151f,.0181f,.0217f,.0241f,.0283f,.0311f,.0337f,.0389f,.0421f,.0457f,.0499f,.0523f,.0569f,.0611f,.0653f};
        double sumSeconds=0;
        for(std::size_t i=0;i<16;++i)
        {
            const float seconds=(style==0?studio[i]:live[i])*scale;
            samples[i]=std::max(4.f,std::round(seconds*static_cast<float>(sr)));
            loss[i].set(sr,samples[i]/static_cast<float>(sr),p);
            const float apSeconds=(.0011f+static_cast<float>((i*7)%17)*.00031f)*scale*(style==0?1.f:1.37f);
            loopAP[i].set(sr,apSeconds,.72f*late,p);
            sumSeconds+=seconds+apSeconds;
        }
        // 参数相关的能量标定，不检测输入，不补偿每个共振峰。
        injection=static_cast<float>(std::sqrt(1-std::exp(-13.81551056*(sumSeconds/16)/p.decay)));
        for(std::size_t c=0;c<2;++c)
        {
            lowCut[c].set(sr,p.lowCut);highCut[c].set(sr,p.highCut);
            // 长尾默认状态的几何早反射需要单独的空气吸收；否则离散高频峰会显得像金属敲击。
            // 短房间保留轮廓；2.3～2.8秒连续引入柔化，避免自动化跨阈值时音色跳变。
            const float softCut=std::min(p.highHz*.82f,p.highCut*.82f);
            const float amount=std::clamp((p.decay-2.3f)/.5f,0.f,1.f);
            const float blend=amount*amount*(3.f-2.f*amount);
            const float earlyCut=blend<=0?p.highCut:blend>=1?softCut:p.highCut+blend*(softCut-p.highCut);
            earlyTone[c].set(sr,earlyCut);
            for(std::size_t j=0;j<3;++j)
            {
                const float time=(.0007f+static_cast<float>(j)*.00103f+static_cast<float>(c)*.00019f)*scale;
                earlyAP[c][j].set(sr,time,.68f*early,p);
                inputAP[c][j].set(sr,(.0013f+static_cast<float>(j)*.00191f+static_cast<float>(c)*.00037f)*scale,
                                      .15f+.58f*late,p);
            }
        }
    }
    std::array<float,2> process(float left,float right) noexcept
    {
        const std::array<float,2> input{left,right};std::array<float,2> er{},excite{};
        for(std::size_t c=0;c<2;++c){pre[c].write(inputHP[c].high(input[c]));source[c].write(pre[c].read(preSamples));}
        for(std::size_t c=0;c<2;++c)
        {
            for(const auto& path:paths[c])er[c]+=source[path.input].read(path.samples)*path.gain;
            for(auto& a:earlyAP[c])er[c]=a.process(er[c]);
            er[c] = .72f * er[c] + .28f * earlyTone[c].low(er[c]);
            float seed=source[c].read(0);
            for(auto& a:inputAP[c])seed=a.process(seed);
            spread[c].write(seed);
            excite[c]=.64f*spread[c].read(spreadSamples)+.48f*spread[c].read(spreadSamples*1.731f)
                      -.36f*spread[c].read(spreadSamples*2.417f)+.48f*spread[c].read(spreadSamples*2.913f);
        }
        Frame y{};float lateL=0,lateR=0;
        for(std::size_t i=0;i<16;++i)
        {
            const float offset=modSamples*static_cast<float>(std::sin(phase+static_cast<double>(i)*2.399963));
            y[i]=loopAP[i].process(loss[i].process(lines[i].read(samples[i]+offset-1)));
            // 两条正交读出方向共享少量中心能量，避免单声道折叠失去主体。
            const float a=(i&1)?-.25f:.25f,b=(i&2)?-.25f:.25f;
            lateL+=y[i]*a;lateR+=y[i]*(.28f*a+.96f*b);
        }
        auto feedback=scatter(y);
        for(std::size_t i=0;i<16;++i)
        {
            const float a=(i&4)?-.25f:.25f,b=(i&8)?-.25f:.25f;
            lines[i].write(feedback[i]+injection*(a*excite[0]+b*excite[1]));
        }
        phase+=phaseStep;if(phase>2*pi)phase-=2*pi;
        return {highCut[0].low(lowCut[0].high(erGain*er[0]+tailGain*lateL)),
                highCut[1].low(lowCut[1].high(erGain*er[1]+tailGain*lateR))};
    }
    const std::array<std::array<Path,48>,2>& reflectionPaths() const noexcept {return paths;}
private:
    void buildGeometry(float size,int style)
    {
        const double scale=.35+.65*size*.01;
        const std::array<double,3> dim=style==0?std::array<double,3>{6.1,4.4,2.9}:std::array<double,3>{11.3,8.1,4.9};
        const auto image=[](int q,double d,double s){return q%2==0?q*d+s:(q+1)*d-s;};
        for(std::size_t ear=0;ear<2;++ear)
        {
            int index=0;double norm=0;
            for(std::size_t channel=0;channel<2;++channel)
            {
                const std::array<double,3> src{dim[0]*(channel==0?.43:.57),dim[1]*.29,dim[2]*.43};
                const std::array<double,3> rec{dim[0]*(ear==0?.48:.52),dim[1]*.66,dim[2]*.46};
                double direct=0;for(std::size_t axis=0;axis<3;++axis)direct+=(src[axis]-rec[axis])*(src[axis]-rec[axis]);direct=std::sqrt(direct);
                for(int x=-2;x<=2;++x)for(int y=-2;y<=2;++y)for(int z=-2;z<=2;++z)
                {
                    const int order=std::abs(x)+std::abs(y)+std::abs(z);if(order<1||order>2)continue;
                    const std::array<int,3> q{x,y,z};double distance=0;
                    for(std::size_t axis=0;axis<3;++axis){const double delta=image(q[axis],dim[axis],src[axis])-rec[axis];distance+=delta*delta;}
                    distance=std::sqrt(distance);
                    const float gain=static_cast<float>(std::pow(style==0?.68:.79,order)/distance);
                    paths[ear][static_cast<std::size_t>(index++)]={static_cast<float>((distance-direct)*scale/343.*sr),gain,channel};
                    norm+=gain*gain;
                }
            }
            const float level=static_cast<float>(.7/std::sqrt(norm));for(auto& path:paths[ear])path.gain*=level;
        }
    }
    double sr=48000,phase=0,phaseStep=0;
    float geometrySize=-1,preSamples=0,spreadSamples=1,modSamples=0,erGain=.5f,tailGain=1,injection=.3f;
    int geometryStyle=-1;
    std::array<Delay,2> pre,source,spread;
    std::array<std::array<Path,48>,2> paths{};
    std::array<std::array<Allpass,3>,2> earlyAP,inputAP;
    std::array<Delay,16> lines;std::array<Allpass,16> loopAP;std::array<Loss,16> loss;
    std::array<float,16> samples{};
    std::array<Pole,2> inputHP,lowCut,highCut;
    std::array<Pole,2> earlyTone;
};
}
