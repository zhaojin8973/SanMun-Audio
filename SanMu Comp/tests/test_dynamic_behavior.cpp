#include "dsp/engines/FetEngine.h"
#include "dsp/engines/VcaEngine.h"
#include "dsp/engines/OptoEngine.h"
#include "dsp/engines/VoxEngine.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <functional>
#include <string>
#include "LevProbeSignal.h"

using namespace dsp_engines;
namespace
{
constexpr double pi = juce::MathConstants<double>::pi;
double gain(double db) { return std::pow(10.0, db / 20.0); }
double decibels(double value) { return 20.0 * std::log10(std::max(value, 1.0e-12)); }
struct Measurement { double outputDb, residualPercent, gr; };

// 整秒整周期相干投影：残差包含全部谐波与非谐波能量，不把失真当噪声排除。
template<class Engine>
Measurement tone(Engine& engine, double sr, double hz, double inputDb, std::ostream* trace = nullptr)
{
    engine.prepare({sr, 1, 1});
    juce::AudioBuffer<float> buffer(1, 1);
    double sinSum = 0, cosSum = 0, energy = 0, dc = 0, gr = 0;
    const int count = static_cast<int>(sr);
    for (int n = 0; n < 2 * count; ++n)
    {
        const double phase = 2 * pi * hz * n / sr;
        const float input = static_cast<float>(gain(inputDb) * std::sin(phase));
        buffer.setSample(0, 0, input);
        engine.process(buffer);
        const double y = buffer.getSample(0, 0);
        if (!std::isfinite(y)) throw std::runtime_error("输出包含非有限数值");
        if (n >= count)
        {
            sinSum += y * std::sin(phase);
            cosSum += y * std::cos(phase);
            energy += y * y;
            dc += y;
            gr += engine.getCurrentGrDb();
        }
        if (trace && n >= 2 * count - count / 10)
            *trace << (n - 2 * count) / sr << ',' << input << ',' << y << ',' << engine.getCurrentGrDb() << '\n';
    }
    const double fundamentalPower = 2 * (sinSum * sinSum + cosSum * cosSum) / (double(count) * count);
    const double residual = std::max(0.0, energy / count - fundamentalPower - (dc / count) * (dc / count));
    return {decibels(std::sqrt(energy / count)), 100 * std::sqrt(residual / std::max(1.0e-24, fundamentalPower)), gr / count};
}

void setup(FetEngine& e, int ratio = 3, float release = 7, float output = 24)
{
    e.fetDrive = 18; e.fetRatioIdx = ratio; e.attackMs = 7; e.releaseMs = release;
    e.makeupDb = output; e.outputTrimDb = 0; e.headroomDb = 16; e.mixPercent = 100;
    e.sidechainHpHz = 10; e.sidechainEnabled = false;
}
void setup(VcaEngine& e)
{
    e.thresholdDb = -24; e.ratio = 4; e.attackMs = 1; e.releaseMs = 100;
    e.makeupDb = 0; e.outputTrimDb = 0; e.mixPercent = 100; e.sidechainHpHz = 10;
}
void setup(OptoEngine& e, float pr = 60, float output = 40)
{
    e.optoPeakReduction = pr; e.makeupDb = output; e.outputTrimDb = 0;
    e.mixPercent = 100; e.sidechainHpHz = 10;
}

template<class Engine>
void steps(Engine& e, double sr, const std::filesystem::path& path, double duration = 0.5)
{
    e.prepare({sr, 1, 1});
    std::ofstream file(path);
    file << "seconds,input,output,gr_db\n";
    juce::AudioBuffer<float> b(1, 1);
    for (int n = 0; n < static_cast<int>((duration + 3.0) * sr); ++n)
    {
        const double t = n / sr;
        const float x = static_cast<float>(gain(t < 0.1 ? -60 : t < duration + 0.1 ? -6 : -60)
            * std::sin(2 * pi * 1000 * t));
        b.setSample(0, 0, x); e.process(b);
        if (n % std::max(1, static_cast<int>(sr / 10000)) == 0)
            file << t << ',' << x << ',' << b.getSample(0, 0) << ',' << e.getCurrentGrDb() << '\n';
    }
}

void capture(const std::filesystem::path& directory)
{
    std::filesystem::create_directories(directory);
    std::ofstream metrics(directory / "metrics.csv");
    metrics << "engine,sample_rate,hz,input_db,control,output_control,output_rms_db,residual_percent,gr_db\n";
    auto row = [&](const char* name, double sr, double hz, double in, double control, double out, const Measurement& m)
    { metrics << name << ',' << sr << ',' << hz << ',' << in << ',' << control << ',' << out << ',' << m.outputDb << ',' << m.residualPercent << ',' << m.gr << '\n'; };
    for (double sr : {44100., 48000., 96000., 192000.})
    {
        for (double hz : {50., 100., 1000., 10000.})
            for (float release : {1.f, 7.f})
            {
                FetEngine e; setup(e, 3, release);
                std::ofstream trace;
                if (sr == 48000 && hz == 100 && release == 7)
                { trace.open(directory / "fet_100hz.csv"); trace << "seconds,input,output,gr_db\n"; }
                row("FET", sr, hz, -6, release, 24, tone(e, sr, hz, -6, trace.is_open() ? &trace : nullptr));
            }
        VcaEngine v; setup(v); row("VCA", sr, 1000, -6, 100, 0, tone(v, sr, 1000, -6));
        FetEngine f; setup(f); steps(f, sr, directory / ("fet_step_" + std::to_string(static_cast<int>(sr)) + ".csv"));
    }
    for (float output : {24.f, 36.f, 48.f})
    { FetEngine e; setup(e, 0, 7, output); row("FET_OUTPUT", 48000, 1000, -6, 4, output, tone(e, 48000, 1000, -6)); }
    for (int ratio = 0; ratio < 4; ++ratio)
        for (double in : {-24., -18., -12., -6., 0.})
        { FetEngine e; setup(e, ratio); row("FET_RATIO", 48000, 1000, in, ratio, 24, tone(e, 48000, 1000, in)); }
    for (float comp : {0.f, -12.f, -24.f, -36.f})
        for (double in : {-36., -24., -12., -6.})
        { VoxEngine e; e.setComp(comp); e.setGain(-12); e.setGate(-80); row("LEV", 48000, 1000, in, comp, -12, tone(e, 48000, 1000, in)); }
    for (float pr : {0.f, 25.f, 50.f, 75.f, 100.f})
        for (double in : {-30., -18., -6.})
        { OptoEngine e; setup(e, pr); row("OPTO", 48000, 1000, in, pr, 40, tone(e, 48000, 1000, in)); }
    for (float output : {40.f, 60.f, 80.f})
    { OptoEngine e; setup(e, 60, output); row("OPTO_OUTPUT", 48000, 1000, -6, 60, output, tone(e, 48000, 1000, -6)); }
    for (double duration : {0.01, 0.1, 2.0})
    { OptoEngine e; setup(e); steps(e, 48000, directory / ("opto_release_" + std::to_string(duration) + ".csv"), duration); }
    VcaEngine v; setup(v); steps(v, 48000, directory / "vca_step.csv");
    std::cout << "动态测量已写入 " << directory << '\n';
}

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void captureOpto(const std::filesystem::path& directory)
{
    std::filesystem::create_directories(directory);
    std::ofstream file(directory / "opto_frequency.csv");
    file << "sample_rate,hz,output_rms_db,residual_percent\n";
    for (double sr : {44100., 48000., 96000., 192000.})
        for (double hz : {20., 50., 1000.})
        {
            OptoEngine e; setup(e, 0, 40);
            const auto m = tone(e, sr, hz, -30);
            file << sr << ',' << hz << ',' << m.outputDb << ',' << m.residualPercent << '\n';
        }
    std::ofstream curves(directory / "opto_modes.csv");
    curves << "limit,peak_reduction,input_db,gr_db,output_rms_db\n";
    for (bool limit : {false, true})
        for (float pr : {25.f, 50.f, 75.f, 100.f})
            for (double input : {-30., -18., -6.})
            {
                OptoEngine e; setup(e, pr); e.isLimit = limit;
                const auto m = tone(e, 48000, 1000, input);
                curves << limit << ',' << pr << ',' << input << ',' << m.gr << ',' << m.outputDb << '\n';
            }
    std::cout << "Opto 频响及双模式测量已写入 " << directory << '\n';
}

void captureVca(const std::filesystem::path& directory)
{
    std::filesystem::create_directories(directory);
    std::ofstream file(directory / "vca_output.csv");
    file << "makeup_db,output_rms_db,residual_percent\n";
    for (float makeup : {0.f, 12.f, 24.f})
    {
        VcaEngine e; setup(e); e.ratio = 1; e.thresholdDb = 0; e.makeupDb = makeup;
        const auto m = tone(e, 48000, 1000, 0);
        file << makeup << ',' << m.outputDb << ',' << m.residualPercent << '\n';
    }
    std::cout << "VCA 输出级测量已写入 " << directory << '\n';
}

void captureFetStress(const std::filesystem::path& directory)
{
    std::filesystem::create_directories(directory);
    std::ofstream csv(directory / "fet_stress.csv");
    csv << "sample_rate,hz,input_control,ratio_index,output_rms_db,residual_percent,gr_db\n";
    for (double sr : {48000., 192000.})
    for (double hz : {50., 100., 1000., 10000.})
    for (float drive : {18.f, 30.f, 42.f})
    for (int ratio : {0, 3, 4})
    {
        FetEngine e; setup(e, ratio); e.fetDrive = drive;
        std::ofstream trace;
        if (sr == 48000 && hz == 100 && drive == 42 && ratio == 3)
        { trace.open(directory / "fet_stress_trace.csv"); trace << "seconds,input,output,gr_db\n"; }
        const auto m = tone(e, sr, hz, -6, trace.is_open() ? &trace : nullptr);
        csv << sr << ',' << hz << ',' << drive << ',' << ratio << ',' << m.outputDb << ',' << m.residualPercent << ',' << m.gr << '\n';
    }
}

void verifyFet()
{
    // 先验证测量器自身的相干投影，防止分析错误被当成 DSP 失真。
    struct Unity
    {
        void prepare(const juce::dsp::ProcessSpec&) {}
        void process(juce::AudioBuffer<float>&) {}
        float getCurrentGrDb() const { return 0; }
    } identity;
    for (double sr : {44100., 48000., 96000., 192000.})
    {
        auto reference = tone(identity, sr, 1000, -6);
        require(reference.residualPercent < 0.001 && std::abs(reference.outputDb + 9.0103) < 0.001,
                "相干测量基准不正确");
        for (double hz : {50., 100., 1000., 10000.})
        {
            FetEngine e; setup(e);
            const auto m = tone(e, sr, hz, -6);
            // 快释放、深压缩允许低频周期调制，但不能出现原来的反馈振荡。
            const double ceiling = hz == 50 ? 9 : hz == 100 ? 5.5 : 1;
            require(m.residualPercent < ceiling, "FET 快释放残差超过行为基线");
            require(m.gr > 14 && m.gr < 17, "FET 快释放压缩深度异常");
        }
        double previousTime = 0;
        for (float attack : {7.f, 1.f})
        {
            FetEngine e; setup(e, 0, 1); e.attackMs = attack;
            e.prepare({sr, 1, 1});
            juce::AudioBuffer<float> b(1, 1);
            const double target = 0.75 * (-6.0 + 20.0 - 0.5);
            int reached = 0;
            for (int n = 0; n < int(sr * 0.02); ++n)
            {
                b.setSample(0, 0, float(gain(-6))); e.process(b);
                if (!reached && e.getCurrentGrDb() >= 0.95 * target) reached = n + 1;
                require(e.getCurrentGrDb() <= target + 0.01, "FET 阶跃启动过冲");
            }
            const double time = reached / sr;
            require(reached > 0 && time > previousTime, "FET 启动旋钮方向错误");
            require(attack == 7 ? time < 0.0002 : time > 0.001 && time < 0.004, "FET 整体启动时间异常");
            previousTime = time;
        }
        for (float release : {7.f, 1.f})
        {
            FetEngine e; setup(e, 0, release); e.prepare({sr, 1, 1});
            juce::AudioBuffer<float> b(1, 1);
            for (int n = 0; n < int(sr * 0.02); ++n)
            { b.setSample(0, 0, float(gain(-6))); e.process(b); }
            const double initial = e.getCurrentGrDb();
            double previous = initial;
            const double seconds = release == 7 ? 0.05 : 1.1;
            for (int n = 0; n < int(std::lround(sr * seconds)); ++n)
            {
                b.clear(); e.process(b);
                require(e.getCurrentGrDb() <= previous + 1e-6, "FET 静音释放发生反向增长");
                previous = e.getCurrentGrDb();
            }
            // 约 10dB 初始压缩在一档标称时间后恢复约一半；深压缩采用较长恢复，避免周期纹波放大。
            require(previous / initial > 0.45 && previous / initial < 0.65, "FET 释放速度离开规定工作区");
        }
        FetEngine smoothing; setup(smoothing, 0); smoothing.prepare({sr, 1, 1});
        smoothing.makeupDb = 36;
        juce::AudioBuffer<float> quiet(1, 1);
        for (int n = 0; n < int(std::lround(sr * 0.005)); ++n)
        { quiet.setSample(0, 0, 0.001f); smoothing.process(quiet); }
        require(std::abs(decibels(quiet.getSample(0, 0) / 0.001) - 12 * (1 - std::exp(-1.0))) < 0.04,
                "FET 输出参数平滑时长随采样率变化");
    }
    for (int ratio = 0; ratio < 4; ++ratio)
    {
        FetEngine low, high; setup(low, ratio); setup(high, ratio);
        const auto a = tone(low, 48000, 1000, -12), b = tone(high, 48000, 1000, -6);
        constexpr double ratios[] = {4, 8, 12, 20};
        const double actualRatio = 6 / (b.outputDb - a.outputDb);
        require(std::abs(actualRatio / ratios[ratio] - 1) < 0.15, "FET 四个比例档的静态斜率失准");
    }
    Measurement before {};
    for (float output : {24.f, 36.f, 48.f})
    {
        FetEngine e; setup(e, 0, 7, output);
        const auto m = tone(e, 48000, 1000, -6);
        if (output != 24)
        {
            require(std::abs(m.outputDb - before.outputDb - 12) < 0.002, "FET 输出不是独立线性补偿");
            require(std::abs(m.residualPercent - before.residualPercent) < 0.005, "FET 补偿旋钮改变失真");
            require(std::abs(m.gr - before.gr) < 0.001, "FET 输出补偿影响检波");
        }
        before = m;
    }
    for (double sr : {44100., 48000., 96000., 192000.})
    for (double hz : {50., 100., 1000., 10000.})
    for (int ratio : {0, 3, 4})
    {
        FetEngine e; setup(e, ratio); e.fetDrive = 42;
        const auto m = tone(e, sr, hz, -6);
        require(m.gr > 25 && m.gr < 42, "FET 重压缩没有达到测试深度");
        require(m.residualPercent < (hz == 50 ? 5.5 : hz == 100 ? 3.2 : 0.6),
                "FET 重压缩最快时间的增益纹波失真过大");
    }
    std::cout << "FET：跨采样率残差、无启动过冲、时间方向、输出独立性通过\n";
}

void verifyVca()
{
    VcaEngine e; setup(e); e.attackMs = 30; e.releaseMs = 50;
    e.prepare({48000, 1, 1});
    juce::AudioBuffer<float> b(1, 1);
    double last = 0, minimum = 100, maximum = 0;
    for (int n = 0; n < 96000; ++n)
    {
        b.setSample(0, 0, float(gain(n < 48000 ? -6 : -12)));
        e.process(b);
        const double gr = e.getCurrentGrDb();
        if (n > 72000)
        { minimum = std::min(minimum, gr); maximum = std::max(maximum, gr); }
        if (n > 49000) require(gr <= last + 0.00002, "VCA 降至仍压缩的目标后反复切换启动和释放");
        last = gr;
    }
    require(std::abs(last - 9) < 0.01 && maximum - minimum < 0.005, "VCA 无法收敛至非零压缩目标");
    Measurement previous {};
    for (float makeup : {0.f, 12.f, 24.f})
    {
        VcaEngine output; setup(output); output.ratio = 1; output.makeupDb = makeup;
        const auto m = tone(output, 48000, 1000, 0);
        if (makeup > 0)
        {
            require(std::abs(m.outputDb - previous.outputDb - 12) < 0.002, "VCA 输出补偿不成比例");
            require(std::abs(m.residualPercent - previous.residualPercent) < 0.005, "VCA 输出补偿改变非线性失真");
        }
        previous = m;
    }
    VcaEngine monotonic; setup(monotonic); monotonic.ratio = 1; monotonic.prepare({48000, 1, 1});
    double previousOutput = 0;
    for (int n = 0; n < 2000; ++n)
    {
        b.setSample(0, 0, float(n) * 0.01f); monotonic.process(b);
        require(b.getSample(0, 0) >= previousOutput && std::isfinite(b.getSample(0, 0)), "VCA 输出传递曲线反向或溢出");
        previousOutput = b.getSample(0, 0);
    }
    std::cout << "VCA：非零目标释放与稳态精度通过\n";
}

void verifyOpto()
{
    double referenceLow = 0;
    std::array<double, 3> referenceTails {};
    for (double sr : {44100., 48000., 96000., 192000.})
    {
        OptoEngine bypass; setup(bypass, 0);
        const auto low = tone(bypass, sr, 20, -30);
        require(low.gr < 0.001 && low.residualPercent < 0.001, "Opto PR 关闭时仍压缩或引入失真");
        if (sr == 44100) referenceLow = low.outputDb;
        require(std::abs(low.outputDb - referenceLow) < 0.01, "Opto 低频响应随采样率漂移");

        Measurement previous {};
        for (float output : {40.f, 60.f, 80.f})
        {
            OptoEngine e; setup(e, 60, output);
            const auto m = tone(e, sr, 1000, -6);
            require(m.residualPercent < 0.1 && m.gr > 8 && m.gr < 18, "Opto 稳态压缩或基波外残差异常");
            if (output > 40)
            {
                require(m.outputDb > previous.outputDb + 6, "Opto 输出补偿未正常增加音量");
                require(std::abs(m.gr - previous.gr) < 0.001, "Opto 输出补偿反向影响检波");
                require(std::abs(m.residualPercent - previous.residualPercent) < 0.005, "Opto 输出补偿增加非线性失真");
            }
            previous = m;
        }

        std::array<double, 3> tails {};
        int index = 0;
        for (double duration : {0.01, 0.1, 2.0})
        {
            OptoEngine e; setup(e); e.prepare({sr, 1, 1});
            juce::AudioBuffer<float> b(1, 1);
            const int burst = int(std::round(duration * sr));
            for (int n = 0; n < burst; ++n)
            {
                b.setSample(0, 0, float(gain(-6) * std::sin(2 * pi * 1000 * n / sr)));
                e.process(b);
            }
            require(e.getCurrentGrDb() > 2, "Opto 无法捕获短音");
            double last = e.getCurrentGrDb();
            for (int n = 0; n < int(8 * sr); ++n)
            {
                b.clear(); e.process(b);
                const double gr = e.getCurrentGrDb();
                require(std::isfinite(b.getSample(0, 0)) && std::isfinite(gr), "Opto 恢复过程出现非有限值");
                // 留出检波器排空时间；之后不能靠重分配状态权重产生反弹。
                if (n > int(0.03 * sr)) require(gr <= last + 0.00001, "Opto 释放尾部反弹");
                if (n == int(sr)) tails[static_cast<size_t>(index)] = gr;
                last = gr;
            }
            require(last < 0.05, "Opto 静音后不能恢复至接近零衰减");
            ++index;
        }
        require(tails[2] > tails[0] + 1 && tails[2] > tails[1] + 0.5,
                "Opto 长音没有形成独立的慢恢复尾部");
        if (sr == 44100) referenceTails = tails;
        for (size_t i = 0; i < tails.size(); ++i)
            require(std::abs(tails[i] - referenceTails[i]) < 0.05, "Opto 恢复时间随采样率漂移");
    }

    // 对相同输入比较增益比，排除波形过零对自动化连续性判断的干扰。
    OptoEngine reference, automated; setup(reference, 0); setup(automated, 0);
    reference.prepare({48000, 1, 1}); automated.prepare({48000, 1, 1});
    juce::AudioBuffer<float> a(1, 1), b(1, 1);
    double lastDb = 0;
    for (int n = 0; n < 9600; ++n)
    {
        if (n == 4800) automated.makeupDb = 80;
        const float input = float(0.01 * std::sin(2 * pi * 1000 * n / 48000));
        a.setSample(0, 0, input); b.setSample(0, 0, input);
        reference.process(a); automated.process(b);
        if (std::abs(a.getSample(0, 0)) > 0.001)
        {
            const double db = decibels(std::abs(b.getSample(0, 0) / a.getSample(0, 0)));
            require(db >= lastDb - 0.001 && db - lastDb < 0.5, "Opto GAIN 自动化出现增益跳变");
            lastDb = db;
        }
    }
    require(lastDb > 20 && lastDb < 30, "Opto 平滑器没有到达新的输出工作区");
    std::cout << "Opto：跨采样率低频响应、输出独立性、长短音恢复、GAIN 连续性通过\n";
}

void verifyLev()
{
    // 低段力度映射更新后的固定输出样本；参数接口保持兼容，10 dB 附近即可进入可感知平准区。
    struct Reference { double input; float comp; double output, extra; };
    const Reference reference[] = {{-30,-12,-50.4565,0}, {-30,-20,-44.3245,0},
        {-24,-20,-40.5825,0}, {-18,-20,-37.7450,0}, {-12,-24,-35.2419,0}, {-6,-36,-34.1727,0}};
    for (double sr : {44100.,48000.,96000.,192000.})
    for (const auto& row : reference)
    {
        VoxEngine e; e.setComp(row.comp); e.setGate(-80); e.setGain(-30);
        const auto measured = tone(e, sr, 1000, row.input);
        require(std::abs(row.output - measured.outputDb - row.extra) < 0.08, "LEV 压缩增强量偏离试听基线目标");
        require(measured.residualPercent < 0.15, "LEV 稳态人声频段出现过量增益纹波");
        if (row.input == -24 && row.comp == -20)
            require(e.getCompGrDb() > 5.5 && e.getCompGrDb() < 6.1, "LEV 在常见人声电平下压缩深度不正确");
    }
    for (double sr : {44100., 48000., 96000., 192000.})
    {
        require(VoxEngine::getLatencySamples(sr) == 0, "LEV 仍声明额外采样延迟");
        VoxEngine e; e.setComp(0); e.setGain(0); e.setGate(-80); e.prepare({sr, 1, 2});
        juce::AudioBuffer<float> b(2, 1);
        std::vector<float> original(static_cast<size_t>(int(sr * 0.2)), 0.05f);
        original[static_cast<size_t>(int(sr * 0.06))] = 8;
        original[static_cast<size_t>(int(sr * 0.06) + 7)] = -4;
        double lastGain = 0;
        for (int n = 0; n < int(original.size()); ++n)
        {
            b.setSample(0, 0, original[static_cast<size_t>(n)]);
            b.setSample(1, 0, -original[static_cast<size_t>(n)]); e.process(b);
            require(std::abs(b.getSample(0, 0)) <= 0.99901 && b.getSample(0, 0) == -b.getSample(1, 0),
                    "LEV 峰值越界或立体声失配");
            const double actualGain = b.getSample(0, 0) / original[static_cast<size_t>(n)];
            if (n < int(sr * 0.06))
                require(b.getSample(0, 0) == original[static_cast<size_t>(n)], "LEV 提前改变尚未发生的尖峰前音频");
            if (n == int(sr * 0.06))
                require(std::abs(b.getSample(0, 0) - .999) < 1e-6, "LEV 尖峰没有在输入的同一采样输出");
            if (n > int(sr * 0.06) + 7)
                require(actualGain >= lastGain - 1e-6 && actualGain - lastGain < .001,
                        "LEV 尖峰后恢复不连续或过快");
            lastGain = actualGain;
        }
        // 单位曲线下直接逐样本抵消，覆盖全干、并联和全湿，不能仅靠宿主延迟声明判断。
        for (float mix : {0.f, 50.f, 100.f})
        {
            VoxEngine transparent; transparent.setMix(mix); transparent.prepare({sr, 769, 2});
            juce::AudioBuffer<float> signal(2, 769), referenceSignal(2, 769);
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 769; ++n)
                    signal.setSample(c, n, float(.2 * std::sin(.23 * n + c) + (n == 13 ? .4 : 0)));
            referenceSignal.makeCopyOf(signal); transparent.process(signal);
            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < 769; ++n)
                    require(signal.getSample(c,n) == referenceSignal.getSample(c,n), "LEV 干湿音频未实现逐采样零延迟");
            signal.clear(); transparent.process(signal);
            require(signal.getMagnitude(0,769) == 0, "LEV 仍有延迟音频残留");
        }
    }
    VoxEngine low, high;
    for (auto* e : {&low, &high}) { e->setGate(-80); e->setComp(-24); e->setGain(-12); }
    const auto a = tone(low, 48000, 1000, -24);
    const auto b = tone(high, 48000, 1000, -12);
    require(b.outputDb - a.outputDb < 5 && b.outputDb > a.outputDb, "LEV 主体动态没有被有效压缩");
    require(high.getCompGrDb() > 8 && high.getLimiterGrDb() < 1, "LEV 主体压缩仍依靠限幅器承担");
    std::cout << "LEV：零延迟、即时峰值控制、干湿逐样本对齐、压缩增强约 1 dB 通过\n";

    std::array<double, 2> referenceRecovery {};
    for (double sr : {44100.,48000.,96000.,192000.})
    {
        std::array<double, 2> recovery {};
        for (int which = 0; which < 2; ++which)
        {
            const double duration = which == 0 ? .03 : 2;
            VoxEngine e;e.setComp(-20);e.setGain(-30);e.setGate(-80);e.prepare({sr,1,1});
            juce::AudioBuffer<float> b(1,1);
            double maximumGr = 0, previous = 100;
            for (int n = 0; n < int((duration+2.5)*sr); ++n)
            {
                const double t = n/sr;
                b.setSample(0,0,float(lev_probe::sample(t,which == 0 ? 0 : 2)));e.process(b);
                const double gr = e.getCurrentGrDb();
                if (t >= .25 && t < .25+duration+.005) maximumGr = std::max(maximumGr,gr);
                if (n == int((.25+duration+.1)*sr)) recovery[size_t(which)] = gr;
                if (t > .25+duration+.08 && n%int(sr*.002)==0)
                {
                    require(gr <= previous+.025,"LEV 音尾恢复出现反弹");previous=gr;
                }
            }
            require(maximumGr < 18.5,"LEV 主压缩和峰值保护重复叠加，过度压低拍头");
            require(e.getCurrentGrDb() < 0.7,"LEV 停止强音后长时间无法恢复");
        }
        require(recovery[1] > recovery[0]+2,"LEV 短音与长音的恢复没有区分");
        if (sr==44100) referenceRecovery=recovery;
        for(size_t k=0;k<2;++k)
            require(std::abs(recovery[k]-referenceRecovery[k])<.12,"LEV 动态恢复随采样率漂移");
        std::cout << "LEV " << sr << " Hz：短／长音后 100 毫秒 GR=" << recovery[0] << "/" << recovery[1] << '\n';
    }

    // 改变 GAIN 仅改变输出大小；MIX 切换必须平滑，且干湿信号对齐。
    VoxEngine fixed, automated;
    for (auto* e : {&fixed,&automated}) {e->setComp(-20);e->setGain(-30);e->setGate(-80);e->prepare({48000,1,1});}
    juce::AudioBuffer<float> x(1,1),y(1,1);
    double previousGainDb=0;
    for(int n=0;n<48000;++n)
    {
        if(n==24000)automated.setGain(0);
        x.setSample(0,0,.02f);y.setSample(0,0,.02f);fixed.process(x);automated.process(y);
        require(std::abs(fixed.getCompGrDb()-automated.getCompGrDb())<.001,"LEV GAIN 反向影响主压缩");
        if(n>24000)
        {
            const double ratio=decibels(y.getSample(0,0)/x.getSample(0,0));
            require(ratio >= previousGainDb-.001 && ratio-previousGainDb<.3,"LEV GAIN 自动化跳变");
            previousGainDb=ratio;
        }
    }
    require(std::abs(previousGainDb-30)<.01,"LEV 输出增益未到达指定值");
    automated.setMix(0);double previousOutput=y.getSample(0,0);
    for(int n=0;n<4800;++n)
    {
        y.setSample(0,0,.02f);automated.process(y);
        require(std::abs(y.getSample(0,0)-previousOutput)<.002,"LEV MIX 切换出现不连续跳变");
        previousOutput=y.getSample(0,0);
    }
    require(std::abs(previousOutput-.02)<1e-6,"LEV 全干输出不正确");

    VoxEngine gate;gate.setComp(0);gate.setGain(0);gate.setGate(-40);gate.prepare({48000,1,1});
    for(int n=0;n<48000;++n){x.setSample(0,0,.001f);gate.process(x);}
    require(gate.getGateGrDb()>25 && gate.getCompGrDb()==0,"LEV 门控与主压缩职责混乱");
    for(int n=0;n<2400;++n){x.setSample(0,0,.25f);gate.process(x);}
    require(gate.getGateGrDb()<.05,"LEV 门控未及时打开，可能吞掉起音");
    gate.setGate(-80);
    for(int n=0;n<2400;++n){x.setSample(0,0,.001f);gate.process(x);}
    require(gate.getGateGrDb()<.001,"LEV 门关闭设置无效");
    std::cout << "LEV：拍头不过压、持续时间相关恢复、增益与混合自动化、门控通过\n";
}

void verifyLevSmooth()
{
    for (double sr : {44100., 48000., 96000., 192000.})
    {
        const int delay = VoxEngine::getLatencySamples(sr, true);
        require(std::abs(delay / sr - .012) <= .5 / sr, "LEV 平滑时长随采样率变化");
        for (bool bypass : {false, true})
        for (float mix : {0.f, 50.f, 100.f})
        {
            VoxEngine e; e.setSmooth(true); e.setMix(mix); e.prepare({sr, 1, 2});
            juce::AudioBuffer<float> b(2, 1);
            for (int n = 0; n < delay + 20; ++n)
            {
                b.setSample(0,0,n == 0 ? .5f : 0); b.setSample(1,0,n == 0 ? -.5f : 0);
                e.process(b, bypass);
                require(b.getSample(0,0) == (n == delay ? .5f : 0) && b.getSample(1,0) == -b.getSample(0,0),
                        "LEV 平滑模式实际延迟、干湿或旁通不对齐");
            }
        }
        VoxEngine direct, smooth;
        smooth.setSmooth(true);
        direct.prepare({sr,1,1}); smooth.prepare({sr,1,1});
        juce::AudioBuffer<float> a(1,1),b(1,1);
        const int spike = int(sr*.080);
        auto sample = [spike](int n) { return n == spike ? 8.f : .05f; };
        double directStep = 0, smoothStep = 0, lastDirect = 1, lastSmooth = 1;
        bool preReduced = false;
        for (int n=0;n<int(sr*.2);++n)
        {
            a.setSample(0,0,sample(n)); b.setSample(0,0,sample(n));
            direct.process(a); smooth.process(b);
            const double dg = a.getSample(0,0)/sample(n);
            directStep = std::max(directStep,std::abs(dg-lastDirect)); lastDirect=dg;
            require(std::abs(b.getSample(0,0)) <= .99901,"LEV 平滑后重新出现越界峰值");
            if(n>=delay)
            {
                const double sg=b.getSample(0,0)/sample(n-delay);
                smoothStep=std::max(smoothStep,std::abs(sg-lastSmooth));lastSmooth=sg;
                if(n<spike+delay && sg<.8)preReduced=true;
            }
        }
        require(preReduced && smoothStep < .003 && smoothStep < directStep*.01,
                "LEV 平滑只增加延迟，没有显著平缓峰值控制");

        // 关闭峰值控制的触发条件，单独验证主体包络也经过平滑。
        VoxEngine mainDirect,mainSmooth;mainSmooth.setSmooth(true);
        for(auto* e:{&mainDirect,&mainSmooth}) {e->setComp(-20);e->prepare({sr,1,1});}
        double mainDirectStep=0,mainSmoothStep=0,lastD=1,lastS=1;
        auto quietStep=[sr](int n){return n<int(sr*.1)?.005f:.06f;};
        for(int n=0;n<int(sr*.3);++n)
        {
            a.setSample(0,0,quietStep(n));b.setSample(0,0,quietStep(n));mainDirect.process(a);mainSmooth.process(b);
            const double dg=a.getSample(0,0)/(10*quietStep(n));
            if(n>int(sr*.08))mainDirectStep=std::max(mainDirectStep,std::abs(dg-lastD));lastD=dg;
            if(n>=delay)
            {
                const double sg=b.getSample(0,0)/(10*quietStep(n-delay));
                if(n>int(sr*.08)+delay)mainSmoothStep=std::max(mainSmoothStep,std::abs(sg-lastS));lastS=sg;
            }
            require(mainSmooth.getLimiterGrDb()<.001,"主体平滑测试意外依赖了峰值保护");
        }
        require(mainSmoothStep<mainDirectStep*.9,"LEV 主体增益变化没有被平滑");
        std::cout << "LEV 平滑 " << sr << " Hz：峰值增益步进 " << directStep << " → " << smoothStep
                  << "，主体步进 " << mainDirectStep << " → " << mainSmoothStep << '\n';
    }
    for(float comp:{0.f,-12.f,-20.f,-30.f})
    {
        VoxEngine a,b;a.setComp(comp);b.setComp(comp);b.setSmooth(true);
        a.setGain(-12);b.setGain(-12);
        const auto direct=tone(a,48000,1000,-18),smooth=tone(b,48000,1000,-18);
        require(std::abs(direct.outputDb-smooth.outputDb)<.03,"平滑开关通过大幅改变稳态响度制造差异");
        require(smooth.residualPercent<.1,"LEV 平滑产生稳态失真");
    }
    VoxEngine reference,switched;
    for(auto* e:{&reference,&switched}){e->setComp(-20);e->setGain(-12);e->prepare({48000,1,1});}
    juce::AudioBuffer<float> a(1,1),b(1,1);float previous=0;
    for(int n=0;n<48000;++n)
    {
        if(n==12000)switched.setSmooth(true);
        if(n==24000)switched.setSmooth(false);
        a.setSample(0,0,.02f);b.setSample(0,0,.02f);reference.process(a);switched.process(b);
        if(n>10000)require(std::abs(b.getSample(0,0)-previous)<.001,"LEV 切换平滑时清空缓冲或发生明显跳变");
        if(n>24500)require(a.getSample(0,0)==b.getSample(0,0),"关闭平滑后没有完整返回零延迟路径");
        previous=b.getSample(0,0);
    }
    std::cout << "LEV 平滑：干湿/旁通对齐、主体与峰值平滑、稳态电平、开关过渡通过\n";
}
}

int main(int argc, char** argv)
{
    if (argc == 3 && (std::string(argv[1]) == "--lev-dynamics" || std::string(argv[1]) == "--lev-smooth-dynamics"))
    {
        const std::filesystem::path directory(argv[2]);std::filesystem::create_directories(directory);
        for(int scenario=0;scenario<5;++scenario)
        for(int cv : {0,-20,-30})
        for(int gv : {0,-12,-30})
        {
            VoxEngine e;e.setComp(float(cv));e.setGain(float(gv));e.setGate(-80);
            e.setSmooth(std::string(argv[1]) == "--lev-smooth-dynamics");e.prepare({48000,256,1});
            juce::AudioBuffer<float> block(1,256);
            std::ofstream file(directory / ("lev_"+std::to_string(scenario)+"_"+std::to_string(cv)+"_"+std::to_string(gv)+".f32"),std::ios::binary);
            for(int offset=0;offset<48000*4;offset+=256)
            {
                for(int n=0;n<256;++n)block.setSample(0,n,float(lev_probe::sample((offset+n)/48000.,scenario)));
                e.process(block);file.write(reinterpret_cast<const char*>(block.getReadPointer(0)),256*sizeof(float));
            }
        }
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--lev-reference")
    {
        std::ofstream file(argv[2]);
        file << "input_peak_db,comp,gain,output_rms_db,gr_db\n";
        for (double in : {-36.,-30.,-24.,-18.,-12.,-6.})
        for (float comp : {0.f,-6.f,-12.f,-18.f,-20.f,-24.f,-30.f,-36.f})
        {
            VoxEngine e; e.setComp(comp);e.setGain(-30);e.setGate(-80);
            const auto m=tone(e,48000,1000,in);
            file << in << ',' << comp << ",-30," << m.outputDb << ',' << m.gr << '\n';
        }
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--capture") { capture(argv[2]); return 0; }
    if (argc == 3 && std::string(argv[1]) == "--opto-audit") { captureOpto(argv[2]); return 0; }
    if (argc == 3 && std::string(argv[1]) == "--vca-audit") { captureVca(argv[2]); return 0; }
    if (argc == 3 && std::string(argv[1]) == "--fet-stress") { captureFetStress(argv[2]); return 0; }
    try { verifyFet(); verifyVca(); verifyOpto(); verifyLev(); verifyLevSmooth(); }
    catch (const std::exception& e) { std::cerr << "失败：" << e.what() << '\n'; return 1; }
    return 0;
}
