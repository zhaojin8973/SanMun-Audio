#pragma once
#include "ReverbEngine.h"
#include "room/RoomField.h"
#include <array>
namespace dsp_engines
{
// 房间参数适配与平滑；声学状态完全属于Room。
class RoomEngine final : public ReverbEngine
{
public:
    void prepare(double sampleRate,int maxBlockSize) override;
    void reset() override;
    void process(const float* inL,const float* inR,float* outL,float* outR,int numSamples,const ReverbParameters& params) override;
private:
    sanmu_room::Field field;
    std::array<juce::SmoothedValue<float>,14> parameters;
    bool initialized=false;unsigned tick=0;int activeStyle=-1;
};
}
