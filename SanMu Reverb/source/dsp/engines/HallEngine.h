#pragma once
#include "ReverbEngine.h"
#include "hall/HallField.h"
#include "hall/CathedralField.h"
#include <array>

namespace dsp_engines
{
// 仅适配宿主参数；全部 Hall 声学状态由独立的 Field 持有。
class HallEngine final : public ReverbEngine
{
public:
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(const float* inL, const float* inR, float* outL, float* outR,
                 int numSamples, const ReverbParameters& params) override;
private:
    sanmu_hall::Field field;
    sanmu_hall::CathedralField cathedral;
    std::array<juce::SmoothedValue<float>, 14> parameters;
    bool initialized = false;
    unsigned tick = 0;
};
} // namespace dsp_engines
