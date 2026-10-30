#pragma once
#include "HallField.h"

namespace sanmu_hall
{
// 教堂独立声场：较长的传播路径、宽时间分布的注入，以及全场双向读出。
// 基础延迟和损耗类共享，全部声学状态与 Concert Hall 分离。
class CathedralField
{
public:
    static constexpr const char* revision = "Cathedral-C1";
    void prepare(double rate)
    {
        sampleRate = rate;
        for (auto& d : predelay) d.prepare(static_cast<int>(rate * .26));
        cloudDiffuser.prepare(rate, 6, {5.1f, 11.3f, 23.9f, 47.3f, 79.1f, 127.7f}, 0x6137ac29);
        earlyDiffuser.prepare(rate, 6, {7.0f, 13.0f, 23.0f, 37.0f, 61.0f, 97.0f}, 0x35ac9271);
        constexpr std::array<int, channels> samples48k {
            2909, 4211, 3571, 5059, 3911, 6211, 4517, 6971,
            3251, 5471, 3821, 7517, 4933, 8291, 5801, 9049
        };
        for (std::size_t i = 0; i < channels; ++i)
        {
            baseSeconds[i] = static_cast<float>(samples48k[i]) / 48000.0f;
            tank[i].prepare(static_cast<int>(baseSeconds[i] * rate * 1.65 + rate * .004));
            arrival[i].prepare(static_cast<int>(rate * .36));
        }
        for (auto& f : body) f.prepare(rate);
        reset();
    }
    void reset()
    {
        for (auto& d : predelay) d.reset();
        cloudDiffuser.reset(); earlyDiffuser.reset();
        for (std::size_t i = 0; i < channels; ++i)
        {
            tank[i].reset(); arrival[i].reset(); loss[i].reset();
            phase[i] = static_cast<double>(i) * .719;
        }
        for (auto* filters : {&inputLow, &inputHigh, &outputLow, &outputHigh})
            for (auto& f : *filters) f.reset();
        for (auto& f : body) f.reset();
    }
    void configure(const Settings& p) noexcept
    {
        const float size = p.size * .01f;
        const float space = .60f + size;
        const float attack = p.attack * .01f;
        const float diffuse = std::pow(p.late * .01f, 2.5f);
        cloudDiffuser.setDiffusion(diffuse);
        scattering.set(diffuse);
        earlyReadout.set(p.early);
        earlySpread = p.early * .007f * space;
        cloudSpread = .65f + .65f * size;
        earlyGain = .08f + .65f * std::pow(1.0f - attack, .8f);
        lateGain = std::sqrt(.85f + .30f * attack);
        preSamples = static_cast<float>(sampleRate) * (p.predelay * .001f + .002f);
        modulationSamples = static_cast<float>(sampleRate) * p.depth * .000006f;
        modRate = p.rate;
        for (std::size_t i = 0; i < channels; ++i)
        {
            lineSamples[i] = std::round(baseSeconds[i] * space * static_cast<float>(sampleRate));
            const float position = static_cast<float>((i * 11) % channels) / 15.0f;
            arrivalSamples[i] = static_cast<float>(sampleRate) * space
                * (.012f + .20f * attack * attack * (.25f + .75f * position));
            loss[i].set(sampleRate, lineSamples[i] / static_cast<float>(sampleRate), p.decay,
                        p.bassHz, p.bassMult, p.highHz, p.highDb);
        }
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
        const std::array<float, 2> input {left, right};
        std::array<float, 2> filtered {};
        Frame excitation {};
        constexpr float split = .3535533905932738f;
        for (std::size_t c = 0; c < 2; ++c)
        {
            predelay[c].write(input[c]);
            filtered[c] = inputLow[c].low(inputHigh[c].high(predelay[c].read(preSamples)));
            for (std::size_t i = 0; i < 8; ++i) excitation[c * 8 + i] = split * filtered[c];
        }
        const Frame early = earlyDiffuser.process(excitation, earlySpread);
        Frame cloud = cloudDiffuser.process(excitation, cloudSpread);
        Frame returning {};
        for (std::size_t i = 0; i < channels; ++i)
        {
            arrival[i].write(cloud[i]); cloud[i] = arrival[i].read(arrivalSamples[i]);
            // 慢速非同步调制；深度为零时严格回到固定整数路径。
            phase[i] += 2.0 * pi * modRate * (.31 + .013 * static_cast<double>(i)) / sampleRate;
            if (phase[i] >= 2.0 * pi) phase[i] -= 2.0 * pi;
            const float offset = static_cast<float>(std::sin(phase[i])) * modulationSamples;
            returning[i] = tank[i].read(lineSamples[i] + offset - 1.0f);
        }
        Frame feedback = returning;
        for (std::size_t i = 0; i < channels; ++i) feedback[i] = loss[i].process(feedback[i]);
        scattering.process(feedback);
        // 置换仍为正交操作；低扩散时也避免各线永远独自循环。
        for (std::size_t i = 0; i < channels; ++i)
            tank[i].write(cloud[i] + feedback[(i * 5 + 3) % channels]);
        std::array<float, 2> output {};
        for (std::size_t i = 0; i < channels; ++i)
        {
            output[0] += ((i & 1) ? -.25f : .25f) * returning[i];
            output[1] += ((i & 2) ? -.25f : .25f) * returning[i];
        }
        constexpr float wetGain = 3.10f;
        output[0] = wetGain * (lateGain * body[0].process(output[0])
            + earlyGain * earlyReadout.process(early[3], filtered[0]));
        output[1] = wetGain * (lateGain * body[1].process(output[1])
            + earlyGain * earlyReadout.process(early[12], filtered[1]));
        for (std::size_t c = 0; c < 2; ++c)
            output[c] = outputLow[c].low(outputHigh[c].high(output[c]));
        return output;
    }
private:
    double sampleRate = 48000.0;
    std::array<Delay, 2> predelay;
    Diffuser cloudDiffuser, earlyDiffuser;
    EarlyReadout earlyReadout;
    Scattering scattering;
    std::array<Delay, channels> tank, arrival;
    std::array<Loss, channels> loss;
    std::array<Pole, 2> inputLow, inputHigh, outputLow, outputHigh;
    std::array<LateBody, 2> body;
    Frame baseSeconds {}, lineSamples {}, arrivalSamples {};
    std::array<double, channels> phase {};
    float earlySpread = 1.0f, cloudSpread = 1.0f, preSamples = 0.0f;
    float earlyGain = 0.0f, lateGain = 1.0f, modulationSamples = 0.0f, modRate = 0.0f;
};
} // namespace sanmu_hall
