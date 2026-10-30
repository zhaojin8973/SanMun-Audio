# SanMu EQ6 架构文档

> 本文档面向**未来的维护者**（AI Agent 或工程师）。接手本项目前，先通读本文，
> 再进入代码，可以避免把稳定代码推倒重写。

## 1. 项目概览

SanMu EQ6 是一款六段参量均衡器（EQ）音频插件，支持 VST3 和 AU 两种宿主格式。
核心能力包括：六段 Bell / Low Shelf / High Shelf / Low Cut / High Cut 滤波、
Zero Latency 与 Linear Phase 两种处理模式、2× 过采样、实时频谱显示、双主题 UI。

## 2. 技术栈与版本（重要：固定，不追新）

- 框架：JUCE，以 **git 子模块锁定**在 `external/JUCE`（版本 `6.0.8-4433` 附近）。
- 语言：C++20。
- 构建：CMake（最低 3.25）。
- 插件格式：VST3 + AU。

**约定**：JUCE 版本是锁定的，不要因为系统或 DAW 升级而主动升级 JUCE 主版本，
除非有明确必要性（例如苹果强制要求）。注释里偶尔出现的 "JUCE 9" 是历史误写，
以 `external/JUCE` 实际锁定版本为准。

## 3. 目录结构

```text
source/
  PluginProcessor.cpp/.h       插件入口：AudioProcessor，负责 processBlock、状态保存、A/B
  PluginEditor.cpp/.h          编辑器入口：AudioProcessorEditor，负责整体布局与顶层交互
  dsp/
    EqBand.cpp/.h              单段滤波器：系数计算、平滑、级联、频响
    EqProcessor.cpp/.h         六段级联 + 双模式 + FFT 频谱 + 电平
    LinearPhaseEq.cpp/.h       线性相位 FIR（overlap-save）
  params/
    Parameters.cpp/.h          参数定义、范围、默认值、参数 ID
  presets/
    PresetManager.cpp/.h       预设保存/加载
  ui/
    EqBandControl.cpp/.h       单个频段的三圈旋钮（增益/频率/Q）
    FrequencyResponseComponent.cpp/.h  中间大区域：频响曲线 + 频谱 + 控制点
    LookAndFeel.cpp/.h         主题与通用绘制
    VerticalStripControl.cpp/.h 输入/输出 L/R 竖条
    OutputMeterComponent.cpp/.h 输出电平表
    IconPaths.h                矢量图标路径
    WheelDirection.h           滚轮方向换算
tests/                         独立数值验证（DSP 正确性）
docs/技术说明.md              产品决策视角的说明（与本文互补）
```

## 4. 分层与数据流

项目按职责分成三层（不是严格的 MVC，但有清晰的边界）：

- **Model（DSP）**：`dsp/` 下的 EqBand、EqProcessor、LinearPhaseEq。只做音频计算。
- **View（UI）**：`ui/` 与 PluginEditor。只负责显示和把用户操作转成参数修改。
- **Controller（数据状态）**：`PluginProcessor` + `params/Parameters` 的 APVTS。

核心数据流：

```text
用户拖拽/滚轮
   → UI 组件调用 apvts.getParameter(...)->setValueNotifyingHost(...)
   → APVTS 内部参数值（std::atomic）更新
   → 音频线程 processBlock 读参数值
   → EqProcessor.updateBand(...) 更新 EqBand 的原子字段 + 重算系数
   → UI 定时器/重绘读取原子字段与系数，刷新显示
```

反向（音频 → UI 显示）通过原子数据：

```text
音频线程写频谱/电平（atomic 数组、atomic float）
   → UI 线程读 atomic 并绘制
```

## 5. 线程模型（最容易出错的地方）

插件有两条线程，必须严格区分：

- **音频线程**：宿主回调 `processBlock`，实时要求高，**禁止锁和阻塞**。
- **UI 线程**：消息线程，`paint()`、鼠标事件、30Hz Timer 都在这条线程。

共享数据与同步方式：

| 数据 | 写线程 | 读线程 | 同步方式 |
|------|--------|--------|----------|
| 参数值（APVTS） | UI | 音频 | APVTS 内部 atomic |
| EqBand 的 type/enabled/solo/frequency/gain/q/slope | 音频 | UI | `std::atomic`（relaxed） |
| 频谱 spectrumData/PeakData | 音频 | UI | `std::array<std::atomic>` |
| 输入/输出电平 Peak/Rms | 音频 | UI | `std::atomic<float>` |
| 延迟上报 | 音频触发 | 消息线程 | `AsyncUpdater` |

**关键约定**：

- UI 永远**只读** DSP 状态，绝不直接写 DSP 内部字段；改参数一律走 APVTS。
- 音频线程内不要调用任何可能锁 UI 或分配大量内存的操作。
- 频响计算 `getMagnitudeAtFrequency` 仅在 UI 线程调用，使用无锁缓存
  （参数不变时复用上次 SVF 参数），音频线程全程无锁、零内存分配。

## 6. 关键设计决策

- **2× 过采样**：ZeroLatency 级联在 2× 采样率下设计系数，消除高频双线性卷曲。
  倍数统一定义为 `EqProcessor::OVERSAMPLING_FACTOR`，DSP 与 UI 频响评估**必须**用同一倍数。
- **滤波器拓扑**：EqBand 使用 TPT-SVF（状态变量滤波器）双精度内部状态，
  支持 Bell / Shelf / Cut 五种类型，高频卷曲比 Direct Form 更小。
- **Cut 共振**：Q 直接控制主级共振峰（二阶高通/低通共振峰 ≈ 20·log10(Q)），
  额外级 Butterworth 固定 Q 提供斜率、不放大共振。
- **参数域平滑**：`advanceSmoothing` 直接平滑频率/增益/Q，每块用平滑参数重算，
  过渡期间每个中间状态都是合法滤波器，消除参数跳变爆音。
- **双模式**：Linear Phase 用 overlap-save FIR，核长 8193，延迟约 106.7ms；
  Zero Latency 用 TPT-SVF + 过采样，延迟极小。
- **频谱**：8192 点 FFT + 对数频带 + EMA 平滑 + 峰值保持，输出真实 dBFS。
- **UI 视觉**：双主题（Light/Dark）、频段彩色锚点填充、星光粒子、solo 锁定闪光。

## 7. 构建与安装

```bash
cmake --build build-make --target SanMuQ6_VST3 SanMuQ6_AU -j 10
```

产物会自动安装到：

```text
~/Library/Audio/Plug-Ins/VST3/SanMu EQ6.vst3
~/Library/Audio/Plug-Ins/Components/SanMu EQ6.component
```

REAPER 中需要**移除再重新加载**插件，才会生效最新构建。

## 8. 未来扩展约定（防止旧工程失效）

1. **参数 ID 永不改名**：ID 采用 `band<num>_<param>`（如 `band1_gain`）和语义化
   全局 ID（如 `input_gain_l`、`eq_mode`）。新增功能只新增 ID，绝不改已有 ID 或
   调换其含义，否则旧工程的预设和自动化会丢失。
2. **UI 不写 DSP**：保持"UI 只读 DSP、改参数走 APVTS"的边界。
3. **领域常量集中定义**：过采样倍数、FFT 大小、通道数、采样率等定义在
   `EqProcessor.h` / `Parameters.h`，不要散落在函数内部。
4. **改 DSP 口径要同步 UI**：如果动过采样倍数或系数设计，必须同时改 UI 频响评估口径，
   否则曲线会与实际听感错位。

## 9. 维护与恢复

- 分支：当前主线为 `ui-adjustment`，远端为 GitHub `zhaojin8973/Plug-Ins`。
- 提交前先 `cmake --build ...` 确认无 error，再到 REAPER 实测拖拽/听感无回归。
- 出错回退：未提交改动可用 `git restore .`；已提交可用 `git log` 找到基线后回退。
- 混音师验收要点：低切共振峰对标肥波 Q3、颜色过渡无断层、拖拽跟手、无爆音、
  双模式音质差异符合预期。
