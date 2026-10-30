# SanMu Comp — FET (超高速晶体管峰值压限) 插件设计说明书
**SanMu Audio Technical Design & Physical Modeling Specification**

---

## 一、模块定位与设计哲学

`FetEngine`（场效应管峰值压限引擎）是三木音频针对**现代人声动态塑形、鼓组瞬态冲击与打击乐重度压缩**精心打造的纯物理级建模引擎。

### 核心声学目标
1. **微秒级极致抓力**：以极快的响应时间牢牢锁住人声辅音、齿音与鼓皮敲击瞬态，绝不脱节；
2. **爆发力与呼吸律动**：纯正的一阶 RC 物理放电，使大动态在下压后能以自然弹性瞬间复原，杜绝“吃字”与“憋闷”；
3. **录音室经典染色**：复刻模拟离散晶体管非线性饱和与 UTC A-20 输出变压器高频温润电感滤波。

---

## 二、架构拓扑与信号处理流

```mermaid
graph LR
    In[音频输入] --> Drive[Input 驱动放大]
    Drive --> SC_Filter[60Hz~150Hz 侧链低切]
    SC_Filter --> Diode[二极管峰值整流检波]
    Diode --> RC[一阶 RC 定时放电回路]
    RC --> GR_Calc[dB 增益衰减与比率曲线]
    GR_Calc --> CV[FET 栅极压控线性增益]
    Drive --> GainCell[FET 衰减增益单元]
    CV --> GainCell
    GainCell --> OutputStage[Makeup 增益 + 离散非对称饱和]
    OutputStage --> UTC[UTC A-20 变压器 18kHz 滤波]
    UTC --> MixTrim[Dry/Wet 混合 + Output Trim]
    MixTrim --> Out[最终音频输出]
```

---

## 三、时间常数物理标定（Attack & Release）

### 1. 启动时间（Attack Time）
- **旋钮范围**：`1.0`（逆时针最慢）$\sim$ `7.0`（顺时针极速）；
- **物理数值**：**$1600\mu\text{s}$（1.6ms）$\sim$ $20\mu\text{s}$（0.02ms）**；
- **数学映射公式**：
  $$\tau_{\text{att}} = 0.001600 \times \left(\frac{0.000020}{0.001600}\right)^{\frac{\text{Knob}_{\text{att}} - 1.0}{6.0}} \quad (\text{秒})$$
- **各档位声学特性**：
  - **7 档（$20\mu\text{s}$）**：极速瞬态抚平，彻底抹平突发爆破音；
  - **5 档（约 $86\mu\text{s}$）**：紧致人声压限；
  - **3 档（约 $370\mu\text{s}$，出厂默认）**：**黄金工作点**，兼顾人声清晰度与军鼓击打穿透力；
  - **1 档（$1600\mu\text{s}$ / 1.6ms）**：拓展慢启动，让鼓皮击打瞬态完全冲出。

### 2. 释放时间（Release Time）
- **旋钮范围**：`1.0`（逆时针最慢）$\sim$ `7.0`（顺时针极速）；
- **物理数值**：**$1200\text{ms}$（1.2s）$\sim$ $45\text{ms}$（0.045s）**；
- **数学映射公式**：
  $$\tau_{\text{rel}} = 1.20 \times \left(\frac{0.045}{1.20}\right)^{\frac{\text{Knob}_{\text{rel}} - 1.0}{6.0}} \quad (\text{秒})$$
- **各档位声学特性**：
  - **7 档（$45\text{ms}$）**：极速下压与瞬间回弹，激烈呼吸感；
  - **4 档（约 $232\text{ms}$，出厂默认）**：**黄金标准释放点**，人声音轨连贯流畅，绝不吃字；
  - **1 档（$1200\text{ms}$）**：慢速平稳衰减，适合宽松贝斯与原声吉他。

### 3. 放电检波核心算法（纯一阶 RC 物理放电定律）
放电过程严格遵循单 RC 定时网络物理指数放电，移除了任何人工伪多阶段调速干预：
```cpp
const float fetAttackCoeff  = 1.0f - std::exp(-1.0f / (fetAttackSec * static_cast<float>(sr)));
const float fetReleaseCoeff = 1.0f - std::exp(-1.0f / (fetReleaseSec * static_cast<float>(sr)));

if (inLvl > envLvl)
    envLvl += fetAttackCoeff * (inLvl - envLvl);
else
{
    envLvl += fetReleaseCoeff * (0.0f - envLvl);
    if (envLvl < 1.0e-6f)
        envLvl = 0.0f;
}
```

---

## 四、压缩比率、拐点与增益衰减斜率

### 1. 5 档比率配置表
| 比率档位 | 标称比率 $R$ | 基础阈值 $\text{Thresh}_{\text{base}}$ | 斜率修正系数 $\text{SlopeMod}$ | 声学听感定位 |
| :--- | :--- | :--- | :--- | :--- |
| **0 档** | **4:1** | **-20.0 dBFS** | 1.00 | 经典录音室人声与原声乐器平顺压缩 |
| **1 档** | **8:1** | **-21.0 dBFS** | 1.05 | 紧致现代人声与军鼓稳态 |
| **2 档** | **12:1** | **-22.0 dBFS** | 1.10 | 重型摇滚贝斯与强力瞬态压制 |
| **3 档** | **20:1** | **-23.5 dBFS** | 1.15 | 砖墙式超高速峰值限制（Peak Limiter） |
| **4 档 (ALL)** | **ALL (全按)** | **-26.0 dBFS** | 1.35 | 激进暴徒模式（鼓房过载、爆炸性瞬态） |

### 2. 前馈增益衰减计算公式
为了避免大动态过载时发生“压灭/静音”故障，采用标准前馈线性衰减斜率：
$$\text{slope} = \left(1 - \frac{1}{R}\right) \times \text{SlopeMod}$$
- 软拐点宽度 $k_{\text{knee}} = 1.0\text{ dB}$；
- 超出量 $\Delta L = L_{\text{det}} - \text{EffThresh}$；
- 增益衰减：
  $$\text{GR}(\text{dB}) = \begin{cases} 
  0, & \Delta L \le 0 \\
  \text{slope} \cdot \dfrac{\Delta L^2}{2 \cdot k_{\text{knee}}}, & 0 < \Delta L \le k_{\text{knee}} \\
  \text{slope} \cdot \left(\Delta L - \dfrac{k_{\text{knee}}}{2}\right), & \Delta L > k_{\text{knee}}
  \end{cases}$$

---

## 五、灵敏度增益结构与 Headroom（工业级标定）

### 1. 默认开箱即用标定
- **有效阈值**：$\text{EffThresh} = \text{Thresh}_{\text{base}} + (\text{Headroom} - 16.0)$；
- **默认工作状态**：
  - 输入驱动 `FET_INPUT = 18.0`（10:30 方向）；
  - 动态余量 `HEADROOM = 16.0 dB`；
  - 输出补偿 `FET_OUTPUT = 30.0`（1:30 方向）；
  - **开箱声学表现**：标准电平（$-14\text{ dBFS} \sim -12\text{ dBFS}$）的音轨挂上插件，**无需调整即可稳定产生 $3.5 \sim 4.5\text{ dB}$ 黄金紧致压缩**，且输出主观听感响度与原声完美等平衡。

### 2. 旋钮动态范围
- `INPUT (0~48)`：
  - 18.0 = 标称工作驱动（0dB 相对增益）；
  - 24.0 = +6dB 深度驱动；
  - 30.0 = +12dB 重度打桩驱动；
  - 48.0 = +30dB 极限过载打桩。
- `OUTPUT (0~48)`：
  - 24.0 = 0dB Unity 放大；
  - 30.0 = +6dB 经典补偿增益；
  - 48.0 = +24dB 极限放大。

---

## 六、离散电路饱和与变压器滤波建模

### 1. 离散非对称饱和级（Discrete Asymmetric Saturation）
模拟 Class-A 放大器与全按模式下的非对称奇偶次偶合染色：
```cpp
auto process1176OutputStage = [](float x, bool isAllButtons) noexcept -> float
{
    const float drive = isAllButtons ? 1.35f : 1.0f;
    const float in = x * drive;
    const float asym = 0.035f * in * std::fabs(in) / (1.0f + std::fabs(in));
    const float driven = in + asym;
    const float sat = std::tanh(driven * 0.90f) / 0.90f;
    return sat / drive;
};
```

### 2. UTC A-20 输出变压器电感滤波
高通 18kHz 平滑单极点滤波，消除高频数字毛刺，赋予模拟空气感：
```cpp
const float xfmrCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 18000.0f / static_cast<float>(sr));
fetTransformerState[0] += xfmrCoeff * (sigL - fetTransformerState[0]);
```

---

## 七、出厂默认参数表 (Defaults)

| 参数项 | 参数 ID | 默认值 | 物理/声学含义 |
| :--- | :--- | :--- | :--- |
| **模式** | `mode` | `ModeFET` (2) | 切换至 FET 引擎 |
| **Input 驱动** | `fet_input` | `18.0` | 10:30 方向，标准录音室驱动基准 |
| **Ratio 比率** | `fet_ratio` | `0` (4:1) | 4:1 经典平稳压缩比率 |
| **Attack 启动** | `fet_attack` | `3.0` | 约 $370\mu\text{s}$，人声与军鼓黄金点 |
| **Release 释放** | `fet_release` | `4.0` | 约 $232\text{ms}$，12:00 黄金平衡释放 |
| **Output 补偿** | `fet_output` | `30.0` | 1:30 方向，+6dB 等响度补偿 |
| **Headroom 余量**| `headroom` | `16.0 dB` | 标准动态余量工作点 |
| **SC HP 侧链** | `sidechain_hp` | `60.0 Hz` | 滤除超低频次声波，防止误触发 |
| **Mix 干湿比** | `mix` | `100.0%` | 全湿信号输出 |
| **Link 立体声** | `stereo_link` | `100.0%` | 左右声道完全联动检波 |

---

## 八、版本调校与维护记录日志 (Changelog)

- **v1.0 (2026-09-03)**:
  1. 纠正前馈增益衰减斜率公式错位，彻底解决大动态被压灭静音的严重故障；
  2. 启动时间 Attack 标定为 `1 档 1600μs ~ 7 档 20μs`，兼顾鼓组穿透与人声瞬态；
  3. 释放时间 Release 标定为 `1 档 1200ms ~ 7 档 45ms`，默认释放时间设为 `4.0`（约 232ms）；
  4. 彻底移除人工伪多阶段干预调速逻辑，回归纯物理一阶 RC 放电；
  5. 重新校准输入阈值灵敏度至 -20dBFS，实现“开箱即用 4dB”工业级标准体验。
