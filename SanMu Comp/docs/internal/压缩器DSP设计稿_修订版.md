# SanMu Comp 压缩器 DSP 设计稿（修订版）

> **文档状态**：正式修订版（DSP 研发与算法实现技术依据）  
> **面向对象**：DSP 算法工程师、音频插件开发者、音频架构师  
> **适用版本**：SanMu Comp v1.0.0+  
> **核心目标**：统一四大模式（Clean 纯数字、VCA 总线、FET 1176、Opto LA-2A）的物理建模拓扑、彻底消除逻辑矛盾与算法缺陷，提供完备的数学差分方程、增益架构与参数映射体系。

---

## 一、 核心架构与四大模式总览

SanMu Comp 是一款融合“现代手术刀级数字精度”与“经典模拟硬件物理建模”的高品质立体声压缩器。为了彻底避免“只换皮肤、算法同质化”的行业通病，四大模式在 **检测拓扑（Topology）**、**时间常数（Ballistics）**、**增益元件特性（Gain Element）** 及 **谐波染色（Saturation）** 四个维度均采用完全独立的物理建模分支。

### 1.1 四大模式核心对比矩阵

| 模式 | 检测拓扑 | 增益衰减元件 | 压缩比率来源 | 启动/释放时间常数 | 谐波染色与非线性 | 典型应用场景与听感 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Clean** | **前馈 (Feedforward)** | 理想线性数字增益 | 用户连续设定 (1:1~20:1, ∞) | 用户连续设定 (0.01~300ms / 1~2000ms) | **无失真 (THD < 0.0001%)**，绝对透明 | 手术级动态控制、母带精修、现代流行人声 |
| **VCA** | **前馈 (Feedforward)** 对数 RMS/Peak 混合 | 模拟 VCA 仿真 (THAT 2180/dbx) | 用户设定 + dbx OverEasy 渐进软拐 | 经典步进/连续 + **Auto 自适应释放** | **微量 3 次谐波** (THD 0.01%~0.08%) | 混音总线 Glue、鼓组总线、紧凑且有力的低频 |
| **FET** | **反馈 (Feedback)** 单向二极管峰值检波 | JFET 压控电阻 (VCR) 分压网络 | 硬件按钮档位 (4/8/12/20/All) | **极速对数 (20~800μs) + 三态程序依赖释放** | **丰富 2/3 次谐波** (Input 驱动软饱和) | 军鼓、打击乐 Punch、摇滚人声、高能抽吸 |
| **Opto** | **反馈 (Feedback)** 光电积分检波 | T4B (EL 发光板 + CdS 光敏电阻) | 电平自然涌现 (~3:1 Compress / ~10:1 Limit) | **10ms 物理启动 + 双阶段释放 + 记忆阻尼** | **温和电子管/变压器染色** (THD 0.1%~0.3%) | 流行/民谣人声、电贝斯平滑整平、自然顺滑 |

---

## 二、 关键架构决议与纠偏说明（针对初稿的 4 处核心修正）

### 2.1 修正 1：LA-2A 的 Gain 严格定位于“反馈环外的输出纯补偿”
*   **初稿冲突**：正文称 Gain 不参与压缩，附录 B 却称 Gain 在反馈环内会加深压缩。
*   **最终决议**：
    1. 侧链反馈采样点严格置于 **T4 光电衰减之后、Gain 补偿之前**。
    2. Peak Reduction 负责侧链驱动增益（决定压缩深度与涌现比率）；Gain 负责输出 Makeup 电平补偿。
    3. **物理与实用性理由**：如果在数字单采样延迟反馈环中将 Gain 置于环内，会导致用户为了补偿 6dB 响度而推大 Gain 时，压缩器被同步推得更狠，响度越补越小，严重破坏混音手感。主流复刻（UAD LA-2A、Waves CLA-2A）均采用此解耦架构。

### 2.2 修正 2：1176 比率与物理阈值的关系
*   **初稿冲突**：正文称“更高比率对应更高阈值”，附录称“更高比率阈值略低”。
*   **最终决议**：
    1. 硬件 1176 的 Q-bias 偏置网络决定了在 20:1 与 All-Button 模式下，电路偏置使有效阈值更加敏感（相对 dBFS 阈值略微降低约 1~2.5dB），以便于快速捕获峰值并提供激进限幅。
    2. 本设计规定：内部基准阈值为 $-18.0\text{ dBFS}$，比率偏移量为：
       *   4:1：$-17.5\text{ dBFS}$（阈值最高，作用温和）
       *   8:1：$-18.5\text{ dBFS}$
       *   12:1：$-19.2\text{ dBFS}$
       *   20:1 / All-Button：$-20.0\text{ dBFS}$（阈值最低，最易起压）

### 2.3 修正 3：1176 增益结构归一化（防止数字爆音）
*   **初稿问题**：附录 B 提出 1176 采用硬件 45dB 裸增益，若直接套用，推大 Input 会造成数字域 +45dBFS 严重削波。
*   **最终决议**：
    1. 在内部 DSP 采用**归一化工作点**：以 Input=50%（对应硬件 24 刻度）、Output=0dB 为标准 Unity Gain（$0\text{ dB}$ 净增益）。
    2. Input 旋钮（0~100%）在内部映射为 $[-18\text{dB}, +18\text{dB}]$ 的驱动增益（同等幅度将信号推向固定阈值并增加失真）；
    3. Output 旋钮提供 $[-24\text{dB}, +24\text{dB}]$ 的线性 Makeup 补偿，既保留 1176 成对调节的经典手感，又保证 DAW 内部安全。

### 2.4 修正 4：侧链高通滤波（Sidechain HP）时序重构
*   **初稿问题**：代码中将高通滤波器接在整流绝对值 `std::fabs()` 之后，滤除了直流基线，导致包络过零点判定为静音。
*   **最终决议**：侧链高通滤波（一阶或二阶 IIR）必须在**交流音频信号输入侧链的第一时间处理**，滤波完成后再送入整流器与包络检波模块。

---

## 三、 四大模式 DSP 深度建模与算法规范

```
                          ┌────────────────────────────────────────────────────────┐
                          │                   SanMu Comp 信号处理流                │
                          └────────────────────────────────────────────────────────┘

    输入音频 x[n] ─────────────────────────────────────────────────────────────────────────────┐
          │                                                                                    │
          │ (侧链开启)                                                                         │
    ┌─────▼─────────────────────────┐                                                          │
    │ 侧链高通滤波 (Sidechain HPF)   │                                                          │
    └─────┬─────────────────────────┘                                                          │
          │                                                                                    │
          ├─────────────────────────┬────────────────────────────┬────────────────────────┐    │
          │                         │                            │                        │    │
          ▼                         ▼                            ▼                        ▼    │
    【Clean 模式】             【VCA 模式】                 【FET 模式】             【Opto 模式】│
    前馈 Peak/RMS 检波        前馈 对数 RMS 检波           反馈 压缩后前级采样       反馈 T4后采样│
          │                         │                            │                        │    │
    连续 Knee 增益计算         OverEasy 渐进软拐            FET 压控电阻曲线         T4 光电模型  │
          │                         │                            │                        │    │
    一阶球状平滑               Auto / 用户释放平滑          三态程序依赖释放         双阶段释放+记忆│
          │                         │                            │                        │    │
          │                         │ (微量 VCA 染色)            │ (Input 饱和驱动)       │    │
          ▼                         ▼                            ▼                        ▼    │
     增益衰减 g_clean           增益衰减 g_vca              增益衰减 g_fet            增益衰减 g_opto  │
          │                         │                            │                        │    │
          └─────────────────────────┴─────────────┬──────────────┴────────────────────────┘    │
                                                  ▼                                            │
                                         应用增益衰减 c[n] = x[n] * g[n] ◄─────────────────────┘
                                                  │
                                                  ▼
                                       立体声联动 (Stereo Link)
                                                  │
                                                  ▼
                                        输出 Makeup 补偿增益
                                                  │
                                                  ▼
                                      干湿比混合 (Mix Wet/Dry)
                                                  │
                                                  ▼
                                              输出 y[n]
```

---

### 3.1 模式一：Clean（纯数字透明压缩器）

#### 3.1.1 核心特性
*   **拓扑**：前馈式（Feedforward），侧链直接采样输入音频。
*   **检波机制**：支持解耦的瞬时峰值与一阶平滑混合检波。
*   **静态压缩曲线（带连续软拐点 Knee）**：
    设检测输入电平为 $L_{\text{in}} = 20 \log_{10}(\text{det}) \text{ (dBFS)}$，阈值为 $T\text{ (dBFS)}$，压缩比为 $R$，拐点宽度为 $W\text{ (dB)}$：
    
    $$GR(L_{\text{in}}) = \begin{cases} 
      0, & L_{\text{in}} \le T - \frac{W}{2} \\
      \frac{\left(1 - \frac{1}{R}\right) \cdot \left(L_{\text{in}} - T + \frac{W}{2}\right)^2}{2W}, & T - \frac{W}{2} < L_{\text{in}} \le T + \frac{W}{2} \\
      \left(1 - \frac{1}{R}\right) \cdot (L_{\text{in}} - T), & L_{\text{in}} > T + \frac{W}{2}
    \end{cases}$$

#### 3.1.2 时间常数平滑（Ballistics）
在 dB 域对目标衰减量 $GR[n]$ 进行平滑滤波（避免增益调制在过零点产生非线性失真）：
$$\alpha_{\text{att}} = 1 - e^{-\frac{1}{\tau_{\text{att}} \cdot f_s}}, \quad \alpha_{\text{rel}} = 1 - e^{-\frac{1}{\tau_{\text{rel}} \cdot f_s}}$$
$$g_{\text{db}}[n] = \begin{cases}
  g_{\text{db}}[n-1] + \alpha_{\text{att}} \cdot (GR[n] - g_{\text{db}}[n-1]), & GR[n] > g_{\text{db}}[n-1] \text{ (压缩加深)} \\
  g_{\text{db}}[n-1] + \alpha_{\text{rel}} \cdot (GR[n] - g_{\text{db}}[n-1]), & GR[n] \le g_{\text{db}}[n-1] \text{ (释放恢复)}
\end{cases}$$
最终线性增益：$g_{\text{lin}}[n] = 10^{-\frac{g_{\text{db}}[n]}{20}}$。

---

### 3.2 模式二：VCA（经典总线 Glue 压缩器，对标 SSL 4000G / dbx 160）

#### 3.2.1 核心特性
*   **拓扑**：前馈式（Feedforward）。
*   **检波机制**：真有效值（True RMS）对数检波，具有宽平的低频响应与自然的能量响应。
    $$x_{\text{rms}}^2[n] = (1 - \alpha_{\text{rms}}) x_{\text{rms}}^2[n-1] + \alpha_{\text{rms}} x^2[n], \quad \tau_{\text{rms}} \approx 10\text{ms}$$
*   **OverEasy 渐进软拐点**：固定 $4\text{dB}$ 优雅软过渡，在大压缩量下形成典型的“声音粘合感（Glue）”。
*   **Auto Release（经典 SSL 自适应释放）**：
    当用户选择 Auto 释放时，释放时间不再是常数，而是根据信号动态比率 $\frac{\text{Peak}}{\text{RMS}}$ 动态自适应：
    *   瞬态冲击（短时脉冲）：$\tau_{\text{rel}} \approx 100\text{ms}$（迅速恢复，不吃鼓点冲击）；
    *   持续重压（平稳垫乐）：$\tau_{\text{rel}} \approx 1.2\text{s}$（平稳过渡，防止总线低频抽吸/泵浦）。
*   **微量 VCA 染色（THD 0.03%）**：
    模拟 VCA 芯片的非理想对数转换非线性，在深压缩时引入微量的 3 次谐波：
    $$y[n] = x[n] \cdot g_{\text{lin}}[n] \cdot \left(1.0 - 0.03 \cdot (1.0 - g_{\text{lin}}[n]) \cdot x^2[n]\right)$$

---

### 3.3 模式三：FET（1176LN 经典压缩器，对标 Rev E Blackface）

#### 3.3.1 核心特性与信号流
*   **拓扑**：严格**反馈式（Feedback）**。
*   **检测取样点**：取自 JFET 衰减之后、前级放大之后，**严格置于 Output Makeup 旋钮之前**。
*   **增益元件**：JFET 漏源电阻分压电路，衰减上限约为 $-30.0\text{ dB}$（FET 导通电阻无法降至 0）。

#### 3.3.2 单向二极管峰值检波器（Diode Peak Detector）
为杜绝音频过零点导致的包络跳变破音，侧链检波必须为带单向二极管导通特性的峰值检测器：
1. 采样反馈信号：$u[n] = x[n] \cdot g_{\text{fet}}[n-1]$（经过前级放大与 Input 驱动）。
2. 全波整流：$v_{\text{rect}}[n] = |u[n]|$。
3. 对数电平转换：$L_{\text{det}}[n] = 20 \log_{10}(\max(v_{\text{rect}}[n], 10^{-6}))$。
4. **单向充电包络平滑**：
   $$\text{env}[n] = \begin{cases}
     \text{env}[n-1] + \alpha_{\text{att}} \cdot (L_{\text{det}}[n] - \text{env}[n-1]), & L_{\text{det}}[n] > \text{env}[n-1] \\
     \text{env}[n-1] + \alpha_{\text{prog\_rel}} \cdot (L_{\text{det}}[n] - \text{env}[n-1]), & L_{\text{det}}[n] \le \text{env}[n-1]
   \end{cases}$$

#### 3.3.3 比率与偏置矩阵
| Ratio 档位 | 标称比率 $R$ | 内部有效阈值 $T_{\text{eff}}$ | 静态曲线斜率系数 $S$ | 谐波染色系数 $K_{\text{dist}}$ |
| :--- | :--- | :--- | :--- | :--- |
| **4:1** | 4.0 | $-17.5\text{ dBFS}$ | 0.75 | 1.0 (标准) |
| **8:1** | 8.0 | $-18.5\text{ dBFS}$ | 0.875 | 1.1 |
| **12:1** | 12.0 | $-19.2\text{ dBFS}$ | 0.917 | 1.2 |
| **20:1** | 20.0 | $-20.0\text{ dBFS}$ | 0.950 | 1.3 |
| **All-Button (全按)** | ~16.0 (激进弯曲) | $-22.5\text{ dBFS}$ | 1.15 (超调压缩) | 2.5 (重度染色 + 瞬态延时破音) |

#### 3.3.4 三态程序依赖释放（Program-Dependent Release）
1176 标志性的“鼓组弹跳感”来源于其释放时间的程序依赖性：
*   **状态变量**：持续处于高压缩状态的累积时间 $T_{\text{comp}}$（秒）。
*   若 $\text{env}[n] > T_{\text{eff}} + 3\text{dB}$，则 $T_{\text{comp}} = \min(T_{\text{comp}} + \Delta t, 2.0\text{s})$；
*   若 $\text{env}[n] \le T_{\text{eff}}$，则 $T_{\text{comp}} = \max(T_{\text{comp}} - 1.5 \Delta t, 0.0\text{s})$；
*   当前有效释放时间常数在快慢之间动态插值：
    $$w = \text{clamp}\left(\frac{T_{\text{comp}}}{0.5\text{s}}, 0.0, 1.0\right)$$
    $$\tau_{\text{prog\_rel}} = \tau_{\text{user\_rel}} \cdot (1.0 - w) + \max(\tau_{\text{user\_rel}} \cdot 3.5, 1.1\text{s}) \cdot w$$

#### 3.3.5 Input 驱动与 FET 软饱和
输入信号先经过 Input 驱动增益及平滑软饱和函数（限制方波削波，保护高频）：
$$x_{\text{driven}}[n] = \text{HeadroomLin} \cdot \tanh\left(\frac{x[n] \cdot \text{DriveGain}}{\text{HeadroomLin}}\right)$$

---

### 3.4 模式四：Opto（LA-2A 经典光学压缩器，对标 T4B 单元）

#### 3.4.1 核心特性与信号流
*   **拓扑**：严格**反馈式（Feedback）**。
*   **采样点**：取自 T4 光电衰减后、Gain 放大前。
*   **控制旋钮**：仅 Peak Reduction（侧链驱动）与 Gain（输出纯补偿），无用户 Attack/Release 旋钮。

#### 3.4.2 T4B 光电物理单元（EL 面板 + CdS 光敏电阻）数学模型
1. **EL 发光面板起压阈值**：
   EL 面板存在约 100V 的物理点火电压，低于阈值（数字域等效 $-32.0\text{ dBFS}$）不发光：
   $$V_{\text{sc}}[n] = \text{detLevel}[n] + (\text{PeakReduction} \cdot 0.4\text{dB})$$
   $$E_{\text{target}}[n] = \max\left(0.0, V_{\text{sc}}[n] - (-32.0\text{dBFS})\right) \cdot \text{Slope}$$

2. **双阶段释放（修复初稿 Attack 缺陷的正确实现）**：
   在 **Attack（发光加深）阶段，两组载流子均在 10ms 内全速激发**；在 **Release（光照熄灭）阶段，呈现两阶段复合衰减**：
   *   快变分量 $E_{\text{fast}}$：$\tau_{\text{att}} = 10\text{ms}, \tau_{\text{fast\_rel}} = 60\text{ms}$；
   *   慢变分量 $E_{\text{slow}}$：$\tau_{\text{att}} = 10\text{ms}, \tau_{\text{slow\_rel}} = 1.2\text{s} \sim 5.0\text{s}$（随历史记忆增长）。
   
   $$\alpha_{\text{fast}} = \begin{cases} 1 - e^{-\frac{\Delta t}{0.010}}, & E_{\text{target}} > E_{\text{fast}} \text{ (快速启动)} \\ 1 - e^{-\frac{\Delta t}{0.060}}, & E_{\text{target}} \le E_{\text{fast}} \text{ (快速恢复前半)} \end{cases}$$
   $$\alpha_{\text{slow}} = \begin{cases} 1 - e^{-\frac{\Delta t}{0.010}}, & E_{\text{target}} > E_{\text{slow}} \text{ (快速同步充电)} \\ 1 - e^{-\frac{\Delta t}{\tau_{\text{mem\_slow}}}}, & E_{\text{target}} \le E_{\text{slow}} \text{ (长尾慢速恢复)} \end{cases}$$
   
   总有效光强：$E_{\text{total}}[n] = 0.5 \cdot E_{\text{fast}}[n] + 0.5 \cdot E_{\text{slow}}[n]$。

3. **CdS 历史光照记忆效应（Memory Effect）**：
   持续高光照使得半导体深能级陷阱被大量捕获，释放长尾可长达数秒：
   $$M_{\text{mem}}[n] = \text{clamp}\left(M_{\text{mem}}[n-1] + (E_{\text{total}}[n] \cdot 0.25 - 0.05) \cdot \Delta t, 0.0, 1.0\right)$$
   $$\tau_{\text{mem\_slow}} = 1.2\text{s} + 3.8\text{s} \cdot M_{\text{mem}}[n]$$

4. **光敏电阻分压计算**：
   $$R_{\text{photo}}[n] = R_{\text{dark}} \cdot 10^{-\gamma \cdot E_{\text{total}}[n]}, \quad (R_{\text{dark}} = 1\text{ M}\Omega, \gamma \approx 5.5)$$
   $$g_{\text{opto}}[n] = \frac{R_{\text{photo}}[n]}{R_{\text{photo}}[n] + R_{\text{fixed}}}$$
   *   **Compress 模式**：$R_{\text{fixed}} = 70.7\text{ k}\Omega \implies$ 渐进涌现比率约 $3:1$；
   *   **Limit 模式**：$R_{\text{fixed}} = 68.0\text{ k}\Omega \implies$ 极限压缩比率约 $10:1+$。

---

## 四、 全局辅助子系统规范

### 4.1 侧链高通滤波（Sidechain HPF）
*   **滤波器结构**：二阶 Linkwitz-Riley 或二阶 Butterworth 高通滤波（-12dB/oct）；
*   **截止频率范围**：$20\text{ Hz} \sim 1000\text{ Hz}$，默认 $80\text{ Hz}$；
*   **处理时序**：必须置于交流音频信号路径上，在计算整流、RMS 或绝对值**之前**完成滤波。

### 4.2 立体声联动（Stereo Link）
*   **范围**：$0\%\text{ (左右独立 Dual Mono)} \sim 100\%\text{ (完全联动 Linked)}$；
*   **数学实现**：
    在完成各声道的目标增益衰减 $GR_L\text{ (dB)}$ 与 $GR_R\text{ (dB)}$ 计算后：
    $$GR_{\text{linked}} = \max(GR_L, GR_R)$$
    $$GR_{L,\text{final}} = GR_L + (GR_{\text{linked}} - GR_L) \cdot \text{Link}_{\text{norm}}$$
    $$GR_{R,\text{final}} = GR_R + (GR_{\text{linked}} - GR_R) \cdot \text{Link}_{\text{norm}}$$
    保证立体声像（Stereo Imaging）在重压缩下稳定不发生左右晃动。

### 4.3 内部过采样与抗混叠（Oversampling & Anti-Aliasing）
*   **触发条件**：当开启 `OVERSAMPLE` 参数，或处于 FET 模式（包含极速 20μs 增益跳变与饱和）时，内部自动启用 **2x 线性相位 Polyphase 半带过采样**。
*   **延迟上报**：准确向 DAW 宿主上报过采样引入的群延迟（Latency Samples），并在旁通（Bypass）时保持延迟恒定以防宿主切歌爆音。

---

## 五、 参数体系与 APVTS 映射规范（向下兼容定义）

所有参数 ID 保持永久不变，严格遵守项目的向下兼容铁律：

| 参数 ID | 参数名称 | 模式归属 | 物理范围 | 默认值 | 刻度分布 / 单位 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `mode` | 压缩模式 | 通用 | 0:Clean, 1:VCA, 2:FET, 3:Opto | 1 (VCA) | Choice 档位选择 |
| `threshold` | 压缩阈值 | Clean / VCA | $-60.0 \sim 0.0\text{ dB}$ | $-18.0\text{ dB}$ | 线性（步进 0.1dB） |
| `ratio` | 压缩比率 | Clean / VCA | $1.0:1 \sim 20.0:1$ | $4.0:1$ | 对数偏斜（低比率精细） |
| `attack` | 启动时间 | Clean / VCA | $0.01 \sim 300.0\text{ ms}$ | $10.0\text{ ms}$ | 对数偏斜 |
| `release` | 释放时间 | Clean / VCA | $1.0 \sim 2000.0\text{ ms}$ | $150.0\text{ ms}$ | 对数偏斜 |
| `knee` | 软拐点宽度 | Clean | $0.0 \sim 24.0\text{ dB}$ | $6.0\text{ dB}$ | 线性 |
| `makeup` | 输出补偿 | Clean / VCA | $-24.0 \sim +24.0\text{ dB}$ | $0.0\text{ dB}$ | 线性 |
| `mix` | 干湿比 | 通用 | $0.0 \sim 100.0\%$ | $100.0\%$ | 线性百分比 |
| `headroom` | 内部基准电平 | FET | $4.0 \sim 28.0\text{ dB}$ | $16.0\text{ dB}$ | 步进 1dB（UAD 同源） |
| `fet_input` | FET 驱动量 | FET | $0.0 \sim 100.0$ | $50.0$ | 线性（映射 $\pm 18\text{dB}$ 驱动） |
| `fet_ratio` | FET 比率档位 | FET | 0:4:1, 1:8:1, 2:12:1, 3:20:1, 4:All | 0 (4:1) | Choice 档位选择 |
| `fet_attack` | FET 启动时间 | FET | $0.02 \sim 0.80\text{ ms}$ (20~800μs) | $0.20\text{ ms}$ | **反向对数**（顺时针变快） |
| `fet_release`| FET 释放时间 | FET | $50.0 \sim 1100.0\text{ ms}$ | $300.0\text{ ms}$ | **反向对数**（顺时针变快） |
| `fet_output` | FET 输出增益 | FET | $-24.0 \sim +24.0\text{ dB}$ | $0.0\text{ dB}$ | 线性 Makeup 补偿 |
| `opto_peak_reduction` | Opto 侧链驱动 | Opto | $0.0 \sim 100.0$ | $0.0$ | 线性（映射 0~40dB 驱动） |
| `opto_gain` | Opto 输出补偿 | Opto | $-24.0 \sim +24.0\text{ dB}$ | $0.0\text{ dB}$ | 线性 Makeup 补偿 |
| `sidechain_enabled` | 侧链高通开关 | 通用 | 关 / 开 (Bool) | false | 布尔开关 |
| `sidechain_hp` | 侧链高通频率 | 通用 | $20.0 \sim 1000.0\text{ Hz}$ | $80.0\text{ Hz}$ | 对数偏斜 |
| `stereo_link` | 立体声联动 | 通用 | $0.0 \sim 100.0\%$ | $100.0\%$ | 线性百分比 |
| `auto_gain` | 自动增益补偿 | 通用 | 关 / 开 (Bool) | false | 理论能量补偿 |
| `oversample` | 内部过采样 | 通用 | 关 / 开 (Bool) | true | 2x Polyphase IIR |
| `global_bypass` | 全局旁通 | 通用 | 关 / 开 (Bool) | false | 无缝淡入淡出旁通 |

---

## 六、 研发验收与测试验证基准（QA & Verification Plan）

### 6.1 单元测试（Automated DSP Tests）
1. **过零点平滑度测试（Low Frequency Distortion Check）**：
   输入 $50\text{ Hz}$ 正弦波在 FET 模式下重压 $12\text{dB}$，时域波形不得出现过零点高频锯齿毛刺，THD 符合预期。
2. **Opto 瞬态捕获测试（Transient Attack Verification）**：
   输入持续 $10\text{ms}$ 的全幅脉冲猝发音（Tone Burst），验证 Opto 模式能瞬间触发 $90\%$ 以上的目标压缩量，释放曲线呈双阶段指数衰减。
3. **Unity 增益校准测试（Gain Staging Unity Test）**：
   在 FET 模式（Input=50, Output=0dB）且无压缩时，输入 $1\text{kHz} @ -20\text{dBFS}$ 信号，输出电平误差在 $\pm 0.05\text{dB}$ 范围内。
4. **侧链高通独立性测试**：
   开启 Sidechain HP=200Hz 时，输入 50Hz 纯正弦波，压缩量应当完全归零（不受低频触发）。

### 6.2 听感与宿主实机验收（DAW Listening Tests）
*   **REAPER / Logic Pro 轨道实测**：
    *   **人声整平**：Opto 模式在 $3 \sim 6\text{dB}$ 压缩下听感自然温暖、无呼吸抽吸感；
    *   **军鼓 Punch**：FET 模式（Ratio 4:1, Attack 3 点钟最快, Release 5 点钟最快）呈现击打瞬态增强与边缘颗粒感；
    *   **全按模式（All-Button）**：FET 模式在 Ratio=All 时，室内鼓组声音瞬间变得爆炸、紧绷且充满复古过载感；
    *   **总线 Glue**：VCA 模式在 Ratio 2:1 或 4:1、Auto Release 下，对立体声混音总线提供透明且扎实的贴合感。
