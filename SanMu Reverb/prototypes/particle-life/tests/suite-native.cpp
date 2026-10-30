#include "../dsp/suite-bridge.cpp"
#include <cstdio>

int main() {
    for (int mode = 0; mode < 8; ++mode) {
        suite_init(48000);
        constexpr std::array<float, 14> controls {2.4f,20,6000,-18,300,1,100,30,100,100,.8f,20,20,9000};
        std::copy(controls.begin(), controls.end(), suite_parameters());
        suite_commit(mode);
        float* data = suite_buffer();
        for (int start = 0; start < 48000; start += 128) {
            const int count = std::min(128, 48000 - start);
            for (int i = 0; i < count; ++i) {
                data[i] = start + i == 0 ? .3f : 0;
                data[128 + i] = start + i == 64 ? .2f : 0;
            }
            suite_process(count);
            for (int i = 0; i < count; ++i) {
                std::fwrite(data + 256 + i, sizeof(float), 1, stdout);
                std::fwrite(data + 384 + i, sizeof(float), 1, stdout);
            }
        }
    }
}
