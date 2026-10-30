#include "../../../source/dsp/engines/hall/HallField.h"
#include "../../../source/dsp/engines/hall/CathedralField.h"
#include "../../../source/dsp/engines/plate/PlateField.h"
#include "../../../source/dsp/engines/plate/SteelField.h"
#include "../../../source/dsp/engines/room/RoomField.h"
#include "../../../source/dsp/engines/chamber/ChamberField.h"

namespace {
constexpr int capacity = 128;
constexpr std::array<float, 14> defaults {3.99f,20,6000,-24,300,1.5f,100,50,100,100,2.53f,38,10,8010};
constexpr std::array<float, 14> lows {.2f,0,1000,-24,50,.25f,0,0,0,0,.1f,0,10,1000};
constexpr std::array<float, 14> highs {20,250,20000,0,1000,4,100,100,100,100,10,100,500,20000};
std::array<float, capacity * 4> audio {};
std::array<float, 14> targets = defaults;
double rate = 48000;

// 仅作浏览器适配，直接调用已定稿声场；两份状态使切换时能连续淡化。
struct Bank {
    sanmu_hall::Field hall;
    sanmu_hall::CathedralField cathedral;
    sanmu_plate::Field plate;
    sanmu_steel::Field steel;
    sanmu_room::Field room;
    sanmu_chamber::Field chamber;
    std::array<float, 14> values {}, goals {}, steps {};
    std::array<int, 14> remaining {};
    int mode = 0;
    unsigned tick = 0;
    void prepare() {
        hall.prepare(rate); cathedral.prepare(rate); plate.prepare(rate);
        steel.prepare(rate); room.prepare(rate); chamber.prepare(rate);
    }
    void reset(int selected) {
        mode = selected;
        switch (mode) {
            case 0: hall.reset(); break; case 1: cathedral.reset(); break;
            case 2: plate.reset(); break; case 3: steel.reset(); break;
            case 4: case 5: room.reset(); break;
            default: chamber.reset(); break;
        }
        values = goals = targets; remaining.fill(0); tick = 0;
    }
    void commit() {
        const int length = static_cast<int>(rate * (mode >= 6 ? .06 : .05));
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (goals[i] == targets[i]) continue;
            goals[i] = targets[i]; remaining[i] = length;
            steps[i] = (goals[i] - values[i]) / static_cast<float>(length);
        }
    }
    std::array<float, 2> process(float left, float right) {
        for (std::size_t i = 0; i < values.size(); ++i)
            if (remaining[i] > 0) values[i] = --remaining[i] == 0 ? goals[i] : values[i] + steps[i];
        const auto& v = values;
        if (tick == 0) {
            switch (mode) {
                case 0: hall.configure({v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10],v[11],v[12],v[13]}); break;
                case 1: cathedral.configure({v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10],v[11],v[12],v[13]}); break;
                case 2: plate.configure({v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10],v[11],v[12],v[13]}); break;
                case 3: steel.configure({v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10],v[11],v[12],v[13]}); break;
                case 4: case 5: room.configure({v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10],v[11],v[12],v[13]}, mode - 4); break;
                default: chamber.configure({v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],v[9],v[10],v[11],v[12],v[13]}, mode - 6); break;
            }
        }
        tick = (tick + 1) & 31u;
        switch (mode) {
            case 0: return hall.process(left, right); case 1: return cathedral.process(left, right);
            case 2: return plate.process(left, right); case 3: return steel.process(left, right);
            case 4: case 5: return room.process(left, right);
            default: return chamber.process(left, right);
        }
    }
};
std::array<Bank, 2> banks;
int active = 0, requested = 0, progress = 0, fadeLength = 2400;
bool initialized = false;
}

extern "C" {
void suite_reset() {
    banks[active].reset(requested); audio.fill(0); progress = 0;
}
void suite_init(double sampleRate) {
    rate = std::isfinite(sampleRate) && sampleRate >= 8000 && sampleRate <= 192000 ? sampleRate : 48000;
    fadeLength = static_cast<int>(rate * .05);
    for (auto& bank : banks) bank.prepare();
    active = requested = 0; initialized = false; suite_reset();
}
float* suite_buffer() { return audio.data(); }
float* suite_parameters() { return targets.data(); }
void suite_commit(int mode) {
    requested = std::clamp(mode, 0, 7);
    for (std::size_t i = 0; i < targets.size(); ++i)
        targets[i] = std::isfinite(targets[i]) ? std::clamp(targets[i], lows[i], highs[i]) : defaults[i];
    if (!initialized) { banks[active].reset(requested); initialized = true; }
    else if (banks[active].mode == requested) banks[active].commit();
}
void suite_process(int count) {
    count = std::clamp(count, 0, capacity);
    for (int n = 0; n < count; ++n) {
        if (progress == 0 && banks[active].mode != requested) {
            active = 1 - active; banks[active].reset(requested); progress = 1;
        }
        const float left = std::isfinite(audio[n]) ? audio[n] : 0;
        const float right = std::isfinite(audio[capacity + n]) ? audio[capacity + n] : 0;
        auto result = banks[active].process(left, right);
        if (progress > 0) {
            const auto previous = banks[1 - active].process(left, right);
            const double angle = static_cast<double>(progress - 1) / (fadeLength - 1) * 1.5707963267948966;
            for (int ch = 0; ch < 2; ++ch)
                result[ch] = static_cast<float>(result[ch] * std::sin(angle) + previous[ch] * std::cos(angle));
            if (++progress > fadeLength) progress = 0;
        }
        audio[capacity * 2 + n] = result[0]; audio[capacity * 3 + n] = result[1];
    }
}
}
