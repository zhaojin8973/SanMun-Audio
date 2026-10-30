// ==============================================================================
// test_conversational_recommender.cpp - 智能压缩助理单方案与增量微调单元测试
// ==============================================================================

#include <iostream>
#include <cmath>
#include <cassert>

#include <juce_core/juce_core.h>
#include "ai/LlmClient.h"
#include "dsp/AcousticFeatureExtractor.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI initialiser;

    std::cout << "==================================================" << std::endl;
    std::cout << "   AI Co-Pilot Single Preset & Slang Unit Tests   " << std::endl;
    std::cout << "==================================================" << std::endl;

    ai::LlmClient client;

    // [Test 1] 鼓组高置信度映射 (Drums -> FET)
    {
        std::cout << "[Test 1] Testing Drums Mapping to FET Engine..." << std::endl;
        ai::AcousticFeatures drumFeat;
        drumFeat.bpm = 120.0;
        drumFeat.peakDb = -3.0f;
        drumFeat.rmsDb = -16.0f;
        drumFeat.crestFactorDb = 13.0f;
        drumFeat.sourceType = ai::AcousticSourceType::SnareDrum;

        auto resp = client.generateLocalSingleRecommendation(drumFeat);
        std::cout << "  Title: " << resp.preset.title << " | Mode: " << resp.preset.mode << std::endl;
        std::cout << "  Detected: " << resp.detectedSource << std::endl;

        assert(resp.preset.mode == Params::ModeFET);
        assert(resp.detectedSource.contains(juce::String::fromUTF8("鼓组")));
        std::cout << "  PASS: Drums mapped to FET correctly!" << std::endl;
    }

    // [Test 2] 人声高置信度映射 (Vocal -> VOX)
    {
        std::cout << "[Test 2] Testing Vocal Mapping to VOX Engine..." << std::endl;
        ai::AcousticFeatures voxFeat;
        voxFeat.bpm = 120.0;
        voxFeat.peakDb = -5.0f;
        voxFeat.rmsDb = -18.0f;
        voxFeat.dynamicRangeDb = 12.0f;
        voxFeat.sourceType = ai::AcousticSourceType::LeadVocal;

        auto resp = client.generateLocalSingleRecommendation(voxFeat);
        std::cout << "  Title: " << resp.preset.title << " | Mode: " << resp.preset.mode << std::endl;

        assert(resp.preset.mode == Params::ModeVOX);
        assert(resp.detectedSource.contains(juce::String::fromUTF8("人声")));
        std::cout << "  PASS: Vocal mapped to VOX correctly!" << std::endl;
    }

    // [Test 3] 母带总线高置信度映射 (Master Bus -> VCA)
    {
        std::cout << "[Test 3] Testing Master Mix Bus Mapping to VCA Engine..." << std::endl;
        ai::AcousticFeatures busFeat;
        busFeat.bpm = 120.0;
        busFeat.peakDb = -1.5f;
        busFeat.rmsDb = -14.0f;
        busFeat.sourceType = ai::AcousticSourceType::MasterMixBus;

        auto resp = client.generateLocalSingleRecommendation(busFeat);
        std::cout << "  Title: " << resp.preset.title << " | Mode: " << resp.preset.mode << std::endl;

        assert(resp.preset.mode == Params::ModeVCA);
        assert(resp.preset.vcaRatio == 2.0f); // 2:1
        assert(resp.preset.vcaRelease >= 1200.0f); // Auto
        std::cout << "  PASS: Master Bus mapped to VCA 2:1 Auto correctly!" << std::endl;
    }

    // [Test 4] 乐器/伴奏映射 (Instrument -> Opto)
    {
        std::cout << "[Test 4] Testing Instrument Mapping to Opto Engine..." << std::endl;
        ai::AcousticFeatures instFeat;
        instFeat.bpm = 120.0;
        instFeat.peakDb = -4.0f;
        instFeat.rmsDb = -16.0f;
        instFeat.sourceType = ai::AcousticSourceType::MasterInstrumental;

        auto resp = client.generateLocalSingleRecommendation(instFeat);
        std::cout << "  Title: " << resp.preset.title << " | Mode: " << resp.preset.mode << std::endl;

        assert(resp.preset.mode == Params::ModeOpto);
        assert(resp.preset.optoPR >= 40.0f);
        std::cout << "  PASS: Instrument mapped to Opto correctly!" << std::endl;
    }

    // [Test 5] 行话微调：“再猛一点”
    {
        std::cout << "[Test 5] Testing Slang Delta '再猛一点'..." << std::endl;
        ai::AcousticFeatures feat;
        feat.sourceType = ai::AcousticSourceType::KickDrum;
        auto base = client.generateLocalSingleRecommendation(feat);
        float origInput = base.preset.fetInput;

        auto modified = client.processConversationalInput(juce::String::fromUTF8("再猛一点"), feat, base.preset);
        std::cout << "  FET Input: " << origInput << " -> " << modified.preset.fetInput << std::endl;

        assert(modified.preset.fetInput > origInput);
        std::cout << "  PASS: Slang '再猛一点' increased compression amount!" << std::endl;
    }

    // [Test 6] 行话微调：“拍头冲一点”
    {
        std::cout << "[Test 6] Testing Slang Delta '拍头冲一点'..." << std::endl;
        ai::AcousticFeatures feat;
        feat.sourceType = ai::AcousticSourceType::MasterMixBus;
        auto base = client.generateLocalSingleRecommendation(feat);

        auto modified = client.processConversationalInput(juce::String::fromUTF8("拍头冲一点"), feat, base.preset);
        std::cout << "  VCA Attack: " << modified.preset.vcaAttack << " ms" << std::endl;

        assert(modified.preset.vcaAttack >= 30.0f);
        std::cout << "  PASS: Slang '拍头冲一点' slowed down attack!" << std::endl;
    }

    // [Test 7] 行话微调：“有点抽吸”
    {
        std::cout << "[Test 7] Testing Slang Delta '有点抽吸'..." << std::endl;
        ai::AcousticFeatures feat;
        feat.sourceType = ai::AcousticSourceType::MasterMixBus;
        auto base = client.generateLocalSingleRecommendation(feat);

        auto modified = client.processConversationalInput(juce::String::fromUTF8("有点抽吸"), feat, base.preset);
        std::cout << "  VCA Release: " << modified.preset.vcaRelease << " ms" << std::endl;

        assert(modified.preset.vcaRelease >= 1200.0f);
        std::cout << "  PASS: Slang '有点抽吸' enabled Auto release!" << std::endl;
    }

    // [Test 8] 行话微调：“保留气声”
    {
        std::cout << "[Test 8] Testing Slang Delta '保留气声'..." << std::endl;
        ai::AcousticFeatures feat;
        feat.sourceType = ai::AcousticSourceType::LeadVocal;
        auto base = client.generateLocalSingleRecommendation(feat);

        auto modified = client.processConversationalInput(juce::String::fromUTF8("保留气声"), feat, base.preset);
        std::cout << "  VOX Gate: " << modified.preset.voxGate << " dB" << std::endl;

        assert(modified.preset.voxGate <= -80.0f);
        std::cout << "  PASS: Slang '保留气声' opened VOX gate!" << std::endl;
    }

    std::cout << "==================================================" << std::endl;
    std::cout << ">>> ALL SINGLE RECOMMENDATION TESTS PASSED! <<<" << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}
