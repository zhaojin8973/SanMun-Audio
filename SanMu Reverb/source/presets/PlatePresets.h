#pragma once
#include <array>

namespace plate_presets
{
struct Definition
{
    const char* name;
    const char* purpose;
    // Decay、PreDelay、Mix、四个阻尼、Size、Attack、两个扩散、两个调制、LowCut、HighCut。
    std::array<float,15> values;
};
inline constexpr std::array<Definition,3> definitions {{
    {"丝滑板式 · 基准", "清晰起音与温暖细密的纯湿基准；插入使用时再降低Mix。",
     {2.39f,15,100,6000,-18,300,1,100,30,100,100,.8f,20,20,9000}},
    {"丝滑板式 · 温暖人声", "保留字头间隔，控制中低频积累，纯湿试听起点。",
     {2.1f,28,100,6000,-20,300,.85f,90,35,100,100,.65f,15,90,8500}},
    {"丝滑板式 · 军鼓短板", "较快建立与较短尾音，保留鼓身和沙带纹理。",
     {1.15f,7,100,6500,-16,250,.9f,80,10,90,100,.75f,12,70,10000}}
}};
} // namespace plate_presets
