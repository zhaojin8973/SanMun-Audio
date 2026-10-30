// ==============================================================================
// test_linearphase.cpp - 线性相位 overlap-save 引擎数值验证
// 复现大增益（+10dB）场景：
//   0. 单帧数学：convBlock 频域滤波 vs 时域线性卷积（输出段 [M-1..]）
//   1. 稳态：分块输出 vs 全序列卷积参考，扫描最优对齐 lag
//      → 期望 lag = LATENCY-GROUP_DELAY，且与 blockSize 无关（恒定延迟）
//   2. 脉冲延迟：输出峰值位置 = LATENCY（验证 DAW 延迟上报准确）
//   3. 参数跳变：flat→+10dB 切换，瞬态误差应远小于 OLA 的 tail 失配
// 用法：构建 sanmuq6_test_ola 目标后直接运行（无输出参数）
// ==============================================================================

#include "dsp/LinearPhaseEq.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

// ==============================================================================
// 简单峰值滤波器（bell）幅度响应采样器（dB）
// ==============================================================================

static float bellDb(float fHz, double sr, float gainLin, double f0, double q)
{
    const double pi = 3.14159265358979323846;
    const double w0 = 2.0 * pi * f0 / sr;
    const double alpha = std::sin(w0) / (2.0 * q);
    const double c = std::cos(w0);
    const double b0 = 1.0 + alpha * gainLin, b1 = -2.0 * c, b2 = 1.0 - alpha * gainLin;
    const double a0 = 1.0 + alpha / gainLin, a1 = -2.0 * c, a2 = 1.0 - alpha / gainLin;

    const double w = 2.0 * pi * fHz / sr;
    const double zRe = std::cos(w), zIm = -std::sin(w);
    const double zRe2 = 2.0 * zRe * zRe - 1.0, zIm2 = 2.0 * zRe * zIm;

    const double numRe = b0 + b1 * zRe + b2 * zRe2;
    const double numIm = b1 * zIm + b2 * zIm2;
    const double denRe = a0 + a1 * zRe + a2 * zRe2;
    const double denIm = a1 * zIm + a2 * zIm2;

    const double num2 = numRe * numRe + numIm * numIm;
    const double den2 = denRe * denRe + denIm * denIm;
    return static_cast<float>(10.0 * std::log10(std::max(num2 / den2, 1e-12)));
}

struct BellCtx
{
    float  gainLin;
    double f0;
    double q;
};

static float samplerCb(float fHz, double sr, int /*channel*/, void* vctx)
{
    auto* c = static_cast<BellCtx*>(vctx);
    return bellDb(fHz, sr, c->gainLin, c->f0, c->q);
}

// ==============================================================================
// 工具
// ==============================================================================

// 确定性白噪声（固定种子，幅度 ±0.25）
static std::vector<float> makeNoise(int n)
{
    std::vector<float> x(static_cast<size_t>(n));
    uint32_t seed = 0x9E3779B9u;
    for (int i = 0; i < n; ++i)
    {
        seed = seed * 1664525u + 1013904223u;
        const int32_t s = static_cast<int32_t>(seed >> 8);
        x[static_cast<size_t>(i)] = (static_cast<float>(s) / static_cast<float>(1 << 24)) * 0.25f;
    }
    return x;
}

// 时域线性卷积 y[n] = Σ_j h[j] * x[n-j]（完整因果卷积，长度 n+m-1）
static std::vector<float> fullConv(const std::vector<float>& x, const float* h, int m)
{
    const int n = static_cast<int>(x.size());
    std::vector<float> y(static_cast<size_t>(n + m - 1), 0.0f);
    for (int j = 0; j < m; ++j)
    {
        const float hj = h[j];
        for (int i = 0; i < n; ++i)
            y[static_cast<size_t>(i + j)] += hj * x[static_cast<size_t>(i)];
    }
    return y;
}

// 分块处理一段信号（DAW 块 blockSize），写入 out[pos..end)
static void processBlockedInto(LinearPhaseEq& eq,
                               const std::vector<float>& x,
                               int blockSize,
                               int begin, int end,
                               std::vector<float>& out)
{
    juce::AudioBuffer<float> buf(1, blockSize);
    for (int pos = begin; pos < end; pos += blockSize)
    {
        buf.clear();
        const int chunk = std::min(blockSize, end - pos);
        buf.copyFrom(0, 0, x.data() + pos, chunk);
        eq.processBlock(buf);
        std::copy(buf.getReadPointer(0), buf.getReadPointer(0) + chunk, out.data() + pos);
    }
}

// ==============================================================================
// 主流程
// ==============================================================================

int main()
{
    constexpr double sr = 44100.0;
    constexpr int    T = 4 * LinearPhaseEq::FFT_N;  // 16384 采样
    constexpr int    M = LinearPhaseEq::getFirSize();
    constexpr int    L = LinearPhaseEq::BLOCK_L;
    constexpr int    M1 = M - 1;
    // overlap-save 有效输出段起点（丢弃前 M-1 个循环混叠段）
    constexpr int    PREFILL = LinearPhaseEq::LATENCY - LinearPhaseEq::GROUP_DELAY; // 1024

    // ---- 0. 单帧频域滤波 vs 时域线性卷积（隔离 convBlock 帧数学） ----
    {
        BellCtx ctx{ 3.1622776602f, 1000.0, 1.0 };   // +10dB
        LinearPhaseEq eq;
        juce::dsp::ProcessSpec spec{ sr, static_cast<juce::uint32>(2048), 1 };
        eq.prepare(spec);
        eq.rebuildFir(samplerCb, &ctx, sr);

        // 输入帧：历史区(M-1)全 0 + L 个新输入，补零到 N（复现 convBlock 帧组装）
        std::vector<float> x = makeNoise(L);
        std::vector<float> frame(static_cast<size_t>(M1), 0.0f);
        frame.insert(frame.end(), x.begin(), x.end());
        frame.resize(static_cast<size_t>(LinearPhaseEq::FFT_N), 0.0f);

        juce::dsp::FFT fft(LinearPhaseEq::FFT_ORDER);
        std::array<float, LinearPhaseEq::FFT_WORK> hspec{};
        std::copy(eq.getFirTaps(), eq.getFirTaps() + M, hspec.begin());
        fft.performRealOnlyForwardTransform(hspec.data(), false);

        std::array<float, LinearPhaseEq::FFT_WORK> work{};
        std::copy(frame.begin(), frame.begin() + LinearPhaseEq::FFT_N, work.begin());
        fft.performRealOnlyForwardTransform(work.data(), false);
        const int halfN = LinearPhaseEq::FFT_N / 2;
        work[0] *= hspec[0];
        for (int k = 1; k < halfN; ++k)
        {
            const size_t i2k = static_cast<size_t>(2 * k);
            const float re = work[2 * k] * hspec[i2k]     - work[2 * k + 1] * hspec[i2k + 1];
            const float im = work[2 * k] * hspec[i2k + 1] + work[2 * k + 1] * hspec[i2k];
            work[2 * k]     = re;
            work[2 * k + 1] = im;
        }
        work[LinearPhaseEq::FFT_N]     *= hspec[LinearPhaseEq::FFT_N];
        work[LinearPhaseEq::FFT_N + 1]  = 0.0f;
        fft.performRealOnlyInverseTransform(work.data());

        // 时域参考 conv(x, h)[i]，对比输出段 work[M-1+i]（= y_full[i]）
        std::vector<float> yRef = fullConv(x, eq.getFirTaps(), M);
        double maxErr = 0.0;
        for (int i = 0; i < L; ++i)
            maxErr = std::max(maxErr, std::abs(static_cast<double>(work[static_cast<size_t>(M1 + i)] - yRef[static_cast<size_t>(i)])));
        std::printf("== 单帧 overlap-save 频域滤波 vs 时域卷积：输出段 [M-1..M-1+L) max|err|=%.3e\n",
                    maxErr);
    }

    // ---- 1. 稳态：+10dB @1kHz Q=1，扫描最优对齐 lag（期望 = PREFILL，任意 blockSize） ----
    {
        BellCtx ctx{ 3.1622776602f, 1000.0, 1.0 };

        for (int bs : { 512, 1024 })
        {
            juce::dsp::ProcessSpec spec{ sr, static_cast<juce::uint32>(bs), 1 };
            LinearPhaseEq eq;
            eq.prepare(spec);
            eq.rebuildFir(samplerCb, &ctx, sr);

            std::vector<float> x = makeNoise(T);
            std::vector<float> yOla(x.size(), 0.0f);
            processBlockedInto(eq, x, bs, 0, T, yOla);
            std::vector<float> yRef = fullConv(x, eq.getFirTaps(), M);

            std::printf("== 稳态 +10dB，DAW 块 %d：扫描 lag（yOla[i+lag] vs y_full[i]）==\n", bs);
            double bestRms = 1e30; int bestLag = -1;
            for (int lag = 0; lag <= 2 * L; lag += 8)
            {
                double rms = 0.0;
                int cnt = 0;
                for (int i = 512; i < T - lag; ++i)
                {
                    const double e = static_cast<double>(yOla[static_cast<size_t>(i + lag)] - yRef[static_cast<size_t>(i)]);
                    rms += e * e;
                    ++cnt;
                }
                rms = std::sqrt(rms / cnt);
                if (rms < bestRms) { bestRms = rms; bestLag = lag; }
            }
            const double lagRms = [&]{
                double rms = 0.0; int cnt = 0;
                for (int i = 512; i < T - PREFILL; ++i)
                {
                    const double e = static_cast<double>(yOla[static_cast<size_t>(i + PREFILL)] - yRef[static_cast<size_t>(i)]);
                    rms += e * e; ++cnt;
                }
                return std::sqrt(rms / cnt);
            }();
            std::printf("  最优 lag=%d rms=%.3e   | 固定 lag=%d(=PREFILL) rms=%.3e\n",
                        bestLag, bestRms, PREFILL, lagRms);
        }
    }

    // ---- 2. 脉冲延迟：输出峰值位置应 = LATENCY（物理延迟 = 上报值） ----
    {
        BellCtx ctx{ 3.1622776602f, 1000.0, 1.0 };
        for (int bs : { 512, 1024 })
        {
            juce::dsp::ProcessSpec spec{ sr, static_cast<juce::uint32>(bs), 1 };
            LinearPhaseEq eq;
            eq.prepare(spec);
            eq.rebuildFir(samplerCb, &ctx, sr);

            std::vector<float> x(static_cast<size_t>(T), 0.0f);
            x[0] = 1.0f;   // 单位脉冲
            std::vector<float> y(x.size(), 0.0f);
            processBlockedInto(eq, x, bs, 0, T, y);

            double peak = 0.0; int peakAt = -1;
            for (int i = 0; i < T; ++i)
            {
                const double a = std::abs(static_cast<double>(y[static_cast<size_t>(i)]));
                if (a > peak) { peak = a; peakAt = i; }
            }
            std::printf("== 脉冲延迟（DAW 块 %d）：峰值位置=%d，期望 LATENCY=%d（误差 %d）==\n",
                        bs, peakAt, LinearPhaseEq::LATENCY, peakAt - LinearPhaseEq::LATENCY);
        }
    }

    // ---- 3. 参数跳变：全程 +10dB 对照 vs 前半 flat、split 处切到 +10dB ----
    {
        juce::dsp::ProcessSpec spec{ sr, static_cast<juce::uint32>(512), 1 };
        constexpr int split = T / 2;

        // eqA：全程 +10dB（不切换）
        LinearPhaseEq eqA;
        eqA.prepare(spec);
        BellCtx bigCtx{ 3.1622776602f, 1000.0, 1.0 };
        eqA.rebuildFir(samplerCb, &bigCtx, sr);
        std::vector<float> x = makeNoise(T);
        std::vector<float> yConst(x.size(), 0.0f);
        processBlockedInto(eqA, x, 512, 0, T, yConst);

        // eqB：前半 flat，split 处切到 +10dB
        LinearPhaseEq eqB;
        eqB.prepare(spec);
        BellCtx flatCtx{ 1.0f, 1000.0, 1.0 };
        eqB.rebuildFir(samplerCb, &flatCtx, sr);
        std::vector<float> yOla(x.size(), 0.0f);
        processBlockedInto(eqB, x, 512, 0, split, yOla);
        eqB.rebuildFir(samplerCb, &bigCtx, sr);   // 切换
        processBlockedInto(eqB, x, 512, split, T, yOla);

        // 两路均预填充 PREFILL，输出流内容对齐（yB[n] 对应输入 n-PREFILL-α）
        // 切换效应出现在输出流位置 split + PREFILL（输入 split 处的内容到达）
        const int swStart = split + PREFILL;
        const int swEnd   = T - 16;
        std::printf("\n== 参数切换：flat→+10dB（切换后误差 e = yB - yA，信号峰值 %.3f）==\n",
                    *std::max_element(yConst.begin(), yConst.end()));
        for (int w = 0; swStart + w * 128 < swEnd; ++w)
        {
            const int a = swStart + w * 128;
            const int b = std::min(a + 128, swEnd);
            double m = 0.0;
            for (int i = a; i < b; ++i)
                m = std::max(m, std::abs(static_cast<double>(yOla[static_cast<size_t>(i)] - yConst[static_cast<size_t>(i)])));
            std::printf("  切换后 %4d-%4d 采样: max|e| = %.3e\n", a - swStart, b - swStart, m);
        }
    }

    // ---- 4. OLA tail 失配量化（原子切换 flat→+10dB） ----
    // 手工 OLA（跨块 tail 累积）vs ground truth（每帧用该帧核的全局因果卷积）。
    // OLA 在核变化的切换块有 tail 失配误差；overlap-save 每帧独立、无此误差。
    {
        constexpr int nBlocks = T / L;
        BellCtx flatCtx{ 1.0f, 1000.0, 1.0 };
        BellCtx bigCtx{ 3.1622776602f, 1000.0, 1.0 };
        LinearPhaseEq eqFlat, eqBig;
        for (auto* eq : { &eqFlat, &eqBig })
        {
            juce::dsp::ProcessSpec s{ sr, 1024, 1 };
            eq->prepare(s);
        }
        eqFlat.rebuildFir(samplerCb, &flatCtx, sr);
        eqBig.rebuildFir(samplerCb, &bigCtx, sr);
        const float* hFlat = eqFlat.getFirTaps();
        const float* hBig  = eqBig.getFirTaps();

        std::vector<float> x = makeNoise(T);
        const int splitBlock = (T / 2) / L;

        // 手工 OLA：每块用核（flat 或 big），tail 跨块累积（旧核 tail + 新核块输出）。
        // tail 长度 = M-1（FIR 全部余量），随 FIR 尺寸自适应
        constexpr int M1 = M - 1;
        std::vector<float> yOla(static_cast<size_t>(T), 0.0f);
        std::vector<float> tail(static_cast<size_t>(M1), 0.0f);
        for (int b = 0; b < nBlocks; ++b)
        {
            const float* h = (b < splitBlock) ? hFlat : hBig;
            std::vector<float> xb(x.begin() + b * L, x.begin() + (b + 1) * L);
            std::vector<float> yb = fullConv(xb, h, M);   // 长度 M1 + L
            for (int i = 0; i < L; ++i)
                yOla[static_cast<size_t>(b * L + i)] = yb[static_cast<size_t>(i)] + tail[static_cast<size_t>(i)];
            // 新 tail = 旧 tail 后段 + 本块卷积的余量段
            std::vector<float> newTail(static_cast<size_t>(M1), 0.0f);
            for (int i = 0; i < M1 - L; ++i)
                newTail[static_cast<size_t>(i)] = tail[static_cast<size_t>(i + L)] + yb[static_cast<size_t>(L + i)];
            for (int i = M1 - L; i < M1; ++i)
                newTail[static_cast<size_t>(i)] = yb[static_cast<size_t>(L + i)];
            tail = std::move(newTail);
        }

        // ground truth：每帧用该帧核的全局因果卷积（= overlap-save 的精确语义）
        std::vector<float> yGT(static_cast<size_t>(T), 0.0f);
        for (int b = 0; b < nBlocks; ++b)
        {
            const float* h = (b < splitBlock) ? hFlat : hBig;
            for (int i = 0; i < L; ++i)
            {
                float acc = 0.0f;
                const int n = b * L + i;
                for (int j = 0; j < M; ++j)
                    if (n - j >= 0)
                        acc += h[j] * x[static_cast<size_t>(n - j)];
                yGT[static_cast<size_t>(n)] = acc;
            }
        }

        std::printf("\n== 手工 OLA tail 失配（原子切换 flat→+10dB；ground truth=每帧核全卷积）==\n");
        for (int w = -2; w < 6; ++w)
        {
            const int a = splitBlock * L + w * L;
            double m = 0.0;
            for (int i = a; i < a + L; ++i)
                if (i >= 0 && i < T)
                    m = std::max(m, std::abs(static_cast<double>(yOla[static_cast<size_t>(i)] - yGT[static_cast<size_t>(i)])));
            const char* tag = (w < 0) ? "切换前" : (w == 0 ? "切换块" : "切换后");
            std::printf("  块 %+d（%s）: max|OLA 失配| = %.3e\n", w, tag, m);
        }
    }

    return 0;
}
