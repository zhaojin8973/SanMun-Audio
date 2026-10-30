#include "ai/LlmClient.h"
#include <cassert>
#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    ai::LlmClient client;
    ai::AcousticFeatures features;
    features.peakDb = -12;
    features.rmsDb = -20;
    features.dynamicRangeDb = 9;
    const std::pair<ai::AcousticSourceType, int> cases[] = {
        {ai::AcousticSourceType::KickDrum, Params::ModeFET},
        {ai::AcousticSourceType::SnareDrum, Params::ModeFET},
        {ai::AcousticSourceType::LeadVocal, Params::ModeVOX},
        {ai::AcousticSourceType::MasterMixBus, Params::ModeVCA},
        {ai::AcousticSourceType::AcousticPiano, Params::ModeOpto},
        {ai::AcousticSourceType::BassGuitar, Params::ModeOpto}
    };
    for (const auto& [source, expectedMode] : cases)
    {
        features.sourceType = source;
        const auto response = client.generateLocalSingleRecommendation(features);
        assert(response.preset.mode == expectedMode);
        assert(!response.detectedSource.contains("%"));
        assert(response.detectedSource.contains(juce::String::fromUTF8("本地规则")));
        assert(response.preset.vcaThreshold >= -60 && response.preset.vcaThreshold <= 0);
        assert(response.preset.fetInput >= 0 && response.preset.fetInput <= 48);
        assert(response.preset.optoPR >= 0 && response.preset.optoPR <= 100);
    }
    features.sourceType = ai::AcousticSourceType::KickDrum;
    features.peakDb = -3;
    const auto loud = client.generateLocalSingleRecommendation(features);
    features.peakDb = -23;
    const auto quiet = client.generateLocalSingleRecommendation(features);
    assert(quiet.preset.fetInput > loud.preset.fetInput);
    for (auto source : {ai::AcousticSourceType::LeadVocal, ai::AcousticSourceType::AcousticPiano})
    {
        features.sourceType = source;
        features.rmsDb = -14;
        const auto louder = client.generateLocalSingleRecommendation(features).preset;
        features.rmsDb = -26;
        const auto quieter = client.generateLocalSingleRecommendation(features).preset;
        if (source == ai::AcousticSourceType::LeadVocal)
            assert(quieter.voxComp < louder.voxComp - 8);
        else
            assert(quieter.optoPR > louder.optoPR + 5);
    }
    std::cout << "单方案声源路由、参数范围、输入电平适配、能力声明：通过\n";
}
