#include "dsp/AcousticFeatureExtractor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <stdexcept>

static void check(bool value,const char* message)
{ if(!value) throw std::runtime_error(message); }

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        // 合成纯音用于核验频段积分，不能充当真实人声、乐器或母带的识别准确率。
        for(double sr : {44100.,48000.,96000.,192000.})
        for(int band=0;band<5;++band)
        {
            const double frequencies[]={40,120,900,3500,9000};
            ai::ListenCapture c;c.sampleRate=sr;c.mid.resize(size_t(sr));c.side.resize(c.mid.size());
            for(size_t n=0;n<c.mid.size();++n)
                c.side[n]=float(.25*std::sin(2*juce::MathConstants<double>::pi*frequencies[band]*double(n)/sr));
            const auto f=ai::AcousticFeatureExtractor::analyseCapture(c);
            const float energies[]={f.subEnergyPct,f.bassEnergyPct,f.midEnergyPct,f.presenceEnergyPct,f.airEnergyPct};
            check(energies[band]>97,"纯音频段积分或反相声道处理错误");
            check(std::abs(f.spectralCentroidHz-frequencies[band])<10,"谱质心频率偏差过大");
            check(!f.isHighConfidence,"把规则分数当作准确率");
        }
        ai::AcousticFeatureExtractor extractor;extractor.prepare(48000,512);extractor.startListening(20);
        juce::AudioBuffer<float> b(2,512);
        for(int offset=0;offset<960000;offset+=512)
        {
            b.clear();const int count=std::min(512,960000-offset);
            for(int n=0;n<count;++n)
            {
                if(offset+n>=288000) continue;
                const float x=float(.3*std::sin(2*juce::MathConstants<double>::pi*300*(offset+n)/48000));
                b.setSample(0,n,x);b.setSample(1,n,x);
            }
            juce::AudioBuffer<float> view(b.getArrayOfWritePointers(),2,count);extractor.processBlock(view);
        }
        extractor.computeFeatures();const auto f=extractor.getLatestFeatures();
        check(f.dutyCyclePct>98 && std::abs(f.activeSeconds-6)<.03,"尾部静音污染有效段统计");
        // 宽立体声、多频纯音依然可能只是一个合成器；不能仅凭频谱覆盖强判为整曲总线。
        ai::ListenCapture tones;tones.mid.resize(48000);tones.side.resize(48000);
        for(size_t n=0;n<tones.mid.size();++n)
        {
            const double t=double(n)/48000;
            tones.mid[n]=float(.3*std::sin(2*juce::MathConstants<double>::pi*45*t)
                              +.2*std::sin(2*juce::MathConstants<double>::pi*100*t)
                              +.25*std::sin(2*juce::MathConstants<double>::pi*800*t));
            tones.side[n]=float(.15*std::sin(2*juce::MathConstants<double>::pi*5000*t));
        }
        const auto broad=ai::AcousticFeatureExtractor::analyseCapture(tones);
        check(broad.sourceType!=ai::AcousticSourceType::MasterMixBus,"仅凭宽频纯音误判母带总线");
        check(broad.noiseBandFlatness<.01,"稀疏谱线被测成宽带噪声");
        std::cout<<"频谱积分、有效窗口、反相与宽频反例：通过\n";
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
