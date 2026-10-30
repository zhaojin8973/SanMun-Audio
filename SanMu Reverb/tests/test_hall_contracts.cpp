#include "../tools/HallMeasurement.h"
#include "../source/dsp/engines/hall/HallField.h"
#include <cstdio>
#include <cmath>
#include <random>

namespace hm = hall_measurement;
namespace
{
int failures = 0;
void check(bool passed, const char* message)
{
    std::printf("%s：%s\n", passed ? "通过" : "失败", message);
    if (!passed) ++failures;
}
bool finite(const hm::Audio& a)
{
    for (int c = 0; c < a.getNumChannels(); ++c)
        for (int n = 0; n < a.getNumSamples(); ++n)
            if (!std::isfinite(a.getSample(c, n))) return false;
    return true;
}
hm::Params reference(float early = 100, float late = 100)
{
    auto p = hm::baseline();
    p.decay = 3.99f; p.predelay = 20; p.mix = 100;
    p.highFreq = 6000; p.highShelf = -24; p.bassFreq = 300; p.bassMult = 1.5f;
    p.size = 100; p.attack = 50; p.earlyDiff = early; p.lateDiff = late;
    p.modRate = 2.53f; p.modDepth = 38; p.lowCut = 10; p.highCut = 8010;
    return p;
}
double density(const hm::Audio& a, double sr, double first, double last)
{
    double sum = 0;
    int count = 0;
    const int step = static_cast<int>(sr * .02);
    for (int n = static_cast<int>(first * sr); n + step <= static_cast<int>(last * sr); n += step)
    {
        sum += hm::echoDensity(a, n, n + step); ++count;
    }
    return sum / count;
}
double shapeDifference(hm::Audio a, hm::Audio b)
{
    a.applyGain(static_cast<float>(1.0 / std::sqrt(hm::energy(a))));
    b.applyGain(static_cast<float>(1.0 / std::sqrt(hm::energy(b))));
    double e = 0;
    for (int c = 0; c < 2; ++c)
        for (int n = 0; n < a.getNumSamples(); ++n)
        {
            const double d = a.getSample(c,n) - b.getSample(c,n); e += d*d;
        }
    return std::sqrt(e);
}
}

int main()
{
    juce::ScopedNoDenormals guard;
    constexpr double sr = 48000;
    // 验证实际滤波器对关键音高的作用：目标频带小幅调整，两端保留厚度与明亮度。
    for (double rate : {44100.0,48000.0,96000.0,192000.0})
    {
        for (double hz : {80.0,100.0,150.0,175.0,200.0,225.0,250.0,300.0,1000.0,2000.0,4000.0})
        {
            sanmu_hall::LateBody filter; filter.prepare(rate);
            double before = 0, after = 0;
            for (int n = 0; n < static_cast<int>(rate * .3); ++n)
            {
                const float x = static_cast<float>(.1 * std::sin(2 * sanmu_hall::pi * hz * n / rate));
                const float y = filter.process(x);
                if (n >= static_cast<int>(rate * .1)) { before += x*x; after += y*y; }
            }
            const double gainDb = 10 * std::log10(after / before);
            // 新设计明确检验225Hz中心、250Hz附近，以及暖度和高频的边界。
            const bool correct = hz == 225 ? std::abs(gainDb + 2.5) < .02
                : hz == 150 ? gainDb > -1.55 && gainDb < -1.15
                : hz == 175 ? gainDb > -2.1 && gainDb < -1.75
                : hz == 200 || hz == 250 ? gainDb > -2.5 && gainDb < -2.15
                : hz == 300 ? gainDb > -1.95 && gainDb < -1.55
                : hz <= 100 ? std::abs(gainDb) < .6
                : hz >= 2000 ? std::abs(gainDb) < .03 : std::abs(gainDb) < .15;
            check(std::isfinite(gainDb) && correct,
                  "晚期225Hz中心及250Hz清晰度范围符合目标，150Hz暖度与高频边界保留");
        }
    }
    // 从完整基向量重建真实反馈矩阵；不仅检查某一个向量的能量。
    for (float amount : {0.0f, .01f, .25f, .5f, .75f, 1.0f})
    {
        sanmu_hall::Scattering matrix; matrix.set(amount);
        std::array<sanmu_hall::Frame, 16> columns {};
        for (std::size_t i = 0; i < 16; ++i) { columns[i][i] = 1; matrix.process(columns[i]); }
        double worst = 0;
        for (std::size_t i = 0; i < 16; ++i)
            for (std::size_t j = 0; j < 16; ++j)
            {
                double dot = 0;
                for (std::size_t k = 0; k < 16; ++k) dot += columns[i][k] * columns[j][k];
                worst = std::max(worst, std::abs(dot - (i == j ? 1.0 : 0.0)));
            }
        check(worst < 2e-6, "反馈矩阵全基底正交");
        if (amount == 1)
        {
            bool dense = true;
            for (const auto& column : columns)
                for (float x : column) dense &= std::abs(std::abs(x) - .25f) < 1e-6f;
            check(dense, "Late=100 确实散射到全部十六线，防止百分比重复除以100");
        }
    }
    sanmu_hall::Diffuser diffuser; diffuser.prepare(sr);
    double diffusedEnergy = 0;
    for (int n = 0; n < 24000; ++n)
    {
        sanmu_hall::Frame f {};
        if (n == 0) f[0] = 1;
        const auto result = diffuser.process(f, 1);
        for (float x : result) diffusedEnergy += x*x;
    }
    check(std::abs(diffusedEnergy - 1) < 2e-5, "固定整数延迟的六级扩散器保持总能量");
    for (float amount : {0.0f, .25f, .5f, .75f})
    {
        diffuser.reset(); diffuser.setDiffusion(amount);
        double e = 0;
        for (int n = 0; n < 24000; ++n)
        {
            sanmu_hall::Frame f {}; if (n == 0) f[0] = 1;
            const auto result = diffuser.process(f, 1);
            for (float sample : result) e += sample * sample;
        }
        check(std::abs(e - 1) < 2e-5, "晚期扩散各档保持多通道总能量，不用音量变化冒充扩散");
    }

    const auto impulse = hm::impulse(sr, 3, .1);
    hm::Audio renders[4];
    const float pairs[4][2] = {{0,0},{100,0},{0,100},{100,100}};
    for (int i = 0; i < 4; ++i)
    {
        renders[i] = hm::render(impulse, sr, 128, reference(pairs[i][0], pairs[i][1]));
        check(finite(renders[i]) && hm::energy(renders[i]) > 1e-8, "四组扩散输出有限且非静音");
        check(hm::energy(renders[i],0,static_cast<int>(sr*.02)) == 0, "预延迟前严格无湿声");
    }
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j)
            check(shapeDifference(renders[i],renders[j]) > .1, "扩散组合在匹配总能量后仍有形状差异");
    check(hm::maxDifference(renders[3], hm::render(impulse,sr,257,reference(),64)) < 2e-6,
          "任意宿主块与超大块分片一致");
    const double earlyDensity = density(renders[3],sr,.08,.20);
    const double lateDensity = density(renders[3],sr,.20,.50);
    std::printf("回声密度：80~200ms=%.3f，200~500ms=%.3f\n",earlyDensity,lateDensity);
    check(earlyDensity > .65 && lateDensity > .80, "高扩散密度达到预先设定的起音与晚期下限");
    double bridgeMinimum = 1e30, bridgeMaximum = 0;
    for (int ms = 40; ms <= 110; ms += 10)
    {
        const double e = hm::energy(renders[3],ms*48,(ms+20)*48);
        bridgeMinimum = std::min(bridgeMinimum,e); bridgeMaximum = std::max(bridgeMaximum,e);
    }
    check(bridgeMinimum > bridgeMaximum * .01,
          "默认高扩散的早晚反射衔接无超过20dB的空洞，避免分成两次回声");
    const auto middleLate = hm::render(impulse,sr,128,reference(100,50));
    const double sparseDensity = density(renders[1],sr,.20,.50);
    const double middleDensity = density(middleLate,sr,.20,.50);
    std::printf("Late 0/50/100：晚期密度 %.3f / %.3f / %.3f\n",sparseDensity,middleDensity,lateDensity);
    check(sparseDensity < .55 && lateDensity - sparseDensity > .30,
          "Early=100 时 Late 两端仍能区分稀疏与密集，不能仅验证波形不同");
    check(middleDensity > sparseDensity + .15 && middleDensity < lateDensity - .05,
          "Late 的中间值仍有可用调节范围，不能过早到达满密度");

    // 混合独立波形再渲染，必须等于分别渲染后求和，抓住按音频能量归一化的非线性。
    auto x = hm::noise(sr,.08,1,.1f,192);
    auto y = hm::noise(sr,.08,1,.07f,917);
    for (int n = 0; n < y.getNumSamples(); ++n) y.setSample(1,n,-y.getSample(0,n)*.3f);
    hm::Audio sum; sum.makeCopyOf(x);
    for (int c = 0; c < 2; ++c) sum.addFrom(c,0,y,c,0,y.getNumSamples());
    auto expected = hm::render(x,sr,128,reference());
    const auto fy = hm::render(y,sr,128,reference());
    for (int c = 0; c < 2; ++c) expected.addFrom(c,0,fy,c,0,fy.getNumSamples());
    check(hm::maxDifference(expected,hm::render(sum,sr,128,reference())) < 2e-6, "反馈场满足音频叠加性");
    auto middleDiffusion = reference(100,50);
    auto middleExpected = hm::render(x,sr,128,middleDiffusion);
    const auto middleY = hm::render(y,sr,128,middleDiffusion);
    for (int c = 0; c < 2; ++c) middleExpected.addFrom(c,0,middleY,c,0,middleY.getNumSamples());
    check(hm::maxDifference(middleExpected,hm::render(sum,sr,128,middleDiffusion)) < 2e-6,
          "Late中间值也满足叠加性，不能用逐样本能量补偿");
    sum.makeCopyOf(x); sum.applyGain(.001f);
    auto scaled = hm::render(sum,sr,128,reference()); scaled.applyGain(1000);
    check(hm::maxDifference(scaled,hm::render(x,sr,128,reference())) < 2e-6, "低电平与高电平使用同一个线性算法");

    auto a0 = reference(); a0.attack = 0;
    auto a1 = a0; a1.attack = 100;
    const auto first = hm::render(impulse,sr,128,a0);
    const auto last = hm::render(impulse,sr,128,a1);
    const double shift = hm::centroid(last,sr,.5) - hm::centroid(first,sr,.5);
    const double ratio = hm::energy(last) / hm::energy(first);
    std::printf("Attack：前500ms质心移动 %.1f ms，总能量比 %.3f\n",shift*1000,ratio);
    check(shift > .015 && ratio > .60 && ratio < 1.60, "Attack 真正延展到达时间且保持相近能量");
    const double medianShift = hm::energyTime(last,sr,.5,3) - hm::energyTime(first,sr,.5,3);
    check(medianShift > .15, "Attack 两端能量中位时刻至少后移150ms，避免只拉长几十毫秒的扩散");
    for (float early : {0.0f,50.0f,100.0f})
    {
        auto front = reference(early); front.attack = 0;
        auto back = front; back.attack = 100;
        const auto f = hm::render(impulse,sr,128,front), b = hm::render(impulse,sr,128,back);
        const double frontRatio = hm::energy(f,0,5760) / hm::energy(f);
        const double backRatio = hm::energy(b,0,5760) / hm::energy(b);
        check(backRatio < frontRatio * .30 && hm::energy(b,12000,28800) > hm::energy(f,12000,28800) * .8,
              "Attack 在各 Early 档都减少早期占比，并保留后期能量");
    }

    const double correlation = hm::correlation(renders[3],0,renders[3].getNumSamples());
    double el=0,er=0,em=0;
    for (int n=0;n<renders[3].getNumSamples();++n)
    {
        const double l=renders[3].getSample(0,n),r=renders[3].getSample(1,n);
        el+=l*l; er+=r*r; em+=.25*(l+r)*(l+r);
    }
    const double balance = 10*std::log10(el/er), monoRatio=em/(.5*(el+er));
    std::printf("立体声：相关 %.3f，左右能量差 %.2f dB，单声道功率比 %.3f\n",correlation,balance,monoRatio);
    check(std::abs(correlation)<.6 && std::abs(balance)<1.5 && monoRatio>.18,
          "高扩散同时满足宽度、左右平衡及单声道兼容");

    // 在平坦损耗、无调制条件下测试标称 RT60，而不是要求有高低频阻尼时全频仍等于旋钮。
    for (float decay : {1.0f,3.0f,6.0f})
    {
        auto p=reference(); p.decay=decay; p.bassMult=1; p.highShelf=0; p.modDepth=0;
        auto response=hm::render(hm::impulse(sr,decay*1.5+1,.1),sr,128,p);
        const auto fit=hm::fitT20(response,sr);
        std::printf("平坦损耗：设定 %.1fs，测得 %.3fs\n",decay,fit.rt60);
        check(fit.valid && std::abs(fit.rt60/decay-1)<.10, "每线传播时间正确映射 RT60");
    }

    // 原来的128采样随机切换太短，反馈尚未返回就重置，无法排查环路发散。
    for (float late : {0.0f,25.0f,50.0f,75.0f,100.0f})
    {
        auto p=reference(); p.lateDiff=late; p.decay=20; p.bassMult=4; p.highShelf=0; p.size=0;
        const auto r=hm::render(hm::noise(sr,.15,6,.05f,731),sr,257,p);
        check(finite(r) && r.getMagnitude(0,r.getNumSamples()) < 1 &&
              hm::energy(r,static_cast<int>(sr*5),static_cast<int>(sr*6)) <
              hm::energy(r,static_cast<int>(sr),static_cast<int>(sr*2))*1.2,
              "最长低频衰减与最小尺寸下持续反馈不发散");
    }

    for (double rate : {44100.0,96000.0,192000.0})
    {
        const auto r=hm::render(hm::impulse(rate,.6,.1),rate,257,reference());
        check(finite(r) && hm::energy(r,0,static_cast<int>(rate*.02))==0 &&
              density(r,rate,.20,.50)>.75, "多采样率的预延迟和高扩散密度成立");
    }
    std::printf("Hall 新设计测试：%d 项失败\n",failures);
    return failures ? 1 : 0;
}
