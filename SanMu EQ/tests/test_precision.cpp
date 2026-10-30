#include "dsp/PrecisionLinearEq.h"
#include "dsp/LinearMagnitude.h"
#include <iostream>
#include <chrono>
struct Shape { double frequency=20, q=40, gain=12; LinearMagnitude response; };
static void sample(float f,double,float m[4],void* c)
{
    auto& s=*static_cast<Shape*>(c); m[0]=m[3]=static_cast<float>(std::sqrt(s.response.power(double(f)*f))); m[1]=m[2]=0;
}
int main(int argc,char** argv)
{
    const bool audio=argc>1 && juce::String(argv[1])=="--audio";
    bool pass=true;
    for(double sr:{44100.,48000.,96000.,192000.}) for(int quality=0;quality<3;++quality)
    {
        PrecisionLinearEq engine; engine.prepare(sr,quality);
        if(audio)
        {
            if(sr!=48000 || quality!=2) continue;
            Shape shape; shape.response.prepare(TptSvf::Type::Bell,20,40,12); engine.rebuild(sample,&shape);
            const int total=engine.getLatencySamples()*2+int(sr*4);
            juce::AudioBuffer<float> block(2,257);
            double dot=0,ref=0,imag=0; double maximumMs=0;
            const auto start=std::chrono::steady_clock::now();
            for(int at=0;at<total;at+=257)
            {
                for(int i=0;i<257;++i){float x=float(.1*std::sin(2*juce::MathConstants<double>::pi*20*(at+i)/sr));block.setSample(0,i,x);block.setSample(1,i,0);}
                const auto t=std::chrono::steady_clock::now(); engine.processBlock(block,false);
                maximumMs=std::max(maximumMs,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count());
                if(at>total-int(sr*2))for(int i=0;i<257;++i)
                {
                    double p=2*juce::MathConstants<double>::pi*20*(at+i-engine.getLatencySamples())/sr,x=.1*std::sin(p);
                    dot+=block.getSample(0,i)*x; ref+=x*x; imag+=block.getSample(0,i)*.1*std::cos(p);
                    pass=pass&&std::abs(block.getSample(1,i))<1e-7;
                }
            }
            double db=20*std::log10(std::hypot(dot,imag)/ref);
            std::cout<<"真实音频 "<<db<<" dB 相位误差 "<<std::atan2(imag,dot)<<" 延迟 "<<engine.getLatencySamples()<<" 最大回调毫秒 "<<maximumMs<<" 总秒数 "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<std::endl;
            pass=pass&&std::abs(db-12)<.03&&std::abs(std::atan2(imag,dot))<.003;
            continue;
        }
        for(double fc:{20.,40.,80.,1500.})for(double q:{1.,4.,40.})for(double gain:{-24.,12.,24.})
        {
            Shape s{fc,q,gain,{}};s.response.prepare(TptSvf::Type::Bell,fc,q,gain);engine.rebuild(sample,&s);
            double worst=0;
            for(int i=0;i<=40;++i)
            {
                double f=fc*std::pow(2.,(i-20)*.1/q),h=engine.response(f,0),target=std::sqrt(s.response.power(f*f));
                worst=std::max(worst,std::abs(20*std::log10(std::max(1e-14,h)/target)));
            }
            std::cout<<"档位 "<<quality<<" 采样率 "<<sr<<" 频率 "<<fc<<" Q "<<q<<" 增益 "<<gain<<" 中心 "<<20*std::log10(std::max(1e-14,engine.response(fc,0)))<<" 最大误差 "<<worst<<" 延迟 "<<engine.getLatencySamples()<<std::endl;
            if(quality==2)pass=pass&&worst<.1;
            if(quality==0 && q==1 && gain==12)pass=pass&&worst<.05;
        }
    }
    std::cout<<(pass?"精度验证通过":"精度验证失败")<<std::endl; return pass?0:1;
}
