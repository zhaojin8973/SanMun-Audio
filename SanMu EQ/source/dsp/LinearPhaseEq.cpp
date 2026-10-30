// ==============================================================================
// LinearPhaseEq.cpp - 线性相位 EQ 引擎实现
// FIR 合成（频率采样 → 共轭对称频谱 → IFFT → Kaiser 窗 → DC 归一化 → 预变换）
// + 每声道 overlap-save 分块卷积（输出 FIFO 处理非整除块长 + 恒定延迟预填充）
// ==============================================================================

#include "LinearPhaseEq.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
struct LegacySamplerContext
{
    LinearPhaseEq::MagnitudeSampler sampler = nullptr;
    void* context = nullptr;
};

void legacyMatrixSampler(float freqHz, double sr, float matrix[4], void* ptr)
{
    auto& legacy = *static_cast<LegacySamplerContext*>(ptr);
    const float leftDb = legacy.sampler(freqHz, sr, 0, legacy.context);
    const float rightDb = legacy.sampler(freqHz, sr, 1, legacy.context);
    matrix[0] = std::pow(10.0f, leftDb / 20.0f);
    matrix[1] = 0.0f;
    matrix[2] = 0.0f;
    matrix[3] = std::pow(10.0f, rightDb / 20.0f);
}

void identityMatrixSampler(float, double, float matrix[4], void*)
{
    matrix[0] = matrix[3] = 1.0f;
    matrix[1] = matrix[2] = 0.0f;
}

float finiteMatrixGain(float gain)
{
    // 保留多频段叠加的正常大增益；旧的 ±64 限制会把两段 +24 dB 截成约 +36 dB。
    return std::isfinite(gain) ? juce::jlimit(-1.0e12f, 1.0e12f, gain) : 0.f;
}
}

// ==============================================================================
// 0 阶修正贝塞尔函数 I0（Kaiser 窗）
// 级数展开：I0(x) = Σ (x/2)^(2k) / (k!)^2
// ==============================================================================

float LinearPhaseEq::besselI0(float x)
{
    float sum  = 1.0f;
    float term = 1.0f;
    const float xHalf = 0.5f * x;

    for (int k = 1; k < 60; ++k)
    {
        term *= xHalf * xHalf / static_cast<float>(k * k);
        sum  += term;
        if (term < 1e-12f * sum)
            break;
    }
    return sum;
}

// ==============================================================================
// 生命周期
// ==============================================================================

void LinearPhaseEq::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate   = spec.sampleRate;
    numChannels  = static_cast<int>(spec.numChannels);
    maxBlockSize = static_cast<int>(spec.maximumBlockSize);

    fft = std::make_unique<juce::dsp::FFT>(FFT_ORDER);

    // 输入缓冲布局：[历史 (M-1) | staging 新输入]。容量 = 历史 + 一帧新输入 + 一个 DAW 块
    const int inCap  = (FIR_M - 1) + BLOCK_L + maxBlockSize;
    // 输出 FIFO 容量：预填充(LATENCY-α) + 一帧输出 + 一个 DAW 块（2 的幂，掩码取模）
    const int outCap = juce::nextPowerOfTwo((LATENCY - GROUP_DELAY) + BLOCK_L + maxBlockSize);

    for (auto& cs : channels)
    {
        cs.inputBuf.resize(static_cast<size_t>(inCap));
        cs.fftWork.fill(0.0f);
        cs.stagingCount  = 0;
    }
    for (auto& out : outputs)
    {
        out.outputFifo.resize(static_cast<size_t>(outCap));
        out.fifoMask = outCap - 1;
        std::fill(out.outputFifo.begin(), out.outputFifo.end(), 0.0f);
        out.fifoAvailable = LATENCY - GROUP_DELAY;
        out.fifoRead = 0;
        out.fifoWrite = (LATENCY - GROUP_DELAY) & out.fifoMask;
    }
    dynamicFifo.assign(static_cast<size_t>(outCap), {});
    pendingDynamicGain.fill(0); targetDynamicGain.fill(0); currentDynamicGain.fill(0);
    for (auto& meter : appliedReduction) meter.store(0);
    rebuildPending = false;

    // ---- Kaiser 窗表（β≈8，窗中心 = 1.0，保证全平 EQ 时核≈δ 直通） ----
    const float beta    = 8.0f;    // β=8 旁瓣抑制与主瓣宽度平衡最优（实测 β=10 振铃反而抵消低频峰）
    const float i0beta  = besselI0(beta);
    const float invAlpha = 1.0f / static_cast<float>(GROUP_DELAY);
    for (int n = 0; n < FIR_M; ++n)
    {
        const float t = (n - GROUP_DELAY) * invAlpha;  // -1 .. 1
        const float arg = beta * std::sqrt(std::max(0.0f, 1.0f - t * t));
        windowTable[static_cast<size_t>(n)] = besselI0(arg) / i0beta;
    }

    for (int ch = 0; ch < 4; ++ch)
    {
        curFreqResp[static_cast<size_t>(ch)].fill(0.0f);
        targetFreqResp[static_cast<size_t>(ch)].fill(0.0f);
        haveTarget[static_cast<size_t>(ch)] = false;
        curValid[static_cast<size_t>(ch)] = false;
        curLocked[static_cast<size_t>(ch)] = false;
    }
    firValid = false;
}

void LinearPhaseEq::reset(bool preserveFir)
{
    for (auto& cs : channels)
    {
        std::fill(cs.inputBuf.begin(), cs.inputBuf.end(), 0.0f);
        cs.fftWork.fill(0.0f);
        cs.stagingCount  = 0;
    }
    for (auto& out : outputs)
    {
        std::fill(out.outputFifo.begin(), out.outputFifo.end(), 0.0f);
        out.fifoAvailable = LATENCY - GROUP_DELAY;
        out.fifoRead = 0;
        out.fifoWrite = (LATENCY - GROUP_DELAY) & out.fifoMask;
    }
    rebuildPending = false;
    std::fill(dynamicFifo.begin(), dynamicFifo.end(), std::array<float, DYNAMIC_BANDS>{});
    for (auto& meter : appliedReduction) meter.store(0);
    if (preserveFir) return;
    pendingDynamicGain.fill(0); targetDynamicGain.fill(0); currentDynamicGain.fill(0);
    for (int ch = 0; ch < 4; ++ch)
    {
        curFreqResp[static_cast<size_t>(ch)].fill(0.0f);
        targetFreqResp[static_cast<size_t>(ch)].fill(0.0f);
        haveTarget[static_cast<size_t>(ch)] = false;
        curValid[static_cast<size_t>(ch)] = false;
        curLocked[static_cast<size_t>(ch)] = false;
    }
    firValid = false;
}

void LinearPhaseEq::prepareIdentityKernel()
{
    rebuildFir(&identityMatrixSampler, nullptr, sampleRate);
}

// ==============================================================================
// FIR 合成（音频线程、零分配）
// ==============================================================================

void LinearPhaseEq::rebuildFir(MagnitudeSampler sampler, void* ctx, double sr)
{
    LegacySamplerContext legacy { sampler, ctx };
    rebuildFir(&legacyMatrixSampler, &legacy, sr);
}

void LinearPhaseEq::rebuildFir(MatrixSampler sampler, void* ctx, double sr)
{
    sampleRate = sr;
    const int halfN = FFT_N / 2;
    for (int k = 0; k <= halfN; ++k)
    {
        const float freq = (k == 0) ? 1.0f
                                    : static_cast<float>(k) * static_cast<float>(sr) / static_cast<float>(FFT_N);
        float matrix[4] {};
        sampler(freq, static_cast<double>(sr), matrix, ctx);
        for (int path = 0; path < 4; ++path)
            sampledMagnitude[static_cast<size_t>(path)][static_cast<size_t>(k)] =
                finiteMatrixGain(matrix[path]);
    }

    for (int path = 0; path < 4; ++path)
        synthesizePath(path);
    activateTargets();
}

void LinearPhaseEq::beginIncrementalRebuild(MatrixSampler sampler, void* ctx, double sr)
{
    pendingSampler = sampler;
    pendingContext = ctx;
    pendingSampleRate = sr;
    pendingBin = 0;
    pendingPath = 0;
    rebuildPending = true;
}

void LinearPhaseEq::setPendingDynamicGain(int band, float gainDb) noexcept
{
    if (band >= 0 && band < DYNAMIC_BANDS)
        pendingDynamicGain[static_cast<size_t>(band)] = gainDb;
}

void LinearPhaseEq::advanceIncrementalRebuild(int numSamples)
{
    if (!rebuildPending) return;
    const int halfN = FFT_N / 2;
    if (pendingBin <= halfN)
    {
        const int end = std::min(halfN + 1, pendingBin + std::max(256, numSamples * 8));
        for (int k = pendingBin; k < end; ++k)
        {
            const float freq = k == 0 ? 1.0f
                                      : static_cast<float>(k) * static_cast<float>(pendingSampleRate) / static_cast<float>(FFT_N);
            float matrix[4] {};
            pendingSampler(freq, pendingSampleRate, matrix, pendingContext);
            for (int path = 0; path < 4; ++path)
                sampledMagnitude[static_cast<size_t>(path)][static_cast<size_t>(k)] =
                    finiteMatrixGain(matrix[path]);
        }
        pendingBin = end;
        return;
    }

    synthesizePath(pendingPath++);
    if (pendingPath == 4)
    {
        activateTargets();
        rebuildPending = false;
    }
}

void LinearPhaseEq::synthesizePath(int path)
{
    const int halfN = FFT_N / 2;
    static_assert(GROUP_DELAY * 4 == FFT_N);
    constexpr float realPhase[] {1, 0, -1, 0};
    constexpr float imagPhase[] {0, -1, 0, 1};
    for (int k = 0; k <= halfN; ++k)
    {
        const float amp = sampledMagnitude[static_cast<size_t>(path)][static_cast<size_t>(k)];
        // 群延迟恰为四分之一变换长度；精确四相循环避免大角度舍入破坏核对称。
        firDesignWork[static_cast<size_t>(2 * k)]     = amp * realPhase[k & 3];
        firDesignWork[static_cast<size_t>(2 * k + 1)] = amp * imagPhase[k & 3];
    }
        // k = N/2（Nyquist）虚部 = amp·sin(π·α)，α 为整数 → 0，天然满足实信号约束

        // ---- 2. 逆变换 → 理想冲激（自带 1/N 归一化），峰值位于 n=α ----
        fft->performRealOnlyInverseTransform(firDesignWork.data());

        // ---- 3. Kaiser 窗截断到 M taps ----
        auto& taps = firTaps[static_cast<size_t>(path)];
        for (int j = 0; j < FIR_M; ++j)
            taps[static_cast<size_t>(j)] = firDesignWork[static_cast<size_t>(j)] * windowTable[static_cast<size_t>(j)];

        // ---- 4. 补零到 N，前向变换得频域响应 H[k]（供每块直接相乘） ----
        std::fill(firPad.begin(), firPad.end(), 0.0f);
        std::copy(taps.begin(), taps.begin() + FIR_M, firPad.begin());
        std::copy(firPad.begin(), firPad.begin() + FFT_N, firDesignWork.begin());
        std::fill(firDesignWork.begin() + FFT_N, firDesignWork.end(), 0.0f);
        fft->performRealOnlyForwardTransform(firDesignWork.data(), false);

        // 存入目标频谱（每块平滑趋向它；首次直接到位，避免启动音量渐起）
        std::copy(firDesignWork.begin(), firDesignWork.begin() + (halfN + 1) * 2,
                  buildingFreqResp[static_cast<size_t>(path)].begin());
}

void LinearPhaseEq::activateTargets()
{
    targetFreqResp = buildingFreqResp;
    targetDynamicGain = pendingDynamicGain;
    for (int path = 0; path < 4; ++path)
    {
        haveTarget[static_cast<size_t>(path)] = true;
        if (!curValid[static_cast<size_t>(path)])
        {
            curFreqResp[static_cast<size_t>(path)] = targetFreqResp[static_cast<size_t>(path)];
            curValid[static_cast<size_t>(path)] = true;
            curLocked[static_cast<size_t>(path)] = true;
        }
        else curLocked[static_cast<size_t>(path)] = false;
    }

    firValid = true;
}

// ==============================================================================
// overlap-save 分块卷积
// ==============================================================================

void LinearPhaseEq::transformInput(ChannelState& cs)
{
    float* buf = cs.fftWork.data();

    // 帧 = 历史(M-1) + 新 BLOCK_L 个，共 M-1+BLOCK_L = 9216，补零到 N。
    // N ≥ (M-1)+BLOCK_L 即满足无混叠条件；输出取 [M-1..M-1+L-1] 段，
    // 该段是线性卷积的正确段，前 M-1 个循环混叠段被丢弃。
    const int frameLen = (FIR_M - 1) + BLOCK_L;
    std::copy(cs.inputBuf.begin(), cs.inputBuf.begin() + frameLen, buf);
    std::fill(buf + frameLen, buf + FFT_N, 0.0f);

    fft->performRealOnlyForwardTransform(buf, false);
}

void LinearPhaseEq::convBlock(int outputChannel, int inputChannels)
{
    // 同一输入只做一次 FFT；频域累加两条输入，输出只需一次逆变换。
    float* buf = convolutionWork.data();
    const int halfN = FFT_N / 2;
    std::fill(buf, buf + FFT_WORK, 0.0f);
    for (int input = 0; input < inputChannels; ++input)
    {
        const auto& x = channels[static_cast<size_t>(input)].fftWork;
        const auto& h = curFreqResp[static_cast<size_t>(outputChannel * 2 + input)];
        for (int k = 0; k <= halfN; ++k)
        {
            const size_t j = static_cast<size_t>(k * 2);
            buf[j] += x[j] * h[j] - x[j + 1] * h[j + 1];
            buf[j + 1] += x[j] * h[j + 1] + x[j + 1] * h[j];
        }
    }
    buf[1] = buf[FFT_N + 1] = 0;
    fft->performRealOnlyInverseTransform(buf);
}

void LinearPhaseEq::processBlock(juce::AudioBuffer<float>& buffer, int rebuildSamples)
{
    // 卷积帧边界留给音频变换；核生成在下一控制刻度继续，避免两个峰值叠加。
    if (rebuildSamples != 0 && channels[0].stagingCount + buffer.getNumSamples() < BLOCK_L)
        advanceIncrementalRebuild(rebuildSamples < 0 ? buffer.getNumSamples() : rebuildSamples);
    if (!firValid)
    {
        buffer.clear();
        return;
    }

    const int numSamples = buffer.getNumSamples();
    const int numCh = std::min({numChannels, buffer.getNumChannels(), 2});
    if (numSamples == 0 || numCh == 0) return;

    for (int ch = 0; ch < numCh; ++ch)
    {
        ChannelState& cs = channels[static_cast<size_t>(ch)];
        const float* in  = buffer.getReadPointer(ch);

        // ---- 1. 追加新输入到 staging 区（历史区之后） ----
        std::copy(in, in + numSamples, cs.inputBuf.begin() + (FIR_M - 1) + cs.stagingCount);
        cs.stagingCount += numSamples;

        // ---- 2. 每凑满 BLOCK_L 个新输入，排出一帧（overlap-save） ----
    }

    while (channels[0].stagingCount >= BLOCK_L)
    {
        for (int input = 0; input < numCh; ++input)
            transformInput(channels[static_cast<size_t>(input)]);
        std::array<std::array<float, BLOCK_L>, 2> previousOutput{};
        const auto previousDynamicGain = currentDynamicGain;
        const bool moving = !std::all_of(curLocked.begin(), curLocked.end(), [](bool locked) { return locked; });
        if (moving)
        {
            for (int output = 0; output < numCh; ++output)
            {
                convBlock(output, numCh);
                std::copy_n(convolutionWork.data() + FIR_M - 1, BLOCK_L,
                            previousOutput[static_cast<size_t>(output)].begin());
            }
            advanceSmoothing();
        }
        for (int outputChannel = 0; outputChannel < numCh; ++outputChannel)
        {
            auto& outState = outputs[static_cast<size_t>(outputChannel)];
            convBlock(outputChannel, numCh);
            for (int i = 0; i < BLOCK_L; ++i)
            {
                const float mix = moving ? static_cast<float>(i + 1) / BLOCK_L : 1.0f;
                const size_t pos = static_cast<size_t>((outState.fifoWrite + i) & outState.fifoMask);
                const float next = convolutionWork[static_cast<size_t>(FIR_M - 1 + i)];
                outState.outputFifo[pos] = moving
                    ? previousOutput[static_cast<size_t>(outputChannel)][static_cast<size_t>(i)] * (1 - mix) + next * mix : next;
                if (outputChannel == 0)
                {
                    for (int band = 0; band < DYNAMIC_BANDS; ++band)
                    {
                        const auto b = static_cast<size_t>(band);
                        dynamicFifo[pos][b] = previousDynamicGain[b] * (1 - mix) + currentDynamicGain[b] * mix;
                    }
                }
            }
            outState.fifoWrite = (outState.fifoWrite + BLOCK_L) & outState.fifoMask;
            outState.fifoAvailable += BLOCK_L;
        }

        for (int input = 0; input < numCh; ++input)
        {
            auto& inputState = channels[static_cast<size_t>(input)];
            std::memmove(inputState.inputBuf.data(), inputState.inputBuf.data() + BLOCK_L,
                         static_cast<size_t>(FIR_M - 1) * sizeof(float));
            const int leftover = inputState.stagingCount - BLOCK_L;
            if (leftover > 0)
                std::memmove(inputState.inputBuf.data() + (FIR_M - 1),
                             inputState.inputBuf.data() + (FIR_M - 1) + BLOCK_L,
                             static_cast<size_t>(leftover) * sizeof(float));
            inputState.stagingCount -= BLOCK_L;
        }
    }

    // ---- 3. 从两个输出 FIFO 交付 numSamples ----
    for (int outputChannel = 0; outputChannel < numCh; ++outputChannel)
    {
        auto& outState = outputs[static_cast<size_t>(outputChannel)];
        float* out = buffer.getWritePointer(outputChannel);
        for (int i = 0; i < numSamples; ++i)
        {
            if (outState.fifoAvailable > 0)
            {
                out[i] = outState.outputFifo[static_cast<size_t>(outState.fifoRead)];
                if (outputChannel == 0 && i == numSamples - 1)
                    for (int band = 0; band < DYNAMIC_BANDS; ++band)
                        appliedReduction[static_cast<size_t>(band)].store(
                            -dynamicFifo[static_cast<size_t>(outState.fifoRead)][static_cast<size_t>(band)], std::memory_order_relaxed);
                outState.fifoRead = (outState.fifoRead + 1) & outState.fifoMask;
                --outState.fifoAvailable;
            }
            else out[i] = 0.0f;
        }
    }

    // 多余声道清空
    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
}

// ==============================================================================
// 频域平滑过渡——overlap-save 下每帧独立卷积，平滑即合法的时变滤波，
// 参数跳变不会产生 OLA 式 tail 失配爆音
// ==============================================================================

void LinearPhaseEq::advanceSmoothing()
{
    const float f = 1.0f - std::exp(-static_cast<float>(BLOCK_L / (sampleRate * 0.025)));
    const int halfN = FFT_N / 2;

    for (int ch = 0; ch < 4; ++ch)
    {
        if (!haveTarget[static_cast<size_t>(ch)] || curLocked[static_cast<size_t>(ch)])
            continue;

        auto& cur = curFreqResp[static_cast<size_t>(ch)];
        const auto& target = targetFreqResp[static_cast<size_t>(ch)];
        bool converged = true;

        for (int k = 0; k <= halfN; ++k)
        {
            const size_t i2k = static_cast<size_t>(2 * k);
            const float dRe = target[i2k]     - cur[i2k];
            const float dIm = target[i2k + 1] - cur[i2k + 1];
            cur[i2k]     += dRe * f;
            cur[i2k + 1] += dIm * f;
            if (std::abs(dRe) > 1e-5f || std::abs(dIm) > 1e-5f)
                converged = false;
        }

        if (converged)
        {
            cur = target;  // 锁定目标，消除浮点漂移
            curLocked[static_cast<size_t>(ch)] = true;
        }
    }
    for (int band = 0; band < DYNAMIC_BANDS; ++band)
    {
        const auto b = static_cast<size_t>(band);
        // 与响应同权重混合线性增益；表值跟随实际输出 FIFO，而非检测目标。
        const float current = std::pow(10.f, currentDynamicGain[b] / 20.f);
        const float target = std::pow(10.f, targetDynamicGain[b] / 20.f);
        currentDynamicGain[b] = 20.f * std::log10(std::max(1.0e-9f, current + (target - current) * f));
    }
}
