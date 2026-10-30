#include "VoxEngine.h"

namespace dsp_engines
{
namespace
{
double gain(double db) { return std::pow(10.0, db * 0.05); }
double db(double amplitude) { return 20 * std::log10(std::max(amplitude, 1.0e-12)); }
}

void VoxEngine::SmoothEnvelope::prepare(int delay)
{
    queue.resize(static_cast<size_t>(delay + 2));
    average.resize(static_cast<size_t>(delay + 1));
    reset();
}
void VoxEngine::SmoothEnvelope::reset()
{
    head = count = position = 0; index = 0;
    std::fill(average.begin(), average.end(), 1);
    sum = double(average.size());
}
double VoxEngine::SmoothEnvelope::push(double allowedGain)
{
    // 窗口内取最小允许增益，再作同长度均值。某一采样输出时，
    // 均值的每一项都受该采样的峰值约束，因此平滑不会重新放出越界尖峰。
    while (count && index - queue[head].index >= average.size())
    { head = (head + 1) % queue.size(); --count; }
    while (count && queue[(head + count - 1) % queue.size()].gain >= allowedGain) --count;
    queue[(head + count) % queue.size()] = {index++, allowedGain};
    ++count;
    sum += queue[head].gain - average[position];
    average[position] = queue[head].gain;
    position = (position + 1) % average.size();
    return std::clamp(sum / double(average.size()), 0.0, 1.0);
}

VoxEngine::VoxEngine() { prepare({48000, 512, 2}); }
void VoxEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0 ? spec.sampleRate : 48000;
    delaySamples = getLatencySamples(sampleRate, true);
    for (auto& channel : wetDelay) channel.assign(static_cast<size_t>(delaySamples), 0);
    for (auto& channel : dryDelay) channel.assign(static_cast<size_t>(delaySamples), 0);
    gateDelay.assign(static_cast<size_t>(delaySamples), 0);
    smoothMain.prepare(delaySamples); smoothTotal.prepare(delaySamples);
    reset();
}
void VoxEngine::reset()
{
    limiterEnvelope = 1;
    smoothMain.reset(); smoothTotal.reset();
    for (auto& channel : wetDelay) std::fill(channel.begin(), channel.end(), 0);
    for (auto& channel : dryDelay) std::fill(channel.begin(), channel.end(), 0);
    std::fill(gateDelay.begin(), gateDelay.end(), 0);
    delayPosition = 0;
    smoothBlend = smoothEnabled.load(std::memory_order_relaxed) ? 1 : 0;
    inputPower.fill(0); compPower.fill(0);
    gateReduction = fastReduction = slowReduction = 0;
    const auto mapAmount = [](double value) { const auto a = std::clamp(value, 0.0, 36.0); return std::min(36.0, a + 0.25 * a * (1.0 - a / 36.0)); };
    smoothedAmount = mapAmount(-double(compDb.load(std::memory_order_relaxed)));
    smoothedOutput = std::clamp(double(gainDb.load(std::memory_order_relaxed)), -36.0, 0.0);
    smoothedWet = std::clamp(double(mixPercent.load(std::memory_order_relaxed)) / 100, 0.0, 1.0);
    currentGrDb = 0; currentCompGrDb = 0; currentGateGrDb = 0; currentLimiterGrDb = 0; inputEnergyDb = -100;
}
void VoxEngine::process(juce::AudioBuffer<float>& buffer, bool bypass)
{
    const int channels = std::min(2, buffer.getNumChannels());
    if (channels == 0 || buffer.getNumSamples() == 0) return;
    const double requestedAmount = std::clamp(-double(compDb.load(std::memory_order_relaxed)), 0.0, 36.0);
    // 外部参数保持 0～36 dB 兼容；低段采用渐进映射，让约 10 dB 已有可感知平准，36 dB 仍为上限。
    const double amount = std::min(36.0, requestedAmount + 0.25 * requestedAmount * (1.0 - requestedAmount / 36.0));
    const double output = std::clamp(double(gainDb.load(std::memory_order_relaxed)), -36.0, 0.0);
    const double threshold = gateDb.load(std::memory_order_relaxed);
    const double wet = std::clamp(double(mixPercent.load(std::memory_order_relaxed)) / 100, 0.0, 1.0);
    const double smoothTarget = smoothEnabled.load(std::memory_order_relaxed) ? 1 : 0;
    const double switchStep = 1 / (sampleRate * .005);
    const auto coeff = [&](double seconds) { return -std::expm1(-1 / (sampleRate * seconds)); };
    const double parameterCoeff = coeff(0.005), rmsCoeff = coeff(0.010);
    const double gateOpen = coeff(0.003), gateClose = coeff(0.100);
    const double attack = coeff(0.008), memoryCharge = coeff(0.400), slowRelease = coeff(0.600);
    const double limiterRelease = coeff(0.050);
    constexpr double ceiling = 0.999;
    auto* left = buffer.getWritePointer(0);
    auto* right = channels == 2 ? buffer.getWritePointer(1) : nullptr;
    double lastComp = 0, lastLimiter = 0, lastGate = 0, maxInput = -100;
    for (int n = 0; n < buffer.getNumSamples(); ++n)
    {
        const float raw[] = {left[n], right ? right[n] : left[n]};
        smoothedAmount += parameterCoeff * (amount - smoothedAmount);
        smoothedOutput += parameterCoeff * (output - smoothedOutput);
        smoothedWet += parameterCoeff * (wet - smoothedWet);
        for (size_t c = 0; c < static_cast<size_t>(channels); ++c)
            inputPower[c] += rmsCoeff * (double(raw[c]) * raw[c] - inputPower[c]);
        const double inputDb = db(std::sqrt(channels == 2 ? std::max(inputPower[0], inputPower[1]) : inputPower[0]));
        maxInput = std::max(maxInput, inputDb);
        const double gateTarget = threshold <= -79 ? 0 : std::clamp((threshold - inputDb) * 1.5, 0.0, 60.0);
        gateReduction += (gateTarget < gateReduction ? gateOpen : gateClose) * (gateTarget - gateReduction);
        const double gateGain = gain(-gateReduction);
        for (size_t c = 0; c < static_cast<size_t>(channels); ++c)
        {
            const double s = raw[c] * gateGain;
            compPower[c] += rmsCoeff * (s * s - compPower[c]);
        }
        // 连续平准曲线：弱音也有平滑衰减，没有低于固定拐点便完全不工作的死区。
        // COMP 同时控制小信号增益和曲率；由公开 RVox 输入输出测量确定静态行为，
        // 检波与动态恢复是本引擎的独立设计，不宣称复刻其内部算法。
        const double amplitude = std::sqrt(2 * (channels == 2 ? std::max(compPower[0], compPower[1]) : compPower[0]));
        const double drive = gain(smoothedAmount);
        const double baseReduction = db(1 + (drive - 1) * amplitude);
        // 正常压缩区增加约 1 dB，接近零时连续收敛，COMP=0 仍无主压缩。
        const double depthLift = -std::expm1(-baseReduction / 2.0);
        const double target = std::min(60.0, baseReduction + depthLift);
        // 持续压缩才积累慢记忆，短辅音不能像长句一样立即填满慢状态。
        // 恢复随积累量从 60 毫秒连续延长至 240 毫秒；慢尾只持有部分衰减。
        const double history = std::clamp(slowReduction / (0.35 * std::max(fastReduction, 1.0)), 0.0, 1.0);
        const double recovery = coeff(0.060 + 0.180 * history);
        fastReduction += (target > fastReduction ? attack : recovery) * (target - fastReduction);
        const double memoryTarget = 0.35 * std::min(target, fastReduction);
        slowReduction += (memoryTarget > slowReduction ? memoryCharge : slowRelease) * (memoryTarget - slowReduction);
        lastComp = std::max(fastReduction, slowReduction);
        const double makeup = smoothedAmount;
        // 同一采样直接通过：即时峰值保护与主压缩取更严格的增益，不叠加压低拍头。
        // 不预测未来；突发尖峰需要立即衰减，随后以 50 毫秒恢复，音头会比前瞻控制更硬。
        const double preGain = gateGain * gain(makeup);
        const double processed[] = {raw[0] * preGain, raw[1] * preGain};
        const double peak = std::max(std::abs(processed[0]), std::abs(processed[1]));
        const double required = std::min(1.0, ceiling / std::max(peak, 1.0e-12));

        limiterEnvelope = required < limiterEnvelope ? required
            : limiterEnvelope + limiterRelease * (required - limiterEnvelope);
        const double limiterGain = std::clamp(limiterEnvelope, 0.0, 1.0);
        const double appliedGain = std::min(gain(-lastComp), limiterGain);
        // 即使关闭平滑也持续准备轨迹，切换时无需清空包络或等待缓冲填满。
        const double smoothCompGain = smoothMain.push(gain(-lastComp));
        const double smoothAppliedGain = std::min(smoothTotal.push(appliedGain), smoothCompGain);
        smoothBlend += std::clamp(smoothTarget - smoothBlend, -switchStep, switchStep);
        const auto position = static_cast<size_t>(delayPosition);
        const double delayedGate = gateDelay[position];
        gateDelay[position] = gateReduction;
        const double outputGain = gain(smoothedOutput);
        for (size_t c = 0; c < static_cast<size_t>(channels); ++c)
        {
            const double delayedWet = wetDelay[c][position];
            const float delayedDry = dryDelay[c][position];
            wetDelay[c][position] = processed[c]; dryDelay[c][position] = raw[c];
            const double direct = bypass ? raw[c]
                : processed[c] * appliedGain * outputGain * smoothedWet + raw[c] * (1 - smoothedWet);
            const double smooth = bypass ? delayedDry
                : delayedWet * smoothAppliedGain * outputGain * smoothedWet + delayedDry * (1 - smoothedWet);
            (c == 0 ? left : right)[n] = float(direct * (1 - smoothBlend) + smooth * smoothBlend);
        }
        const double totalGr = -db(appliedGain) * (1 - smoothBlend) - db(smoothAppliedGain) * smoothBlend;
        lastComp = lastComp * (1 - smoothBlend) - db(smoothCompGain) * smoothBlend;
        lastLimiter = std::max(0.0, totalGr - lastComp);
        lastGate = gateReduction * (1 - smoothBlend) + delayedGate * smoothBlend;
        delayPosition = (delayPosition + 1) % delaySamples;
    }
    inputEnergyDb.store(float(maxInput), std::memory_order_relaxed);
    currentCompGrDb.store(float(lastComp), std::memory_order_relaxed);
    currentGateGrDb.store(float(lastGate), std::memory_order_relaxed);
    currentLimiterGrDb.store(float(lastLimiter), std::memory_order_relaxed);
    currentGrDb.store(float(lastComp + lastGate + lastLimiter), std::memory_order_relaxed);
}
}
