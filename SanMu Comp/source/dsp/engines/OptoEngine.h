#pragma once

#include "CompressorEngine.h"
#include "../../params/Parameters.h"

namespace dsp_engines
{

/**
 * Opto 风格反馈平准器，尚无实机测量可支持精确复刻声明。
 * 快衰减状态负责起音和恢复，慢状态由持续衰减逐步充入，提供独立的恢复下限。
 * 输出补偿、直流隔离均在反馈环路之外；不附加未经校准的器件染色。
 */
class OptoEngine : public CompressorEngine
{
public:
    OptoEngine();
    ~OptoEngine() override = default;

    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;
    float getCurrentGrDb() const noexcept override;

    // 参数绑定与更新（原子无锁读取）
    std::atomic<float> optoPeakReduction  { Params::Defaults::OPTO_PEAK_REDUCTION };
    std::atomic<bool>  isLimit            { Params::Defaults::OPTO_IS_LIMIT };
    std::atomic<float> makeupDb           { Params::Defaults::OPTO_GAIN };
    std::atomic<float> outputTrimDb       { Params::Defaults::OUTPUT_TRIM };
    std::atomic<float> mixPercent         { Params::Defaults::MIX };
    std::atomic<float> stereoLink         { Params::Defaults::STEREO_LINK };
    std::atomic<float> headroomDb         { Params::Defaults::HEADROOM };
    std::atomic<float> sidechainHpHz      { Params::Defaults::SIDECHAIN_HP };
    std::atomic<bool>  sidechainEnabled   { false };

private:
    std::array<double, 2> feedbackSample {};
    double sampleRate { 48000.0 };

    std::array<double, 2> scHpS1 {}, scHpS2 {}, detectorPower {};
    std::array<double, 2> fastReduction {}, slowReduction {}, totalReduction {};
    std::array<double, 2> dcBlockX1 {}, dcBlockY1 {};
    double smoothedPr = 0, smoothedGainDb = 0, smoothedTrimDb = 0;
    double smoothedMix = 1, smoothedLink = 1, smoothedMode = 0, smoothedScMix = 0;

    std::atomic<float> currentGrDb { 0.0f };
};

} // namespace dsp_engines
