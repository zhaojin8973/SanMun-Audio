#pragma once
#include <cmath>

// 同一组公开输入供参考插件与生产引擎重放；不包含任何压缩处理代码。
namespace lev_probe
{
inline double sample(double seconds, int scenario)
{
    constexpr double pi = 3.14159265358979323846;
    const double duration = scenario == 0 ? 0.03 : scenario == 1 ? 0.3 : 2.0;
    double amplitude = seconds >= 0.25 && seconds < 0.25 + duration ? std::pow(10., -6./20) : std::pow(10., -48./20);
    double frequency = 1000;
    if (scenario == 3)
    {
        // 连续音节包络，覆盖强弱变化与句间恢复。
        const double age = std::fmod(seconds, 0.55);
        amplitude = (1 - std::exp(-age / .012)) * std::exp(-age / .16)
                    * (seconds < 2 ? .12 : .42) + .0001;
        return amplitude * (.7 * std::sin(2*pi*180*seconds) + .2*std::sin(2*pi*540*seconds)
                            + .1*std::sin(2*pi*2300*seconds));
    }
    if (scenario == 4) { frequency = 70; amplitude = std::pow(10., -12./20); }
    return amplitude * std::sin(2*pi*frequency*seconds);
}
}
