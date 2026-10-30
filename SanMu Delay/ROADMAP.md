# SanMu Delay - 开发路线图 (ROADMAP)

## Phase 1: 核心 DSP 架构与骨架搭建 (当前阶段)
- [x] 需求调研、痛点定义与 4 大离散引擎拍板
- [x] 工程根目录与 CMake 体系初始化 (C++20, JUCE 9)
- [ ] 基础 DSP 原语库就绪 (`DelayLine`, `AllpassFilter`, `TptSvf`, `DuckingDetector`)
- [ ] APVTS 参数系统与单一数据源定稿 (`Parameters.h/.cpp`)
- [ ] 4 大独立引擎实现 (`Modern`, `Tape`, `BBD`, `Diffused`)
- [ ] 核心处理器调度 (`DelayProcessor`)：拓扑矩阵 (Stereo/PingPong/Cross)、平滑时间过渡、Freeze 与外部 Sidechain Ducking
- [ ] 宿主外壳与总线集成 (`PluginProcessor` 主+侧链总线配置、尾长上报)
- [ ] 自动化 DSP 单元测试验证 (`test_delay_dsp`)

## Phase 2: 交互与界面视觉打磨
- [ ] 依据 `02-UI设计规范.md` 打造单板米白玻璃拟物界面
- [ ] 浅色 (Light) 与深色 (Dark) 双主题无缝切换
- [ ] 实时回声网格 / 衰减时序可视化与 Ducking 动态指示
- [ ] 双击重置、微调手感与快捷预设管理

## Phase 3: 智能扩展与产品完善
- [ ] 预设库扩展（经典延迟声效、Vocal Throw、Ambient Wash）
- [ ] 宿主自动化稳定性与 CPU 极致优化
- [ ] 后续探索性智能 AI 辅助模块（待核心完备后讨论）
