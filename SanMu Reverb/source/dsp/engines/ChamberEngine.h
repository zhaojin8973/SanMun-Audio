#pragma once
#include "ReverbEngine.h"
#include "chamber/ChamberField.h"
#include <array>

namespace dsp_engines
{
// 回声室独立参数适配层；历史第三编号归入柔暖模式。
class ChamberEngine final : public ReverbEngine
{
public:
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(const float* inL, const float* inR, float* outL, float* outR,
                 int numSamples, const ReverbParameters& params) override;
private:
    sanmu_chamber::Field field;
    std::array<juce::SmoothedValue<float>, 14> parameters;
    bool initialized = false;
    unsigned tick = 0;
    int activeStyle = -1;
};
}
