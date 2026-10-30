#include "HallMeasurement.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

namespace hm = hall_measurement;

namespace
{
juce::var object() { return juce::var(new juce::DynamicObject()); }
void put(juce::var& v, const char* key, const juce::var& value)
{ v.getDynamicObject()->setProperty(key, value); }
juce::var number(double x) { return std::isfinite(x) ? juce::var(x) : juce::var(); }
void writeJson(const juce::File& file, const juce::var& v)
{
    if (!file.replaceWithText(juce::JSON::toString(v, false)))
        throw std::runtime_error("无法保存 JSON：" + file.getFullPathName().toStdString());
}

struct Field { const char* name; float hm::Params::* member; float low, high; };
const Field fields[] = {
    {"decay", &hm::Params::decay, 0.2f, 20.0f},
    {"predelay", &hm::Params::predelay, 0, 250},
    {"mix", &hm::Params::mix, 0, 100},
    {"high-freq", &hm::Params::highFreq, 1000, 20000},
    {"high-shelf", &hm::Params::highShelf, -24, 0},
    {"bass-freq", &hm::Params::bassFreq, 50, 1000},
    {"bass-mult", &hm::Params::bassMult, 0.25f, 4},
    {"size", &hm::Params::size, 0, 100},
    {"attack", &hm::Params::attack, 0, 100},
    {"early-diff", &hm::Params::earlyDiff, 0, 100},
    {"late-diff", &hm::Params::lateDiff, 0, 100},
    {"mod-rate", &hm::Params::modRate, 0.1f, 10},
    {"mod-depth", &hm::Params::modDepth, 0, 100},
    {"low-cut", &hm::Params::lowCut, 10, 500},
    {"high-cut", &hm::Params::highCut, 1000, 20000}
};

double parse(const std::string& s, double low, double high)
{
    size_t consumed = 0;
    const double x = std::stod(s, &consumed);
    if (consumed != s.size() || !std::isfinite(x) || x < low || x > high)
        throw std::runtime_error("数值超出范围：" + s);
    return x;
}

void writeWav(const juce::File& path, const hm::Audio& a, double sr)
{
    std::unique_ptr<juce::OutputStream> stream = path.createOutputStream();
    if (!stream) throw std::runtime_error("无法创建 WAV");
    juce::WavAudioFormat format;
    auto options = juce::AudioFormatWriterOptions().withSampleRate(sr)
        .withNumChannels(a.getNumChannels()).withBitsPerSample(32)
        .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
    auto writer = format.createWriterFor(stream, options);
    if (!writer || !writer->writeFromAudioSampleBuffer(a, 0, a.getNumSamples()))
        throw std::runtime_error("WAV 写入失败");
}

hm::Audio readWav(const juce::File& path, double sr, double tail)
{
    juce::AudioFormatManager manager;
    manager.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(manager.createReaderFor(path));
    if (!reader) throw std::runtime_error("无法读取音频：" + path.getFullPathName().toStdString());
    if (reader->sampleRate != sr) throw std::runtime_error("输入采样率不一致；请明确先重采样，不自动改变参考录音");
    if (reader->numChannels != 1 && reader->numChannels != 2) throw std::runtime_error("只支持单声道或立体声输入");
    if (reader->lengthInSamples + std::ceil(sr * tail) > sr * 600)
        throw std::runtime_error("单次最多处理 600 秒");
    hm::Audio a(2, static_cast<int>(reader->lengthInSamples + std::ceil(sr * tail)));
    a.clear();
    if (!reader->read(&a, 0, static_cast<int>(reader->lengthInSamples), 0, true, true))
        throw std::runtime_error("读取音频失败");
    if (reader->numChannels == 1) a.copyFrom(1, 0, a, 0, 0, a.getNumSamples());
    return a;
}

juce::var metrics(const hm::Audio& a, double sr, bool impulse)
{
    auto v = object();
    double peak = 0;
    bool finite = true;
    for (int c = 0; c < a.getNumChannels(); ++c)
        for (int n = 0; n < a.getNumSamples(); ++n)
        {
            const double x = a.getSample(c, n);
            finite = finite && std::isfinite(x);
            peak = std::max(peak, std::abs(x));
        }
    put(v, "finite", finite);
    put(v, "peak", number(peak));
    put(v, "peak_dbfs", number(20 * std::log10(peak)));
    const double total = hm::energy(a);
    put(v, "energy_sum_squares", number(total));
    put(v, "rms_dbfs", number(10 * std::log10(total / (a.getNumSamples() * a.getNumChannels()))));
    put(v, "lr_correlation", number(hm::correlation(a, 0, a.getNumSamples())));
    const int first = hm::firstNonzero(a);
    put(v, "onset_seconds_above_1e_minus9", first < 0 ? juce::var() : number(first / sr));
    put(v, "first_500ms_energy_centroid_seconds", number(hm::centroid(a, sr, 0.5)));
    put(v, "first_500ms_energy_median_seconds", number(hm::energyTime(a, sr, 0.5, 0.5)));
    if (impulse)
    {
        const auto fit = hm::fitT20(a, sr);
        put(v, "broadband_t20_fit_valid", fit.valid);
        put(v, "broadband_t20_rt60_seconds", fit.valid ? number(fit.rt60) : juce::var());
        put(v, "broadband_t20_r_squared", number(fit.rSquared));
    }
    juce::Array<juce::var> segments;
    const double edges[] = {0, 0.02, 0.08, 0.2, 0.5, 1, 2, 4, 8, 16, 32, 64, 600};
    for (size_t i = 1; i < std::size(edges); ++i)
    {
        const int start = static_cast<int>(edges[i - 1] * sr);
        const int end = std::min(a.getNumSamples(), static_cast<int>(edges[i] * sr));
        if (start >= end) break;
        auto segment = object();
        const double e = hm::energy(a, start, end);
        put(segment, "start_seconds", edges[i - 1]); put(segment, "end_seconds", end / sr);
        put(segment, "energy_fraction", total > 0 ? number(e / total) : juce::var());
        put(segment, "rms_dbfs", number(10 * std::log10(e / ((end - start) * a.getNumChannels()))));
        put(segment, "lr_correlation", number(hm::correlation(a, start, end)));
        put(segment, "normalized_echo_density", number(hm::echoDensity(a, start, end)));
        segments.add(segment);
    }
    put(v, "segments", segments);
    put(v, "interpretation", juce::String::fromUTF8("客观描述，不能据此宣称声音相似或人工听审通过；全频衰减不是中频标称 Decay。"));
    return v;
}

void envelope(const juce::File& path, const hm::Audio& a, double sr)
{
    std::ofstream out(path.getFullPathName().toStdString());
    if (!out) throw std::runtime_error("无法写入包络 CSV");
    out << "time_seconds,rms_dbfs,edc_db,energy_fraction,lr_correlation,normalized_echo_density\n";
    const auto edc = hm::backwardEnergy(a);
    const int step = std::max(1, static_cast<int>(std::round(sr * 0.01)));
    for (int start = 0; start < a.getNumSamples(); start += step)
    {
        const int end = std::min(a.getNumSamples(), start + step);
        const double e = hm::energy(a, start, end);
        out << start / sr << ',' << 10 * std::log10(std::max(1e-30, e / ((end - start) * a.getNumChannels())))
            << ',' << 10 * std::log10(std::max(1e-30, edc[static_cast<size_t>(start)] / std::max(edc[0], 1e-30)))
            << ',' << e / std::max(edc[0], 1e-30)
            << ',' << hm::correlation(a, start, end) << ',' << hm::echoDensity(a, start, end) << '\n';
    }
}
}

int main(int argc, char** argv)
{
    try
    {
        if (argc == 1 || std::string(argv[1]) == "--help")
        {
            std::cout << "混响离线渲染与测量（不安装插件）\n"
                "必填：--output 新目录\n"
                "输入：--source impulse|noise|wav --input 文件 --reference 同输入的湿声文件\n"
                "运行：--sample-rate 48000 --block 128 --duration 1 --tail 8 --amplitude 0.1 --seed 4242\n"
                "类型：--category 0=Plate，1=Room，2=Hall（默认），3=Chamber\n"
                "模式：--mode 0/1/2；Hall的0为Concert Hall，2为Cathedral\n"
                "参数：";
            for (const auto& f : fields) std::cout << " --" << f.name;
            std::cout << "\n脉冲 duration 默认为 0，默认渲染 8 秒。WAV 保持原采样率且不归一化。\n";
            return 0;
        }
        std::map<std::string, std::string> args;
        for (int i = 1; i < argc; i += 2)
        {
            if (i + 1 >= argc || std::string(argv[i]).rfind("--", 0) != 0)
                throw std::runtime_error("选项必须按 --名称 值 成对提供");
            if (!args.emplace(std::string(argv[i]).substr(2), argv[i + 1]).second)
                throw std::runtime_error("重复选项");
        }
        auto take = [&](const std::string& key, const std::string& fallback) {
            const auto it = args.find(key); if (it == args.end()) return fallback;
            const auto value = it->second; args.erase(it); return value;
        };
        const auto source = take("source", "impulse");
        const auto output = take("output", "");
        const auto inputPath = take("input", "");
        const auto referencePath = take("reference", "");
        const double sr = parse(take("sample-rate", "48000"), 8000, 192000);
        const double blockValue = parse(take("block", "128"), 1, 32768);
        if (blockValue != std::floor(blockValue)) throw std::runtime_error("block 必须为整数");
        const int block = static_cast<int>(blockValue);
        const double duration = parse(take("duration", source == "noise" ? "1" : "0"), 0, 120);
        const double tail = parse(take("tail", "8"), 0.1, 120);
        const double amplitude = parse(take("amplitude", "0.1"), 0.000001, 1);
        const double seedValue = parse(take("seed", "4242"), 1, 4294967295.0);
        if (seedValue != std::floor(seedValue)) throw std::runtime_error("seed 必须为整数");
        const auto seed = static_cast<uint32_t>(seedValue);
        hm::Params p = hm::baseline();
        const double category = parse(take("category", "2"), 0, 3);
        if (category != std::floor(category)) throw std::runtime_error("category 必须为整数");
        p.category = static_cast<int>(category);
        const double mode = parse(take("mode", "0"), 0, 2);
        if (mode != std::floor(mode)) throw std::runtime_error("mode 必须为整数");
        p.mode = static_cast<int>(mode);
        for (const auto& f : fields)
        {
            const auto value = take(f.name, "");
            if (!value.empty()) p.*(f.member) = static_cast<float>(parse(value, f.low, f.high));
        }
        if (!args.empty()) throw std::runtime_error("未知选项：" + args.begin()->first);
        if (output.empty()) throw std::runtime_error("必须指定 --output");
        if (source != "impulse" && source != "noise" && source != "wav") throw std::runtime_error("未知输入类型");
        if (source != "wav" && !inputPath.empty()) throw std::runtime_error("--input 需要 --source wav");
        if (source == "noise" && duration <= 0) throw std::runtime_error("噪声 duration 必须大于零");
        const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile(output);
        if (dir.exists()) throw std::runtime_error("输出目录已存在，为保留证据拒绝覆盖");
        hm::Audio input = source == "wav"
            ? readWav(juce::File::getCurrentWorkingDirectory().getChildFile(inputPath), sr, tail)
            : source == "noise" ? hm::noise(sr, duration, tail, static_cast<float>(amplitude), seed)
                                : hm::impulse(sr, duration + tail, amplitude);
        for (int c = 0; c < input.getNumChannels(); ++c)
            for (int n = 0; n < input.getNumSamples(); ++n)
                if (!std::isfinite(input.getSample(c, n))) throw std::runtime_error("输入含非有限样本");
        juce::ScopedNoDenormals noDenormals;
        const auto rendered = hm::render(input, sr, block, p);
        auto outputMetrics = metrics(rendered, sr, source == "impulse");
        if (!static_cast<bool>(outputMetrics.getProperty("finite", false))) throw std::runtime_error("DSP 输出含非有限样本");
        juce::var comparison;
        hm::Audio reference;
        if (!referencePath.empty())
        {
            reference = readWav(juce::File::getCurrentWorkingDirectory().getChildFile(referencePath), sr, 0);
            if (reference.getNumSamples() != rendered.getNumSamples())
                throw std::runtime_error("参考长度与输出不一致；必须来自同一个 input.wav 且保持起点/尾长");
            comparison = object();
            put(comparison, "reference", metrics(reference, sr, source == "impulse"));
            put(comparison, "candidate", outputMetrics);
            put(comparison, "candidate_minus_reference_energy_db", number(10 * std::log10(hm::energy(rendered) / hm::energy(reference))));
            put(comparison, "alignment", "未移位、未裁切、未归一化。音源与插件状态由参考 manifest 另行核对。");
        }
        if (!dir.createDirectory()) throw std::runtime_error("无法创建输出目录");
        writeWav(dir.getChildFile("input.wav"), input, sr);
        writeWav(dir.getChildFile("output.wav"), rendered, sr);
        writeJson(dir.getChildFile("metrics.json"), outputMetrics);
        envelope(dir.getChildFile("envelope.csv"), rendered, sr);
        if (!referencePath.empty())
        {
            writeJson(dir.getChildFile("comparison.json"), comparison);
            envelope(dir.getChildFile("reference-envelope.csv"), reference, sr);
        }
        auto manifest = object();
        const char* engines[] = {"Plate", "Room", "Hall", "Chamber"};
        put(manifest, "schema_version", 1); put(manifest, "engine", engines[p.category]);
        put(manifest, "category", p.category); put(manifest, "mode", p.mode);
        put(manifest, "source", juce::String(source)); put(manifest, "source_path", juce::String(inputPath));
        put(manifest, "reference_path", juce::String(referencePath)); put(manifest, "sample_rate", sr); put(manifest, "block_size", block);
        put(manifest, "samples", rendered.getNumSamples()); put(manifest, "channels", input.getNumChannels());
        put(manifest, "duration_option_seconds", duration); put(manifest, "tail_seconds", tail);
        put(manifest, "amplitude_option", amplitude); put(manifest, "seed", static_cast<juce::int64>(seed));
        put(manifest, "seed_algorithm", juce::String::fromUTF8("xorshift32，高 24 位映射至 [-1,1)，双声道相同"));
        auto params = object();
        for (const auto& f : fields) put(params, f.name, p.*(f.member));
        put(manifest, "parameters", params);
        juce::Array<juce::var> command;
        for (int i = 0; i < argc; ++i) command.add(juce::String(argv[i]));
        put(manifest, "command_argv", command);
        put(manifest, "created_utc", juce::Time::getCurrentTime().toISO8601(true));
        put(manifest, "input_sha256", juce::SHA256(dir.getChildFile("input.wav")).toHexString());
        put(manifest, "output_sha256", juce::SHA256(dir.getChildFile("output.wav")).toHexString());
        put(manifest, "executable_sha256", juce::SHA256(juce::File::getSpecialLocation(juce::File::currentExecutableFile)).toHexString());
        put(manifest, "status", juce::String::fromUTF8("仅完成离线测量；尚不代表与 Valhalla 相似或听审通过"));
        writeJson(dir.getChildFile("manifest.json"), manifest);
        std::cout << "已保存：" << dir.getFullPathName() << "\n" << juce::JSON::toString(outputMetrics, false) << '\n';
        return 0;
    }
    catch (const std::exception& e) { std::cerr << "错误：" << e.what() << '\n'; return 1; }
}
