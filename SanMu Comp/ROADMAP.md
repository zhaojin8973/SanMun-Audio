# SanMu EQ6 优化路线图

> 记录从架构评估中确定的优化项，按优先级逐个完成，每完成一项勾选一项。
> 每项都独立提交、独立验证（编译 + REAPER 实机听感），避免一次性大改引入回归。

## Phase 1：音频线程无锁化（最高优先，真风险）

- [x] 提取“参数 → 系数”纯函数，作为音频线程与 UI 线程共用的单一数据源
- [x] 音频线程 Linear Phase FIR 设计改为无锁读取目标系数
- [x] UI 频响求值改为本地无锁计算，不再读取 DSP 内部系数对象
- [x] 移除 EqBand 的 coeffMutex 与 activeCoefficients
- [x] 验证：频响曲线、拖拽、低切共振与改动前一致，无爆音

## Phase 2：音质升级（逐项独立做、独立听验）

- [x] 参数域平滑（替代系数域线性插值）
- [x] SVF / TPT 双精度滤波器拓扑
- [ ] Massberg 模拟去卷曲（SVF 已改善高频卷曲，此项收益降低，暂缓）

## Phase 3：混音功能补全

- [ ] 临时 Solo（Option/Alt + 悬停/拖动）
- [ ] 频谱 Tilt + Pre/Post 双频谱
- [ ] M/S 处理 + Auto Gain
- [ ] 线性相位多档延迟 + 后台重建
- [ ] 动态段数（需设计参数向下兼容）

## Phase 4：AAX 格式（Pro Tools）

- [ ] 申请 Avid AAX SDK 与 PACE/iLok 授权
- [ ] AAX 编译目标与授权集成

## Phase 5：打包分发

- [ ] Universal Binary（arm64 + x86_64）
- [ ] Apple 签名 + 公证
- [ ] .pkg 安装器

## Phase 6：套件化

- [ ] 抽取公共 UI / DSP 库到 common/
- [ ] 新建 SanMu Comp / Delay / Reverb 独立工程
