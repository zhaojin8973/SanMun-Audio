#include "PrecisionLinearEq.h"
#include <complex>
#include <cmath>
#include <algorithm>

namespace
{
constexpr int B = 64, N = 128, K = 65, AA = 95, AD = 47;
using Stereo = std::array<float, 2>;
using Spectrum = std::array<std::complex<float>, K>;
float lowWeight(double f, double rate)
{
    const double x = juce::jlimit(0., 1., (f / rate - .12) / .08);
    return static_cast<float>(.5 + .5 * std::cos(juce::MathConstants<double>::pi * x));
}
double bessel(double x)
{
    double sum = 1, term = 1;
    for (int k = 1; k < 50; ++k) { term *= x * x / (4. * k * k); sum += term; if (term < sum * 1e-15) break; }
    return sum;
}
const std::array<float, AA>& antiAlias()
{
    static const auto taps = []
    {
        std::array<float, AA> h{}; double sum = 0;
        for (int i = 0; i < AA; ++i)
        {
            const double x = i - AD, t = x / AD;
            const double sinc = x == 0 ? .5 : std::sin(.5 * juce::MathConstants<double>::pi * x) / (juce::MathConstants<double>::pi * x);
            h[static_cast<size_t>(i)] = static_cast<float>(sinc * bessel(12 * std::sqrt(std::max(0., 1 - t*t))) / bessel(12));
            sum += h[static_cast<size_t>(i)];
        }
        for (auto& v : h) v /= static_cast<float>(sum);
        return h;
    }();
    return taps;
}
struct Resampler
{
    std::array<Stereo, AA> history{}; int position = 0;
    Stereo filter(Stereo x)
    {
        history[static_cast<size_t>(position)] = x;
        Stereo y{}; const auto& h = antiAlias();
        for (int i = 0; i < AA; ++i)
        {
            const auto& v = history[static_cast<size_t>((position - i + AA) % AA)];
            y[0] += h[static_cast<size_t>(i)] * v[0]; y[1] += h[static_cast<size_t>(i)] * v[1];
        }
        position = (position + 1) % AA;
        return y;
    }
};
struct Delay
{
    std::vector<Stereo> data; size_t position = 0;
    void prepare(int n) { data.assign(static_cast<size_t>(std::max(0, n)), {}); position = 0; }
    void clear() { std::fill(data.begin(), data.end(), Stereo{}); position = 0; }
    Stereo process(Stereo x)
    {
        if (data.empty()) return x;
        const auto y = data[position]; data[position] = x; position = (position + 1) % data.size(); return y;
    }
};
// 每个分区只做 128 点变换。所有历史输入共用，当前核与目标核使用相同历史。
struct PartitionedMatrix
{
    juce::dsp::FFT fft{7};
    int parts = 0, head = 0, fill = 0, read = 0;
    std::array<std::vector<Spectrum>, 2> input;
    std::array<std::vector<Spectrum>, 4> current, target, building;
    std::array<std::array<float, B>, 2> previous{}, staging{}, output{};
    std::array<float, 2*N> work{};
    bool moving = false;
    double rate = 48000;
    void prepare(int taps, double sr)
    {
        rate = sr; parts = (taps + B - 1) / B;
        for (auto& v : input) v.assign(static_cast<size_t>(parts), {});
        for (auto* group : {&current, &target, &building}) for (auto& v : *group) v.assign(static_cast<size_t>(parts), {});
        reset();
    }
    void reset()
    {
        for (auto& v : input) std::fill(v.begin(), v.end(), Spectrum{});
        previous = {}; staging = {}; output = {}; head = fill = read = 0;
    }
    void makePartition(int path, int part, const std::vector<float>& taps)
    {
        work.fill(0);
        for (int i = 0; i < B && part * B + i < static_cast<int>(taps.size()); ++i) work[static_cast<size_t>(i)] = taps[static_cast<size_t>(part * B + i)];
        fft.performRealOnlyForwardTransform(work.data(), true);
        auto& dest = building[static_cast<size_t>(path)][static_cast<size_t>(part)];
        for (int k = 0; k < K; ++k) dest[static_cast<size_t>(k)] = {work[static_cast<size_t>(2*k)], work[static_cast<size_t>(2*k+1)]};
    }
    void activate() { target.swap(building); moving = true; }
    std::array<float, B> convolve(int out)
    {
        Spectrum sum{};
        for (int in = 0; in < 2; ++in) for (int p = 0; p < parts; ++p)
        {
            const auto& x = input[static_cast<size_t>(in)][static_cast<size_t>((head - p + parts) % parts)];
            const auto& h = current[static_cast<size_t>(out*2+in)][static_cast<size_t>(p)];
            for (int k = 0; k < K; ++k) sum[static_cast<size_t>(k)] += x[static_cast<size_t>(k)] * h[static_cast<size_t>(k)];
        }
        work.fill(0);
        for (int k = 0; k < K; ++k) { work[static_cast<size_t>(2*k)] = sum[static_cast<size_t>(k)].real(); work[static_cast<size_t>(2*k+1)] = sum[static_cast<size_t>(k)].imag(); }
        fft.performRealOnlyInverseTransform(work.data());
        std::array<float, B> result{}; std::copy_n(work.data()+B, B, result.begin()); return result;
    }
    Stereo process(Stereo x)
    {
        Stereo y {output[0][static_cast<size_t>(read)], output[1][static_cast<size_t>(read)]};
        read = (read + 1) % B;
        for (int ch = 0; ch < 2; ++ch) staging[static_cast<size_t>(ch)][static_cast<size_t>(fill)] = x[static_cast<size_t>(ch)];
        if (++fill != B) return y;
        fill = 0;
        for (int ch = 0; ch < 2; ++ch)
        {
            work.fill(0);
            std::copy(previous[static_cast<size_t>(ch)].begin(), previous[static_cast<size_t>(ch)].end(), work.begin());
            std::copy(staging[static_cast<size_t>(ch)].begin(), staging[static_cast<size_t>(ch)].end(), work.begin()+B);
            previous[static_cast<size_t>(ch)] = staging[static_cast<size_t>(ch)];
            fft.performRealOnlyForwardTransform(work.data(), true);
            auto& dest = input[static_cast<size_t>(ch)][static_cast<size_t>(head)];
            for (int k = 0; k < K; ++k) dest[static_cast<size_t>(k)] = {work[static_cast<size_t>(2*k)], work[static_cast<size_t>(2*k+1)]};
        }
        std::array<std::array<float, B>, 2> old{};
        if (moving)
        {
            old[0] = convolve(0); old[1] = convolve(1);
            const float mix = static_cast<float>(1 - std::exp(-B/(rate*.03)));
            bool settled = true;
            for (int path = 0; path < 4; ++path) for (int p = 0; p < parts; ++p) for (int k = 0; k < K; ++k)
            {
                auto& c = current[static_cast<size_t>(path)][static_cast<size_t>(p)][static_cast<size_t>(k)];
                const auto t = target[static_cast<size_t>(path)][static_cast<size_t>(p)][static_cast<size_t>(k)];
                const auto delta = t-c;
                if (std::abs(delta.real()) + std::abs(delta.imag()) > 1e-7f) settled = false;
                c += delta * mix;
            }
            output[0] = convolve(0); output[1] = convolve(1);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < B; ++i)
            {
                const float f = static_cast<float>(i+1)/B;
                output[static_cast<size_t>(ch)][static_cast<size_t>(i)] = old[static_cast<size_t>(ch)][static_cast<size_t>(i)] * (1-f) + output[static_cast<size_t>(ch)][static_cast<size_t>(i)]*f;
            }
            if (settled) { for (int p = 0; p < 4; ++p) std::copy(target[static_cast<size_t>(p)].begin(), target[static_cast<size_t>(p)].end(), current[static_cast<size_t>(p)].begin()); moving = false; }
        }
        else { output[0] = convolve(0); output[1] = convolve(1); }
        head = (head + 1) % parts;
        return y;
    }
};
struct Branch
{
    double rate = 48000;
    int factor = 1, tapsCount = 1025, fftSize = 4096, delay = 0;
    bool last = false;
    std::unique_ptr<juce::dsp::FFT> designFft;
    std::vector<float> work, window;
    std::array<std::vector<float>, 4> magnitude, taps;
    std::unique_ptr<PartitionedMatrix> convolver;
    Delay alignment;
    Resampler down, up;
    Stereo childOutput{};
    int phase = 0;
};
int depthFor(double rate, int quality)
{
    const double minimum[] {3000, 750, 375}; int depth = 0;
    while (rate > minimum[juce::jlimit(0,2,quality)] && depth < 10) { rate *= .5; ++depth; }
    return depth;
}
// 高精度档的低频末级使用 4097 点窗口，避免 20 Hz / 高 Q 时频率栅格过粗；
// 极限档保留 8193 点末级，用于离线导出和母带精修。
int tapCount(int quality, bool last) { return quality == 0 ? 1025 : quality == 1 ? (last ? 8193 : 4097) : last ? 8193 : 4097; }
}

struct PrecisionLinearEq::Impl
{
    double rate = 48000; int quality = 0, latency = 0;
    std::vector<std::unique_ptr<Branch>> branches;
    Delay dry;
    MatrixSampler sampler = nullptr; void* context = nullptr;
    int stage = 0, bin = 0, path = 0, part = -1;
    bool pending = false;
    std::array<float, 24> pendingGain{}, appliedGain{}, targetGain{};
    std::array<std::atomic<float>, 24> reduction{};
    void designStep(int budget)
    {
        if (!pending) return;
        auto& b = *branches[static_cast<size_t>(stage)];
        if (bin <= b.fftSize/2)
        {
            const int end = std::min(b.fftSize/2+1, bin+budget);
            for (; bin < end; ++bin)
            {
                const double f = bin * b.rate / b.fftSize;
                const float weight = (stage == 0 ? 1.f : lowWeight(f, b.rate*2)) - (b.last ? 0.f : lowWeight(f,b.rate));
                float m[4]{}; if (weight > 0) sampler(static_cast<float>(f), rate, m, context);
                for (int p = 0; p < 4; ++p)
                {
                    const float value = (m[p] - (p == 0 || p == 3 ? 1.f : 0.f)) * weight;
                    b.magnitude[static_cast<size_t>(p)][static_cast<size_t>(bin)] = std::isfinite(value) ? juce::jlimit(-1e12f,1e12f,value) : 0;
                }
            }
            return;
        }
        if (part < 0)
        {
            std::fill(b.work.begin(),b.work.end(),0.f);
            for (int k = 0; k <= b.fftSize/2; ++k) b.work[static_cast<size_t>(2*k)] = b.magnitude[static_cast<size_t>(path)][static_cast<size_t>(k)];
            b.designFft->performRealOnlyInverseTransform(b.work.data());
            const int d = (b.tapsCount-1)/2;
            auto& taps = b.taps[static_cast<size_t>(path)];
            for (int n = 0; n < b.tapsCount; ++n)
            {
                const int t = std::abs(n-d);
                // 显式对称化，消除单精度反变换的左右舍入差。
                const float h = .5f*(b.work[static_cast<size_t>(t)] + b.work[static_cast<size_t>((b.fftSize-t)%b.fftSize)]);
                taps[static_cast<size_t>(n)] = h * b.window[static_cast<size_t>(n)];
            }
            part = 0; return;
        }
        // 小变换分批完成，最后统一发布所有分支的整组矩阵。
        for (int i = 0; i < 8 && part < b.convolver->parts; ++i, ++part) b.convolver->makePartition(path,part,b.taps[static_cast<size_t>(path)]);
        if (part < b.convolver->parts) return;
        part = -1;
        if (++path < 4) return;
        path = bin = 0;
        if (++stage < static_cast<int>(branches.size())) return;
        for (auto& branch : branches) branch->convolver->activate();
        targetGain = pendingGain;
        pending = false;
    }
    Stereo processLevel(int level, Stereo x)
    {
        auto& b = *branches[static_cast<size_t>(level)];
        auto y = b.convolver->process(b.alignment.process(x));
        if (!b.last)
        {
            const auto down = b.down.filter(x);
            Stereo inserted{};
            if (b.phase == 0)
            {
                b.childOutput = processLevel(level+1, down);
                inserted = {2*b.childOutput[0],2*b.childOutput[1]};
            }
            b.phase ^= 1;
            const auto low = b.up.filter(inserted);
            y[0] += low[0]; y[1] += low[1];
        }
        return y;
    }
};

PrecisionLinearEq::PrecisionLinearEq() : impl(std::make_unique<Impl>()) {}
PrecisionLinearEq::~PrecisionLinearEq() = default;
int PrecisionLinearEq::latencyFor(double sr, int quality)
{
    const int factor = 1 << depthFor(sr,quality);
    return factor * ((tapCount(quality,true)-1)/2+B) + 2*AD*(factor-1);
}
void PrecisionLinearEq::prepare(double sr, int quality)
{
    impl = std::make_unique<Impl>(); auto& s = *impl;
    s.rate = sr; s.quality = juce::jlimit(0,2,quality); s.latency = latencyFor(sr,s.quality);
    const int depth = depthFor(sr,s.quality);
    for (int level = 0; level <= depth; ++level)
    {
        auto b = std::make_unique<Branch>(); b->factor = 1 << level; b->rate = sr / b->factor; b->last = level == depth;
        b->tapsCount = tapCount(s.quality,b->last); b->fftSize = juce::nextPowerOfTwo((b->tapsCount-1)*4);
        b->designFft = std::make_unique<juce::dsp::FFT>(juce::roundToInt(std::log2(b->fftSize)));
        b->work.resize(static_cast<size_t>(b->fftSize*2)); b->window.resize(static_cast<size_t>(b->tapsCount));
        for (auto& v : b->magnitude) v.resize(static_cast<size_t>(b->fftSize/2+1));
        for (auto& v : b->taps) v.resize(static_cast<size_t>(b->tapsCount));
        const int d = (b->tapsCount-1)/2;
        for (int n = 0; n < b->tapsCount; ++n)
        {
            const double x = std::abs(n-d)/double(d);
            b->window[static_cast<size_t>(n)] = x <= .8 ? 1.f : static_cast<float>(.5+.5*std::cos(juce::MathConstants<double>::pi*(x-.8)/.2));
        }
        b->convolver = std::make_unique<PartitionedMatrix>(); b->convolver->prepare(b->tapsCount,b->rate);
        // 经过上下采样各一次后的完整延迟，按该支路采样率补齐。
        const int resampleDelay = 2*AD*(b->factor-1);
        const int localTotal = (s.latency-resampleDelay)/b->factor;
        b->alignment.prepare(localTotal-d-B);
        s.branches.push_back(std::move(b));
    }
    s.dry.prepare(s.latency);
}
void PrecisionLinearEq::reset(bool preserve)
{
    auto& s = *impl; s.dry.clear(); s.pending = false;
    for (auto& b : s.branches)
    {
        b->alignment.clear(); b->convolver->reset(); b->down = {}; b->up = {}; b->phase = 0; b->childOutput = {};
        if (!preserve) for (auto* group : {&b->convolver->current,&b->convolver->target}) for (auto& v : *group) std::fill(v.begin(),v.end(),Spectrum{});
    }
    s.pendingGain = {}; s.appliedGain = {}; s.targetGain = {};
    for (auto& v : s.reduction) v.store(0);
}
void PrecisionLinearEq::beginRebuild(MatrixSampler sampler, void* context)
{
    auto& s = *impl; s.sampler = sampler; s.context = context; s.stage=s.bin=s.path=0; s.part=-1; s.pending=true;
}
void PrecisionLinearEq::rebuild(MatrixSampler sampler, void* context)
{
    beginRebuild(sampler,context);
    while (impl->pending) impl->designStep(1024);
    // 同步诊断与离线初始化可直接安装核，运行期仍使用渐变。
    for (auto& b : impl->branches) { b->convolver->current = b->convolver->target; b->convolver->moving = false; }
}
bool PrecisionLinearEq::isRebuildPending() const noexcept { return impl->pending; }
int PrecisionLinearEq::getLatencySamples() const noexcept { return impl->latency; }
void PrecisionLinearEq::setPendingDynamicGain(int band,float gain) noexcept { impl->pendingGain[static_cast<size_t>(band)] = gain; }
float PrecisionLinearEq::getAppliedReduction(int band) const noexcept { return impl->reduction[static_cast<size_t>(band)].load(std::memory_order_relaxed); }
void PrecisionLinearEq::processBlock(juce::AudioBuffer<float>& buffer,bool tick)
{
    auto& s = *impl;
    if (tick) s.designStep(512);
    const int count = buffer.getNumSamples(), channels = buffer.getNumChannels();
    for (int n = 0; n < count; ++n)
    {
        Stereo x {buffer.getSample(0,n),channels>1?buffer.getSample(1,n):0};
        const auto dry = s.dry.process(x), wet = s.processLevel(0,x);
        buffer.setSample(0,n,dry[0]+wet[0]); if (channels>1) buffer.setSample(1,n,dry[1]+wet[1]);
    }
    const float mix = static_cast<float>(1-std::exp(-count/(s.rate*.03)));
    for (int i = 0; i < 24; ++i) { const auto j=static_cast<size_t>(i); s.appliedGain[j] += mix*(s.targetGain[j]-s.appliedGain[j]); s.reduction[j].store(-s.appliedGain[j],std::memory_order_relaxed); }
}
double PrecisionLinearEq::response(double f,int path) const
{
    double sum = path==0||path==3?1.:0.; double resample = 1.;
    for (size_t level=0;level<impl->branches.size();++level)
    {
        const auto& b=*impl->branches[level];
        if (f < b.rate*.5)
        {
            double h=0; const auto& taps=b.taps[static_cast<size_t>(path)]; const int d=(b.tapsCount-1)/2;
            for (int n=0;n<b.tapsCount;++n) h+=taps[static_cast<size_t>(n)]*std::cos(2*juce::MathConstants<double>::pi*f*(n-d)/b.rate);
            sum+=h*resample;
        }
        double aa=0; for(int n=0;n<AA;++n) aa+=antiAlias()[static_cast<size_t>(n)]*std::cos(2*juce::MathConstants<double>::pi*f*(n-AD)/b.rate);
        resample*=aa*aa;
    }
    return sum;
}
