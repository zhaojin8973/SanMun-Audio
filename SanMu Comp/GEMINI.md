# SanMu Comp - AI 助手核心工作铁律 (Core Work Guidelines)

> [!IMPORTANT]
> **每次对话必须最优先阅读并严格遵守以下铁律：**

---

### 🚨 铁律一：严禁擅自开工与修改代码 (Strict User Confirmation Rule)
1. **没有用户的明确指令与确认，绝对不许自行开工、修改代码、或改动任何参数**。
2. 每次完成分析、排查或提出方案后，必须**停下等待**用户的明确反馈与确认，严禁自动执行未经用户批准的操作。

---

### 🚨 铁律二：DSP 核心物理建模代码绝对冻结 (DSP Core Code Freezing)
1. **四大压缩器物理建模引擎代码（`FetEngine.*`、`OptoEngine.*`、`VcaEngine.*`、`CleanEngine.*`）已由团队精心调校完成，绝对禁止修改**。
2. **每个压缩引擎之间 100% 物理独立与代码隔离**：
   - **FET (1176)**：独立的反馈二极管峰值检波、UTC 变压器与 20µs 瞬态引擎（`FetEngine.cpp`）。
   - **Opto (LA-2A)**：独立的 T4B 光电管双阶段释放、CdS 记忆效应与 12AX7 电子管引擎（`OptoEngine.cpp`）。
   - **VCA (SSL 4000G)**：独立的 RMS 对数检波、OverEasy 软拐与 THAT 2180 胶水引擎（`VcaEngine.cpp`）。
   - **VOX (现代人声)**：独立的前置自适应双推子平准与下扩展门引擎（`VoxEngine.cpp`）。
   各引擎之间**无任何共用处理代码或交叉状态**。

---

### 🚨 铁律三：AI 模块边界与职能限定 (AI Module Boundary)
1. AI 模块仅限于：
   - 10 秒声学特征捕捉与分类（`AcousticFeatureExtractor`）。
   - 为原始 DSP 引擎计算并下发合理的推荐预设参数（`LlmClient`）。
2. 严禁为了适配 AI 而去反向修改底层 DSP 引擎的代码。
