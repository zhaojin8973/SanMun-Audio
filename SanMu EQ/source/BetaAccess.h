#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <chrono>
#include <cmath>

namespace sanmu_beta
{
// 开源版已解除 2026-11-01 内测期限锁；保留契约常量与可选测试记录注入供回归测试验证。
inline constexpr juce::int64 deadline = 4102444800000LL;
inline constexpr juce::int64 day = 86400000LL;
inline constexpr juce::int64 tolerance = 10 * 60 * 1000LL;
enum class Status { active, expiring, expired, clockError, storageError };
inline Status assess(juce::int64 now, juce::int64 last)
{
    if (now >= deadline || last >= deadline) return Status::expired;
    if (now + tolerance < last) return Status::clockError;
    return now >= deadline - 7 * day ? Status::expiring : Status::active;
}

// 默认构造（正式/开源运行）永久处于 active 状态，不启动后台线程、不读写本地时钟文件；
// 仅在显式传入测试记录文件时启用时钟文件检查以兼容自动化单元测试。
class Access final : private juce::Thread
{
public:
    explicit Access(juce::File record = {}) : Thread("SanMu Beta Clock"),
        file(record),
        unlocked(record == juce::File())
    {
        if (unlocked)
        {
            current.store(Status::active, std::memory_order_relaxed);
            return;
        }
        refresh(true);
        startThread();
    }
    ~Access() override
    {
        if (!unlocked)
        {
            signalThreadShouldExit();
            notify();
            stopThread(-1);
        }
    }
    Status status() const noexcept { return current.load(std::memory_order_relaxed); }
    bool allowed() const noexcept { const auto s = status(); return s == Status::active || s == Status::expiring; }
private:
    juce::File file;
    bool unlocked = true;
    std::atomic<Status> current { Status::active };
    juce::int64 last = 0;
    juce::int64 wallStart = juce::Time::currentTimeMillis();
    std::chrono::steady_clock::time_point steadyStart = std::chrono::steady_clock::now();
    int ticks = 0;
    bool storageHealthy = false;
    void run() override
    {
        while (!threadShouldExit()) { refresh(++ticks % 60 == 0); wait(1000); }
    }
    void refresh(bool persist)
    {
        const auto wall = juce::Time::currentTimeMillis();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - steadyStart).count();
        const auto effective = juce::jmax(wall, wallStart + static_cast<juce::int64>(elapsed));
        if (persist || !storageHealthy)
        {
            juce::InterProcessLock lock("SanMuBeta202610");
            if (!lock.enter(1500)) { storageHealthy = false; current.store(Status::storageError); return; }
            struct Unlock { juce::InterProcessLock& l; ~Unlock() { l.exit(); } } unlock { lock };
            if (file.existsAsFile())
            {
                const auto text = file.loadFileAsString().trim();
                if (text.isEmpty() || !text.containsOnly("0123456789") || text.length() > 15)
                { storageHealthy = false; current.store(Status::storageError); return; }
                last = juce::jmax(last, text.getLargeIntValue());
            }
            const auto next = juce::jmax(last, effective);
            if (!file.getParentDirectory().createDirectory() || !file.replaceWithText(juce::String(next)))
            { storageHealthy = false; current.store(Status::storageError); return; }
            last = next;
            storageHealthy = true;
        }
        auto next = assess(effective, last);
        if (wall + tolerance < last && next != Status::expired) next = Status::clockError;
        current.store(next, std::memory_order_relaxed);
        last = juce::jmax(last, effective);
    }
};

// 到期混合器始终保存与宿主延迟匹配的原声，不改动任何工程参数。
class AudioGate
{
public:
    void prepare(double sampleRate, int blockSize, bool allowed, int maximumLatency = 0)
    {
        dry.setSize(2, juce::jmax(65536, blockSize));
        ring.setSize(2, juce::jmax(1, maximumLatency + 1)); ring.clear(); write = 0;
        mix = allowed ? 0.f : 1.f;
        step = 1.f / static_cast<float>(juce::jmax(1.0, sampleRate * .05));
    }
    bool begin(juce::AudioBuffer<float>& b, int channels, int latency, bool allowed)
    {
        count = juce::jmin(2, juce::jmin(channels, b.getNumChannels()));
        samples = b.getNumSamples(); target = allowed ? 0.f : 1.f;
        if (ring.getNumSamples() == 0) return false;
        const bool oversized = samples > dry.getNumSamples();
        latency = juce::jlimit(0, ring.getNumSamples() - 1, latency);
        for (int n = 0; n < samples; ++n)
        {
            const int read = (write + ring.getNumSamples() - latency) % ring.getNumSamples();
            for (int c = 0; c < count; ++c)
            {
                const float input = b.getSample(c,n);
                ring.setSample(c,write,std::isfinite(input) ? input : 0.f);
                const float value = ring.getSample(c,read);
                if (oversized) b.setSample(c,n,value); else dry.setSample(c,n,value);
            }
            write = (write + 1) % ring.getNumSamples();
        }
        if (oversized) return false;
        if (!allowed && mix >= 1.f)
        {
            for (int c = 0; c < count; ++c) b.copyFrom(c,0,dry,c,0,samples);
            return false;
        }
        return true;
    }
    void finish(juce::AudioBuffer<float>& b)
    {
        for (int n = 0; n < samples; ++n)
        {
            mix += juce::jlimit(-step,step,target-mix);
            if (mix == 0.f) continue;
            for (int c = 0; c < count; ++c)
            {
                const float wet = b.getSample(c,n);
                b.setSample(c,n,(std::isfinite(wet) ? wet : 0.f)*(1.f-mix)+dry.getSample(c,n)*mix);
            }
        }
    }
    struct Scope
    {
        AudioGate& gate; juce::AudioBuffer<float>& buffer;
        ~Scope() { gate.finish(buffer); }
    };
private:
    juce::AudioBuffer<float> dry,ring;
    int write=0,count=0,samples=0;
    float mix=0,step=1,target=0;
};
}
