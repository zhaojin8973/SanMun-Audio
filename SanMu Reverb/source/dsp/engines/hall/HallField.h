#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// 独立 Hall 声场模块。这里不依赖项目已有的延迟、全通、矩阵或阻尼原语。
namespace sanmu_hall
{
inline constexpr const char* revision = "Hall-0906E";
constexpr std::size_t channels = 16;
using Frame = std::array<float, channels>;
constexpr double pi = 3.14159265358979323846;

class Delay
{
public:
    void prepare(int maximum)
    {
        limit = std::max(1, maximum);
        std::size_t capacity = 1;
        while (capacity < static_cast<std::size_t>(limit + 4)) capacity <<= 1;
        samples.assign(capacity, 0.0f);
        mask = capacity - 1;
        cursor = 0;
    }
    void reset() { std::fill(samples.begin(), samples.end(), 0.0f); cursor = 0; }
    void write(float value) noexcept
    {
        samples[cursor] = value;
        cursor = (cursor + 1) & mask;
    }
    // 写后读：零延迟为刚写入的样本。线性插值不放大两个读点的峰值。
    float read(float delay) const noexcept
    {
        delay = std::clamp(delay, 0.0f, static_cast<float>(limit));
        const auto integer = static_cast<std::size_t>(delay);
        const float fraction = delay - static_cast<float>(integer);
        const auto index = (cursor - 1 - integer) & mask;
        return samples[index] + fraction * (samples[(index - 1) & mask] - samples[index]);
    }
private:
    std::vector<float> samples;
    std::size_t cursor = 0, mask = 0;
    int limit = 1;
};

// 双线性变换的一阶低通；用互补高低通构造单调架式衰减。
class Pole
{
public:
    void set(double rate, float hz) noexcept
    {
        const double g = std::tan(pi * std::clamp(static_cast<double>(hz), 1.0, rate * 0.45) / rate);
        coefficient = static_cast<float>(g / (1.0 + g));
    }
    float low(float x) noexcept
    {
        const float v = (x - state) * coefficient;
        const float y = state + v;
        state = y + v;
        return y;
    }
    float high(float x) noexcept { return x - low(x); }
    void reset() noexcept { state = 0.0f; }
private:
    float coefficient = 0.0f, state = 0.0f;
};

// 仅对晚期读出做轻微钟形削减（中心最深、向两侧平滑回到零）。
// 系数按 RBJ 公式计算：https://www.w3.org/TR/audio-eq-cookbook/
// 不进入反馈环，不改变早期反射、低频衰减倍率或按输入电平触发压缩。
class LateBody
{
public:
    void prepare(double rate) noexcept
    {
        // 0906D 将曲线向250Hz延展，同时保留150～175Hz附近的厚度。
        constexpr double centreHz = 225.0, q = 1.10, gainDb = -2.5;
        const double a = std::pow(10.0, gainDb / 40.0);
        const double w = 2.0 * pi * centreHz / rate;
        const double alpha = std::sin(w) / (2.0 * q);
        const double denominator = 1.0 + alpha / a;
        b0 = (1.0 + alpha * a) / denominator;
        b1 = -2.0 * std::cos(w) / denominator;
        b2 = (1.0 - alpha * a) / denominator;
        a1 = b1;
        a2 = (1.0 - alpha / a) / denominator;
        reset();
    }
    void reset() noexcept { z1 = z2 = 0.0; }
    float process(float input) noexcept
    {
        // 低频与高采样率下保留双精度状态，避免相近系数相减造成可见误差。
        const double output = b0 * input + z1;
        z1 = b1 * input - a1 * output + z2;
        z2 = b2 * input - a2 * output;
        return static_cast<float>(output);
    }
private:
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
};

inline void hadamard(Frame& v) noexcept
{
    for (std::size_t stride = 1; stride < channels; stride *= 2)
        for (std::size_t base = 0; base < channels; base += stride * 2)
            for (std::size_t i = 0; i < stride; ++i)
            {
                const float a = v[base + i], b = v[base + i + stride];
                v[base + i] = a + b;
                v[base + i + stride] = a - b;
            }
    for (auto& x : v) x *= 0.25f;
}

// 四层成对旋转的乘积。全部角度均正交，不插值矩阵，不按音频幅度归一化。
class Scattering
{
public:
    void set(float amount) noexcept
    {
        const float angle = std::clamp(amount, 0.0f, 1.0f) * static_cast<float>(pi * 0.25);
        c = std::cos(angle); s = std::sin(angle);
    }
    void process(Frame& v, bool reflected = false) const noexcept
    {
        for (std::size_t stride = 1; stride < channels; stride *= 2)
            for (std::size_t base = 0; base < channels; base += stride * 2)
                for (std::size_t i = 0; i < stride; ++i)
                {
                    const float a = v[base + i], b = v[base + i + stride];
                    v[base + i] = c * a + s * b;
                    v[base + i + stride] = (c * b - s * a) * (reflected ? -1.0f : 1.0f);
                }
    }
private:
    float c = 1.0f, s = 0.0f;
};

class Diffuser
{
public:
    static constexpr std::size_t stages = 6;
    void prepare(double rate, std::size_t count = stages,
                 std::array<float, stages> rangesMs = { 3.7f, 5.9f, 9.3f, 14.9f, 23.1f, 36.7f },
                 uint32_t seed = 0x71f205ab)
    {
        activeStages = std::clamp(count, std::size_t(1), stages);
        matrix.set(1.0f);
        // 不等长级联让起止包络平缓。分层取样时延固定种子，重置与块大小不改变声音。
        uint32_t random = seed;
        for (std::size_t stage = 0; stage < activeStages; ++stage)
            for (std::size_t line = 0; line < channels; ++line)
            {
                random ^= random << 13; random ^= random >> 17; random ^= random << 5;
                const float jitter = static_cast<float>(random >> 8) / 16777216.0f;
                const auto stratum = (line * 5 + stage * 3) % channels;
                lengths[stage][line] = std::round(static_cast<float>(rate) * 0.001f * rangesMs[stage]
                    * (static_cast<float>(stratum) + 0.15f + 0.7f * jitter) / 16.0f);
                delays[stage][line].prepare(static_cast<int>(lengths[stage][line] * 2.5f + 8));
                signs[stage][line] = (random & 1) ? 1.0f : -1.0f;
            }
    }
    void reset() { for (auto& stage : delays) for (auto& line : stage) line.reset(); }
    void setDiffusion(float amount) noexcept { matrix.set(amount); }
    Frame process(Frame frame, float spread) noexcept
    {
        for (std::size_t stage = 0; stage < activeStages; ++stage)
        {
            for (std::size_t line = 0; line < channels; ++line)
            {
                delays[stage][line].write(frame[line]);
                frame[line] = signs[stage][line] * delays[stage][line].read(lengths[stage][line] * spread);
            }
            // 带符号的正交蝶形：满值等价于归一化 Hadamard，低值减少分叉路径。
            // 通过路径能量的分散程度调密度，避免仅缩短时延后中段仍已完全密集。
            matrix.process(frame, true);
        }
        return frame;
    }
private:
    std::size_t activeStages = stages;
    Scattering matrix;
    std::array<std::array<Delay, channels>, stages> delays;
    std::array<Frame, stages> lengths {}, signs {};
};

class Loss
{
public:
    void set(double rate, float seconds, float decay, float bassHz, float bassMult,
             float highHz, float highDb) noexcept
    {
        // 每条线按自己的传播时长计算绝对增益，三个频带上限都严格小于一。
        middle = std::exp(-6.90775527898f * seconds / decay);
        bottom = std::exp(-6.90775527898f * seconds / (decay * bassMult));
        const float highDecay = decay * std::pow(10.0f, highDb / 40.0f);
        const float top = std::exp(-6.90775527898f * seconds / highDecay);
        topRatio = top / middle;
        bass.set(rate, bassHz); treble.set(rate, highHz);
    }
    float process(float x) noexcept
    {
        const float low = bass.low(x);
        const float shelf = middle * x + (bottom - middle) * low;
        return topRatio * shelf + (1.0f - topRatio) * treble.low(shelf);
    }
    void reset() noexcept { bass.reset(); treble.reset(); }
private:
    Pole bass, treble;
    float middle = 0.0f, bottom = 0.0f, topRatio = 1.0f;
};

struct Settings
{
    float decay = 3.99f, predelay = 20.0f, highHz = 6000.0f, highDb = -24.0f;
    float bassHz = 300.0f, bassMult = 1.5f, size = 100.0f, attack = 50.0f;
    float early = 100.0f, late = 100.0f, rate = 2.53f, depth = 38.0f;
    float lowCut = 10.0f, highCut = 8010.0f;
};

// 零扩散时保持原居中输入的总早期能量，并让左右输入各自保持方向。
// 5%以上完全走原读出，避免改变已经听审认可的常用设置。
class EarlyReadout
{
public:
    void set(float early) noexcept
    {
        const float t = std::clamp(early * .2f, 0.0f, 1.0f);
        blend = t * t * (3.0f - 2.0f * t);
    }
    float process(float original, float input) const noexcept
    {
        if (blend >= 1.0f) return original;
        constexpr float centredGain = -0.104231750500987f;
        const float centred = centredGain * input;
        return centred + blend * (original - centred);
    }
private:
    float blend = 1.0f;
};

class Field
{
public:
    void prepare(double rate)
    {
        sampleRate = rate;
        for (auto& delay : predelay) delay.prepare(static_cast<int>(rate * 0.26));
        diffuser.prepare(rate);
        earlyDiffuser.prepare(rate);
        for (auto& filter : lateBody) filter.prepare(rate);
        constexpr std::array<int, channels> samples48k {
            1429, 2203, 1667, 2903, 1879, 3253, 2081, 2477,
            1559, 2683, 1787, 3089, 2357, 3469, 1999, 3691
        };
        for (std::size_t i = 0; i < channels; ++i)
        {
            baseSeconds[i] = static_cast<float>(samples48k[i]) / 48000.0f;
            tank[i].prepare(static_cast<int>(baseSeconds[i] * rate * 1.1 + rate * 0.002));
            arrival[i].prepare(static_cast<int>(rate * .17));
        }
        reset();
    }
    void reset()
    {
        for (auto& delay : predelay) delay.reset();
        diffuser.reset();
        earlyDiffuser.reset();
        for (auto& filter : lateBody) filter.reset();
        for (std::size_t i = 0; i < channels; ++i)
        {
            tank[i].reset(); arrival[i].reset(); loss[i].reset(); phase[i] = static_cast<double>(i) * 0.391;
        }
        for (auto* filters : { &inputLow, &inputHigh, &outputLow, &outputHigh })
            for (auto& filter : *filters) filter.reset();
    }
    void configure(const Settings& p) noexcept
    {
        sizeScale = 0.45f + 0.0055f * p.size;
        // 独立的短扩散支路负责早期反射；六级长扩散及反馈散射负责晚期密度。
        // 避免 Early=100 时，无论 Late 怎样调节都先得到同样的密集输入。
        earlySpread = p.early * .007f * (.75f + sizeScale * .25f);
        earlyReadout.set(p.early);
        lateSpread = .75f + sizeScale * .25f;
        // 六级扩散的路径数增长很快，用幂曲线把可听变化展开到旋钮中段。
        const float lateAmount = std::pow(p.late * .01f, 2.5f);
        diffuser.setDiffusion(lateAmount);
        const float attack = p.attack * .01f;
        earlyGain = .18f + 1.62f * std::pow(1.0f - attack, .7f);
        // 早期存在感消退的同时，适量把能量转向晚期，避免远近感退化成音量旋钮。
        lateGain = std::sqrt(.7f + .6f * attack);
        preSamples = (p.predelay * 0.001f + 0.002f) * static_cast<float>(sampleRate);
        scattering.set(lateAmount);
        modulationSamples = p.depth * 0.0000035f * static_cast<float>(sampleRate);
        modRate = p.rate;
        for (std::size_t i = 0; i < channels; ++i)
        {
            lineSamples[i] = std::round(baseSeconds[i] * sizeScale * static_cast<float>(sampleRate));
            // 独立的晚期注入时序：每条线只推迟一次，不增加反馈周期或改变 Decay。
            // 即使 Early=0，Attack 仍改变反射场的建立；固定参数下始终线性。
            arrivalSamples[i] = static_cast<float>(sampleRate) * .16f * attack * attack * sizeScale
                * (.4f + .6f * static_cast<float>((i * 7) % channels) / 15.0f);
            loss[i].set(sampleRate, lineSamples[i] / static_cast<float>(sampleRate), p.decay,
                        p.bassHz, p.bassMult, p.highHz, p.highDb);
        }
        // 固定、轻微的输入带宽约束负责柔和首圈；输出 EQ 保持独立语义。
        for (std::size_t c = 0; c < 2; ++c)
        {
            inputLow[c].set(sampleRate, 7800.0f);
            inputHigh[c].set(sampleRate, 8.0f);
            outputLow[c].set(sampleRate, p.highCut);
            outputHigh[c].set(sampleRate, p.lowCut);
        }
    }
    std::array<float, 2> process(float left, float right) noexcept
    {
        const std::array<float, 2> input { left, right };
        std::array<float, 2> filtered {};
        Frame excitation {};
        constexpr float split = 0.3535533905932738f;
        for (std::size_t c = 0; c < 2; ++c)
        {
            predelay[c].write(input[c]);
            const float x = inputLow[c].low(inputHigh[c].high(predelay[c].read(preSamples)));
            filtered[c] = x;
            for (std::size_t i = 0; i < 8; ++i) excitation[c * 8 + i] = x * split;
        }
        const Frame early = earlyDiffuser.process(excitation, earlySpread);
        Frame cloud = diffuser.process(excitation, lateSpread);
        for (std::size_t i = 0; i < channels; ++i)
        {
            arrival[i].write(cloud[i]);
            cloud[i] = arrival[i].read(arrivalSamples[i]);
        }
        Frame returning {};
        for (std::size_t i = 0; i < channels; ++i)
        {
            phase[i] += 2.0 * pi * modRate * (0.73 + 0.037 * static_cast<double>(i)) / sampleRate;
            if (phase[i] >= 2.0 * pi) phase[i] -= 2.0 * pi;
            // 先读后写需要减一；独立连续相位避免主相位回卷时的跳变。
            const float offset = static_cast<float>(std::sin(phase[i])) * modulationSamples;
            returning[i] = tank[i].read(lineSamples[i] + offset - 1.0f);
        }
        Frame feedback = returning;
        for (std::size_t i = 0; i < channels; ++i) feedback[i] = loss[i].process(feedback[i]);
        scattering.process(feedback);
        for (std::size_t i = 0; i < channels; ++i) tank[i].write(cloud[i] + feedback[i]);

        std::array<float, 2> output {};
        // 两个等范数的独立读出向量。正负号只选择声场方向，并非 R=-L。
        for (std::size_t i = 0; i < 8; ++i)
        {
            const float signL = (i & 1) ? -split : split;
            const float signR = (i & 2) ? -split : split;
            output[0] += signL * returning[i];
            output[1] += signR * returning[i + 8];
        }
        // Attack 增大时减少靠前的反射，晚期能量由上面的注入时序逐步铺开。
        // 输出标定只位于反馈环外，不改变衰减或按输入电平做动态归一化。
        constexpr float wetGain = 1.72f;
        output[0] = wetGain * (lateGain * lateBody[0].process(output[0]) + earlyGain * earlyReadout.process(early[3], filtered[0]));
        output[1] = wetGain * (lateGain * lateBody[1].process(output[1]) + earlyGain * earlyReadout.process(early[12], filtered[1]));
        for (std::size_t c = 0; c < 2; ++c)
            output[c] = outputLow[c].low(outputHigh[c].high(output[c]));
        return output;
    }
private:
    double sampleRate = 48000.0;
    std::array<Delay, 2> predelay;
    Diffuser diffuser, earlyDiffuser;
    EarlyReadout earlyReadout;
    std::array<Delay, channels> tank, arrival;
    std::array<Loss, channels> loss;
    Scattering scattering;
    std::array<Pole, 2> inputLow, inputHigh, outputLow, outputHigh;
    std::array<LateBody, 2> lateBody;
    Frame baseSeconds {}, lineSamples {}, arrivalSamples {};
    std::array<double, channels> phase {};
    float sizeScale = 1.0f, earlySpread = 1.0f, lateSpread = 1.0f, preSamples = 0.0f;
    float earlyGain = 0.0f, lateGain = 1.0f;
    float modulationSamples = 0.0f, modRate = 0.0f;
};
} // namespace sanmu_hall
