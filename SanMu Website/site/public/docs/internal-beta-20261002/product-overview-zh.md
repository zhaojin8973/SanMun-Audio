# SanMu Audio 内测产品说明

## 内测范围

SanMu Audio 当前内测包含四款音频插件。所有内测版本有效至北京时间 2026 年 10 月 31 日 23:59；到期后插件保留工程参数，但停止效果处理并输出原声。

| 产品 | 版本 | 定位 | macOS 格式 | Windows 计划格式 |
|---|---:|---|---|---|
| SanMu EQ6 | 0.1.5 | 最多 24 个频段槽位的动态参量均衡与线性相位处理 | VST3 / AU / AAX | VST3 / AAX |
| SanMu Comp | 0.1.17 | LEV、VCA、FET、Opto 四模式压缩 | VST3 / AU / AAX | VST3 / AAX |
| SanMu Delay | 0.1.5 | 多引擎、多路由延迟 | VST3 / AU / AAX | VST3 / AAX |
| SanMu Reverb | 1.0.4 | Plate、Room、Hall、Chamber 四类混响 | VST3 / AU / AAX | VST3 / AAX |

## 产品简介

### SanMu EQ6

SanMu EQ6 是面向混音和母带工作的参量均衡器。它提供最多 24 个频段槽位、Zero Latency 与 Linear Phase 两种处理路径、实时频谱、峰值保持、频段独听和 Mid/Side 工作方式。Linear Phase 适合母带及相位敏感素材；Zero Latency 适合实时录音和混音。

### SanMu Comp

SanMu Comp 提供 LEV、VCA、FET、Opto 四种压缩模式。LEV 适合平准和动态控制，VCA 适合总线胶合，FET 适合快速抓取瞬态，Opto 适合较平滑的光电式动态。界面同时显示输入、输出、增益衰减和监听测算结果。

### SanMu Delay

SanMu Delay 提供 Modern、Tape、BBD、Diffused 四种延迟引擎，以及 Independent、Ping-Pong、Cross 三种路由。延迟时间支持 Sync、BPM 和 Free 模式，并提供 Ducking、反馈、调制和 Echo 时间编辑。

### SanMu Reverb

SanMu Reverb 提供 Plate、Room、Hall、Chamber 四类空间。控制范围覆盖 Predelay、Decay、Size、Attack、早晚扩散、低高频阻尼、调制和输出 EQ。声场界面提供干声、湿声和混合视图；浅色主题使用水波表现湿声，深色主题使用粒子/尘埃表现声场。

## 支持范围

### macOS

- Intel：最低 macOS 10.15 Catalina。
- Apple 芯片：最低 macOS 11.0；推荐当前受支持的 macOS。
- Pro Tools 使用 AAX；Logic Pro 使用 AU；其他宿主可使用 VST3。
- 内测安装包为 Apple Developer ID 签名的 Universal 2 包。

### Windows

- 目标：Windows 10 22H2 和 Windows 11 x64。
- VST3 可用于 Cubase、REAPER、Studio One 等支持 VST3 的宿主。
- AAX 仅用于 Windows Pro Tools。
- Windows 不提供 Audio Unit 格式。
- Windows 构建、签名、宿主扫描和实机回归完成前，官网不开放 Windows 下载按钮。
