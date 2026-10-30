# SanMu Audio Plugin Suite · 三木音频开源插件套件

<p align="center">
  <strong>现代 C++20 DSP × JUCE 9 × 原生 WebGL / Canvas 视觉交互的专业混音插件套件</strong><br>
  <em>A Modern Open-Source Audio Plugin Suite Built with C++20 DSP, JUCE 9 & Native WebView/WebGL UI</em>
</p>

<p align="center">
  <a href="./LICENSE"><img src="https://img.shields.io/badge/License-AGPLv3-blue.svg" alt="License: AGPLv3"></a>
  <img src="https://img.shields.io/badge/Platform-macOS%20%7C%20Windows-lightgrey.svg" alt="Platform: macOS | Windows">
  <img src="https://img.shields.io/badge/Formats-VST3%20%7C%20AU%20%7C%20AAX-teal.svg" alt="Formats: VST3 | AU | AAX">
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C.svg" alt="C++20">
</p>

---

## ✨ 项目简介 / Overview

**SanMu Audio（三木音频）** 是一套面向现代音乐制作人、贴唱/器乐混音师与音频开发者的专业效果器插件合集。全系采用 **C++20 高精度实时音频 DSP** 与 **零延迟内嵌 WebView / WebGL 视觉界面** 架构，兼顾录音室级低失真声音品质与直观的实时视觉反馈。

本仓库包含全部 **4 款核心效果器插件**、跨平台 WebKit/WebView2 兼容打包层以及官方网站源码：

| 插件 / Plugin | 版本 | 核心特性 / Key Features |
| :--- | :---: | :--- |
| **[SanMu EQ6](./SanMu%20EQ)** | `v0.1.5` | 6 段参量/动态均衡器 · 零延迟 / 自然相位 / **线性相位 (Linear Phase)** 模式 · 双通道实时 FFT 频谱与自动选峰 · 频段独听 (Solo) 与动态阈值自动跟随 |
| **[SanMu Comp](./SanMu%20Comp)** | `v0.1.17` | 五引擎多风格压缩器（**Clean / Vox / VCA / FET / Opto**）· 极简 Vocal 单旋钮模式与 Pro 专业模式无缝切换 · 实时增益衰减轨迹与声学特征辅助设置 |
| **[SanMu Delay](./SanMu%20Delay)** | `v0.1.5` | 四引擎立体声延迟效果器（**Modern / Tape 磁带抖晃与饱和 / BBD 模拟暖色 / Diffused 空间扩散**）· 宿主 BPM 同步 / 附点三连音双轨可视化时间轴 · 内置避让 (Ducking) 与冻结 (Freeze) |
| **[SanMu Reverb](./SanMu%20Reverb)** | `v1.0.4` | 四引擎算法混响（**Plate / Room / Hall / Chamber**）· 基于 **WebGL 粒子生命系统与动态水波折光** 的实时声场可视化 · 独立预延迟、阻尼与立体声宽度控制 |

---

## 📦 获取方式：一键安装包 vs. 源码自编译

### 1. 音乐人 / 混音师（推荐：官方签名公证一键安装包）
绝大多数音乐制作人无需自行配置 C++ 编译环境。我们提供已完成 **Apple Developer ID 签名、Apple 公证（Notarization）与 PACE AAX 数字签名** 的双平台一键安装包（支持 Logic Pro、Pro Tools、Cubase、Studio One、Ableton Live、REAPER 等主流宿主），开箱即用、无系统拦截弹窗：
- **官方网站与预设库下载**：通过 **SanMu Audio 官方网站** 获取最新已签名安装包（`.pkg` / `.exe`）与官方混音实战预设包（Preset Pack）。

### 2. 开发者（免费克隆源码自行编译）
全套 C++ DSP 与前端界面源码遵循 [AGPLv3](./LICENSE) 开源协议，你可以直接克隆仓库并在本机编译 `VST3` 与 `AU` 格式。

---

## 🛠️ 从源码构建 / Building from Source

### 环境要求 / Prerequisites
- **CMake** `>= 3.25`
- **Node.js** `>= 18`（用于将插件前端 UI 离线内联打包进 C++ BinaryData）
- **macOS**：Xcode 15+ / AppleClang（支持 `arm64 + x86_64` Universal 2 构建，最低兼容 macOS 10.15 Catalina）
- **Windows**：Visual Studio 2022（Desktop development with C++）+ Microsoft Edge WebView2 Runtime

> **关于 AAX 格式说明**：由于 Avid AAX SDK 受商业 NDA 协议限制，公开仓库不包含 `aax-sdk`。CMake 会自动检测本机是否存在 AAX SDK：若未检测到，将自动构建 **VST3 + AU** 格式，不会中断编译；若你拥有 Avid 开发者授权，可通过 `-DSANMU_AAX_SDK_PATH=/path/to/aax-sdk-2-9-0` 开启 AAX 构建。

### macOS 快速构建

```bash
# 1. 克隆仓库并拉取 JUCE 子模块
git clone --recurse-submodules https://github.com/zhaojin8973/SanMu-Audio.git
cd SanMu-Audio

# 2. 安装前端离线打包工具依赖
(cd packaging/web && npm ci)
(cd "SanMu Reverb/prototypes/particle-life" && npm ci --ignore-scripts)

# 3. 编译任意插件（以 SanMu EQ6 为例，同理可替换为 "SanMu Comp" / "SanMu Delay" / "SanMu Reverb"）
cmake -S "SanMu EQ" -B build/eq -DCMAKE_BUILD_TYPE=Release
cmake --build build/eq --config Release -j8
```

编译产物位于 `build/<plugin>/source/*_artefacts/Release/` 下的 `VST3/` 与 `AU/` 目录，直接复制到 `~/Library/Audio/Plug-Ins/VST3` 或 `~/Library/Audio/Plug-Ins/Components` 即可在宿主中扫描使用。

### Windows 快速构建

在 **Visual Studio 2022 Developer PowerShell** 中执行：

```powershell
git submodule update --init --recursive
./packaging/windows/build_sanmu_windows.ps1 -Products All
```

---

## 🗂️ 仓库目录结构 / Repository Structure

- [`SanMu EQ/`](./SanMu%20EQ) — SanMu EQ6 参量/动态/线性相位均衡器源码（`source/dsp/` C++ 算法 + `ui/` 前端视图）
- [`SanMu Comp/`](./SanMu%20Comp) — SanMu Comp 五模式动态压缩器源码（含 `Clean`/`Vox`/`VCA`/`FET`/`Opto` 引擎）
- [`SanMu Delay/`](./SanMu%20Delay) — SanMu Delay 四模式立体声延迟器源码（含 `Modern`/`Tape`/`BBD`/`Diffused` 引擎）
- [`SanMu Reverb/`](./SanMu%20Reverb) — SanMu Reverb 四模式算法混响与 WebGL 粒子声场界面源码
- [`packaging/web/`](./packaging/web) — 跨平台 WebView 离线内联打包器与 Safari 13 / WebGL1 降级兼容层
- [`packaging/windows/`](./packaging/windows) — Windows x64 VST3 自动化构建脚本
- [`SanMu Website/`](./SanMu%20Website) — SanMu Audio 官方网站源码（Astro + Preact + Supabase）

---

## 📄 开源协议 / License

本项目基于 **[GNU Affero General Public License v3.0 (AGPLv3)](./LICENSE)** 开源。
