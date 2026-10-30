#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <algorithm>

// 音频线程只提交采样；频谱变换、映射和平滑均在后台执行。
class SpectrumAnalyser final : private juce::Thread
{
public:
    static constexpr int points = 500, order = 14, size = 1 << order, capacity = 65536;
    SpectrumAnalyser() : Thread("SanMu EQ spectrum"), fifo(capacity), shortFft(11),
        shortWindow(2048, juce::dsp::WindowingFunction<float>::hann) { clearDisplay(); }
    void setResolution(int choice) { resolution.store(juce::jlimit(0, 3, choice)); }
    ~SpectrumAnalyser() override { stopThread(-1); }
    void prepare(double sr)
    {
        stopThread(-1); rate = sr; hop = std::max(256, static_cast<int>(std::round(sr / 60.0)));
        fifo.reset(); resetRequested.store(false); write = pending = 0; for (auto& c : ring) c.fill(0); clearDisplay(); startThread();
    }
    void reset() { resetRequested.store(true); }
    void push(const juce::AudioBuffer<float>& input, const juce::AudioBuffer<float>& output)
    {
        const int count = std::min(input.getNumSamples(), output.getNumSamples()); int a, n, b, m;
        if (count <= 0 || input.getNumChannels() == 0 || output.getNumChannels() == 0) return;
        fifo.prepareToWrite(count, a, n, b, m);
        const auto copy = [&](int dest, int length, int offset)
        {
            for (int channel = 0; channel < 4; ++channel)
            {
                const auto& buffer = channel < 2 ? input : output;
                const int ch = std::min(channel % 2, buffer.getNumChannels() - 1);
                std::copy_n(buffer.getReadPointer(ch) + offset, length, queue[static_cast<size_t>(channel)].data() + dest);
            }
        };
        copy(a, n, 0); copy(b, m, n); fifo.finishedWrite(n + m);
    }
    float get(int index, int trace = 1) const { return display[static_cast<size_t>(trace)][static_cast<size_t>(juce::jlimit(0, points - 1, index))].load(); }
    bool ready() const { return frames.load() > 0; }
    uint64_t sequence() const { return frames.load(); }
private:
    void clearDisplay()
    {
        for (auto& c : smooth) c.fill(-150);
        peak.fill(-150); hold.fill(0);
        for (auto& c : display) for (auto& v : c) v.store(-150);
        frames.store(0);
    }
    void run() override
    {
        double lastData = juce::Time::getMillisecondCounterHiRes(), lastDecay = lastData;
        while (!threadShouldExit())
        {
            if (resetRequested.exchange(false)) { fifo.finishedRead(fifo.getNumReady()); for (auto& c : ring) c.fill(0); write = pending = 0; clearDisplay(); }
            int a, n, b, m; fifo.prepareToRead(fifo.getNumReady(), a, n, b, m);
            const auto consume = [&](int source, int count)
            {
                for (int sample = 0; sample < count; ++sample)
                {
                    for (int channel = 0; channel < 4; ++channel)
                        ring[static_cast<size_t>(channel)][static_cast<size_t>(write)] = queue[static_cast<size_t>(channel)][static_cast<size_t>(source + sample)];
                    write = (write + 1) % size;
                    if (++pending >= hop) { pending = 0; analyse(); }
                }
            };
            consume(a, n); consume(b, m); fifo.finishedRead(n + m);
            const double now = juce::Time::getMillisecondCounterHiRes();
            if (n + m > 0) { lastData = lastDecay = now; }
            else if (ready() && now - lastData > 80 && now - lastDecay >= 16)
            {
                const float dt = static_cast<float>((now - lastDecay) * .001); lastDecay = now;
                // 部分宿主停播后不再调用音频处理，显示仍需按墙钟时间释放。
                for (int i = 0; i < points; ++i)
                {
                    const auto index = static_cast<size_t>(i);
                    for (size_t trace = 0; trace < 2; ++trace) { auto& s = smooth[trace][index]; s = std::max(-150.f, s - 90.f * dt); display[trace][index].store(s); }
                    if (hold[index] > 0) hold[index] -= dt; else peak[index] = std::max(-150.f, peak[index] - 18.f * dt);
                    display[2][index].store(peak[index]);
                }
            }
            if (n + m == 0) wait(3);
        }
    }
    void analyse()
    {
        const auto index = static_cast<size_t>(resolution.load());
        const int length = 1 << (11 + static_cast<int>(index));
        if (lastResolution != static_cast<int>(index)) { clearDisplay(); lastResolution = static_cast<int>(index); }
        for (int channel = 0; channel < 4; ++channel)
        {
            auto& work = longWork[static_cast<size_t>(channel)]; auto& fast = shortWork[static_cast<size_t>(channel)];
            for (int i = 0; i < length; ++i) work[static_cast<size_t>(i)] = ring[static_cast<size_t>(channel)][static_cast<size_t>((write + size - length + i) % size)];
            std::copy_n(work.data() + length - 2048, 2048, fast.data());
            windows[index].multiplyWithWindowingTable(work.data(), length); shortWindow.multiplyWithWindowingTable(fast.data(), 2048);
            transforms[index].performRealOnlyForwardTransform(work.data(), true); shortFft.performRealOnlyForwardTransform(fast.data(), true);
        }
        const float dt = static_cast<float>(hop / rate), attack = 1 - std::exp(-dt / .015f), release = 1 - std::exp(-dt / .18f);
        for (int i = 0; i < points; ++i)
        {
            const float f = 20 * std::pow(1000.f, static_cast<float>(i) / (points - 1));
            const float blend = juce::jlimit(0.f, 1.f, std::log2(f / 250.f) / 2);
            for (int trace = 0; trace < 2; ++trace)
            {
                const auto power = [&](const auto& works, int length)
                {
                    const float bin = static_cast<float>(f * length / rate);
                    if (bin >= length * .5f) return 0.f;
                    const auto at = [&](int index)
                    {
                        float p = 0;
                        for (int ch = trace * 2; ch < trace * 2 + 2; ++ch)
                        {
                            const auto& data = works[static_cast<size_t>(ch)];
                            const float re = data[static_cast<size_t>(2 * index)], im = data[static_cast<size_t>(2 * index + 1)]; p += re * re + im * im;
                        }
                        return p * 2.f / static_cast<float>(length * length);
                    };
                    const int lo = static_cast<int>(bin), hi = lo + 1; const float mix = bin - static_cast<float>(lo);
                    float value = at(lo) * (1 - mix) + at(hi) * mix;
                    // 高频像素可能覆盖多个 bin，保留区间峰值以免窄峰漏检。
                    const float halfWidth = bin * .00695f;
                    if (halfWidth >= .5f)
                        for (int k = std::max(0, static_cast<int>(std::ceil(bin - halfWidth))); k <= std::min(length / 2, static_cast<int>(std::floor(bin + halfWidth))); ++k) value = std::max(value, at(k));
                    return value;
                };
                const float p = power(longWork, length) * (1 - blend) + power(shortWork, 2048) * blend;
                const float db = juce::jlimit(-150.f, 20.f, 10 * std::log10(std::max(1e-15f, p)));
                auto& s = smooth[static_cast<size_t>(trace)][static_cast<size_t>(i)]; s += (db - s) * (db > s ? attack : release);
                display[static_cast<size_t>(trace)][static_cast<size_t>(i)].store(s);
            }
            auto& p = peak[static_cast<size_t>(i)]; auto& h = hold[static_cast<size_t>(i)]; const float v = smooth[1][static_cast<size_t>(i)];
            if (v >= p) { p = v; h = .35f; } else if (h > 0) h -= dt; else p = std::max(v, p - 18 * dt);
            display[2][static_cast<size_t>(i)].store(p);
        }
        frames.fetch_add(1);
    }
    juce::AbstractFifo fifo;
    std::array<juce::dsp::FFT, 4> transforms {juce::dsp::FFT(11), juce::dsp::FFT(12), juce::dsp::FFT(13), juce::dsp::FFT(14)};
    using Window = juce::dsp::WindowingFunction<float>;
    std::array<Window, 4> windows {Window(2048, Window::hann), Window(4096, Window::hann), Window(8192, Window::hann), Window(16384, Window::hann)};
    juce::dsp::FFT shortFft;
    Window shortWindow;
    std::atomic<int> resolution {2};
    int lastResolution = 2;
    std::array<std::array<float, capacity>, 4> queue{};
    std::array<std::array<float, size>, 4> ring{};
    std::array<std::array<float, size * 2>, 4> longWork{};
    std::array<std::array<float, 4096>, 4> shortWork{};
    std::array<std::array<float, points>, 2> smooth{};
    std::array<float, points> peak{}, hold{};
    std::array<std::array<std::atomic<float>, points>, 3> display{};
    std::atomic<uint64_t> frames{0}; std::atomic<bool> resetRequested{false};
    int write = 0, pending = 0, hop = 800; double rate = 48000;
};
