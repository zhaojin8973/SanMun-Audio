#include "../../../source/dsp/engines/hall/HallField.h"
#include <array>
#include <cmath>

namespace {
constexpr int capacity = 128;
sanmu_hall::Field field;
std::array<float, capacity * 4> audio {};
std::array<float, 14> values {}, targets {}, committed {}, steps {};
std::array<int, 14> remaining {};
constexpr std::array<float, 14> defaults {3.99f, 20, 6000, -24, 300, 1.5f, 100, 50, 100, 100, 2.53f, 38, 10, 8010};
constexpr std::array<float, 14> lows {.2f, 0, 1000, -24, 50, .25f, 0, 0, 0, 0, .1f, 0, 10, 1000};
constexpr std::array<float, 14> highs {20, 250, 20000, 0, 1000, 4, 100, 100, 100, 100, 10, 100, 500, 20000};
int rampSamples = 2400;
unsigned tick = 0;
bool initialized = false;
}

extern "C" {
void hall_reset()
{
    field.reset();
    audio.fill(0);
    remaining.fill(0);
    initialized = false;
    tick = 0;
}

void hall_init(double rate)
{
    rate = std::isfinite(rate) && rate >= 8000 && rate <= 192000 ? rate : 48000;
    field.prepare(rate);
    rampSamples = static_cast<int>(std::floor(rate * .05));
    hall_reset();
}

float* hall_buffer() { return audio.data(); }
float* hall_parameters() { return targets.data(); }

void hall_commit()
{
    // 与插件保持相同的 50 毫秒线性平滑；重复目标不会重新开始平滑。
    for (std::size_t i = 0; i < targets.size(); ++i)
    {
        const float target = std::isfinite(targets[i]) ? std::clamp(targets[i], lows[i], highs[i]) : defaults[i];
        targets[i] = target;
        if (initialized && target == committed[i]) continue;
        committed[i] = target;
        if (!initialized) { values[i] = target; remaining[i] = 0; }
        else { remaining[i] = rampSamples; steps[i] = (target - values[i]) / static_cast<float>(rampSamples); }
    }
    initialized = true;
}

void hall_process(int count)
{
    count = std::clamp(count, 0, capacity);
    for (int n = 0; n < count; ++n)
    {
        for (std::size_t i = 0; i < values.size(); ++i)
            if (remaining[i] > 0)
                values[i] = --remaining[i] == 0 ? targets[i] : values[i] + steps[i];
        if (tick == 0)
            field.configure({values[0], values[1], values[2], values[3], values[4], values[5], values[6],
                             values[7], values[8], values[9], values[10], values[11], values[12], values[13]});
        tick = (tick + 1) & 31u;
        if (!std::isfinite(audio[n])) audio[n] = 0;
        if (!std::isfinite(audio[capacity + n])) audio[capacity + n] = 0;
        const auto output = field.process(audio[n], audio[capacity + n]);
        audio[capacity * 2 + n] = output[0];
        audio[capacity * 3 + n] = output[1];
    }
}
}
