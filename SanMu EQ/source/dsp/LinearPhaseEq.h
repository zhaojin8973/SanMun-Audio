// ==============================================================================
// LinearPhaseEq.h - 线性相位 EQ 引擎（overlap-save 分块卷积）
// 从整链 IIR 幅度响应合成对称 FIR 核，做零相位失真（恒定群延迟）卷积
// ==============================================================================

#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <vector>
#include <memory>

/**
 * LinearPhaseEq：线性相位（Linear Phase）EQ 处理引擎。
 *
 * 设计目标：
 * - 从外部（EqProcessor）的整链 IIR 幅度响应，通过频率采样法 + IFFT
 *   合成四路有符号对称 FIR 核（静态时共用恒定群延迟）
 * - 用 overlap-save 分块卷积实时处理音频。每帧独立卷积、无跨块
 *   overlap-add 尾累积，因此频域响应可以逐帧平滑演化（合法的时变
 *   滤波），过渡是否可闻仍须结合实际音乐验收。
 * - 全部工作缓冲在 prepare() 预分配，process 路径零分配、零 malloc，
 *   可在音频线程安全运行（含参数变化时重建 FIR）
 *
 * 不依赖 juce::dsp::Convolution：其异步 IR 加载线程契约不适合
 * 「音频线程频繁重建核」的场景。
 */
class LinearPhaseEq
{
public:
    // ---- 编译期尺寸（约 106.7ms @ 48kHz，牺牲延迟换相位/低频精度） ----
    // FIR 长度决定频域分辨率 ≈ 采样率/FIR_M：
    // - 2049 tap → 23Hz：34Hz 的窄 Bell 峰（Q=1 带宽 34Hz）只有 1.5 个分辨率
    //   单元，Kaiser 窗截断后峰被抹平（实测 +12dB 只生效 30%）→ 低频提升形同虚设
    // - 4097 tap → 12Hz：34Hz Q=1 峰约 3 个分辨率单元，提升精度恢复
    // - 8193 tap → 5.9Hz：34Hz Q=1 峰约 6 个分辨率单元，窄峰（Q=10 带宽 3.4Hz）
    //   也有部分还原能力；相位稳定性随核长进一步提高
    // 代价：群延迟更大（产品已拍板"可牺牲更多延迟换相位稳定"）
    static constexpr int BLOCK_L     = 1024;    // 每帧新输入数 L（overlap-save 帧输出段）
    static constexpr int FIR_M       = 8193;    // FIR 核长（奇数）
    static constexpr int FFT_ORDER   = 14;      // N = 2^14 = 16384（≥ 历史+帧 = 8192+1024）
    static constexpr int FFT_N       = 1 << FFT_ORDER;
    static constexpr int FFT_WORK    = FFT_N * 2;      // JUCE 实 FFT 要求 2N
    static constexpr int GROUP_DELAY = (FIR_M - 1) / 2; // α = 4096（FIR 群延迟）
    // 总延迟 = 预填充静音(LATENCY-α) + FIR 群延迟 α = 5120 ≈ 106.7ms @48k。
    // 预填充使输出流相对输入恒定延迟 LATENCY，与 DAW blockSize 无关，
    // 因此 DAW 延迟补偿始终准确。
    static constexpr int LATENCY     = BLOCK_L + GROUP_DELAY; // 5120 ≈ 106.7ms @48k

    // 幅度采样回调：返回 freqHz 处指定声道的整链幅度（dB）
    using MagnitudeSampler = float (*)(float freqHz, double sr, int channel, void* ctx);
    // 有符号 2×2 传递矩阵：顺序为 outL/inL、outL/inR、outR/inL、outR/inR。
    using MatrixSampler = void (*)(float freqHz, double sr, float matrix[4], void* ctx);

    // ---- 生命周期 ----
    /** 准备引擎：建 FFT、按 maximumBlockSize 缩放缓冲、生成 Kaiser 窗表 */
    void prepare(const juce::dsp::ProcessSpec& spec);

    /** 清空所有声道状态，标记 FIR 无效，重新预填充延迟静音 */
    void reset(bool preserveFir = false);
    void prepareIdentityKernel();

    /** 从幅度回调合成新的对称 FIR 核（音频线程、零分配，为左右声道分别合成） */
    void rebuildFir(MagnitudeSampler sampler, void* ctx, double sr);
    /** 从有符号 2×2 传递矩阵合成四路 FIR 核，支持 L/R 与 M/S 混用。 */
    void rebuildFir(MatrixSampler sampler, void* ctx, double sr);
    void beginIncrementalRebuild(MatrixSampler sampler, void* ctx, double sr);
    bool isRebuildPending() const noexcept { return rebuildPending; }
    void setPendingDynamicGain(int band, float gainDb) noexcept;
    float getAppliedReduction(int band) const noexcept { return appliedReduction[static_cast<size_t>(band)].load(std::memory_order_relaxed); }
    const float* getMatrixFirTaps(int path) const noexcept { return firTaps[static_cast<size_t>(path)].data(); }

    /** 处理一个音频块（2×2 矩阵 overlap-save 分块卷积） */
    void processBlock(juce::AudioBuffer<float>& buffer, int rebuildSamples = -1);

    /**
     * 每个 1024 采样卷积帧把当前频域响应朝目标平滑过渡一步。
     * overlap-save 每帧独立卷积、无跨块 tail，因此逐帧演化的响应是
     * 时变滤波；输出使用同一输入历史的线性交叉淡化。
     */
    void advanceSmoothing();

    /** 线性相位引擎引入的总延迟（采样数） */
    int getLatencySamples() const noexcept { return LATENCY; }

    // ---- 仅供测试/诊断只读访问 ----
    /** FIR 核（M 个时域 taps），供参考卷积对比验证 */
    const float* getFirTaps(int channel = 0) const noexcept { return firTaps[static_cast<size_t>(channel == 0 ? 0 : 3)].data(); }
    static constexpr int getFirSize() noexcept { return FIR_M; }

private:
    // ---- 每声道 overlap-save 状态 ----
    struct ChannelState
    {
        // 输入缓冲布局：[历史 (M-1) 个 | staging 个新输入]
        // 帧 = 历史 + BLOCK_L 个新输入（共 M-1+BLOCK_L，补零到 N）
        std::vector<float> inputBuf;
        std::array<float, FFT_WORK> fftWork{};  // 帧 FFT 工作区（2N）
        int stagingCount  = 0;   // inputBuf 内已累积未入帧的新输入数
    };

    std::array<ChannelState, 2> channels;  // 两路输入历史（mono 用 ch0）

    struct OutputState
    {
        std::vector<float> outputFifo;
        int fifoAvailable = 0;
        int fifoRead = 0;
        int fifoWrite = 0;
        int fifoMask = 0;
    };
    std::array<OutputState, 2> outputs;

    std::unique_ptr<juce::dsp::FFT> fft;   // 唯一 FFT 实例，重建与卷积共用（串行）

    // ---- FIR 设计缓冲（全部预分配，进程路径只复用） ----
    std::array<float, FFT_WORK> firDesignWork{};  // 幅度采样 + 频谱构造 + IFFT 工作区
    std::array<float, FFT_N>    firPad{};         // 补零后的核（forward 用）
    std::array<std::array<float, FFT_N>, 4> firTaps{};        // 四路时域核 h[0..M-1]
    // bins 0..N/2 的交错布局需索引到 2*(N/2)+1 = N+1 = 4097，故用 FFT_WORK 容量
    std::array<std::array<float, FFT_WORK>, 4> curFreqResp{};     // 当前生效的四路 H[k]
    std::array<std::array<float, FFT_WORK>, 4> targetFreqResp{};  // 目标四路 H[k]
    std::array<std::array<float, FFT_WORK>, 4> buildingFreqResp{}; // 未完成四路不得写入目标
    std::array<std::array<float, FFT_N / 2 + 1>, 4> sampledMagnitude{};
    std::array<float, FIR_M>    windowTable{};    // Kaiser 窗表

    MatrixSampler pendingSampler = nullptr;
    void* pendingContext = nullptr;
    double pendingSampleRate = 44100.0;
    int pendingBin = 0;
    int pendingPath = 0;
    bool rebuildPending = false;
    static constexpr int DYNAMIC_BANDS = 24;
    std::array<float, DYNAMIC_BANDS> pendingDynamicGain{}, targetDynamicGain{}, currentDynamicGain{};
    std::array<std::atomic<float>, DYNAMIC_BANDS> appliedReduction{};
    std::vector<std::array<float, DYNAMIC_BANDS>> dynamicFifo;
    std::array<float, FFT_WORK> convolutionWork{};

    double sampleRate   = 44100.0;
    int    numChannels  = 2;
    int    maxBlockSize = 512;
    bool   firValid = false;   // FIR 是否可用
    std::array<bool, 4> haveTarget { false, false, false, false }; // 目标频谱是否已生成
    std::array<bool, 4> curValid   { false, false, false, false }; // 当前频谱是否已初始化
    std::array<bool, 4> curLocked  { false, false, false, false }; // 当前频谱是否已收敛锁定

    /** 执行单个 overlap-save 帧卷积（前向 → 频域乘 → 逆变换），
     *  输出有效段 [M-1 .. M-1+L-1] 留在 fftWork 供调用方取用 */
    void transformInput(ChannelState& cs);
    void convBlock(int outputChannel, int inputChannels);
    void advanceIncrementalRebuild(int numSamples);
    void synthesizePath(int path);
    void activateTargets();

    /** 0 阶修正贝塞尔函数 I0（Kaiser 窗用） */
    static float besselI0(float x);
};
