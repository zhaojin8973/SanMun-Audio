#pragma once
#include <cmath>
#include <juce_data_structures/juce_data_structures.h>

// 用户偏好独立于工程、预设和声音参数；每个产品共用一份本机设置。
// 只合并本实例实际改过的白名单字段，避免旧窗口把其他实例的选择写回。
class SanMuUiPreferences
{
public:
    explicit SanMuUiPreferences(const juce::String& product, juce::File directory = {})
        : file((directory == juce::File() ? defaultDirectory() : directory)
                 .getChildFile(product + ".json")), lock("SanMuUiPreferences-" + product) {}

    juce::var load(const juce::var& fallback)
    {
        auto result = fallback.isObject() ? fallback.clone() : juce::var(new juce::DynamicObject());
        const auto saved = read();
        if (saved.isObject()) for (const auto& p : saved.getDynamicObject()->getProperties())
            if (allowed(p.name.toString(), p.value)) result.getDynamicObject()->setProperty(p.name, p.value);
        last = settings(result); return result;
    }
    bool save(const juce::var& state)
    {
        const auto next = settings(state); auto patch = juce::var(new juce::DynamicObject());
        for (const auto& p : next.getDynamicObject()->getProperties())
            if (!last.hasProperty(p.name) || juce::JSON::toString(last[p.name]) != juce::JSON::toString(p.value))
                patch.getDynamicObject()->setProperty(p.name, p.value);
        if (patch.getDynamicObject()->getProperties().isEmpty() && file.existsAsFile()) return true;
        // 只在消息线程的显式界面编辑时落盘；声音处理线程不访问磁盘。
        if (!lock.enter(100)) return false;
        struct Unlock { juce::InterProcessLock& value; ~Unlock() { value.exit(); } } guard {lock};
        const auto stored = read(); auto merged = stored.isObject() ? settings(stored) : next.clone();
        for (const auto& p : patch.getDynamicObject()->getProperties()) merged.getDynamicObject()->setProperty(p.name, p.value);
        if (file.getParentDirectory().createDirectory().failed()) return false;
        juce::TemporaryFile temporary(file);
        if (!temporary.getFile().replaceWithText(juce::JSON::toString(merged)) || !temporary.overwriteTargetFileWithTemporary()) return false;
        last = next; return true;
    }
    static juce::var settings(const juce::var& state)
    {
        auto result = juce::var(new juce::DynamicObject());
        if (state.isObject()) for (const auto& p : state.getDynamicObject()->getProperties())
            if (allowed(p.name.toString(), p.value)) result.getDynamicObject()->setProperty(p.name, p.value);
        return result;
    }
private:
    static juce::File defaultDirectory()
    {
        const auto testDirectory = juce::SystemStats::getEnvironmentVariable("SANMU_TEST_PREFERENCES_DIR", {});
        if (testDirectory.isNotEmpty()) return juce::File(testDirectory);
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Application Support/SanMu Audio/Preferences");
    }
    static bool allowed(const juce::String& key, const juce::var& v)
    {
        if (key == "theme") return v == "light" || v == "dark";
        if (key == "language") return v == "en" || v == "zh";
        if (key == "tips" || key == "tooltipLanguage") return v == "en" || v == "zh" || v == "follow" || v == "auto" || v == "off";
        if (key == "view") return v == "dry" || v == "wet" || v == "overlay";
        for (const auto* k : {"history", "spectrum", "spectrumPre", "spectrumPost", "spectrumGrab", "visualsEnabled", "outputLink"})
            if (key == k) return v.isBool();
        const bool numeric = (v.isInt() || v.isInt64() || v.isDouble()) && std::isfinite(double(v));
        if (key == "range") return v == "auto" || (numeric && (double(v) == 12 || double(v) == 24 || double(v) == 30));
        if (key == "scale" || key == "uiScale") return numeric && double(v) >= .5 && double(v) <= 2;
        if (key == "width") return numeric && double(v) >= 720 && double(v) <= 1920;
        if (key == "height") return numeric && double(v) >= 620 && double(v) <= 1440;
        if (key == "layoutVersion") return numeric && double(v) == 2;
        if (key == "spectrumRange") return numeric && (double(v) == 60 || double(v) == 90 || double(v) == 120);
        if (key == "spectrumResolution") return numeric && double(v) >= 0 && double(v) <= 3 && std::floor(double(v)) == double(v);
        if (key == "spectrumSpeed") return numeric && double(v) >= 0 && double(v) <= 2 && std::floor(double(v)) == double(v);
        if (key == "spectrumTilt") return numeric && double(v) >= 0 && double(v) <= 6;
        if (key == "grRange") return numeric && double(v) > 0 && double(v) <= 60;
        if (key == "levelRange") return numeric && (double(v) == 30 || double(v) == 60);
        if (key == "layers") {
            if (!v.isObject()) return false;
            for (const auto& p : v.getDynamicObject()->getProperties())
                if (!p.value.isBool() || (p.name.toString() != "input" && p.name.toString() != "output" && p.name.toString() != "gr" && p.name.toString() != "in" && p.name.toString() != "out")) return false;
            return true;
        }
        return false;
    }
    juce::var read() const { return file.getSize() <= 32768 ? juce::JSON::parse(file) : juce::var(); }
    juce::File file;
    juce::InterProcessLock lock;
    juce::var last;
};
