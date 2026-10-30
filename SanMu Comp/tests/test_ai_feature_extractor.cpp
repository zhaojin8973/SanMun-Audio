// ==============================================================================
// test_ai_feature_extractor.cpp - AI 声学多模态特征提取与 JSON 序列化单元测试
// ==============================================================================

#include <iostream>
#include <cmath>
#include <cassert>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/AcousticFeatureExtractor.h"
#include "ai/LlmClient.h"

static void generateSineWave(juce::AudioBuffer<float>& buffer, float frequency, float amplitude, double sampleRate)
{
    const float phaseIncrement = 2.0f * juce::MathConstants<float>::pi * frequency / static_cast<float>(sampleRate);
    float currentPhase = 0.0f;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const float val = amplitude * std::sin(currentPhase);
        currentPhase += phaseIncrement;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, sample, val);
    }
}

// 等待门限只控制素材采集，不参与声音处理。
static bool verifySignalWait()
{
    ai::AcousticFeatureExtractor e;
    const auto check=[](bool ok,const char* message){if(!ok)std::cerr<<message<<'\n';return ok;};
    for(const double rate:{44100.0,48000.0,96000.0,192000.0})for(const int channels:{1,2})
    {
        e.prepare(rate,256);e.startListening(5.0,true);
        juce::AudioBuffer<float> silent(channels,256);silent.clear();
        for(int i=0;i<int(rate*6/256)+1;++i)e.processBlock(silent);
        if(!check(e.isWaitingForSignal()&&e.getRecordedSeconds()==0,"静音消耗了等待阶段"))return false;
        // 非有限值及 -80 dBFS 底噪均不能触发。
        silent.applyGain(0);silent.setSample(0,0,std::numeric_limits<float>::quiet_NaN());
        silent.setSample(0,1,std::numeric_limits<float>::infinity());silent.setSample(0,2,.0001f);e.processBlock(silent);
        if(!check(e.isWaitingForSignal(),"异常值或底噪触发了采集"))return false;
        silent.clear();silent.setSample(channels-1,32,.1f);e.processBlock(silent);
        if(!check(!e.isWaitingForSignal()&&std::abs(e.getRecordedSeconds()*rate-224)<.01,"首个有效输入未按采样点触发"))return false;
        // 触发后的短暂停顿仍计入连续的五秒片段。
        silent.clear();for(int i=0;i<int(rate*5/256)+1;++i)e.processBlock(silent);
        if(!check(!e.isListening()&&e.copyCapture().mid.size()==size_t(rate*5),"未完整采集五秒素材"))return false;
        e.cancelListening();e.startListening(5,true);e.cancelListening();
        silent.setSample(0,0,.1f);e.processBlock(silent);
        if(!check(!e.isListening()&&!e.isWaitingForSignal()&&e.getRecordedSeconds()==0,"取消后仍然采集"))return false;
        e.startListening(5,true);e.reset();if(!check(!e.isWaitingForSignal(),"重置未取消等待"))return false;
    }
    e.prepare(48000,256);e.startListening(5,true);juce::AudioBuffer<float> stereo(2,256);
    for(int n=0;n<256;++n){stereo.setSample(0,n,.1f);stereo.setSample(1,n,-.1f);}e.processBlock(stereo);
    if(!check(!e.isWaitingForSignal()&&e.copyCapture().side.front()>.09f,"反相立体声漏检"))return false;
    e.cancelListening();std::cout<<"等待输入、异常值、左右声道、反相、四采样率、五秒计时和取消：通过\n";return true;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiser;

    std::cout << "==================================================" << std::endl;
    std::cout << "   SanMu Copilot AI Acoustic Extractor Tests      " << std::endl;
    std::cout << "==================================================" << std::endl;

    if(!verifySignalWait())return 1;

    // 1. 初始化提取器
    ai::AcousticFeatureExtractor extractor;
    extractor.prepare(48000.0, 512);

    // 2. 生成 3 秒 -12dBFS 1000Hz 测试信号
    juce::AudioBuffer<float> buffer(2, 48000 * 3);
    const float amp = std::pow(10.0f, -12.0f / 20.0f);
    generateSineWave(buffer, 1000.0f, amp, 48000.0);

    bool callbackTriggered = false;
    bool checksPassed = true;
    extractor.onAnalysisCompleted = [&](const ai::AcousticFeatures& feat) {
        callbackTriggered = true;
        std::cout << "  [Callback] Peak: " << feat.peakDb << " dBFS, RMS: " << feat.rmsDb 
                  << " dBFS, Crest Factor: " << feat.crestFactorDb << " dB, BPM: " << feat.bpm << std::endl;

        // 验证正弦波波峰因数理论值约为 3.01 dB
        if (std::abs(feat.crestFactorDb - 3.01f) > 1.0f)
        {
            checksPassed = false;
            std::cerr << "  FAIL: Crest factor deviation too large for sine wave!" << std::endl;
        }

        // 验证 JSON 序列化合法性
        juce::String jsonStr = feat.toJsonString("测试人声需求");
        std::cout << "  [JSON Output]:\n" << jsonStr << std::endl;

        auto parsed = juce::JSON::parse(jsonStr);
        if (!parsed.isObject())
        {
            checksPassed = false;
            std::cerr << "  FAIL: JSON serialization produced invalid JSON!" << std::endl;
        }
    };

    // 3. 启动监听并灌入音频块
    extractor.startListening(3.0);
    const int blockSize = 512;
    for (int i = 0; i < buffer.getNumSamples(); i += blockSize)
    {
        const int len = std::min(blockSize, buffer.getNumSamples() - i);
        juce::AudioBuffer<float> block(buffer.getArrayOfWritePointers(), 2, i, len);
        extractor.processBlock(block, 128.0, 4, 4);
    }

    // 直接执行声学特征计算
    extractor.computeFeatures();

    std::cout << "==================================================" << std::endl;
    if (callbackTriggered && checksPassed)
    {
        std::cout << ">>> ALL AI ACOUSTIC EXTRACTOR TESTS PASSED! <<<" << std::endl;
        return 0;
    }
    else
    {
        std::cerr << ">>> AI EXTRACTOR CALLBACK WAS NOT TRIGGERED! <<<" << std::endl;
        return 1;
    }
}
