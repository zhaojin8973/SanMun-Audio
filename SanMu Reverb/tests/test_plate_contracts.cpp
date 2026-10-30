#include "../tools/HallMeasurement.h"
#include "../source/dsp/engines/plate/PlateField.h"
#include <cstdio>
#include <cmath>
#include <limits>

namespace hm=hall_measurement;
namespace
{
int failures=0;
int selectedMode=0;
void check(bool passed,const char* message)
{std::printf("%s：%s\n",passed?"通过":"失败",message);if(!passed)++failures;}
hm::Params baseline()
{
    auto p=hm::baseline();p.category=0;p.mode=selectedMode;p.decay=2.39f;p.predelay=15;
    p.highFreq=6000;p.highShelf=-18;p.bassFreq=300;p.bassMult=1;
    p.size=100;p.attack=30;p.earlyDiff=p.lateDiff=100;p.modRate=.8f;p.modDepth=20;
    p.lowCut=20;p.highCut=9000;return p;
}
bool finite(const hm::Audio& a)
{
    for(int c=0;c<a.getNumChannels();++c)for(int n=0;n<a.getNumSamples();++n)
        if(!std::isfinite(a.getSample(c,n)))return false;
    return true;
}
double density(const hm::Audio& a,double sr,double start,double end)
{
    double sum=0;int count=0,step=static_cast<int>(sr*.02);
    for(int n=static_cast<int>(sr*start);n+step<=static_cast<int>(sr*end);n+=step)
    {sum+=hm::echoDensity(a,n,n+step);++count;}
    return sum/count;
}
double shapeDifference(hm::Audio a,hm::Audio b)
{
    a.applyGain(static_cast<float>(1/std::sqrt(hm::energy(a))));
    b.applyGain(static_cast<float>(1/std::sqrt(hm::energy(b))));
    for(int c=0;c<a.getNumChannels();++c)a.addFrom(c,0,b,c,0,a.getNumSamples(),-1);
    return std::sqrt(hm::energy(a));
}
void stereo(const hm::Audio& a)
{
    double el=0,er=0,em=0;
    for(int n=0;n<a.getNumSamples();++n)
    {const double l=a.getSample(0,n),r=a.getSample(1,n);el+=l*l;er+=r*r;em+=.25*(l+r)*(l+r);}
    const double balance=10*std::log10(el/er),corr=hm::correlation(a,0,a.getNumSamples()),mono=em/(.5*(el+er));
    std::printf("立体声：左右差 %.3fdB，相关 %.3f，单声道功率比 %.3f\n",balance,corr,mono);
    check(std::abs(balance)<1.5&&std::abs(corr)<.6&&mono>.18,"宽度、左右能量和单声道兼容同时成立");
}
}
int main(int argc,char** argv)
{
    selectedMode=argc>1&&juce::String(argv[1])=="--steel"?1:0;
    juce::ScopedNoDenormals guard;
    constexpr double sr=48000;
    // 用全部基向量核对实际多路散射矩阵，不能只检查一个输入的能量。
    std::array<sanmu_plate::Frame,sanmu_plate::branchCount> basis{};
    for(std::size_t i=0;i<sanmu_plate::branchCount;++i){sanmu_plate::Frame x{};x[i]=1;basis[i]=sanmu_plate::scatter(x);}
    double orthogonalError=0;
    for(std::size_t i=0;i<sanmu_plate::branchCount;++i)for(std::size_t j=0;j<sanmu_plate::branchCount;++j)
    {
        double dot=0;for(std::size_t k=0;k<sanmu_plate::branchCount;++k)dot+=basis[i][k]*basis[j][k];
        orthogonalError=std::max(orthogonalError,std::abs(dot-(i==j?1.:0.)));
    }
    check(orthogonalError<1e-7,"八路反馈散射全基底正交，无隐藏增益");
    // 单独检查新增传播吸收：各采样率均无增益，低频适量损耗，高频边界保留。
    for(double rate:{44100.,48000.,96000.,192000.})
    {
        double lowDb=0,highDb=0;
        for(double hz:{40.,100.,200.,260.,500.,1000.,4000.})
        {
            sanmu_plate::LowBodyLoss loss;loss.set(rate,.05f,2.39f);
            double before=0,after=0;
            for(int n=0;n<static_cast<int>(rate*.3);++n)
            {
                const float x=static_cast<float>(.1*std::sin(2*sanmu_plate::pi*hz*n/rate));
                const float y=loss.process(x);
                if(n>static_cast<int>(rate*.1)){before+=x*x;after+=y*y;}
            }
            const double db=10*std::log10(after/before);
            check(std::isfinite(db)&&db<=.00001&&db>-.35,"传播吸收为温和无增益滤波");
            if(hz==100)lowDb=db;
            if(hz==4000)highDb=db;
        }
        check(lowDb<-.2&&highDb>-.005,"100Hz有效吸收，4kHz没有提亮或明显削弱");
    }
    auto p=baseline();const auto input=hm::impulse(sr,4);
    // 用户实际采用新插入默认值切换类型；关闭调制时也必须有连续的扩散。
    for(float depth:{0.f,38.f})
    {
        auto fresh=hm::baseline();fresh.category=0;fresh.mode=selectedMode;fresh.predelay=20;fresh.highCut=8000;fresh.modDepth=depth;
        const auto r=hm::render(input,sr,128,fresh);
        check(finite(r)&&density(r,sr,.04,.12)>.65&&density(r,sr,.12,.5)>.8,
            "新插入默认及关闭调制均维持连续密度");
        stereo(r);
    }
    const auto wet=hm::render(input,sr,128,p);
    check(finite(wet)&&hm::energy(wet)>1e-8,"基准输出有限且非静音");
    check(hm::energy(wet,0,720)==0,"预延迟之前严格无湿声");
    const double early=density(wet,sr,.04,.12),late=density(wet,sr,.12,.5);
    std::printf("基准密度：40～120ms %.3f，120～500ms %.3f\n",early,late);
    check(early>.65&&late>.8,"高扩散密度达到预先设定的下限");
    double minimum=1e30,maximum=0;
    for(int ms=40;ms<200;ms+=10)
    {const double e=hm::energy(wet,ms*48,(ms+20)*48);minimum=std::min(minimum,e);maximum=std::max(maximum,e);}
    std::printf("建立后能量窗口跨度 %.3fdB\n",10*std::log10(maximum/minimum));
    check(minimum>maximum*.01,"建立后无超过20dB的能量空洞");
    stereo(wet);
    for(int channel:{0,1})
    {
        hm::Audio one;one.makeCopyOf(input);one.clear(channel,0,one.getNumSamples());
        stereo(hm::render(one,sr,128,p));
    }
    const auto mono=hm::render(hm::impulse(sr,4,.1,0,1),sr,128,p);
    check(finite(mono)&&hm::energy(mono)>1e-8,"单声道宿主可输出有效湿声");
    check(hm::maxDifference(wet,hm::render(input,sr,257,p,64))<2e-6,"不同宿主块大小与超大块分片结果一致");
    hm::Audio e0,l0,a0,a100;
    for(int control=0;control<3;++control)
    {
        auto lo=p,hi=p;
        if(control==0)lo.earlyDiff=0;
        if(control==1)lo.lateDiff=0;
        if(control==2){lo.attack=0;hi.attack=100;}
        const auto first=hm::render(input,sr,128,lo),last=hm::render(input,sr,128,hi);
        const double difference=shapeDifference(first,last);
        std::printf("控制%d匹配电平后的结构差 %.3f\n",control,difference);
        check(difference>.2,"EARLY、LATE、Attack端点真实改变时间结构");
        if(control==0)e0=first;
        if(control==1)l0=first;
        if(control==2){a0=first;a100=last;}
    }
    const double centroidShift=hm::centroid(a100,sr,.5)-hm::centroid(a0,sr,.5);
    const double attackEnergy=hm::energy(a100)/hm::energy(a0);
    std::printf("Attack初期质心后移 %.2fms，能量比 %.3f\n",1000*centroidShift,attackEnergy);
    check(centroidShift>.015&&attackEnergy>.6&&attackEnergy<1.6,"Attack铺开初始能量且无过大音量变化");
    std::printf("Early0/100起音密度 %.3f/%.3f；Late0/100尾部密度 %.3f/%.3f\n",
        density(e0,sr,.04,.12),early,density(l0,sr,.12,.5),late);
    // 低扩散板式仍有输入扩散，不能要求它像无扩散Hall一样成为裸延迟。
    for(float decay:{1.f,3.f,6.f})for(float size:{0.f,100.f})for(float diffusion:{0.f,100.f})
    {
        auto flat=p;flat.decay=decay;flat.size=size;flat.lateDiff=diffusion;
        flat.bassMult=1;flat.highShelf=0;flat.modDepth=0;
        const auto response=hm::render(hm::impulse(sr,decay*1.5+1),sr,128,flat);
        const auto fit=hm::fitT20(response,sr);
        std::printf("衰减%.1fs/尺寸%.0f/晚扩散%.0f：实测%.3fs\n",decay,size,diffusion,fit.rt60);
        check(fit.valid&&std::abs(fit.rt60/decay-1)<.10,"尺寸与晚扩散端点保持标称衰减");
    }
    auto x=hm::noise(sr,.1,1,.1f,177),y=hm::noise(sr,.1,1,.06f,329);
    for(int n=0;n<y.getNumSamples();++n)y.setSample(1,n,-.3f*y.getSample(0,n));
    hm::Audio sum;sum.makeCopyOf(x);
    for(int c=0;c<2;++c)sum.addFrom(c,0,y,c,0,y.getNumSamples());
    auto expected=hm::render(x,sr,128,p);const auto fy=hm::render(y,sr,128,p);
    for(int c=0;c<2;++c)expected.addFrom(c,0,fy,c,0,fy.getNumSamples());
    check(hm::maxDifference(expected,hm::render(sum,sr,128,p))<2e-6,"满足线性叠加，不使用信号相关音量补偿");
    for(double rate:{44100.,48000.,96000.,192000.})
    {
        const auto r=hm::render(hm::impulse(rate,2),rate,257,p);
        check(finite(r)&&hm::energy(r)>1e-8,"不同采样率有效且有限");
        check(density(r,rate,.04,.12)>.65&&density(r,rate,.12,.5)>.8,"不同采样率维持扩散密度");
    }
    auto silence=hm::impulse(sr,.2,0);
    check(hm::energy(hm::render(silence,sr,128,p))==0,"零输入不产生杂音");
    for(float diffusion:{0.f,50.f,100.f})
    {
        auto extreme=p;extreme.decay=20;extreme.bassMult=4;extreme.highShelf=0;extreme.size=0;
        extreme.lateDiff=diffusion;extreme.modDepth=100;extreme.modRate=10;
        const auto r=hm::render(hm::noise(sr,.15,8,.05f,221),sr,257,extreme);
        check(finite(r)&&r.getMagnitude(0,r.getNumSamples())<1&&hm::energy(r,6*48000,7*48000)<hm::energy(r,48000,2*48000)*1.2,
            "最长低频衰减、最小空间和最大调制下反馈不发散");
    }
    ReverbProcessor processor;processor.prepare(sr,128);
    hm::Audio block(2,128);uint32_t random=733;
    double peak=0;bool valid=true;
    for(int b=0;b<3000;++b)
    {
        auto automated=p;
        automated.size=static_cast<float>(50+50*std::sin(b*.003));
        automated.attack=static_cast<float>(50+50*std::sin(b*.002));
        automated.earlyDiff=static_cast<float>(50+50*std::sin(b*.005));
        automated.lateDiff=static_cast<float>(50+50*std::sin(b*.004));
        automated.predelay=static_cast<float>(125+125*std::sin(b*.001));
        if(b==1200)automated.highShelf=std::numeric_limits<float>::quiet_NaN();
        for(int n=0;n<128;++n)
        {random^=random<<13;random^=random>>17;random^=random<<5;const float v=b<1500?(static_cast<float>(random>>8)/8388608.f-1)*.02f:0;
         block.setSample(0,n,v);block.setSample(1,n,v);}
        if(b==800)block.setSample(0,5,std::numeric_limits<float>::infinity());
        processor.process(block,automated);valid=valid&&finite(block);peak=std::max(peak,static_cast<double>(block.getMagnitude(0,128)));
    }
    check(valid&&peak<1,"连续参数变化与非有限异常输入不破坏运行");
    // 两个风格来回切换后再回到旧分支，覆盖双缓冲内核的状态复用。
    processor.reset();valid=true;peak=0;
    for(int b=0;b<1500;++b)
    {
        auto style=p;style.mode=(b/113)%2;
        for(int n=0;n<128;++n)
        {
            const float v=b<900?.04f*std::sin(static_cast<float>((b*128+n)*2*sanmu_plate::pi*190/sr)):0;
            block.setSample(0,n,v);block.setSample(1,n,v);
        }
        processor.process(block,style);valid=valid&&finite(block);
        peak=std::max(peak,static_cast<double>(block.getMagnitude(0,128)));
    }
    check(valid&&peak<1,"两个板式风格反复切换无非有限值或异常放大");
    std::printf("Plate失败项：%d\n",failures);return failures?1:0;
}
