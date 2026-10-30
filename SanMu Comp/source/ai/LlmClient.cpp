// ==============================================================================
// LlmClient.cpp - 大语言模型 / 智能压缩助理推理客户端实现
// ==============================================================================

#include "LlmClient.h"

namespace ai
{
namespace
{
float vocalCompStartingPoint(const AcousticFeatures& features)
{
    // 由独立平准曲线估计起点，输出仍需经过捕获素材校准。
    const float target = features.dynamicRangeDb > 14 ? 6 : features.dynamicRangeDb > 8 ? 5 : 4;
    // 只跟随 LEV 已确定的增强曲线更新参数反算，不改变声源识别和选型规则。
    float lower = 0, upper = target;
    for (int i = 0; i < 16; ++i)
    {
        const float middle = (lower + upper) * 0.5f;
        if (middle - std::expm1(-middle / 2) < target) lower = middle;
        else upper = middle;
    }
    const float baseTarget = (lower + upper) * 0.5f;
    const float amplitude = std::pow(10.0f, (features.rmsDb + 3.0103f) / 20);
    const float amount = 20 * std::log10(1 + (std::pow(10.0f, baseTarget / 20) - 1) / std::max(amplitude, 1.0e-6f));
    return -std::round(std::clamp(amount, 0.0f, 36.0f) * 10) / 10;
}
float optoStartingPoint(const AcousticFeatures& features)
{
    // 由 COMP 静态反馈曲线反求 PR 起点；动态压缩量仍以素材渲染和听测为准。
    const float target = features.dynamicRangeDb > 12 ? 6 : features.dynamicRangeDb > 7 ? 5 : 4;
    const float q = target / 2;
    const float over = 0.5f * (q + std::sqrt(q * q + 64 * q));
    const float drive = over - features.rmsDb + target - 24;
    return std::clamp(100 * std::pow(std::max(0.0f, (drive + 10) / 48), 1 / 1.65f), 0.0f, 100.0f);
}
}

LlmClient::LlmClient()
{
    loadSettings();
}

void LlmClient::setApiKey(const juce::String& key)
{
    apiKey = key.trim();
    saveSettings();
}

juce::String LlmClient::getApiKey() const
{
    return apiKey;
}

void LlmClient::setProvider(LlmProvider provider)
{
    currentProvider = provider;
    saveSettings();
}

LlmProvider LlmClient::getProvider() const
{
    return currentProvider;
}

void LlmClient::setCustomEndpoint(const juce::String& endpoint)
{
    customEndpoint = endpoint.trim();
    saveSettings();
}

juce::String LlmClient::getCustomEndpoint() const
{
    return customEndpoint;
}

void LlmClient::saveSettings()
{
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("SanMuAudio");
    appDataDir.createDirectory();

    juce::PropertiesFile::Options opts;
    opts.applicationName = "SanMuCompAI";
    opts.filenameSuffix = ".settings";
    opts.folderName = "SanMuAudio";
    opts.storageFormat = juce::PropertiesFile::storeAsXML;

    juce::PropertiesFile props(appDataDir.getChildFile("comp_ai_settings.xml"), opts);
    props.setValue("apiKey", apiKey);
    props.setValue("provider", static_cast<int>(currentProvider));
    props.setValue("customEndpoint", customEndpoint);
    props.saveIfNeeded();
}

void LlmClient::loadSettings()
{
    auto appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("SanMuAudio");
    auto file = appDataDir.getChildFile("comp_ai_settings.xml");
    if (!file.existsAsFile())
        return;

    juce::PropertiesFile::Options opts;
    opts.applicationName = "SanMuCompAI";
    opts.filenameSuffix = ".settings";
    opts.folderName = "SanMuAudio";
    opts.storageFormat = juce::PropertiesFile::storeAsXML;

    juce::PropertiesFile props(file, opts);
    apiKey = props.getValue("apiKey", "");
    currentProvider = static_cast<LlmProvider>(props.getIntValue("provider", 0));
    customEndpoint = props.getValue("customEndpoint", "");
}

SingleRecommendationResponse LlmClient::generateLocalSingleRecommendation(
    const AcousticFeatures& features,
    const juce::String& userPrompt) const
{
    SingleRecommendationResponse resp;
    CompressorPresetRecommendation& rec = resp.preset;

    const juce::String pLower = userPrompt.toLowerCase();

    // 1. 判断是否有人工明确指定意图
    bool hasExplicitIntent = false;

    if (pLower.contains(juce::String::fromUTF8("母带")) || pLower.contains(juce::String::fromUTF8("总线"))
        || pLower.contains("master") || pLower.contains("bus") || pLower.contains("glue"))
    {
        hasExplicitIntent = true;
        rec.mode = Params::ModeVCA;
        rec.title = juce::String::fromUTF8("【混音母带胶水】VCA 模拟总线胶水");
        resp.detectedSource = juce::String::fromUTF8("母带总线 / 全曲混音 (意图匹配 按用户指定)");
        rec.vcaThreshold = std::clamp(features.rmsDb - 5.0f, -40.0f, -10.0f);
        rec.vcaRatio = 2.0f;
        rec.vcaAttack = 30.0f;
        rec.vcaRelease = 2000.0f;
        rec.vcaScHp = 80.0f;
        rec.rationale = juce::String::fromUTF8("VCA 模拟总线 2:1 软拐点 + Auto 智能双阶段释放，用于总线动态控制；实际衰减量取决于输入素材。");
    }
    else if (pLower.contains(juce::String::fromUTF8("鼓")) || pLower.contains(juce::String::fromUTF8("军鼓"))
          || pLower.contains(juce::String::fromUTF8("底鼓")) || pLower.contains("drum") || pLower.contains("snare") || pLower.contains("kick"))
    {
        hasExplicitIntent = true;
        rec.mode = Params::ModeFET;
        rec.title = juce::String::fromUTF8("【极速瞬态打击】FET 场效应管峰值压限");
        resp.detectedSource = juce::String::fromUTF8("鼓组 / 打击乐 (意图匹配 按用户指定)");
        const bool isSubHeavy = (features.subEnergyPct >= 15.0f) || pLower.contains(juce::String::fromUTF8("底鼓")) || pLower.contains("kick");
        rec.fetScHp = isSubHeavy ? 90.0f : 60.0f;
        rec.fetInput = isSubHeavy ? std::clamp(17.5f - features.peakDb, 18.0f, 34.0f)
                                  : std::clamp(15.0f - features.peakDb, 18.0f, 30.0f);
        rec.fetOutput = 27.0f;
        rec.fetRatioIdx = 0; // 4:1
        rec.fetAttack = 3.0f;
        rec.fetRelease = 4.6f;
        rec.rationale = juce::String::fromUTF8("FET 极速峰值抓取抚平打击乐冲顶，保留敲击拍头瞬态穿透力与扎实质感。");
    }
    else if (pLower.contains(juce::String::fromUTF8("人声")) || pLower.contains(juce::String::fromUTF8("主唱"))
          || pLower.contains(juce::String::fromUTF8("说唱")) || pLower.contains("vocal") || pLower.contains("lead"))
    {
        hasExplicitIntent = true;
        rec.mode = Params::ModeVOX;
        rec.title = juce::String::fromUTF8("【自适应动态平准】LEV 现代平准");
        resp.detectedSource = juce::String::fromUTF8("现代人声 / 主唱 (意图匹配 按用户指定)");
        rec.voxComp = vocalCompStartingPoint(features);
        rec.voxGate = -80.0f;
        rec.voxGain = 0.0f;
        rec.rationale = juce::String::fromUTF8("主体动态平准与曲线补偿，配合下扩展门和峰值控制；请结合素材调整压缩深度。");
    }
    else if (pLower.contains(juce::String::fromUTF8("钢琴")) || pLower.contains(juce::String::fromUTF8("吉他"))
          || pLower.contains(juce::String::fromUTF8("弦乐")) || pLower.contains(juce::String::fromUTF8("乐器"))
          || pLower.contains(juce::String::fromUTF8("伴奏")) || pLower.contains("piano") || pLower.contains("guitar") || pLower.contains("acoustic"))
    {
        hasExplicitIntent = true;
        rec.mode = Params::ModeOpto;
        rec.title = juce::String::fromUTF8("【温润光电平准】Opto 经典双阶段光敏");
        resp.detectedSource = juce::String::fromUTF8("乐器伴奏 / 钢琴弦乐 (意图匹配 按用户指定)");
        rec.optoPR = optoStartingPoint(features);
        rec.optoGain = 40.0f;
        rec.rationale = juce::String::fromUTF8("Opto 双阶段光敏迟滞曲线平滑抚平乐器与伴奏强弱起伏，赋予丰满温润的录音室模拟质感。");
    }

    // 2. 无人工输入时，基于声学指纹 4 大互斥分类映射
    if (!hasExplicitIntent)
    {
        switch (features.sourceType)
        {
            case AcousticSourceType::KickDrum:
            case AcousticSourceType::SnareDrum:
            case AcousticSourceType::DrumBus:
            case AcousticSourceType::DrumLoop:
            case AcousticSourceType::PercussionGroup:
            case AcousticSourceType::TomsPerc:
            {
                rec.mode = Params::ModeFET;
                rec.title = juce::String::fromUTF8("【极速瞬态打击】FET 场效应管峰值压限");
                resp.detectedSource = juce::String::fromUTF8("鼓组 / 打击乐 (本地规则推测，待听审确认)");
                const bool isSubHeavy = (features.subEnergyPct >= 15.0f) || (features.sourceType == AcousticSourceType::KickDrum);
                rec.fetScHp = isSubHeavy ? 90.0f : 60.0f;
                rec.fetInput = isSubHeavy ? std::clamp(17.5f - features.peakDb, 18.0f, 34.0f)
                                          : std::clamp(15.0f - features.peakDb, 18.0f, 30.0f);
                rec.fetOutput = 27.0f;
                rec.fetRatioIdx = 0; // 4:1
                rec.fetAttack = 3.0f;
                rec.fetRelease = 4.6f;
                rec.rationale = juce::String::fromUTF8("FET 极速峰值抓取抚平打击乐冲顶，保留敲击拍头瞬态穿透力与扎实质感。");
                break;
            }

            case AcousticSourceType::LeadVocal:
            case AcousticSourceType::RockVocal:
            case AcousticSourceType::BackingVocals:
            {
                rec.mode = Params::ModeVOX;
                rec.title = juce::String::fromUTF8("【自适应动态平准】LEV 现代平准");
                resp.detectedSource = juce::String::fromUTF8("现代人声 / 主唱 (本地规则推测，待听审确认)");
                rec.voxComp = vocalCompStartingPoint(features);
                rec.voxGate = -80.0f;
                rec.voxGain = 0.0f;
                rec.rationale = juce::String::fromUTF8("主体动态平准与曲线补偿，配合下扩展门和峰值控制；请结合素材调整压缩深度。");
                break;
            }

            case AcousticSourceType::MasterMixBus:
            case AcousticSourceType::MasterPopRnb:
            case AcousticSourceType::MasterRockMetal:
            case AcousticSourceType::MasterElectronic:
            {
                rec.mode = Params::ModeVCA;
                rec.title = juce::String::fromUTF8("【混音母带胶水】VCA 模拟总线胶水");
                resp.detectedSource = juce::String::fromUTF8("母带总线 / 全曲混音 (本地规则推测，待听审确认)");
                rec.vcaThreshold = std::clamp(features.rmsDb - 5.0f, -40.0f, -10.0f);
                rec.vcaRatio = 2.0f;
                rec.vcaAttack = 30.0f;
                rec.vcaRelease = 2000.0f;
                rec.vcaScHp = 80.0f;
                rec.rationale = juce::String::fromUTF8("VCA 模拟总线 2:1 软拐点 + Auto 智能双阶段释放，用于总线动态控制；实际衰减量取决于输入素材。");
                break;
            }

            case AcousticSourceType::MasterInstrumental:
            case AcousticSourceType::MasterCinematic:
            case AcousticSourceType::AcousticGuitarStrum:
            case AcousticSourceType::AcousticGuitarFinger:
            case AcousticSourceType::ElectricGuitarClean:
            case AcousticSourceType::ElectricGuitarDist:
            case AcousticSourceType::ElectricGuitarSolo:
            case AcousticSourceType::AcousticPiano:
            case AcousticSourceType::ElectricPianoKeys:
            case AcousticSourceType::BassGuitar:
            case AcousticSourceType::SynthBass808:
            case AcousticSourceType::AmbiguousInstrument:
            default:
            {
                rec.mode = Params::ModeOpto;
                rec.title = juce::String::fromUTF8("【温润光电平准】Opto 经典双阶段光敏");
                resp.detectedSource = juce::String::fromUTF8("乐器伴奏 / 钢琴弦乐 (本地规则推测，待听审确认)");
                rec.optoPR = optoStartingPoint(features);
                rec.optoGain = 40.0f;
                rec.rationale = juce::String::fromUTF8("Opto 双阶段光敏迟滞曲线平滑抚平乐器与伴奏强弱起伏，赋予丰满温润的录音室模拟质感。");
                break;
            }
        }
    }

    rec.detectedSource = resp.detectedSource;

    resp.conversationalReply = juce::String::fromUTF8("已结合当前音轨实测物理电平（Peak: ")
                             + juce::String(features.peakDb, 1) + " dBFS, RMS: "
                             + juce::String(features.rmsDb, 1) + " dBFS），为您量身定制【"
                             + resp.detectedSource + "】起始方案，请结合素材听审。\n• "
                             + rec.title + ": " + rec.rationale;

    return resp;
}

SingleRecommendationResponse LlmClient::processConversationalInput(
    const juce::String& userText,
    const AcousticFeatures& features,
    const CompressorPresetRecommendation& currentPreset)
{
    SingleRecommendationResponse resp = generateLocalSingleRecommendation(features, userText);
    resp.preset = currentPreset;
    const juce::String u = userText.toLowerCase();

    // 增量调节语义解析
    if (u.contains(juce::String::fromUTF8("猛")) || u.contains(juce::String::fromUTF8("重")) || u.contains(juce::String::fromUTF8("深"))
        || u.contains(juce::String::fromUTF8("多压")) || u.contains("more") || u.contains("harder"))
    {
        resp.preset.fetInput = std::min(resp.preset.fetInput + 2.5f, 40.0f);
        resp.preset.vcaThreshold = std::max(resp.preset.vcaThreshold - 2.5f, -50.0f);
        resp.preset.voxComp = std::max(resp.preset.voxComp - 3.0f, -32.0f);
        resp.preset.optoPR = std::min(resp.preset.optoPR + 6.0f, 95.0f);
        resp.conversationalReply = juce::String::fromUTF8("收到！已为您将压缩深度增加了约 2.5dB，动态更加紧实有力！");
    }
    else if (u.contains(juce::String::fromUTF8("松")) || u.contains(juce::String::fromUTF8("轻")) || u.contains(juce::String::fromUTF8("少压"))
          || u.contains(juce::String::fromUTF8("紧")) || u.contains("less") || u.contains("relax"))
    {
        resp.preset.fetInput = std::max(resp.preset.fetInput - 2.5f, 5.0f);
        resp.preset.vcaThreshold = std::min(resp.preset.vcaThreshold + 2.5f, 0.0f);
        resp.preset.voxComp = std::min(resp.preset.voxComp + 3.0f, 0.0f);
        resp.preset.optoPR = std::max(resp.preset.optoPR - 6.0f, 0.0f);
        resp.conversationalReply = juce::String::fromUTF8("收到！已为您放宽了压缩量约 2.5dB，释放更多自然动态与呼吸感！");
    }
    else if (u.contains(juce::String::fromUTF8("拍头")) || u.contains(juce::String::fromUTF8("冲")) || u.contains("punch") || u.contains("attack"))
    {
        resp.preset.vcaAttack = 30.0f;
        resp.preset.fetAttack = 2.0f;
        resp.conversationalReply = juce::String::fromUTF8("收到！已为您将起音时间放慢至 30ms，让敲击拍头瞬态完整穿透，打击感更硬朗！");
    }
    else if (u.contains(juce::String::fromUTF8("抽吸")) || u.contains(juce::String::fromUTF8("平滑")) || u.contains("pumping") || u.contains("smooth"))
    {
        resp.preset.vcaRelease = 2000.0f;
        resp.conversationalReply = juce::String::fromUTF8("收到！已为您开启 Auto 智能双阶段释放，彻底消除不自然的起伏抽吸感，保持声场平滑！");
    }
    else if (u.contains(juce::String::fromUTF8("气声")) || u.contains(juce::String::fromUTF8("门限")) || u.contains("gate"))
    {
        resp.preset.voxGate = -80.0f;
        resp.conversationalReply = juce::String::fromUTF8("收到！已为您将下扩展噪声门限完全开启降至 -80dBFS，完整保留人声全部呼吸气声与微弱尾音！");
    }

    return resp;
}

void LlmClient::requestRecommendation(
    const AcousticFeatures& features,
    const juce::String& userPrompt,
    std::function<void(bool success, const SingleRecommendationResponse& resp, const juce::String& errorMsg)> callback)
{
    const auto localResp = generateLocalSingleRecommendation(features, userPrompt);
    juce::MessageManager::callAsync([callback = std::move(callback), localResp]()
    {
        if (callback) callback(true, localResp, {});
    });
}

} // namespace ai
