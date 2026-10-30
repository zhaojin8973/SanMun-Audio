// 通过公开 Audio Unit 接口测量本机已安装 RVox；不读取插件内部实现。
#import <AudioToolbox/AudioToolbox.h>
#import <Foundation/Foundation.h>
#include <cmath>
#include <cstdio>
#include <vector>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include "../tests/LevProbeSignal.h"

struct Signal { double amplitude = 0.1; int scenario = -1; };

static void check(OSStatus status, const char* where)
{ if (status) throw std::runtime_error(std::string(where) + ": " + std::to_string(status)); }
static OSStatus input(void* context, AudioUnitRenderActionFlags*, const AudioTimeStamp* time,
                      UInt32, UInt32 count, AudioBufferList* buffers)
{
    const auto signal = *static_cast<Signal*>(context);
    for (UInt32 c = 0; c < buffers->mNumberBuffers; ++c)
    {
        auto* p = static_cast<float*>(buffers->mBuffers[c].mData);
        for (UInt32 n = 0; n < count; ++n)
            p[n] = float(signal.scenario < 0
                ? signal.amplitude * std::sin(2 * M_PI * 1000 * (time->mSampleTime + n) / 48000)
                : lev_probe::sample((time->mSampleTime + n) / 48000, signal.scenario));
    }
    return noErr;
}
int main(int argc, char** argv)
{
    @autoreleasepool {
    try {
        AudioComponentDescription desc {'aufx','MXVM','ksWV',0,0};
        AudioComponent component = AudioComponentFindNext(nullptr, &desc);
        if (!component) throw std::runtime_error("未找到 RVox 单声道 AU");
        AudioUnit unit = nullptr;
        check(AudioComponentInstanceNew(component, &unit), "创建实例");
        AudioStreamBasicDescription format {48000,kAudioFormatLinearPCM,
            kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved,
            4,1,4,1,32,0};
        check(AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input,0,&format,sizeof(format)),"输入格式");
        check(AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output,0,&format,sizeof(format)),"输出格式");
        UInt32 frames = 256;
        check(AudioUnitSetProperty(unit,kAudioUnitProperty_MaximumFramesPerSlice,kAudioUnitScope_Global,0,&frames,sizeof(frames)),"块长");
        Signal signal;
        AURenderCallbackStruct callback {input,&signal};
        check(AudioUnitSetProperty(unit,kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input,0,&callback,sizeof(callback)),"输入回调");
        check(AudioUnitInitialize(unit),"初始化");
        UInt32 bytes = 0;
        check(AudioUnitGetPropertyInfo(unit,kAudioUnitProperty_ParameterList,kAudioUnitScope_Global,0,&bytes,nullptr),"参数清单");
        std::vector<AudioUnitParameterID> ids(bytes/sizeof(AudioUnitParameterID));
        check(AudioUnitGetProperty(unit,kAudioUnitProperty_ParameterList,kAudioUnitScope_Global,0,ids.data(),&bytes),"参数读取");
        ids.insert(ids.begin(),0);
        for (auto id : ids)
        {
            AudioUnitParameterInfo info {}; bytes = sizeof(info);
            check(AudioUnitGetProperty(unit,kAudioUnitProperty_ParameterInfo,kAudioUnitScope_Global,id,&info,&bytes),"参数信息");
            char name[256] {};
            if (info.cfNameString) CFStringGetCString(info.cfNameString,name,sizeof(name),kCFStringEncodingUTF8);
            else std::snprintf(name,sizeof(name),"%s",info.name);
            std::printf("参数 %u %s 范围 %g %g 默认 %g 标记 %x\n",id,name,info.minValue,info.maxValue,info.defaultValue,unsigned(info.flags));
            for(float val : {0.f,250.f,500.f,750.f,1000.f}) {
                AudioUnitParameterStringFromValue text {id,&val,nullptr}; bytes=sizeof(text);
                const auto status=AudioUnitGetProperty(unit,kAudioUnitProperty_ParameterStringFromValue,kAudioUnitScope_Global,0,&text,&bytes);
                if (!status && text.outString) { char s[128]{};CFStringGetCString(text.outString,s,sizeof(s),kCFStringEncodingUTF8);std::printf("  %g => %s\n",val,s);CFRelease(text.outString); }
            }
        }
        if (argc < 2) { AudioComponentInstanceDispose(unit); return 0; }
        if (argc == 3 && std::string(argv[1]) == "--dynamics")
        {
            const std::filesystem::path directory(argv[2]);
            std::filesystem::create_directories(directory);
            Float64 latency = 0; bytes = sizeof(latency);
            check(AudioUnitGetProperty(unit,kAudioUnitProperty_Latency,kAudioUnitScope_Global,0,&latency,&bytes),"延迟");
            std::ofstream metadata(directory / "rvox-latency.txt"); metadata << latency << '\n';
            std::vector<float> audio(frames);
            AudioBufferList buffers {1,{{1,frames*4,audio.data()}}};
            for (int scenario = 0; scenario < 5; ++scenario)
            for (int cv : {0,-20,-30})
            for (int gv : {0,-12,-30})
            {
                signal.scenario = scenario;
                check(AudioUnitSetParameter(unit,1,kAudioUnitScope_Global,0,0,0),"门限");
                check(AudioUnitSetParameter(unit,0,kAudioUnitScope_Global,0,float((cv+36)/36.0*1000),0),"压缩");
                check(AudioUnitSetParameter(unit,2,kAudioUnitScope_Global,0,float((gv+36)/36.0*1000),0),"输出");
                CFRunLoopRunInMode(kCFRunLoopDefaultMode,0.05,false);
                check(AudioUnitReset(unit,kAudioUnitScope_Global,0),"复位");
                std::ofstream output(directory / ("rvox_"+std::to_string(scenario)+"_"+std::to_string(cv)+"_"+std::to_string(gv)+".f32"),std::ios::binary);
                for(int offset=0;offset<48000*4;offset+=int(frames))
                {
                    AudioTimeStamp time{};time.mSampleTime=offset;time.mFlags=kAudioTimeStampSampleTimeValid;
                    AudioUnitRenderActionFlags flags=0;buffers.mBuffers[0].mDataByteSize=frames*4;
                    check(AudioUnitRender(unit,&flags,&time,0,frames,&buffers),"动态渲染");
                    if(offset%8192==0) CFRunLoopRunInMode(kCFRunLoopDefaultMode,.001,false);
                    output.write(reinterpret_cast<const char*>(audio.data()),frames*sizeof(float));
                }
            }
            AudioComponentInstanceDispose(unit);std::puts("RVox 动态测量完成");return 0;
        }
        // 参数编号由上一次读取的公开参数清单提供，不猜测内部编号。
        if (argc < 5) throw std::runtime_error("需要 CSV 路径和 Gate/Comp/Gain 参数编号");
        const auto gate = UInt32(std::stoul(argv[2])), comp = UInt32(std::stoul(argv[3])), output = UInt32(std::stoul(argv[4]));
        FILE* file = std::fopen(argv[1],"w");
        if (!file) throw std::runtime_error("无法创建测量文件");
        std::fprintf(file,"input_peak_db,comp,gain,output_rms_db,output_peak_db,gr_db\n");
        std::vector<float> audio(frames);
        AudioBufferList buffers {1,{{1,frames*4,audio.data()}}};
        for (double in : {-36.,-30.,-24.,-18.,-12.,-6.})
        for (double cv : {0.,-6.,-12.,-18.,-20.,-24.,-30.,-36.})
        {
            signal.amplitude = std::pow(10.,in/20);
            check(AudioUnitSetParameter(unit,gate,kAudioUnitScope_Global,0,0,0),"门限");
            check(AudioUnitSetParameter(unit,comp,kAudioUnitScope_Global,0,float((cv+36)/36*1000),0),"压缩");
            check(AudioUnitSetParameter(unit,output,kAudioUnitScope_Global,0,float(6.0/36*1000),0),"输出");
            // Waves AU 的控制消息经主线程派发；等待公开参数更新完成后再测量。
            CFRunLoopRunInMode(kCFRunLoopDefaultMode,0.05,false);
            check(AudioUnitReset(unit,kAudioUnitScope_Global,0),"复位");
            double energy = 0, peak = 0, gr=0; int count = 0, blocks=0;
            for (int offset = 0; offset < 48000 * 3; offset += int(frames))
            {
                AudioTimeStamp time {}; time.mSampleTime=offset;time.mFlags=kAudioTimeStampSampleTimeValid;
                AudioUnitRenderActionFlags flags=0;
                buffers.mBuffers[0].mDataByteSize=frames*4;
                check(AudioUnitRender(unit,&flags,&time,0,frames,&buffers),"渲染");
                if(offset % 8192==0) CFRunLoopRunInMode(kCFRunLoopDefaultMode,0.001,false);
                if(offset >= 96000) for(float x:audio) { energy+=double(x)*x; peak=std::max(peak,double(std::abs(x)));++count; }
                if(offset>=96000) { Float32 value=0;check(AudioUnitGetParameter(unit,4,kAudioUnitScope_Global,0,&value),"衰减表");gr+=36-value*.036;++blocks; }
            }
            std::fprintf(file,"%g,%g,-30,%.8f,%.8f,%.8f\n",in,cv,10*std::log10(std::max(energy/count,1e-24)),20*std::log10(std::max(peak,1e-12)),gr/blocks);
        }
        std::fclose(file); AudioComponentInstanceDispose(unit);
        std::puts("RVox 公开接口测量完成");
    } catch(const std::exception& e) { std::fprintf(stderr,"失败：%s\n",e.what());return 1; }
    }
}
