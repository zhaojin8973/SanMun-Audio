// 通过公开 AU 接口验证已安装的 SanMu Comp；不以源码中的延迟常量代替实际渲染。
#import <AudioToolbox/AudioToolbox.h>
#import <Foundation/Foundation.h>
#include <cmath>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

static void check(OSStatus result, const char* operation)
{
    if (result) throw std::runtime_error(std::string(operation) + "：" + std::to_string(result));
}

static float signal(int sample)
{
    return float(.17 * std::sin(.23 * sample) + (sample == 13 ? .4 : 0));
}

static OSStatus renderInput(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp* time,
                            UInt32, UInt32 count, AudioBufferList* buffers)
{
    for (UInt32 c = 0; c < buffers->mNumberBuffers; ++c)
        for (UInt32 n = 0; n < count; ++n)
            static_cast<float*>(buffers->mBuffers[c].mData)[n] = signal(int(time->mSampleTime) + int(n)) * (c ? -1 : 1);
    return noErr;
}

int main()
{
    @autoreleasepool {
        AudioUnit unit = nullptr;
        try {
            AudioComponentDescription description {'aufx', 'SMC1', 'SmAu', 0, 0};
            auto component = AudioComponentFindNext(nullptr, &description);
            if (!component) throw std::runtime_error("未找到已安装的 SanMu Comp AU");
            UInt32 version = 0;
            check(AudioComponentGetVersion(component, &version), "版本读取");
            std::printf("已安装 AU 版本：%u.%u.%u\n", version >> 16, (version >> 8) & 255, version & 255);
            if (version != 0x00000109) throw std::runtime_error("加载版本不是本轮 0.1.9");
            check(AudioComponentInstanceNew(component, &unit), "创建实例");
            UInt32 bytes = 0;
            check(AudioUnitGetPropertyInfo(unit, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global, 0, &bytes, nullptr), "参数列表长度");
            std::vector<AudioUnitParameterID> ids(bytes / sizeof(AudioUnitParameterID));
            check(AudioUnitGetProperty(unit, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global, 0, ids.data(), &bytes), "参数列表");
            std::map<std::string, AudioUnitParameterID> parameters;
            for (auto id : ids)
            {
                AudioUnitParameterInfo info {}; bytes = sizeof(info);
                check(AudioUnitGetProperty(unit, kAudioUnitProperty_ParameterInfo, kAudioUnitScope_Global, id, &info, &bytes), "参数名称");
                char name[256] {};
                if (info.cfNameString) CFStringGetCString(info.cfNameString, name, sizeof(name), kCFStringEncodingUTF8);
                else std::snprintf(name, sizeof(name), "%s", info.name);
                parameters[name] = id;
                if ((info.flags & kAudioUnitParameterFlag_CFNameRelease) && info.cfNameString) CFRelease(info.cfNameString);
            }
            // 经 AU 的公开文字转换接口换算，避免混淆界面分贝和 AU 的归一化参数。
            auto set = [&](const char* name, const char* value) {
                auto text = CFStringCreateWithCString(nullptr, value, kCFStringEncodingUTF8);
                AudioUnitParameterValueFromString conversion {parameters.at(name), text, 0};
                UInt32 size = sizeof(conversion);
                const auto status = AudioUnitGetProperty(unit, kAudioUnitProperty_ParameterValueFromString,
                    kAudioUnitScope_Global, 0, &conversion, &size);
                CFRelease(text);
                check(status, "界面参数转换");
                check(AudioUnitSetParameter(unit, parameters.at(name), kAudioUnitScope_Global, 0, conversion.outValue, 0), name);
                if (std::string(name) == "Compressor Mode")
                {
                    Float32 actual = -1;
                    check(AudioUnitGetParameter(unit, parameters.at(name), kAudioUnitScope_Global, 0, &actual), "模式回读");
                    AudioUnitParameterStringFromValue formatted {parameters.at(name), &actual, nullptr};
                    size = sizeof(formatted);
                    check(AudioUnitGetProperty(unit, kAudioUnitProperty_ParameterStringFromValue,
                        kAudioUnitScope_Global, 0, &formatted, &size), "模式名称回读");
                    char result[128] {};
                    if (formatted.outString)
                    {
                        CFStringGetCString(formatted.outString, result, sizeof(result), kCFStringEncodingUTF8);
                        CFRelease(formatted.outString);
                    }
                    if (std::string(result) != value) throw std::runtime_error("AU 模式名称转换不一致");
                }
            };
            for (double sr : {44100., 48000., 96000., 192000.})
            for (bool smooth : {false, true})
            {
                AudioStreamBasicDescription format {sr, kAudioFormatLinearPCM,
                    kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved,
                    4, 1, 4, 2, 32, 0};
                check(AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, sizeof(format)), "输入格式");
                check(AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &format, sizeof(format)), "输出格式");
                const int delay = smooth ? int(std::lround(sr * .012)) : 0;
                UInt32 frames = UInt32(std::max(257, delay + 193));
                check(AudioUnitSetProperty(unit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &frames, sizeof(frames)), "块长");
                AURenderCallbackStruct callback {renderInput, nullptr};
                check(AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callback, sizeof(callback)), "输入回调");
                set("VOX Comp", "0"); set("VOX Gate", "-80"); set("VOX Gain", "0"); set("VOX Mix", "100");
                set("Compressor Mode", "VOX");
                set("LEV Smooth", smooth ? "On" : "Off");
                check(AudioUnitInitialize(unit), "初始化");
                std::vector<float> left(frames), right(frames);
                struct { UInt32 count; AudioBuffer buffers[2]; } storage {2, {{1, frames * 4, left.data()}, {1, frames * 4, right.data()}}};
                int cursor = 0;
                for (int mode : {0, 1, 2, 3, 0})
                {
                    const char* names[] = {"VOX", "VCA", "FET", "Opto"};
                    set("Compressor Mode", names[mode]);
                    AudioTimeStamp time {}; time.mSampleTime = cursor; time.mFlags = kAudioTimeStampSampleTimeValid;
                    AudioUnitRenderActionFlags flags = 0;
                    check(AudioUnitRender(unit, &flags, &time, 0, frames, reinterpret_cast<AudioBufferList*>(&storage)), "音频渲染");
                    Float64 latency = -1; bytes = sizeof(latency);
                    check(AudioUnitGetProperty(unit, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &latency, &bytes), "宿主延迟属性");
                    const double expectedLatency = mode == 0 ? delay / sr : 0;
                    if (std::abs(latency - expectedLatency) > 1e-9) throw std::runtime_error("AU 延迟与当前模式不一致");
                    if (mode == 0)
                        for (UInt32 n = 0; n < frames; ++n)
                        {
                            const float x = int(n) < delay ? 0 : signal(cursor + int(n) - delay);
                            if (left[n] != x || right[n] != -x)
                            {
                                std::fprintf(stderr, "采样率 %.0f，时间 %.0f，采样 %u：期望 %.9g，输出 %.9g / %.9g\n", sr, time.mSampleTime, n, x, left[n], right[n]);
                                for (const char* name : {"Compressor Mode", "VOX Comp", "VOX Gate", "VOX Gain", "VOX Mix", "LEV Smooth"})
                                {
                                    Float32 value = 0;
                                    check(AudioUnitGetParameter(unit, parameters.at(name), kAudioUnitScope_Global, 0, &value), "失败时参数核对");
                                    std::fprintf(stderr, "%s = %g\n", name, value);
                                }
                                throw std::runtime_error("LEV 实际输出未按声明延迟对齐");
                            }
                        }
                    cursor += int(frames);
                }
                // 不重新初始化便切换工作方式，检查公开延迟属性及短过渡结束后的音频。
                set("LEV Smooth", smooth ? "Off" : "On");
                const int changedDelay = smooth ? 0 : int(std::lround(sr * .012));
                for (int block = 0; block < 12; ++block)
                {
                    AudioTimeStamp time {}; time.mSampleTime = cursor; time.mFlags = kAudioTimeStampSampleTimeValid;
                    AudioUnitRenderActionFlags flags = 0;
                    check(AudioUnitRender(unit, &flags, &time, 0, frames, reinterpret_cast<AudioBufferList*>(&storage)), "开关切换渲染");
                    Float64 latency = -1; bytes = sizeof(latency);
                    check(AudioUnitGetProperty(unit, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &latency, &bytes), "切换延迟属性");
                    if (std::abs(latency - changedDelay / sr) > 1e-9) throw std::runtime_error("AU 开关切换未更新延迟");
                    if (block == 11)
                        for (UInt32 n = 0; n < frames; ++n)
                            if (left[n] != signal(cursor + int(n) - changedDelay)) throw std::runtime_error("AU 切换后音频未与延迟对齐");
                    cursor += int(frames);
                }
                check(AudioUnitUninitialize(unit), "结束当前采样率");
                std::printf("%.0f Hz，平滑%s：LEV 延迟 %d 采样；模式路由、实际音频及运行中切换通过\n", sr, smooth ? "开" : "关", delay);
            }
            AudioComponentInstanceDispose(unit);
            std::puts("已安装 AU 零延迟与平滑模式检查通过；不代替 DAW 试听验收。");
        } catch (const std::exception& error) {
            if (unit) AudioComponentInstanceDispose(unit);
            std::fprintf(stderr, "失败：%s\n", error.what());
            return 1;
        }
    }
}
