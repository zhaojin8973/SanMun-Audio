// ==============================================================================
// DuckingDetector.h - 侧链避让检波与增益衰减器
//
// 特性：
//   1. 支持内部干声自避让与外部宿主 Sidechain 总线路由
//   2. 5ms 极速 Attack 抓瞬态，50~2000ms 平滑球状释放
//   3. 工业级 RMS 功率检测与软拐点增益衰减计算
//   4. 提供原子级 currentGrDb 上报给 UI 仪表
// ==============================================================================

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>
#include <algorithm>

namespace dsp_primitives
{

class DuckingDetector
{
public:
    DuckingDetector() = default;

    void prepare(double sr) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        reset();
        updateCoeffs(300.0f);
    }

    void reset() noexcept
    {
        rmsPower = 0.0f;
        smoothedGrDb = 0.0f;
        reportedGrDb.store(0.0f, std::memory_order_relaxed);
    }

    void updateCoeffs(float releaseMs) noexcept
    {
        // 5ms 极速 Attack
        const float attTimeSec = 0.005f;
        alphaAtt = std::exp(-1.0f / (static_cast<float>(sampleRate) * attTimeSec));

        // 用户可调 Release (50 ~ 2000ms)
        const float relTimeSec = std::max(0.05f, releaseMs * 0.001f);
        alphaRel = std::exp(-1.0f / (static_cast<float>(sampleRate) * relTimeSec));

        // 10ms RMS 积分窗口
        const float rmsWindowSec = 0.01f;
        alphaRms = std::exp(-1.0f / (static_cast<float>(sampleRate) * rmsWindowSec));
    }

    // 逐采样处理并返回施加于湿声的乘数 (0.0 ~ 1.0)
    [[nodiscard]] inline float processSample(float sidechainL, float sidechainR,
                                            bool enabled, float thresholdDb,
                                            float amountPercent) noexcept
    {
        if (!enabled || amountPercent <= 0.001f)
        {
            smoothedGrDb = 0.0f;
            reportedGrDb.store(0.0f, std::memory_order_relaxed);
            return 1.0f;
        }

        // 1. RMS 功率追踪
        const float instantPower = 0.5f * (sidechainL * sidechainL + sidechainR * sidechainR);
        rmsPower = alphaRms * rmsPower + (1.0f - alphaRms) * instantPower;

        // 防次正常数
        if (rmsPower < 1.0e-12f)
            rmsPower = 0.0f;

        const float levelDb = (rmsPower > 1.0e-12f)
                            ? (10.0f * std::log10(rmsPower))
                            : -120.0f;

        // 2. 目标衰减分贝 (最大 24dB 衰减深度)
        float targetGrDb = 0.0f;
        if (levelDb > thresholdDb)
        {
            const float overshoot = levelDb - thresholdDb;
            // 线性软斜率映射，最大 24dB
            const float maxDb = 24.0f * (amountPercent * 0.01f);
            targetGrDb = std::min(maxDb, overshoot * (amountPercent * 0.01f) * 1.5f);
        }

        // 3. Attack / Release 弹道平滑
        if (targetGrDb > smoothedGrDb)
            smoothedGrDb = alphaAtt * smoothedGrDb + (1.0f - alphaAtt) * targetGrDb;
        else
            smoothedGrDb = alphaRel * smoothedGrDb + (1.0f - alphaRel) * targetGrDb;

        if (smoothedGrDb < 0.005f)
            smoothedGrDb = 0.0f;

        // 4. 计算湿声增益缩放系数
        const float duckGain = std::pow(10.0f, -smoothedGrDb * 0.05f);

        return duckGain;
    }

    void finishBlock() noexcept
    {
        reportedGrDb.store(smoothedGrDb, std::memory_order_relaxed);
    }

    [[nodiscard]] float getReportedGrDb() const noexcept
    {
        return reportedGrDb.load(std::memory_order_relaxed);
    }

private:
    double sampleRate { 48000.0 };
    float alphaAtt { 0.0f };
    float alphaRel { 0.0f };
    float alphaRms { 0.0f };

    float rmsPower { 0.0f };
    float smoothedGrDb { 0.0f };

    std::atomic<float> reportedGrDb { 0.0f };
};

} // namespace dsp_primitives
