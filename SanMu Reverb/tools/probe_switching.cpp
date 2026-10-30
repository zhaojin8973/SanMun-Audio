#include "HallMeasurement.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <thread>

// 逐块记录原始样本与耗时，记录写盘全部发生在处理结束后。
int main(int argc,char** argv)
{
    if(argc<2)return 2;
    juce::ScopedNoDenormals guard;
    const auto directory=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    if(directory.exists())return 2;
    directory.createDirectory();
    constexpr double seconds=12;
    for(double rate:{48000.,96000.})for(int blockSize:{32,128,512})
    {
        ReverbProcessor processor;processor.prepare(rate,blockSize);
        const int count=static_cast<int>(seconds*rate)/blockSize;
        juce::AudioBuffer<float> audio(2,count*blockSize);audio.clear();
        struct Row{double microseconds;float peak;int category,mode;};
        std::vector<Row> rows(static_cast<std::size_t>(count));
        uint32_t random=33;
        for(int n=0;n<audio.getNumSamples();++n)
        {
            const double t=n/rate,hit=std::fmod(t,.37);
            random^=random<<13;random^=random>>17;random^=random<<5;
            const double noise=static_cast<double>(random>>8)/8388608.-1.;
            const float x=static_cast<float>((noise*.5*std::exp(-hit/.025)+.25*std::sin(2*3.141592653589793*185*hit)*std::exp(-hit/.075))*std::min(1.,hit/.0005));
            audio.setSample(0,n,t<9?x:0);audio.setSample(1,n,t<9?x:0);
        }
        for(int b=0;b<count;++b)
        {
            const double t=b*blockSize/rate;
            auto p=hall_measurement::baseline();p.decay=3.99f;p.predelay=20;p.highShelf=-24;p.bassMult=1.5f;p.attack=50;p.modRate=2.53f;p.modDepth=38;p.lowCut=10;p.highCut=8000;
            constexpr std::array<std::array<int,2>,8> states{{{0,0},{0,1},{1,0},{1,1},{2,0},{2,2},{3,0},{3,1}}};
            const int step=static_cast<int>(t/(t<4?.43:t<8?.037:.61));
            const auto state=states[static_cast<std::size_t>(step)%states.size()];p.category=state[0];p.mode=state[1];
            juce::AudioBuffer<float> slice(audio.getArrayOfWritePointers(),2,b*blockSize,blockSize);
            const auto start=std::chrono::steady_clock::now();processor.process(slice,p);
            rows[static_cast<std::size_t>(b)]={std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count(),slice.getMagnitude(0,blockSize),p.category,p.mode};
        }
        const auto stem=std::to_string(static_cast<int>(rate))+"_"+std::to_string(blockSize);
        std::ofstream csv(directory.getChildFile(stem+".csv").getFullPathName().toStdString());
        csv<<"block,microseconds,peak,category,mode\n"<<std::setprecision(10);
        for(std::size_t i=0;i<rows.size();++i){const auto& r=rows[i];csv<<i<<','<<r.microseconds<<','<<r.peak<<','<<r.category<<','<<r.mode<<'\n';}
        std::ofstream raw(directory.getChildFile(stem+".f32").getFullPathName().toStdString(),std::ios::binary);
        for(int c=0;c<2;++c)raw.write(reinterpret_cast<const char*>(audio.getReadPointer(c)),audio.getNumSamples()*static_cast<int>(sizeof(float)));
        std::printf("完成 %.0fHz / %d采样\n",rate,blockSize);
    }
    // 不经过调度器的单独清空成本，区分音频运算与大块内存写入。
    std::ofstream resets(directory.getChildFile("reset.csv").getFullPathName().toStdString());resets<<"category,microseconds\n";
    for(int category:{0,1,2,3})
    {
        std::unique_ptr<dsp_engines::ReverbEngine> engine=category==0
            ?std::unique_ptr<dsp_engines::ReverbEngine>(new dsp_engines::PlateEngine)
            :category==1?std::unique_ptr<dsp_engines::ReverbEngine>(new dsp_engines::RoomEngine)
            :category==2?std::unique_ptr<dsp_engines::ReverbEngine>(new dsp_engines::HallEngine)
            :std::unique_ptr<dsp_engines::ReverbEngine>(new dsp_engines::ChamberEngine);
        engine->prepare(96000,32);
        for(int i=0;i<256;++i){const auto start=std::chrono::steady_clock::now();engine->reset();resets<<category<<','<<std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()<<'\n';}
    }

    // 在无新输入的尾音中切换，并在参数自动化中切换，隔离切换本身的瞬态。
    {
        constexpr double rate=48000; constexpr int block=32; constexpr int total=rate*3;
        juce::AudioBuffer<float> audio(2,total); audio.clear();
        for(int c=0;c<2;++c){audio.setSample(c,rate,1.0f);audio.setSample(c,rate+1,-.7f);}
        ReverbProcessor processor;processor.prepare(rate,block);
        std::ofstream events(directory.getChildFile("transition_events.csv").getFullPathName().toStdString());
        events<<"sample,event,boundary,max_window,max_value\n";
        constexpr std::array<std::array<int,2>,8> states{{{0,0},{0,1},{1,0},{1,1},{2,0},{2,2},{3,0},{3,1}}};
        int oldCategory=0,oldMode=0;std::array<float,2> previous{};
        for(int pos=0;pos<total;pos+=block)
        {
            const double t=pos/rate;
            auto p=hall_measurement::baseline();p.mix=100;p.decay=3.99f;p.predelay=20;p.attack=50;p.earlyDiff=100;p.lateDiff=100;p.modRate=2.53f;p.modDepth=38;p.highCut=8000;p.lowCut=10;
            const int index=t<1.10?0:std::min(7,1+static_cast<int>((t-1.10)/.24));
            p.category=states[static_cast<std::size_t>(index)][0];p.mode=states[static_cast<std::size_t>(index)][1];
            juce::AudioBuffer<float> slice(audio.getArrayOfWritePointers(),2,pos,block);
            processor.process(slice,p);
            const float boundary=std::max(std::abs(slice.getSample(0,0)-previous[0]),std::abs(slice.getSample(1,0)-previous[1]));
            const float window=std::max(slice.getMagnitude(0,block),slice.getMagnitude(1,block));
            if(p.category!=oldCategory||p.mode!=oldMode)
                events<<pos<<","<<oldCategory<<"/"<<oldMode<<"->"<<p.category<<"/"<<p.mode<<","<<boundary<<","<<window<<","<<std::max(boundary,window)<<"\n";
            previous={slice.getSample(0,block-1),slice.getSample(1,block-1)};oldCategory=p.category;oldMode=p.mode;
        }
    }

    {
        constexpr double rate=48000; constexpr int block=32; constexpr int total=rate*2;
        juce::AudioBuffer<float> audio(2,total); audio.clear();
        for(int n=0;n<total;++n){const double t=n/rate;const double h=std::fmod(t,.31);const float x=static_cast<float>(.35*std::exp(-h/.035)+.12*std::sin(2*3.141592653589793*190*h)*std::exp(-h/.09));audio.setSample(0,n,x);audio.setSample(1,n,x);}
        ReverbProcessor processor;processor.prepare(rate,block);
        std::ofstream events(directory.getChildFile("ui_intermediate_events.csv").getFullPathName().toStdString());
        events<<"sample,event,boundary,max_window\n";
        int state=0;std::array<float,2> previous{};
        for(int pos=0;pos<total;pos+=block)
        {
            const double t=pos/rate;auto p=hall_measurement::baseline();p.mix=100;p.decay=3.99f;p.predelay=20;p.attack=50;p.earlyDiff=100;p.lateDiff=100;p.modRate=2.53f;p.modDepth=38;p.highCut=8000;p.lowCut=10;
            // 从Live Room回到Plate时，模拟界面先写类别、下一块才写Mode 0。
            if(t<.70){p.category=1;p.mode=1;}
            else if(t<.70+block/rate){p.category=0;p.mode=1;}
            else if(t<.70+2*block/rate){p.category=0;p.mode=0;}
            else {p.category=0;p.mode=0;}
            int next=(t<.70)?0:(t<.70+block/rate?1:(t<.70+2*block/rate?2:3));
            juce::AudioBuffer<float> slice(audio.getArrayOfWritePointers(),2,pos,block);processor.process(slice,p);
            float boundary=std::max(std::abs(slice.getSample(0,0)-previous[0]),std::abs(slice.getSample(1,0)-previous[1]));
            if(next!=state)events<<pos<<","<<state<<"->"<<next<<","<<boundary<<","<<std::max(slice.getMagnitude(0,block),slice.getMagnitude(1,block))<<"\n";
            previous={slice.getSample(0,block-1),slice.getSample(1,block-1)};state=next;
        }
    }
}
