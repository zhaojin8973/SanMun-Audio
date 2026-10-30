#pragma once

#include "CompressorEngine.h"
#include "../../params/Parameters.h"

namespace dsp_engines
{

/**
 * FET 风格反馈压缩器，尚无实机传递曲线或失真测量可支持精确复刻声明。
 * 同采样隐式反馈求解：在计算当前衰减时同时满足衰减后侧链的控制方程。
 * 启动 20~800 微秒、释放控制常数 50~1100 毫秒，深压缩恢复较长；输出补偿为线性增益。
 */
class FetEngine : public CompressorEngine
{
public:
    FetEngine();
    ~FetEngine() override = default;

    void prepare(const juce::dsp::ProcessSpec& spec) override;
    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;
    float getCurrentGrDb() const noexcept override;

    // 参数绑定与更新（原子无锁读取）
    std::atomic<float> fetDrive           { Params::Defaults::FET_INPUT };
    std::atomic<int>   fetRatioIdx        { 0 };
    std::atomic<float> attackMs           { Params::Defaults::FET_ATTACK };
    std::atomic<float> releaseMs          { Params::Defaults::FET_RELEASE };
    std::atomic<float> makeupDb           { Params::Defaults::FET_OUTPUT };
    std::atomic<float> outputTrimDb       { Params::Defaults::OUTPUT_TRIM };
    std::atomic<float> headroomDb         { Params::Defaults::HEADROOM };
    std::atomic<float> mixPercent         { Params::Defaults::MIX };
    std::atomic<float> stereoLink         { Params::Defaults::STEREO_LINK };
    std::atomic<float> sidechainHpHz      { Params::Defaults::SIDECHAIN_HP };
    std::atomic<bool>  sidechainEnabled   { false };

private:
    double sampleRate { 48000.0 };

    std::array<float, 2> scHpS1 { 0.0f, 0.0f };
    std::array<float, 2> scHpS2 { 0.0f, 0.0f };
    std::array<double, 2> gainReductionDb {};

    float smoothedDriveDb { 0.0f };
    float smoothedMakeupDb { 0.0f };

    std::atomic<float> currentGrDb { 0.0f };
};

} // namespace dsp_engines
