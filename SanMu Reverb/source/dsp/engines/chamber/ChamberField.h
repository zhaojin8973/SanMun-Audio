#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// 独立回声室：反射簇、分布式注入、十二路有耗散射与双位置拾音。
// 声学启发的算法，不是实测空间或其他混响类别的复刻。
namespace sanmu_chamber
{
inline constexpr double pi = 3.14159265358979323846;
inline constexpr const char* revision = "Chamber-C1";
struct Settings
{
    float decay = 2, predelay = 15, highHz = 6000, highDb = -16, bassHz = 300, bassMult = 1;
    float size = 100, attack = 35, early = 100, late = 100, rate = .5f, depth = 20;
    float lowCut = 20, highCut = 9000;
};

// 控制目标每三十二采样更新，读头和系数逐采样插值，避免控制周期的阶梯跳变。
class Ramp
{
public:
    void set(float target) noexcept
    {
        if (!ready) { value = target; step = 0; ready = true; remaining = 0; }
        else { destination = target; step = (target - value) / 32; remaining = 32; }
    }
    float next() noexcept
    {
        if (remaining > 0) { value += step; if (--remaining == 0) value = destination; }
        return value;
    }
    void reset() noexcept { ready = false; value = step = destination = 0; remaining = 0; }
private:
    float value = 0, step = 0, destination = 0;
    int remaining = 0;
    bool ready = false;
};

class Delay
{
public:
    void prepare(double sr, double seconds)
    {
        limit = static_cast<int>(std::ceil(sr * seconds)) + 16;
        std::size_t capacity = 1;
        while (capacity < static_cast<std::size_t>(limit + 4)) capacity <<= 1;
        data.assign(capacity, 0); mask = capacity - 1; cursor = 0;
    }
    void reset() noexcept { std::fill(data.begin(), data.end(), 0); cursor = 0; }
    void write(float x) noexcept { data[cursor] = x; cursor = (cursor + 1) & mask; }
    // 零对应最近写入的样本；反馈先读后写时减一，传播时长保持正确。
    float read(float samples) const noexcept
    {
        const float safe = std::clamp(samples, 0.f, static_cast<float>(limit));
        const auto whole = static_cast<std::size_t>(safe);
        const float fraction = safe - static_cast<float>(whole);
        const auto i = (cursor - 1 - whole) & mask;
        return data[i] + fraction * (data[(i - 1) & mask] - data[i]);
    }
private:
    std::vector<float> data;
    std::size_t mask = 0, cursor = 0;
    int limit = 1;
};

class Pole
{
public:
    void set(double sr, float hz) noexcept
    {
        const double g = std::tan(pi * std::clamp(static_cast<double>(hz), 1., sr * .45) / sr);
        coefficient.set(static_cast<float>(g / (1 + g)));
    }
    float low(float x) noexcept
    {
        const float v = (x - state) * coefficient.next(), y = state + v;
        state = y + v;
        return y;
    }
    float high(float x) noexcept { return x - low(x); }
    void reset() noexcept { state = 0; coefficient.reset(); }
private:
    Ramp coefficient;
    float state = 0;
};

// 每段存储均有按时间计算的损耗，全通内部的传播也计入衰减。
// 低频额外吸收以平滑倍率作用，不用高Q陷波或输入相关的动态处理。
class Loss
{
public:
    void set(double sr, float seconds, const Settings& p, int style) noexcept
    {
        const float g = std::exp(-6.9077553f * seconds / p.decay);
        const float bassTime = p.decay * p.bassMult / (style == 0 ? 1.32f : 1.42f);
        mid.set(g);
        bass.set(std::exp(-6.9077553f * seconds / bassTime));
        top.set(std::exp(-6.9077553f * seconds / (p.decay * std::pow(10.f, p.highDb / 65.f))) / g);
        lowPole.set(sr, p.bassHz);
        highPole.set(sr, p.highHz);
    }
    float process(float x) noexcept
    {
        const float m = mid.next(), l = bass.next(), h = top.next();
        const float y = m * x + (l - m) * lowPole.low(x);
        return h * y + (1 - h) * highPole.low(y);
    }
    void reset() noexcept { mid.reset(); bass.reset(); top.reset(); lowPole.reset(); highPole.reset(); }
private:
    Ramp mid, bass, top;
    Pole lowPole, highPole;
};

class Diffuser
{
public:
    void prepare(double sr) { memory.prepare(sr, .05); }
    void set(double sr, float seconds, float coefficient, const Settings& p, int style)
    {
        const float length = std::max(1.f, std::round(static_cast<float>(sr) * seconds));
        samples.set(length); gain.set(coefficient);
        loss.set(sr, length / static_cast<float>(sr), p, style);
    }
    float process(float x) noexcept
    {
        const float d = loss.process(memory.read(samples.next() - 1)), g = gain.next();
        const float v = x + g * d;
        memory.write(v);
        return d - g * v;
    }
    void reset() noexcept { memory.reset(); samples.reset(); gain.reset(); loss.reset(); }
private:
    Delay memory;
    Ramp samples, gain;
    Loss loss;
};

using Frame = std::array<float, 12>;
using Matrix = std::array<Frame, 12>;
inline Matrix makeScattering()
{
    Matrix result {};
    // 归一化余弦基底加行重排，十二条支路充分耦合且不增加总能量。
    for (std::size_t row = 0; row < 12; ++row)
        for (std::size_t col = 0; col < 12; ++col)
        {
            const auto frequency = (row * 5 + 3) % 12;
            result[row][col] = static_cast<float>((frequency == 0 ? std::sqrt(1. / 12) : std::sqrt(2. / 12))
                * std::cos(pi * (static_cast<double>(col) + .5) * static_cast<double>(frequency) / 12));
        }
    return result;
}

class Field
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        scattering = makeScattering();
        for (auto& d : pre) d.prepare(sr, .26);
        for (auto& d : reflections) d.prepare(sr, .13);
        for (auto& d : excitation) d.prepare(sr, .25);
        for (auto& d : lines) d.prepare(sr, .1);
        for (auto& channel : earlyDiffusers) for (auto& d : channel) d.prepare(sr);
        for (auto& channel : seedDiffusers) for (auto& d : channel) d.prepare(sr);
        for (auto& d : loopDiffusers) d.prepare(sr);
        for (std::size_t i = 0; i < 12; ++i) wallWeights[i] = .28f * std::pow(.91f, static_cast<float>(i));
        reset();
    }
    void reset() noexcept
    {
        for (auto& d : pre) d.reset();
        for (auto& d : reflections) d.reset();
        for (auto& d : excitation) d.reset();
        for (auto& d : lines) d.reset();
        for (auto& c : earlyDiffusers) for (auto& d : c) d.reset();
        for (auto& c : seedDiffusers) for (auto& d : c) d.reset();
        for (auto& d : loopDiffusers) d.reset();
        for (auto& f : losses) f.reset();
        for (auto* bank : { &speakerHP, &speakerLP, &earlyLP, &outputHP, &outputLP })
            for (auto& f : *bank) f.reset();
        for (auto& v : lineSamples) v.reset();
        for (auto& v : seedSamples) v.reset();
        for (auto& c : pathSamples) for (auto& v : c) v.reset();
        for (auto* v : { &preSamples, &earlyGain, &lateGain, &injection, &modDepth, &phaseStep }) v->reset();
        phase = 0;
    }
    void configure(const Settings& p, int style)
    {
        const float scale = .46f + .54f * p.size * .01f;
        const float attack = p.attack * .01f, early = p.early * .01f, late = p.late * .01f;
        const bool velvet = style == 0;
        preSamples.set(static_cast<float>(sr) * p.predelay * .001f);
        earlyGain.set((velvet ? .34f : .48f) * (1 - .74f * attack));
        lateGain.set((velvet ? 2.4f : 2.3f) * (1 + .08f * attack));
        phaseStep.set(static_cast<float>(2 * pi * p.rate / sr));
        modDepth.set(static_cast<float>(sr) * p.depth * .00000055f);
        constexpr Frame velvetTimes { .0193f, .0227f, .0269f, .0311f, .0347f, .0389f,
                                     .0433f, .0479f, .0523f, .0571f, .0617f, .0671f };
        constexpr Frame stoneTimes { .0137f, .0181f, .0239f, .0283f, .0361f, .0419f,
                                    .0491f, .0553f, .0637f, .0719f, .0797f, .0883f };
        double averageTime = 0;
        for (std::size_t i = 0; i < 12; ++i)
        {
            const float t = (velvet ? velvetTimes[i] : stoneTimes[i]) * scale;
            const float samples = std::max(4.f, std::round(t * static_cast<float>(sr)));
            lineSamples[i].set(samples);
            losses[i].set(sr, samples / static_cast<float>(sr), p, style);
            const float diffusionTime = (.0017f + static_cast<float>((i * 7) % 13) * .00043f) * scale;
            loopDiffusers[i].set(sr, diffusionTime, .10f + .62f * late, p, style);
            averageTime += samples / sr + diffusionTime;
            // 十二个不同到达时间逐渐展开，Attack不检测音量，也不门控尾音。
            const float fraction = (static_cast<float>((i * 5) % 12) + .5f) / 12;
            const float onset = (.003f + fraction * (velvet ? .011f : .019f)
                + attack * (.014f + fraction * (velvet ? .070f : .093f))) * scale;
            seedSamples[i].set(static_cast<float>(sr) * onset);
        }
        injection.set(static_cast<float>(std::sqrt(1 - std::exp(-13.81551056 * averageTime / (12 * p.decay)))));
        constexpr Frame wallTimes { .0053f, .0071f, .0097f, .0129f, .0163f, .0209f,
                                    .0247f, .0293f, .0359f, .0421f, .0473f, .0539f };
        for (std::size_t c = 0; c < 2; ++c)
        {
            speakerHP[c].set(sr, velvet ? 92.f : 115.f);
            speakerLP[c].set(sr, velvet ? 11500.f : 15500.f);
            earlyLP[c].set(sr, velvet ? 6400.f : 9600.f);
            outputHP[c].set(sr, p.lowCut); outputLP[c].set(sr, p.highCut);
            for (std::size_t i = 0; i < 12; ++i)
            {
                // 两侧首反射同到达；之后以不同传播路径建立宽度，不做左右反相。
                const float offset = i == 0 ? 0 : (c == 0 ? 1.f : -1.f) * (.00017f + static_cast<float>(i % 3) * .00013f);
                const float wall = velvet ? wallTimes[i] : wallTimes[i] * (1.1f + static_cast<float>(i % 4) * .065f);
                pathSamples[c][i].set(static_cast<float>(sr) * (wall + offset) * scale);
            }
            for (std::size_t j = 0; j < 4; ++j)
            {
                const float asymmetry = static_cast<float>(c) * (.00013f + .00009f * static_cast<float>(j));
                const float t = (.00083f + static_cast<float>(j * j + j) * .00043f + asymmetry) * scale;
                earlyDiffusers[c][j].set(sr, t, (velvet ? .70f : .60f) * early, p, style);
                seedDiffusers[c][j].set(sr, t * (velvet ? 1.71f : 1.23f), .16f + .56f * late, p, style);
            }
        }
    }
    std::array<float, 2> process(float left, float right) noexcept
    {
        const std::array<float, 2> input { left, right };
        std::array<float, 2> er {};
        const float predelay = preSamples.next();
        for (std::size_t c = 0; c < 2; ++c)
        {
            pre[c].write(speakerLP[c].low(speakerHP[c].high(input[c])));
            reflections[c].write(pre[c].read(predelay));
        }
        for (std::size_t c = 0; c < 2; ++c)
        {
            for (std::size_t i = 0; i < 12; ++i)
            {
                const float position = pathSamples[c][i].next();
                er[c] += wallWeights[i] * (.78f * reflections[c].read(position) + .22f * reflections[1 - c].read(position));
            }
            for (auto& d : earlyDiffusers[c]) er[c] = d.process(er[c]);
            er[c] = earlyLP[c].low(er[c]);
            float seed = reflections[c].read(0);
            for (auto& d : seedDiffusers[c]) seed = d.process(seed);
            excitation[c].write(seed);
        }
        Frame state {};
        const float depth = modDepth.next(), inject = injection.next();
        for (std::size_t i = 0; i < 12; ++i)
        {
            const float modulation = depth * static_cast<float>(std::sin(phase + static_cast<double>(i) * 2.399963));
            state[i] = loopDiffusers[i].process(losses[i].process(lines[i].read(lineSamples[i].next() + modulation - 1)));
        }
        float lateL = 0, lateR = 0;
        // 分离的正权拾音区域加少量共同区域；矩阵内部符号不用于输出反相扩宽。
        for (std::size_t i = 0; i < 4; ++i) { lateL += .45f * state[i]; lateR += .45f * state[i + 4]; }
        for (std::size_t i = 8; i < 12; ++i) { lateL += .20f * state[i]; lateR += .20f * state[i]; }
        for (std::size_t i = 0; i < 12; ++i)
        {
            float feedback = 0;
            for (std::size_t j = 0; j < 12; ++j) feedback += scattering[i][j] * state[j];
            const float time = seedSamples[i].next();
            const float a = (i & 1u) == 0 ? .288675135f : -.288675135f;
            const float b = (i % 4) < 2 ? .288675135f : -.288675135f;
            lines[i].write(feedback + inject * (a * excitation[0].read(time) + b * excitation[1].read(time)));
        }
        phase += phaseStep.next();
        if (phase >= 2 * pi) phase -= 2 * pi;
        // 默认脉冲相对已认可Plate低约2.5 dB；固定输出标定，不改变反馈或频谱。
        constexpr float calibration = 1.333333333f;
        const float e = calibration * earlyGain.next(), l = calibration * lateGain.next();
        return { outputLP[0].low(outputHP[0].high(e * er[0] + l * lateL)),
                 outputLP[1].low(outputHP[1].high(e * er[1] + l * lateR)) };
    }
private:
    double sr = 48000, phase = 0;
    Matrix scattering {};
    Frame wallWeights {};
    std::array<Delay, 2> pre, reflections, excitation;
    std::array<Delay, 12> lines;
    std::array<std::array<Diffuser, 4>, 2> earlyDiffusers, seedDiffusers;
    std::array<Diffuser, 12> loopDiffusers;
    std::array<Loss, 12> losses;
    std::array<Pole, 2> speakerHP, speakerLP, earlyLP, outputHP, outputLP;
    std::array<Ramp, 12> lineSamples, seedSamples;
    std::array<std::array<Ramp, 12>, 2> pathSamples;
    Ramp preSamples, earlyGain, lateGain, injection, modDepth, phaseStep;
};
}
