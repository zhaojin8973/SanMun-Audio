// ==============================================================================
// LlmClient.h - 大语言模型 / 智能压缩助理推理客户端
// ==============================================================================

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "../dsp/AcousticFeatureExtractor.h"
#include "../params/Parameters.h"

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace ai
{

enum class LlmProvider
{
    GeminiFlash = 0,   // Google Gemini 1.5 Flash (官方免费额度)
    DeepSeek,          // DeepSeek V3 (国内超低成本免翻墙)
    OpenAI             // OpenAI GPT-4o-mini
};

class LlmClient
{
public:
    LlmClient();
    ~LlmClient() = default;

    // ---- API 配置管理 ----
    void setApiKey(const juce::String& key);
    juce::String getApiKey() const;

    void setProvider(LlmProvider provider);
    LlmProvider getProvider() const;

    void setCustomEndpoint(const juce::String& endpoint);
    juce::String getCustomEndpoint() const;

    void saveSettings();
    void loadSettings();

    // ---- 核心异步推理调用 ----

    /**
     * 使用本地规则计算起始方案，在消息线程回调；当前不发送网络请求。
     */
    void requestRecommendation(
        const AcousticFeatures& features,
        const juce::String& userPrompt,
        std::function<void(bool success, const SingleRecommendationResponse& resp, const juce::String& errorMsg)> callback);

    /**
     * 本地声学规则推荐，无需网络与 API Key；推荐结果需素材听审。
     */
    SingleRecommendationResponse generateLocalSingleRecommendation(
        const AcousticFeatures& features,
        const juce::String& userPrompt = {}) const;

    /**
     * 自然语言微调与行话语义解析引擎（单一方案增量演进）
     */
    SingleRecommendationResponse processConversationalInput(
        const juce::String& userText,
        const AcousticFeatures& features,
        const CompressorPresetRecommendation& currentPreset = {});

    // 兼容别名
    SingleRecommendationResponse generateLocalDualRecommendations(
        const AcousticFeatures& features,
        const juce::String& userPrompt = {}) const
    {
        return generateLocalSingleRecommendation(features, userPrompt);
    }

private:
    juce::String apiKey;
    LlmProvider currentProvider = LlmProvider::GeminiFlash;
    juce::String customEndpoint;
};

} // namespace ai
