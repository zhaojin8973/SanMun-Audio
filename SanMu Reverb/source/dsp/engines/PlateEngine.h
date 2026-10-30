#pragma once
#include "ReverbEngine.h"
#include "plate/PlateField.h"
#include "plate/SteelField.h"
#include <array>
namespace dsp_engines
{
// 板式参数适配层；与Hall没有声学状态或内核依赖。
class PlateEngine final : public ReverbEngine
{
public:
    void prepare(double sampleRate,int maxBlockSize) override;
    void reset() override;
    void process(const float* inL,const float* inR,float* outL,float* outR,int numSamples,const ReverbParameters& params) override;
private:
    sanmu_plate::Field field;
    sanmu_steel::Field steel;
    std::array<juce::SmoothedValue<float>,14> parameters;
    bool initialized=false;
    unsigned tick=0;
    int activeStyle=-1;
};
}
