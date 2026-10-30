#include "../tools/HallMeasurement.h"
#include "../source/dsp/engines/room/RoomField.h"
#include <cstdio>
#include <limits>
namespace hm=hall_measurement;
namespace
{
int failures=0;
void check(bool pass,const char* message){std::printf("%s：%s\n",pass?"通过":"失败",message);if(!pass)++failures;}
bool finite(const hm::Audio& x)
{for(int c=0;c<x.getNumChannels();++c)for(int n=0;n<x.getNumSamples();++n)if(!std::isfinite(x.getSample(c,n)))return false;return true;}
double density(const hm::Audio& x,double sr,double start,double end)
{double d=0;int count=0;const int w=static_cast<int>(sr*.02);for(int n=static_cast<int>(start*sr);n+w<=static_cast<int>(end*sr);n+=w){d+=hm::echoDensity(x,n,n+w);++count;}return d/count;}
double difference(hm::Audio a,hm::Audio b)
{a.applyGain(static_cast<float>(1/std::sqrt(hm::energy(a))));b.applyGain(static_cast<float>(1/std::sqrt(hm::energy(b))));for(int c=0;c<2;++c)a.addFrom(c,0,b,c,0,a.getNumSamples(),-1);return std::sqrt(hm::energy(a));}
hm::Params settings(int mode)
{auto p=hm::baseline();p.category=1;p.mode=mode;p.decay=.8f;p.predelay=5;p.attack=25;p.highShelf=-12;p.bassMult=1;p.modDepth=0;return p;}
void stereo(const hm::Audio& x)
{
    double l=0,r=0,m=0;for(int n=0;n<x.getNumSamples();++n){double a=x.getSample(0,n),b=x.getSample(1,n);l+=a*a;r+=b*b;m+=(a+b)*(a+b)*.25;}
    const double db=10*std::log10(l/r),corr=hm::correlation(x,0,x.getNumSamples()),mono=m/(.5*(l+r));
    std::printf("左右差 %.3fdB，相关 %.3f，单声道功率比 %.3f\n",db,corr,mono);
    check(std::abs(db)<1.5&&std::abs(corr)<.85&&mono>.2,"中央声源能量均衡且单声道兼容");
}
}
int main()
{
    juce::ScopedNoDenormals guard;constexpr double sr=48000;
    std::array<sanmu_room::Frame,16> basis{};
    for(int i=0;i<16;++i){sanmu_room::Frame x{};x[static_cast<std::size_t>(i)]=1;basis[static_cast<std::size_t>(i)]=sanmu_room::scatter(x);}
    double error=0;for(int i=0;i<16;++i)for(int j=0;j<16;++j){double dot=0;for(int k=0;k<16;++k)dot+=basis[i][k]*basis[j][k];error=std::max(error,std::abs(dot-(i==j?1.:0.)));}
    check(error<1e-7,"十六路散射全基底正交");
    const auto input=hm::impulse(sr,3);
    for(int mode:{0,1})
    {
        std::printf("房间模式 %d\n",mode);auto p=settings(mode);
        sanmu_room::Field f;f.prepare(sr);sanmu_room::Settings v;f.configure(v,mode);
        const auto paths=f.reflectionPaths();bool geometry=true;
        for(const auto& ear:paths)for(const auto& path:ear)geometry&=path.samples>sr*.001&&std::isfinite(path.gain)&&path.gain>0;
        v.size=0;f.configure(v,mode);const auto small=f.reflectionPaths();
        for(int c=0;c<2;++c)for(int i=0;i<48;++i)geometry&=std::abs(small[c][i].samples/paths[c][i].samples-.35)<1e-6;
        check(geometry,"几何路径全为非直达反射，尺寸缩放改变路径时间");
        const auto wet=hm::render(input,sr,128,p);
        check(finite(wet)&&hm::energy(wet)>1e-8,"基准有效且非静音");
        check(hm::energy(wet,0,240)==0,"预延迟之前严格无湿声");stereo(wet);
        const double d=density(wet,sr,.12,.5);std::printf("120～500毫秒密度 %.3f\n",d);check(d>.8,"晚期高扩散连续密度");
        check(hm::maxDifference(wet,hm::render(input,sr,257,p,64))<2e-6,"块大小与分片不改变结果");
        for(int control=0;control<3;++control)
        {
            auto lo=p,hi=p;if(control==0)lo.earlyDiff=0;if(control==1)lo.lateDiff=0;if(control==2){lo.attack=0;hi.attack=100;}
            const auto a=hm::render(input,sr,128,lo),b=hm::render(input,sr,128,hi);const double diff=difference(a,b);
            std::printf("控制%d结构差 %.3f\n",control,diff);check(diff>.2,"三个控制均真实改变结构");
            if(control==2){const double shift=1000*(hm::centroid(b,sr,.5)-hm::centroid(a,sr,.5));std::printf("Attack质心后移 %.3f毫秒，能量比 %.3f\n",shift,hm::energy(b)/hm::energy(a));check(shift>8,"Attack将能量往后铺开");}
            stereo(a);stereo(b);
        }
        for(float decay:{.4f,1.f,3.f})for(float size:{0.f,100.f})for(float late:{0.f,100.f})
        {
            auto flat=p;flat.decay=decay;flat.size=size;flat.lateDiff=late;flat.highShelf=0;flat.bassMult=1;
            const auto fit=hm::fitT20(hm::render(hm::impulse(sr,decay*1.6+1),sr,128,flat),sr);
            std::printf("衰减 %.1f 尺寸 %.0f 扩散 %.0f：RT60 %.3f\n",decay,size,late,fit.rt60);
            check(fit.valid&&std::abs(fit.rt60/decay-1)<.15,"平坦阻尼衰减误差小于15%");
        }
        for(double rate:{44100.,48000.,96000.,192000.})
        {
            const auto r=hm::render(hm::impulse(rate,2),rate,257,p);check(finite(r)&&density(r,rate,.12,.5)>.8,"四种采样率尾音有限且连续");
        }
        auto fresh=p;fresh.decay=3.99f;fresh.predelay=20;fresh.highShelf=-24;fresh.bassMult=1.5f;fresh.attack=50;fresh.modRate=2.53f;fresh.modDepth=38;fresh.lowCut=10;fresh.highCut=8000;
        const auto defaultWet=hm::render(input,sr,128,fresh);stereo(defaultWet);check(finite(defaultWet)&&density(defaultWet,sr,.12,.5)>.8,"真实新插入默认的长房间同样连续");
        // 同一脉冲、相邻衰减值，直接检验旧2.5秒阈值两侧的音频连续性。
        auto below=fresh,above=fresh;below.decay=2.4999f;above.decay=2.5001f;
        const auto lower=hm::render(input,sr,128,below),upper=hm::render(input,sr,128,above);
        const double relativeJump=hm::maxDifference(lower,upper)/lower.getMagnitude(0,lower.getNumSamples());
        std::printf("2.5秒附近最大相对样本差 %.7f\n",relativeJump);
        check(relativeJump<.001,"衰减经过2.5秒时无早反射音色台阶");
        const auto x=hm::noise(sr,.1,2,.04f,11);auto twice=x;twice.applyGain(2);auto expected=hm::render(x,sr,128,p);expected.applyGain(2);
        check(hm::maxDifference(expected,hm::render(twice,sr,128,p))<2e-6,"幅度线性，无输入相关电平补偿");
        check(hm::energy(hm::render(hm::impulse(sr,.5,0),sr,128,p))==0,"零输入严格静音");
        const auto mono=hm::render(hm::impulse(sr,2,.1,0,1),sr,128,p);check(finite(mono)&&hm::energy(mono)>1e-8,"单声道宿主有效");
        for(float diffusion:{0.f,100.f})
        {
            auto extreme=p;extreme.decay=20;extreme.bassMult=4;extreme.highShelf=0;extreme.size=0;extreme.modDepth=100;extreme.modRate=10;extreme.lateDiff=diffusion;
            const auto z=hm::render(hm::noise(sr,.15,8,.04f,13),sr,257,extreme);
            check(finite(z)&&z.getMagnitude(0,z.getNumSamples())<1&&hm::energy(z,6*48000,7*48000)<hm::energy(z,48000,2*48000)*1.2,"极端反馈衰减不发散");
        }
    }
    check(difference(hm::render(input,sr,128,settings(0)),hm::render(input,sr,128,settings(1)))>.2,"两种风格同参数具有真实结构差异");
    check(hm::maxDifference(hm::render(input,sr,128,settings(1)),hm::render(input,sr,128,settings(2)))==0,"旧第三编号明确兼容Live而非隐藏第三风格");
    ReverbProcessor processor;processor.prepare(sr,128);hm::Audio block(2,128);bool valid=true;float peak=0;
    for(int i=0;i<1600;++i)
    {
        auto p=settings((i/100)%3);p.size=50+50*std::sin(i*.013f);p.attack=50+50*std::sin(i*.011f);p.earlyDiff=50+50*std::sin(i*.019f);p.lateDiff=100-p.earlyDiff;
        for(int n=0;n<128;++n){block.setSample(0,n,.05f*std::sin((i*128+n)*.073f));block.setSample(1,n,.04f*std::sin((i*128+n)*.091f));}
        if(i%200==0){block.setSample(0,0,std::numeric_limits<float>::quiet_NaN());p.decay=std::numeric_limits<float>::infinity();}
        processor.process(block,p);valid&=finite(block);peak=std::max(peak,block.getMagnitude(0,128));
    }
    std::printf("自动化峰值 %.4f\n",peak);check(valid&&peak<1,"模式切换、连续自动化与非法输入安全");
    std::printf("房间契约：%d项失败\n",failures);return failures?1:0;
}
