#include "../../../source/dsp/engines/hall/HallField.h"
#include <fstream>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc < 3) return 1;
    const int alternate = std::stoi(argv[2]);
    const int sampleRate = argc > 3 ? std::stoi(argv[3]) : 48000;
    sanmu_hall::Settings parameters;
    if (alternate) parameters = {1.7f, 100, 4000, -12, 220, .85f, 35, 85, 0, 25, 1.3f, 0, 120, 5100};
    sanmu_hall::Field field;
    field.prepare(sampleRate);
    field.configure(parameters);
    std::ofstream stream(argv[1], std::ios::binary);
    for (int n = 0; n < sampleRate * 3; ++n)
    {
        const float input = n == sampleRate / 10 ? .25f : 0.0f;
        const auto output = field.process(input, input * .8f);
        stream.write(reinterpret_cast<const char*>(output.data()), sizeof(float) * 2);
    }
    return stream.good() ? 0 : 2;
}
