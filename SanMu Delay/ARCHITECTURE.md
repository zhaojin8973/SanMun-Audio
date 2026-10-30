# SanMu Delay - 架构设计规范 (Architecture Specification)

## 1. 概览与定位
`SanMu Delay`（三牧延迟）是 SanMu Audio 音频套件中的核心空间与调制效果器。
兼具现代高保真工作流的极速简洁，以及 4 大专属物理建模引擎的声音塑造能力：
- **MODERN**：32 位浮点超清纯净延迟，平滑无怪声过渡，TPT-SVF 线性阻尼。
- **TAPE**：经典 1970 磁带录音机物理建模，环内软饱和、磁头录放 EQ 预加重/去加重与温和 Wow/Flutter 漂移。
- **BBD**：1980 模拟桶链设备建模，时钟频率动态阻尼滤波、压扩芯片非线性温暖与复古颗粒。
- **DIFFUSED**：水洗空间混响延迟，直接复用 `SanMu Reverb` 的全通能量守恒扩散原语，使回声自然晕染为高级 Hall 尾音。

## 2. 系统分层与目录结构
```
SanMu Delay/
├── CMakeLists.txt
├── ARCHITECTURE.md
├── ROADMAP.md
├── external/JUCE/
├── tests/
│   └── test_delay_dsp.cpp
└── source/
    ├── CMakeLists.txt
    ├── PluginProcessor.h / .cpp    (宿主接口、侧链总线配置、尾长上报、APVTS 宿主同步)
    ├── PluginEditor.h / .cpp       (轻量级参数交互界面，为最终玻璃拟物 UI 预留接口)
    ├── params/
    │   └── Parameters.h / .cpp     (参数 ID、单一数据源、范围与默认值工厂)
    └── dsp/
        ├── DelayProcessor.h / .cpp (总调度器：拓扑路由、双缓冲淡变、Ducking、Freeze)
        ├── primitives/
        │   ├── DelayLine.h         (2 的幂次寻址、4 点 3 阶 Hermite 分数插值、Anti-Denormal)
        │   ├── AllpassFilter.h     (Schroeder 规范型能量守恒全通扩散器)
        │   ├── TptSvf.h            (双精度状态可变滤波器，用于环内阻尼)
        │   └── DuckingDetector.h   (VCA RMS 检波器与单极点对数球状释放器)
        └── engines/
            ├── DelayEngine.h       (4 大引擎纯虚统一接口)
            ├── ModernEngine.h / .cpp
            ├── TapeEngine.h / .cpp
            ├── BbdEngine.h / .cpp
            └── DiffusedEngine.h / .cpp
```

## 3. 音频线程铁律 (Audio Thread Invariants)
1. **零堆分配（Zero-Allocation）**：所有缓冲（延迟线、淡变缓冲、滤波状态）必须在 `prepareToPlay()` 一次性分配，`processBlock()` 内严禁任何 `new`、`malloc`、`std::vector::push_back` 或容器扩容。
2. **零互斥锁（Zero-Lock）**：音频线程严禁持有任何 `std::mutex`、`CriticalSection`。UI 读写状态必须通过 `std::atomic`（relaxed/acquire-release）或 APVTS 参数机制。
3. **零 I/O 与日志（Zero-I/O & Zero-Log）**：音频线程内严禁 `std::cout`、`DBG`、文件写操作或网络请求。
4. **防次正常数（Anti-Denormal）**：延迟线写入和 processBlock 顶部强制使用 `juce::ScopedNoDenormals`，并在延迟线内部钳位极小值（`< 1e-15f`），杜绝 CPU 异常暴增。
5. **延迟上报规范**：湿路延迟为效果本体，`getLatencySamples()` 恒定返回 0；`getTailLengthSeconds()` 必须上报足够的延迟尾长（如 10 秒），防止宿主在停止走带时硬切断回声。
