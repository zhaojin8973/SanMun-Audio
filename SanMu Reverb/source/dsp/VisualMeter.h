#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>

// 单生产者、单消费者的固定容量队列；音频线程只做数值统计和无锁写入。
class VisualMeter
{
public:
    struct Frame
    {
        std::array<float, 4> dry {}, wet {}, peaks {};
        float dryRms = 0, wetRms = 0, peak = 0, pan = 0, wetPan = 0, correlation = 0;
        double duration = 0;
    };
    std::atomic<bool> enabled { false };
    void prepare(double rate)
    {
        sampleRate = rate;
        for (int i = 0; i < 3; ++i) coefficients[static_cast<size_t>(i)] =
            1.0 - std::exp(-2.0 * 3.141592653589793 * std::array<double, 3>{240, 1400, 5000}[static_cast<size_t>(i)] / rate);
        reset();
    }
    void reset() { poles.fill(0); clear(); }
    bool pop(Frame& frame)
    {
        const auto position = read.load(std::memory_order_relaxed);
        if (position == write.load(std::memory_order_acquire)) return false;
        frame = frames[position]; read.store((position + 1) % frames.size(), std::memory_order_release); return true;
    }
    void push(float inL, float inR, float wetL, float wetR, float outL, float outR)
    {
        const std::array<float, 4> values { inL, inR, wetL, wetR };
        for (size_t ch = 0; ch < 4; ++ch)
        {
            const double x = values[ch]; sums[ch] += x * x;
            double previous = 0;
            for (size_t band = 0; band < 3; ++band)
            {
                auto& pole = poles[ch * 3 + band]; pole += coefficients[band] * (x - pole);
                const auto value = pole - previous; energy[(ch / 2) * 4 + band] += value * value; previous = pole;
            }
            energy[(ch / 2) * 4 + 3] += (x - previous) * (x - previous);
        }
        cross += static_cast<double>(wetL) * wetR;
        peaks[0] = std::max(peaks[0], std::abs(inL)); peaks[1] = std::max(peaks[1], std::abs(inR));
        peaks[2] = std::max(peaks[2], std::abs(outL)); peaks[3] = std::max(peaks[3], std::abs(outR));
        if (++count < 512) return;
        Frame frame;
        for (size_t band = 0; band < 4; ++band)
        {
            frame.dry[band] = static_cast<float>(std::sqrt(energy[band] / (2 * count)));
            frame.wet[band] = static_cast<float>(std::sqrt(energy[band + 4] / (2 * count)));
        }
        frame.duration = count / sampleRate; frame.peaks = peaks;
        frame.peak = std::max(peaks[0], peaks[1]);
        frame.dryRms = static_cast<float>(std::sqrt((sums[0] + sums[1]) / (2 * count)));
        frame.wetRms = static_cast<float>(std::sqrt((sums[2] + sums[3]) / (2 * count)));
        frame.pan = static_cast<float>((sums[1] - sums[0]) / (sums[0] + sums[1] + 1e-12));
        frame.wetPan = static_cast<float>((sums[3] - sums[2]) / (sums[2] + sums[3] + 1e-12));
        frame.correlation = static_cast<float>(cross / std::sqrt(sums[2] * sums[3] + 1e-24));
        const auto position = write.load(std::memory_order_relaxed), next = (position + 1) % frames.size();
        if (next != read.load(std::memory_order_acquire)) { frames[position] = frame; write.store(next, std::memory_order_release); }
        clear();
    }
private:
    void clear() { energy.fill(0); sums.fill(0); peaks.fill(0); cross = 0; count = 0; }
    std::array<Frame, 128> frames {};
    std::atomic<size_t> read { 0 }, write { 0 };
    std::array<double, 12> poles {};
    std::array<double, 3> coefficients {};
    std::array<double, 8> energy {};
    std::array<double, 4> sums {};
    std::array<float, 4> peaks {};
    double cross = 0, sampleRate = 48000;
    int count = 0;
};
