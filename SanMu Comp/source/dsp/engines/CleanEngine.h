#pragma once

#include "CompressorEngine.h"
#include "../../params/Parameters.h"

namespace dsp_engines
{

/**
 * 现代纯数字透明母带级压缩器引擎
 * - 前馈对数域检测 + 连续可变软拐点（Knee 0~24dB）
 * - 一阶球状平滑（Ballistics），THD < 0.0001% 零失真透明
 */
class CleanEngine : public CompressorEngine
{
public:
    CleanEngine();
    ~CleanEngine() override = default;

    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;
    float getCurrentGrDb() const noexcept override;

    // 参数绑定与更新（原子无锁读取）
    std::atomic<float> thresholdDb        { Params::Defaults::THRESHOLD };
    std::atomic<float> ratio              { Params::Defaults::RATIO };
    std::atomic<float> kneeDb             { Params::Defaults::KNEE };
    std::atomic<float> attackMs           { Params::Defaults::ATTACK };
    std::atomic<float> releaseMs          { Params::Defaults::RELEASE };
    std::atomic<float> makeupDb           { Params::Defaults::MAKEUP };
    std::atomic<float> outputTrimDb       { Params::Defaults::OUTPUT_TRIM };
    std::atomic<float> mixPercent         { Params::Defaults::MIX };
    std::atomic<float> stereoLink         { Params::Defaults::STEREO_LINK };
    std::atomic<float> sidechainHpHz      { Params::Defaults::SIDECHAIN_HP };
    std::atomic<bool>  sidechainEnabled   { false };

private:
    static float computeCleanGrDb(float levelDb, float threshDb, float ratioVal, float kneeWidthDb) noexcept;

    double sampleRate { 48000.0 };

    // 引擎完全私有状态（杜绝跨模式干扰）
    std::array<float, 2> scHpS1 { 0.0f, 0.0f };
    std::array<float, 2> scHpS2 { 0.0f, 0.0f };
    std::array<float, 2> cleanEnvDb { 0.0f, 0.0f };

    std::atomic<float> currentGrDb { 0.0f };
};

} // namespace dsp_engines
