#include "../tools/HallMeasurement.h"
#include <cstdio>
#include <limits>
#include <chrono>
namespace hm = hall_measurement;
namespace
{
int failures = 0;
void check(bool good, const char* message)
{ std::printf("%s：%s\n",good?"通过":"失败",message); if(!good) ++failures; }
bool finite(const hm::Audio& x)
{ for(int c=0;c<x.getNumChannels();++c) for(int n=0;n<x.getNumSamples();++n) if(!std::isfinite(x.getSample(c,n))) return false; return true; }
double difference(hm::Audio a,hm::Audio b)
{
    a.applyGain(static_cast<float>(1/std::sqrt(hm::energy(a))));
    b.applyGain(static_cast<float>(1/std::sqrt(hm::energy(b))));
    for(int c=0;c<2;++c) a.addFrom(c,0,b,c,0,a.getNumSamples(),-1);
    return std::sqrt(hm::energy(a));
}
double density(const hm::Audio& x,double sr)
{
    double sum=0;int count=0,w=static_cast<int>(sr*.02);
    for(int n=static_cast<int>(sr*.12);n+w<static_cast<int>(sr*.5);n+=w)
    { sum+=hm::echoDensity(x,n,n+w);++count; }
    return sum/count;
}
void stereo(const hm::Audio& x)
{
    double l=0,r=0,m=0;
    for(int n=0;n<x.getNumSamples();++n)
    { const double a=x.getSample(0,n),b=x.getSample(1,n);l+=a*a;r+=b*b;m+=(a+b)*(a+b)*.25; }
    const double balance=10*std::log10(l/r),correlation=hm::correlation(x,0,x.getNumSamples()),mono=m/(.5*(l+r));
    std::printf("左右差 %.3f dB，相关 %.3f，单声道功率比 %.3f\n",balance,correlation,mono);
    check(std::abs(balance)<1.5&&std::abs(correlation)<.85&&mono>.25,"中央输入声像与单声道兼容");
}
hm::Params settings(int mode)
{
    auto p=hm::baseline();p.category=3;p.mode=mode;p.decay=2.1f;p.predelay=15;
    p.attack=35;p.highShelf=-16;p.bassMult=1.1f;p.modDepth=20;p.modRate=.6f;p.lowCut=35;p.highCut=9500;return p;
}
}
int main()
{
    juce::ScopedNoDenormals guard;
    constexpr double sr=48000;
    const auto matrix=sanmu_chamber::makeScattering();double error=0;
    for(int i=0;i<12;++i)for(int j=0;j<12;++j)
    { double dot=0;for(int k=0;k<12;++k)dot+=matrix[i][k]*matrix[j][k];error=std::max(error,std::abs(dot-(i==j?1.:0.))); }
    check(error<2e-7,"全部散射基底正交，不凭主观印象假定反馈稳定");
    const auto input=hm::impulse(sr,5);
    for(int mode:{0,1})
    {
        std::printf("回声室模式 %d\n",mode);auto p=settings(mode);
        const auto wet=hm::render(input,sr,128,p);
        check(finite(wet)&&hm::energy(wet)>1e-8&&hm::energy(wet,0,720)==0,"基准非静音且预延迟前没有湿声");
        stereo(wet);std::printf("密度 %.3f\n",density(wet,sr));check(density(wet,sr)>.8,"高扩散晚期场连续");
        check(hm::maxDifference(wet,hm::render(input,sr,257,p,64))<2e-6,"块大小与内部分片不改变声音");
        for(int control=0;control<3;++control)
        {
            auto lo=p,hi=p;
            if(control==0){lo.earlyDiff=0;hi.earlyDiff=100;}
            if(control==1){lo.lateDiff=0;hi.lateDiff=100;}
            if(control==2){lo.attack=0;hi.attack=100;}
            auto a=hm::render(input,sr,128,lo),b=hm::render(input,sr,128,hi);
            const double d=difference(a,b);std::printf("控制 %d 结构差 %.3f\n",control,d);check(d>.2,"控制改变归一化波形");
            stereo(a);stereo(b);
            if(control==2)
            {
                const double shift=1000*(hm::centroid(b,sr,.5)-hm::centroid(a,sr,.5));
                std::printf("Attack 能量质心后移 %.3f 毫秒\n",shift);check(shift>10,"Attack使能量向后铺展");
            }
        }
        for(float decay:{.4f,1.f,3.f})for(float size:{0.f,100.f})for(float diffusion:{0.f,100.f})
        {
            auto flat=p;flat.decay=decay;flat.size=size;flat.lateDiff=diffusion;flat.highShelf=0;
            // 此处令内置低频吸收后的时间等于中频，检验全频标称衰减。
            flat.bassMult=mode==0?1.32f:1.42f;flat.modDepth=0;
            const auto fit=hm::fitT20(hm::render(hm::impulse(sr,decay*2+1),sr,128,flat),sr);
            std::printf("目标 %.2f 尺寸 %.0f 扩散 %.0f 实测 %.3f\n",decay,size,diffusion,fit.rt60);
            check(fit.valid&&std::abs(fit.rt60/decay-1)<.15,"平坦阻尼RT60误差小于15%");
        }
        for(double rate:{44100.,48000.,96000.,192000.})
        {const auto x=hm::render(hm::impulse(rate,2),rate,257,p);check(finite(x)&&density(x,rate)>.8,"四采样率有限且密度连续");}
        auto fresh=p;fresh.decay=3.99f;fresh.predelay=20;fresh.highShelf=-24;fresh.bassMult=1.5f;
        fresh.attack=50;fresh.modRate=2.53f;fresh.modDepth=38;fresh.lowCut=10;fresh.highCut=8000;
        const auto normal=hm::render(input,sr,128,fresh);stereo(normal);check(finite(normal)&&density(normal,sr)>.8,"新插入默认长尾的密度与稳定性");
        auto x=hm::noise(sr,.15,4,.03f,208);auto twice=x;twice.applyGain(2);
        auto expected=hm::render(x,sr,128,p);expected.applyGain(2);
        check(hm::maxDifference(expected,hm::render(twice,sr,128,p))<2e-6,"幅度线性，没有电平检测或隐藏压缩");
        check(hm::energy(hm::render(hm::impulse(sr,.5,0),sr,128,p))==0,"零输入严格静音");
        auto dry=p;dry.mix=0;check(hm::maxDifference(x,hm::render(x,sr,128,dry))==0,"干声端点透明");
        check(finite(hm::render(hm::impulse(sr,1,.1,0,1),sr,64,p)),"单声道总线有效");
        for(float late:{0.f,100.f})
        {
            auto extreme=p;extreme.decay=20;extreme.bassMult=4;extreme.highShelf=0;extreme.size=0;
            extreme.modRate=10;extreme.modDepth=100;extreme.lateDiff=late;
            const auto z=hm::render(hm::noise(sr,.2,12,.05f,89),sr,32,extreme);
            check(finite(z)&&z.getMagnitude(0,z.getNumSamples())<1&&hm::energy(z,10*48000,11*48000)<hm::energy(z,48000,2*48000)*1.1,"极端参数反馈不发散");
        }
    }
    check(difference(hm::render(input,sr,128,settings(0)),hm::render(input,sr,128,settings(1)))>.4,"同参数两模式具有真实结构差异");
    check(hm::maxDifference(hm::render(input,sr,128,settings(0)),hm::render(input,sr,128,settings(2)))==0,"旧第三编号兼容柔暖模式");
    ReverbProcessor processor;processor.prepare(96000,32);hm::Audio block(2,32);bool valid=true;float peak=0;
    double maximumMicros=0;
    for(int i=0;i<24000;++i)
    {
        auto p=settings((i/600)%2);p.category=(i/2400)%4;
        p.predelay=125+125*std::sin(i*.002);p.attack=50+50*std::sin(i*.003);
        p.size=50+50*std::sin(i*.001);p.earlyDiff=50+50*std::sin(i*.0013);p.lateDiff=100-p.earlyDiff;
        for(int n=0;n<32;++n){block.setSample(0,n,.03f*std::sin((i*32+n)*.071));block.setSample(1,n,.03f*std::sin((i*32+n)*.079));}
        if(i%1000==0){p.decay=std::numeric_limits<float>::infinity();block.setSample(0,0,std::numeric_limits<float>::quiet_NaN());}
        const auto start=std::chrono::steady_clock::now();processor.process(block,p);
        const double elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
        maximumMicros=std::max(maximumMicros,elapsed);valid&=finite(block);peak=std::max(peak,block.getMagnitude(0,32));
    }
    std::printf("连续自动化与类别切换峰值 %.4f，最慢块 %.1f 微秒（期限333微秒，调度时间不等同纯计算）\n",peak,maximumMicros);
    check(valid&&peak<1,"类别切换、参数自动化和非法输入无无效值或失控峰值");
    std::printf("回声室契约：%d 项失败\n",failures);return failures?1:0;
}
