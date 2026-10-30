// ==============================================================================
// AcousticFeatureExtractor.h - 实时声学多模态特征提取器
//
// 专为 AI 智能听音（AI Listen）设计：
// 1. 音频主线程以不等待的尝试锁采集；
// 2. 精确计算：Peak (dBFS)、True RMS (dBFS)、波峰因数 (Crest Factor dB)、动态范围；
// 3. 5 频段声学能量分布：超低频 (<60Hz)、低频 (60-250Hz)、中频 (250-2kHz)、临场感 (2k-6kHz)、高频空气感 (>6kHz)；
// 4. 从宿主获取工程 BPM 速度与拍号，用于音符释放时间计算；
// 5. 保留轻量 JSON 诊断接口；当前 LISTEN 在本地完成分析与候选重放。
// ==============================================================================

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../params/Parameters.h"

#include <atomic>
#include <vector>
#include <array>
#include <string>
#include <mutex>
#include <functional>

namespace ai
{

/**
 * 结构化的单套压缩器参数建议
 */
struct CompressorPresetRecommendation
{
    int mode = Params::ModeVOX; // Params::CompMode
    juce::String title;         // 如 "现代流行贴脸" / "美式爆破军鼓"
    juce::String rationale;     // 专业声学解析理由
    juce::String detectedSource;// 识别出的音轨类型与置信度

    // VOX 参数
    float voxComp = -11.3f;
    float voxGate = -48.0f;
    float voxGain = 0.0f;
    float voxMix  = 100.0f;
    bool voxSmooth = false;

    // VCA 参数
    float vcaThreshold = -20.0f;
    float vcaRatio     = 4.0f;
    float vcaAttack    = 10.0f;
    float vcaRelease   = 150.0f;
    float vcaMakeup    = 0.0f;
    float vcaScHp      = 60.0f;
    float vcaMix       = 100.0f;

    // FET 参数
    float fetInput     = 18.0f;
    int   fetRatioIdx  = 0;      // 0=4:1, 1=8:1, 2=12:1, 3=20:1, 4=All-Buttons
    float fetAttack    = 3.0f;
    float fetRelease   = 5.0f;
    float fetOutput    = 30.0f;
    float fetScHp      = 60.0f;
    float fetMix       = 100.0f;

    // Opto 参数
    float optoPR       = 40.0f;
    float optoGain     = 40.0f;
    bool  optoIsLimit  = false;
    float optoMix      = 100.0f;
    float optoScHp     = 60.0f;

    // 同一活跃输入窗口的立体声有效电平匹配；是响度近似，不是 LUFS 或真峰值保证。
    float calibratedPeakDb = -120.0f;
    float calibrationTargetDb = -120.0f;
    float calibrationInputRmsDb = -120.0f;
    float calibratedRmsDb = -120.0f;
    bool calibrationPeakLimited = false;
    bool calibrationWithinRange = false;
    bool usable = false;
    float requestedGrDb = 0.0f;
    float measuredGrDb = 0.0f;
    float maximumGrDb = 0.0f;
};

/**
 * 单一最佳方案响应结构体
 */
struct SingleRecommendationResponse
{
    CompressorPresetRecommendation preset;
    juce::String detectedSource;      // 本地规则推测的声源类型
    juce::String conversationalReply; // 简要专业声学解析
};

enum class AcousticSourceType
{
    Unknown = 0,

    // 核心高置信度物理母体 (High-Confidence Core Physical Types)
    KickDrum,               // 底鼓 / 大鼓 (Kick / Bass Drum)
    SnareDrum,              // 军鼓 / 小鼓 / 打击乐 (Snare Drum / Percussion)
    BassGuitar,             // 电贝斯 / 低频持续态 (Bass Guitar / Low End)
    LeadVocal,              // 现代人声 / 主唱 (Lead Vocal)
    MasterMixBus,           // 母带总线 / 全曲混音 (Master / Mix Bus)

    // 待确认复合乐器 (Ambiguous / Requires Confirmation)
    AmbiguousInstrument,    // 复合原声乐器 / 伴奏织体 (待用户确认是钢琴、吉他、管弦乐等)

    // 细分意图定位类型 (Explicitly Requested via Chat or Preset)
    RockVocal,              // 摇滚 / 强爆发力人声
    BackingVocals,          // 和声组 / 合唱团
    TomsPerc,               // 嗵鼓 / 桶鼓 / 定音鼓
    DrumBus,                // 鼓组总线
    DrumLoop,               // 鼓节奏循环
    PercussionGroup,        // 打击乐组 / 摇铃 / 康加
    AcousticGuitarStrum,    // 原声木吉他扫弦
    AcousticGuitarFinger,   // 原声木吉他指弹
    ElectricGuitarClean,    // 电吉他清音 / Funk 节奏
    ElectricGuitarDist,     // 电吉他失真 / 强力节奏
    ElectricGuitarSolo,     // 电吉他 Solo / 主音
    AcousticPiano,          // 原声大钢琴
    ElectricPianoKeys,      // 电钢琴 / Rhodes / 合成器键盘
    SynthBass808,           // 合成器贝斯 / 808 Sub
    MasterPopRnb,           // 流行 / R&B / 嘻哈 成品母带
    MasterRockMetal,        // 摇滚 / 金属 成品母带
    MasterElectronic,       // 电子 / EDM 舞曲母带
    MasterCinematic,        // 电影原声 / 史诗管弦乐
    MasterInstrumental,     // 纯音乐 / 爵士 / 伴奏总线

    // 历史兼容别名
    SnarePerc = SnareDrum,
    AcousticInst = AcousticPiano
};

/**
 * 声学指纹与工程元数据结构体
 */
struct AcousticFeatures
{
    // 工程与时域元数据
    double bpm = 120.0;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;
    double sampleRate = 48000.0;

    // 动态与能量特征
    float peakDb = -60.0f;
    float rmsDb = -60.0f;
    float crestFactorDb = 0.0f;       // Peak - RMS (瞬态尖锐度，越大约尖锐)
    float dynamicRangeDb = 0.0f;      // 动态离散跨度
    float spectralCentroidHz = 1000.0f; // 频谱质心 (Hz)
    float dutyCyclePct = 100.0f;        // 活跃占空比 (%)
    float durationSeconds = 0.0f;
    float activeSeconds = 0.0f;
    int percussiveOnsetCount = 0;
    float percussiveOnsetRatio = 0.0f;
    float noiseBandFlatness = 0.0f;
    float spectralShapeVariation = 0.0f;
    float centroidVariation = 0.0f;
    float stereoWidthRatio = 0.0f;      // 立体声侧向能量宽度比 (Side RMS / Mid RMS)
    std::string transientCharacter;   // "sharp_transient" / "smooth_sustained" / "dynamic_vocal"

    // 5 频段声学能量百分比 (总和 100%)
    float subEnergyPct = 0.0f;        // < 60 Hz
    float bassEnergyPct = 0.0f;       // 60 - 250 Hz
    float midEnergyPct = 0.0f;        // 250 - 2000 Hz
    float presenceEnergyPct = 0.0f;   // 2000 - 6000 Hz
    float airEnergyPct = 0.0f;        // > 6000 Hz

    // 声源推断与置信度
    AcousticSourceType sourceType = AcousticSourceType::Unknown;
    bool isHighConfidence = false;     // 本地规则尚无概率校准，不能当作识别准确率
    std::string inferredSource;       // 推断或检测到的声源描述
    std::string inferredGenreHint;    // 风格倾向提示
    std::string clarifyingQuestion;   // 当置信度低时的主动询问语

    // 将特征包序列化为诊断 JSON，不自动发送到外部服务
    juce::String toJsonString(const juce::String& userPrompt = {}) const;
};

// 一次捕获的独立快照；后台分析不再持有实时采集锁。
struct ListenCapture
{
    std::vector<float> mid, side;
    double sampleRate = 48000, bpm = 120;
    int timeSigNum = 4, timeSigDen = 4;
};

class AcousticFeatureExtractor : private juce::Timer
{
public:
    AcousticFeatureExtractor();
    ~AcousticFeatureExtractor() override;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    /**
     * 启动听音监听（默认 5.0 秒，支持 1~30 秒或手动停止）
     */
    void startListening(double targetDurationSeconds = 5.0, bool waitForSignal = false);
    bool isWaitingForSignal() const noexcept { return waitingForSignal.load(std::memory_order_relaxed); }

    /**
     * 提前手动停止监听并立即执行声学特征分析
     */
    void stopListeningAndCompute();

    /**
     * 是否正在听音中
     */
    bool isListening() const noexcept { return listeningActive.load(std::memory_order_relaxed); }

    /**
     * 听音进度 (0.0 ~ 1.0)
     */
    float getProgress() const noexcept;

    /**
     * 已录制的音频秒数
     */
    double getRecordedSeconds() const noexcept;

    /**
     * 实时输入电平（dBFS，供 UI 动效绘制）
     */
    float getLiveInputPeakDb() const noexcept { return liveInputPeak.load(std::memory_order_relaxed); }

    /**
     * 音频处理主线程实时推进（不等待）
     */
    void processBlock(const juce::AudioBuffer<float>& buffer, double currentBpm = 120.0, int timeSigNum = 4, int timeSigDen = 4);

    /**
     * 提取完成后的最新声学特征（由 UI 或 Worker 线程安全读取）
     */
    AcousticFeatures getLatestFeatures() const;
    void acceptAnalysis(const AcousticFeatures& features)
    { std::lock_guard<std::mutex> lock(featureMutex); cachedFeatures = features; }

    /**
     * 触发声学特征计算（一般在采集满后自动调用）
     */
    void computeFeatures();
    ListenCapture copyCapture() const;
    static AcousticFeatures analyseCapture(const ListenCapture& capture, bool inferSource = true);
    static void calibrateCapture(const ListenCapture& capture, CompressorPresetRecommendation& rec,
                                 const std::atomic<bool>* cancelled = nullptr);
    void cancelListening();
    std::function<void(ListenCapture)> onCaptureCompleted;

    /**
     * 设置宿主音轨名称（作为辅助先验线索）
     */
    void setHostTrackName(const juce::String& name) { std::lock_guard<std::mutex> lock(featureMutex); hostTrackName = name; }
    juce::String getHostTrackName() const { std::lock_guard<std::mutex> lock(featureMutex); return hostTrackName; }

    /**
     * 对推荐方案执行虚拟 DSP 闭环增益与电平仿真校准
     * 利用捕获片段校准活跃段有效电平，并验证采样峰值；不代表 LUFS 或真峰值测量。
     */
    void calibrateRecommendationWithVirtualDsp(CompressorPresetRecommendation& rec) const;

    /**
     * 监听完成回调
     */
    std::function<void(const AcousticFeatures&)> onAnalysisCompleted;

private:
    juce::String hostTrackName;

    std::atomic<double> sampleRate{48000.0};
    std::atomic<bool> listeningActive{ false };
    std::atomic<bool> waitingForSignal{ false };
    std::atomic<int64_t> samplesCollected{ 0 };
    std::atomic<int64_t> totalTargetSamples{48000 * 10};
    mutable juce::SpinLock captureLock;
    std::atomic<bool> analysisPending{false};
    void timerCallback() override;
    std::atomic<float> liveInputPeak{ -100.0f };

    // 环形缓冲采样存储（最大 30 秒 @ 192kHz）
    static constexpr int kMaxBufferSamples = 192000 * 30;
    std::vector<float> captureBuffer;
    std::vector<float> captureSideBuffer;

    // 宿主元数据
    std::atomic<float> hostBpm{ 120.0f };
    std::atomic<int> hostTimeSigNum{ 4 };
    std::atomic<int> hostTimeSigDen{ 4 };

    // 计算得到的最终声学特征缓存
    AcousticFeatures cachedFeatures;
    mutable std::mutex featureMutex;
};

} // namespace ai
