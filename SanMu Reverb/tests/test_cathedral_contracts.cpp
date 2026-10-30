#include "../tools/HallMeasurement.h"
#include <cstdio>

namespace hm = hall_measurement;
namespace
{
int failures = 0;
void check(bool pass, const char* text)
{
    std::printf("%s：%s\n", pass ? "通过" : "失败", text);
    if (!pass) ++failures;
}
hm::Params settings()
{
    auto p = hm::baseline();
    p.mode=2; p.predelay=20; p.decay=3.99f; p.highFreq=6000; p.highShelf=-24;
    p.bassFreq=300; p.bassMult=1.5f; p.size=100; p.attack=50;
    p.earlyDiff=100; p.lateDiff=100; p.modRate=2.53f; p.modDepth=38;
    p.lowCut=10; p.highCut=8010;
    return p;
}
bool finite(const hm::Audio& y)
{
    for (int c=0;c<y.getNumChannels();++c)
        for (int n=0;n<y.getNumSamples();++n)
            if (!std::isfinite(y.getSample(c,n))) return false;
    return true;
}
double density(const hm::Audio& y,double sr,double begin,double end)
{
    double sum=0;int count=0;
    for (int n=static_cast<int>(sr*begin);n+sr*.02<=sr*end;n+=static_cast<int>(sr*.02))
    { sum+=hm::echoDensity(y,n,n+static_cast<int>(sr*.02));++count; }
    return sum/count;
}
}
int main()
{
    juce::ScopedNoDenormals guard;
    constexpr double sr=48000;
    // 在真实路由中验证前段左右等响及单边方向，不只验证校正函数。
    for (double rate : {44100.,48000.,96000.,192000.})
        for (int mode : {0,2})
        {
            auto p=settings();p.mode=mode;p.earlyDiff=0;
            auto x=hm::impulse(rate,.4,.1);
            auto y=hm::render(x,rate,128,p);
            double error=0,energy=0;
            for (int n=0;n<rate*.04;++n)
            { error=std::max(error,static_cast<double>(std::abs(y.getSample(0,n)-y.getSample(1,n))));energy+=y.getSample(0,n)*y.getSample(0,n); }
            check(energy>1e-8 && error<1e-8,"零EARLY的前段湿声居中且同相，多采样率成立");
            for (int active : {0,1})
            {
                x=hm::impulse(rate,.4,.1);x.clear(1-active,0,x.getNumSamples());
                y=hm::render(x,rate,128,p);
                double silent=0;
                for (int n=0;n<rate*.04;++n) silent+=std::abs(y.getSample(1-active,n));
                check(silent<1e-8,"零EARLY的单边输入前段不反相分流到另一边");
            }
        }
    auto p=settings();
    const auto x=hm::impulse(sr,5,.1);
    const auto y=hm::render(x,sr,128,p);
    auto hall=p;hall.mode=0;
    const auto h=hm::render(x,sr,128,hall);
    const double shift=hm::centroid(y,sr,.5)-hm::centroid(h,sr,.5);
    const double dense=density(y,sr,.2,.7);
    double el=0,er=0,em=0;
    for (int n=static_cast<int>(sr*.3);n<sr*3;++n)
    {
        const double l=y.getSample(0,n),r=y.getSample(1,n);
        el+=l*l;er+=r*r;em+=.25*(l+r)*(l+r);
    }
    const double balance=10*std::log10(el/er),cor=hm::correlation(y,static_cast<int>(sr*.3),static_cast<int>(sr*3));
    std::printf("教堂相对音乐厅前500ms质心后移 %.2fms，密度 %.3f，左右差 %.2fdB，相关 %.3f\n",shift*1000,dense,balance,cor);
    check(finite(y) && shift>.04,"同衰减参数的教堂建立比音乐厅至少后移40ms");
    check(dense>.75,"教堂高扩散形成连续的密集尾音");
    check(std::abs(balance)<1.5 && std::abs(cor)<.6 && em/(.5*(el+er))>.18,"教堂同时满足左右平衡、宽度与单声道兼容");
    check(hm::maxDifference(y,hm::render(x,sr,257,p,64))<2e-6,"教堂不依赖宿主块大小");
    auto sparse=p;sparse.lateDiff=0;
    const double lowDensity=density(hm::render(x,sr,128,sparse),sr,.2,.7);
    std::printf("教堂Late两端密度 %.3f / %.3f\n",lowDensity,dense);
    check(dense-lowDensity>.25,"教堂Late两端有明确的密度变化");
    auto first=p;first.attack=0;auto last=p;last.attack=100;
    const auto f=hm::render(x,sr,128,first),b=hm::render(x,sr,128,last);
    const double attackShift=hm::centroid(b,sr,.8)-hm::centroid(f,sr,.8);
    std::printf("教堂Attack两端质心后移 %.2fms\n",attackShift*1000);
    check(attackShift>.04 && hm::energy(b)>.3*hm::energy(f),"教堂Attack改变建立时间并保留后方能量");
    for (float decay : {3.f,8.f})
    {
        auto flat=p;flat.decay=decay;flat.bassMult=1;flat.highShelf=0;flat.modDepth=0;
        const auto r=hm::render(hm::impulse(sr,decay*1.5+1,.1),sr,128,flat);
        const auto fit=hm::fitT20(r,sr);
        std::printf("教堂平坦衰减：设定 %.1fs，实测 %.3fs\n",decay,fit.rt60);
        check(fit.valid && std::abs(fit.rt60/decay-1)<.15,"教堂保持面板Decay的标称衰减含义");
    }
    for (float late : {0.f,25.f,50.f,75.f,100.f})
    {
        auto extreme=p;extreme.size=0;extreme.decay=20;extreme.bassMult=4;extreme.highShelf=0;extreme.lateDiff=late;
        const auto r=hm::render(hm::noise(sr,.15,6,.05f,731),sr,257,extreme);
        check(finite(r) && r.getMagnitude(0,r.getNumSamples())<1
              && hm::energy(r,240000,288000)<hm::energy(r,48000,96000)*1.2,"教堂最长低频衰减和最小尺寸持续反馈不发散");
    }
    auto noise=hm::noise(sr,.1,1,.04f,123);
    auto noise2=hm::noise(sr,.1,1,.03f,765);
    auto total=noise;
    for(int c=0;c<2;++c)total.addFrom(c,0,noise2,c,0,noise2.getNumSamples());
    auto expected=hm::render(noise,sr,128,p);const auto other=hm::render(noise2,sr,128,p);
    for(int c=0;c<2;++c)expected.addFrom(c,0,other,c,0,other.getNumSamples());
    check(hm::maxDifference(expected,hm::render(total,sr,128,p))<2e-6,"教堂满足输入叠加性，不按音量动态归一化");
    for(double rate : {44100.,96000.,192000.})
    {
        const auto r=hm::render(hm::impulse(rate,1,.1),rate,257,p);
        check(finite(r) && hm::energy(r,0,static_cast<int>(rate*.02))==0 && density(r,rate,.2,.7)>.75,"教堂多采样率的预延迟和密度成立");
    }
    std::printf("教堂与零值端点检查：%d项失败\n",failures);
    return failures?1:0;
}
