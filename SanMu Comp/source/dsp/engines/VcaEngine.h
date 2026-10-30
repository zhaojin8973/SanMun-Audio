#pragma once

#include "CompressorEngine.h"
#include "../../params/Parameters.h"

namespace dsp_engines
{

/**
 * VCA 经典模拟总线 Glue 压缩器引擎
 * - 前馈真有效值（True RMS，10ms 积分）检波
 * - OverEasy 渐进软拐点（4dB 经典过渡）
 * - THAT 2180 VCA 3 次谐波胶水感微染色
 */
class VcaEngine : public CompressorEngine
{
public:
    VcaEngine();
    ~VcaEngine() override = default;

    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;
    float getCurrentGrDb() const noexcept override;

    // 参数绑定与更新（原子无锁读取）
    std::atomic<float> thresholdDb        { Params::Defaults::THRESHOLD };
    std::atomic<float> ratio              { Params::Defaults::RATIO };
    std::atomic<float> attackMs           { Params::Defaults::ATTACK };
    std::atomic<float> releaseMs          { Params::Defaults::RELEASE };
    std::atomic<float> makeupDb           { Params::Defaults::MAKEUP };
    std::atomic<float> outputTrimDb       { Params::Defaults::OUTPUT_TRIM };
    std::atomic<float> mixPercent         { Params::Defaults::MIX };
    std::atomic<float> stereoLink         { Params::Defaults::STEREO_LINK };
    std::atomic<float> sidechainHpHz      { Params::Defaults::SIDECHAIN_HP };
    std::atomic<bool>  sidechainEnabled   { false };

private:
    static float computeVcaGrDb(float levelDb, float threshDb, float ratioVal) noexcept;

    double sampleRate { 48000.0 };

    // 引擎完全私有状态（杜绝跨模式干扰）
    std::array<float, 2> scHpS1 { 0.0f, 0.0f };
    std::array<float, 2> scHpS2 { 0.0f, 0.0f };
    std::array<float, 2> vcaRmsState { 0.0f, 0.0f };
    std::array<float, 2> vcaFastEnvDb { 0.0f, 0.0f };
    std::array<float, 2> vcaSlowEnvDb { 0.0f, 0.0f };

    std::atomic<float> currentGrDb { 0.0f };
};

} // namespace dsp_engines
