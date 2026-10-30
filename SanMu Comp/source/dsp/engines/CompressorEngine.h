#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>
#include <array>
#include <atomic>

namespace dsp_engines
{

/**
 * 模块化压缩引擎抽象基类
 */
class CompressorEngine
{
public:
    virtual ~CompressorEngine() = default;

    virtual void prepare(const juce::dsp::ProcessSpec& spec) = 0;
    virtual void reset() = 0;
    virtual void process(juce::AudioBuffer<float>& buffer) = 0;
    virtual float getCurrentGrDb() const noexcept = 0;
};

} // namespace dsp_engines
