> 历史记录提示（2026-09-05）：本文保留原始迭代过程。其中对 Valhalla 内部结构的断言、以峰值测试宣布听感问题解决、以及“全部通过/已发布”等结论，均不作为当前验收依据。当前诊断与复测以 `HALL_REMEDIATION_2026-09-05.md` 和实际构建输出为准。

# SanMu Reverb 全局代码编写与参数设计逻辑全量深度审查报告

> **审计基准**：macOS JUCE 8 / C++20 VST3 & AU 架构  
> **审查范围**：4 大引擎实现 (`HallEngine`, `PlateEngine`, `RoomEngine`, `ChamberEngine`)、底层 DSP 原语 (`primitives/`)、参数系统 (`Parameters`)、顶层调度与平滑 (`ReverbProcessor`, `PluginProcessor`)、UI 与状态 (`PluginEditor`, `PresetManager`)。

---

## 目录
1. [致命与高危缺陷 (Priority 1: 稳定性与核心功能失效)](#1-致命与高危缺陷-priority-1)
2. [参数逻辑断层与伪参数 (Priority 2: 旋钮与引擎脱节)](#2-参数逻辑断层与伪参数-priority-2)
3. [各引擎声学架构与物理建模缺陷 (Priority 3: 架构不均衡与金属感病灶)](#3-各引擎声学架构与物理建模缺陷-priority-3)
4. [参数映射、范围与滤波数学不一致 (Priority 4: 边界条件与数值计算)](#4-参数映射范围与滤波数学不一致-priority-4)
5. [实时性、插值与 DAW 自动化平滑 (Priority 5: 调制与杂音防护)](#5-实时性插值与-daw-自动化平滑-priority-5)
6. [整改路线与优先级建议 (Roadmap)](#6-整改路线与优先级建议)

---

## 1. 致命与高危缺陷 (Priority 1)

### 1.1 `Attack` 旋钮在所有 4 大引擎中全线空跑 (Ghost / Dummy Parameter)
- **涉及文件**：
  - [`AttackEnvelope.h:L37-L46`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/dsp/primitives/AttackEnvelope.h#L37-L46)
  - [`HallEngine.cpp`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/dsp/engines/HallEngine.cpp) (全篇未引用 `params.attack`)
  - [`PlateEngine.cpp:L161-L162`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/dsp/engines/PlateEngine.cpp#L161-L162)
  - [`RoomEngine.cpp:L117-L118`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/dsp/engines/RoomEngine.cpp#L117-L118)
  - [`ChamberEngine.cpp:L134-L135`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/dsp/engines/ChamberEngine.cpp#L134-L135)
- **现状剖析**：
  在 `AttackEnvelope.h` 中，为了修复之前低通滤波导致的电平衰减，`processTankInput` 和 `processInput` 均被直接写死为 `return input;`。
  而在目前测试最多的 `HallEngine.cpp` 中，`params.attack` 甚至根本没有被传入任何函数。
- **听感影响**：
  用户在界面上调节 ATTACK 旋钮（0%~100%），混响起音、Bloom 升腾过程没有任何物理变化。
- **修复方案**：
  实现真正的混响能量起音包络（Tank Bloom Envelope）：在进入环路或环路首圈反馈时，通过可变斜率的窗函数软启动积分，使残响能量在 0ms~120ms 范围内自然缓升，让人声瞬态咬字后大厅声场从容绽放。

---

### 1.2 `LoopDampingFilter` 直流增益失控与环路内缺乏 DC Blocker (低频积聚与爆音隐患)
- **涉及文件**：
  - [`OnePoleFilters.h:L160-L191`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/dsp/primitives/OnePoleFilters.h#L160-L191)
  - [`HallEngine.cpp:L244-L257`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/dsp/engines/HallEngine.cpp#L244-L257)
- **数学本质**：
  在 `LoopDampingFilter::process(float x)` 中：
  $$y = y_{\text{Damp}} + k_{\text{Bass}} \cdot s_{\text{Bass}}$$
  在直流端（$f = 0\text{ Hz}, z = 1$），一阶低通的直流增益恒为 $1.0$，$s_{\text{Bass}}$ 的直流增益也为 $1.0$。
  因此该滤波器的直流增益为：
  $$H(0) = 1.0 + k_{\text{Bass}}$$
  当用户将 `Bass Mult` 调大（$> 1.0$）时，$k_{\text{Bass}}$ 可达到 $+0.35$。此时滤波器的直流增益为 **$1.35$（$+2.6\text{ dB}$）**！
  而在反馈环路中，循环增益 $g_{\text{loop}} \approx 0.98$。两相连乘：
  $$G_{\text{loop, DC}} = 0.98 \times 1.35 = 1.323 > 1.0$$
- **环路缺少 DC 隔直滤波**：
  在 `HallEngine.cpp`（以及 Plate, Room, Chamber）中，`lowCutFilter` 仅放置在最终的湿声输出端（第 279-281 行），**反馈循环环路内部没有任何高通滤波或隔直器（DC Blocker）**。
- **听感影响**：
  在长衰减（Decay > 4s）且 Bass Mult > 1.0 时，环路内部会无休止自激累积极低频次声波（< 10Hz）和直流偏置，可能导致内部 DelayLine 数值饱和或在切断音频后产生低频轰鸣。

---

### 1.3 Concert Hall 模式下写死截断 `Bass Mult` 限制了用户调节自由度
- **涉及文件**：
  - [`HallEngine.cpp:L175`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/dsp/engines/HallEngine.cpp#L175)
- **代码定位**：
  ```cpp
  const float effBassMult = (params.mode == 0) ? std::max(1.25f, params.bassMult * 1.20f) : params.bassMult;
  ```
- **问题分析**：
  在 Concert Hall（模式 0）下，如果用户想要一个干净通透、低频紧致不浑浊的混响，将面板上的 `Bass Mult` 拧到 0.25x 或 0.5x，`std::max(1.25f, ...)` 会强行将其锁死在 1.25x！
  用户完全失去了将大厅低频衰减变短的能力。这是之前迭代中为了追求低频厚度遗留的“硬编码补丁”。

---

## 2. 参数逻辑断层与伪参数 (Priority 2)

除了 `HallEngine` 完整实现了 `Early Diff` 和 `Late Diff` 以外，**其他 3 大引擎存在大面积的旋钮功能虚设**：

| 引擎分类 | Early Diff 旋钮响应 | Late Diff 旋钮响应 | Attack 旋钮响应 | 状态评估 |
| :--- | :---: | :---: | :---: | :--- |
| **Hall (大厅)** | ✅ 动态控制前置全通反馈深度 | ✅ 动态控制环内全通漫射深度 | ❌ 空跑 (未接入代码) | 核心三层架构已通，需补全 Attack |
| **Plate (金属板)** | ❌ **完全无效** (内部写死 0.70~0.80) | ❌ **完全无效** (全篇未引用) | ❌ 空跑 (`return input;`) | 3 个旋钮在 Plate 下均无效 |
| **Room (房间)** | ⚠️ 仅微弱控制早期树全通平滑 | ❌ **完全无效** (矩阵使用写死 Householder) | ❌ 空跑 (`return input;`) | Late Diff 无效，无离散回声到扩散演化 |
| **Chamber (混响室)** | ❌ **完全无效** (全篇未引用) | ❌ **完全无效** (全篇未引用) | ❌ 空跑 (`return input;`) | 3 个旋钮在 Chamber 下均无效 |

### 2.1 闲置的军火库：`ContinuousHadamard8` 处于孤立未启用状态
- **涉及文件**：[`HadamardMatrix.h:L69-L99`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/dsp/primitives/HadamardMatrix.h#L69-L99)
- **现状**：
  代码中已经编写了数学上极其优美的 8 阶连续吉文斯-阿达马旋转矩阵 (`ContinuousHadamard8`)，支持从 $\theta = 0$（严格单位阵，8 条支路 100% 解耦无串扰，纯净 Delay）连续平滑旋转到 $\theta = \pi/4$（严格阿达马全向漫反射）。
  然而全局 Grep 显示，该原语没有被任何一个引擎调用！

---

## 3. 各引擎声学架构与物理建模缺陷 (Priority 3)

### 3.1 HallEngine：LFO 调制直接作用在第一声主 Delay 上，造成起音首击音高微抖
- **代码定位**：[`HallEngine.cpp:L230-L231`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/dsp/engines/HallEngine.cpp#L230-L231)
  ```cpp
  float tapL1 = tankDelayL1.readHermite(curD_1 + modL); // ~140.4 ms
  float tapR1 = tankDelayR1.readHermite(curD_1 + modR); // ~140.4 ms
  ```
- **机理缺陷**：
  `tapL1` 和 `tapR1` 是从干声进入 Tank 后的**第一声独立跳出的主 Delay 脉冲**。
  将 `modL/modR` 直接加在 `curD_1` 上，会导致这第一声 Delay 产生多普勒微弱变调（轻微 Flanging/Chorus 感）。
- **行业经典设计 (Valhalla / Lexicon 480L)**：
  第一声主 Delay 应当**坚如磐石、绝对音高恒定（No Modulation）**，使人声字尾的第一次回声清晰坚定；而将调制放置在**循环 Tank 内部的二次延迟线或扩散全通上**（如 `tankDelayL2` 或 `loopDiffuser`），使得混响尾音随着时间推移自然合唱化、云雾化。

---

### 3.2 RoomEngine：4 阶小延迟 FDN 模态密度严重过低，金属梳状铃音（Metallic Ringing）无法根除
- **代码定位**：[`RoomEngine.cpp:L20-L25`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/dsp/engines/RoomEngine.cpp#L20-L25)
  ```cpp
  baseDelays = { 541, 683, 827, 997 }; // 48kHz 下仅 11.2ms ~ 20.8ms
  ```
- **声学本质**：
  4 条仅有 11ms~20ms 的延迟线组成的 FDN，每八度内的极点模态数量极少。当施加反馈时，在这些延迟互质频率点会产生强烈的共振梳状滤波器峰值，无论怎么调 EQ，瞬态打击乐或人声一进来，就会像“在铁桶或金属管道里喊话”，产生刺耳的金属嗡鸣。
- **整改方案**：
  Room 引擎应扩充为 8 阶 FDN，采用阿达马矩阵（或连续阿达马旋转），并将环路延迟阶梯拉开至 25ms~65ms，结合微调制打散共鸣。

---

### 3.3 RoomEngine：早期反射树在 4.3ms~11.5ms 密布抽头，破坏干声清晰度
- **代码定位**：[`EarlyReflections.h:L54-L67`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/dsp/primitives/EarlyReflections.h#L54-L67)
  ```cpp
  { 4.3f, 0.82f, 0.18f }, { 7.1f, 0.18f, 0.80f }, { 11.5f, 0.70f, 0.28f }...
  ```
- **声学本质**：
  这正是用户之前在听感中指出的“干声好像被加了一层空心音”的根源所在。4.3ms 对应的梳状零点在 116Hz、232Hz、464Hz，直接把人声基频和胸腔共鸣切出深坑。
  在现代立体声混响中，0~15ms 必须留出声学真空区（ITDG），即使是小房间，前 15ms 也不应存在高幅度的伪反射。

---

### 3.4 ChamberEngine：干声直冲 6 阶腔体无前置扩散，首击生硬
- **代码定位**：[`ChamberEngine.cpp:L168-L173`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/dsp/engines/ChamberEngine.cpp#L168-L173)
  ```cpp
  fdnDelays[0].write(states[0] + sL * 0.90f);
  ```
- **问题分析**：
  真实的 Chamber（如 Capitol Studios 地下回声室）是通过定向扬声器打在粗糙石膏/水泥墙面上，声音在到达拾音麦克风前已经经历了一次漫反射。
  目前代码中直接将带通信号硬生生注入 6 条延迟线，导致第 20ms 出现离散的第 1 声硬敲击，缺乏 Chamber 特有的“毛茸茸的水泥包围感”。

---

## 4. 参数映射、范围与滤波数学不一致 (Priority 4)

### 4.1 `BassMult` 参数定义范围与底层滤波器钳位范围冲突
- **参数定义**：[`Parameters.cpp:L92`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/params/Parameters.cpp#L92)
  面板允许范围：`0.25f` ~ `4.0f`，默认 `1.50f`。
- **底层钳位**：[`OnePoleFilters.h:L154`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/dsp/primitives/OnePoleFilters.h#L154)
  `safeBassMult = juce::jlimit(0.5f, 3.5f, bassMult);`
- **矛盾**：
  当用户把旋钮拧在 0.25x ~ 0.50x 之间时，滤波器完全无响应，死锁在 0.5x；
  当用户拧在 3.5x ~ 4.0x 之间时，滤波器死锁在 3.5x。UI 显示的有效范围被底层静默截断。

### 4.2 `Size` 旋钮耦合前置全通扩散器时延带来的瞬态涂抹
- **代码定位**：[`HallEngine.cpp:L152-L155`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/dsp/engines/HallEngine.cpp#L152-L155)
  在当前 `HallEngine` 中，`sizeScale`（0.6x ~ 1.2x）同时缩放了输入全通扩散器 `diffDelaysL/R`。
  在 Size = 100% 时，4 级全通的总时延达到约 31ms。当 Early Diff = 0% 时，这 4 级全通蜕变为纯延迟线，无形中给输入信号增加了 31ms 的固定延迟；当 Early Diff 增大时，则把瞬态在进入 Tank 前就涂抹了 31ms。
  **建议**：前置全通扩散器的时延应保持在 3ms~10ms 的短固定尺寸（仅用于消除相位自激），不随整体空间尺寸做大幅缩放。

---

## 5. 实时性、插值与 DAW 自动化平滑 (Priority 5)

### 5.1 参数未进行块内或逐采样点平滑 (SmoothedValue 缺失)
- **代码定位**：[`PluginProcessor.cpp:L51-L91`](file:///Users/zhaojin/Documents/SanMun%20Audio/SanMu%20Reverb/source/PluginProcessor.cpp#L51-L91) 与 `HallEngine.cpp:L199-L284`
- **现状**：
  目前参数是按音频块（Block，如 128 或 256 采样点）在处理函数开头直接读取浮点快照。
  在块内部，`loopGainL/R`、`curD_1` 等核心物理变量是阶跃变化的。
- **风险**：
  在静态测试或慢速微调旋钮时完全正常，但若用户在 DAW（如 REAPER、Logic）中画自动化包络（Automation），或用鼠标飞快拖拽 `Size`、`Decay`、`Mix` 旋钮时，延迟线读写指针会发生离散跳跃，产生细微的“拉链声”（Zipper Noise）。
- **建议**：
  对延迟时间、增益因子等核心控制量引入 `juce::SmoothedValue<float>` 或一阶惯性低通平滑。

---

## 6. 整改路线与优先级建议 (Roadmap)

为了让 SanMu Reverb 从“非常接近”彻底跃升为“超越 Valhalla 的商业级混响”，建议按以下四个阶段稳步演进：

### 第一阶段：扫清暗病与稳定性加固 (Quick & Critical Fixes)
1. **解除 Concert Hall 的 `Bass Mult` 1.25x 硬编码锁**，还原 0.25x~4.0x 全量动态范围。
2. **在 `LoopDampingFilter` 内引入 DC Blocker**，并在反馈环路内加入 15Hz 高通隔直，彻底消除 DC 积聚与自激轰鸣风险。
3. **将 `HallEngine` 的 LFO 调制位置从第一声主 Delay 移至环内二次延迟线与全通扩散器**，确保第一声主 Delay 绝对音高纯净稳定，尾音饱满合唱。
4. **对齐 `OnePoleFilters.h` 与 `Parameters.cpp` 的参数范围**（BassMult 0.25x~4.0x）。

### 第二阶段：激活并实现物理 `Attack` (Bloom Envelope)
1. 在 `AttackEnvelope.h` 中实现工业级无损起音升腾包络。
2. 作用于进入 Tank 环路的输入脉冲，使 Attack 旋钮从 0%（瞬态立即进环，干脆紧致）到 100%（在 100ms 内平滑缓慢升腾，产生经典的后排长厅 Bloom 听感）。

### 第三阶段：全面激活 Plate, Room, Chamber 的物理控制
1. **PlateEngine**：接入 `Early Diff`（输入全通增益）和 `Late Diff`（双半环全通反馈），接入 `Attack`。
2. **RoomEngine**：升级为 8 阶 FDN 拓扑，接入 `ContinuousHadamard8` 实现 0% 独立回声到 100% 漫反射的平滑旋转；拉开首击空隙，消除 4.3ms 的梳状滤波金属音。
3. **ChamberEngine**：接入前置漫射与 `Early/Late Diff`，接入 `Attack`，营造好莱坞地下水泥腔体的致密感。

### 第四阶段：商业级平滑与自动化优化
1. 在关键参数更新处引入参数平滑，杜绝快速拖动旋钮与 DAW 自动化时的拉链杂音。
