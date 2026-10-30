# Linear Phase 与四模式实施交付记录

日期：2026-09-25。状态：AU / VST3 / AAX 已覆盖安装，可进入 DAW 试听；尚不是无条件正式发布结论。

## 已落地的声音与交互契约

| 模式 | 处理特点 | 当前数值证据 |
| --- | --- | --- |
| Clean | 共用最小相位均衡，不加入非线性染色 | −18 dBFS 测试音的二至八次谐波总失真约 0.0000012% |
| Tube | 带偏置软饱和，主要增加偶次谐波，随输入电平增加饱和 | −18 dBFS 时约 0.649% 总谐波失真；偶次主导断言通过 |
| Discrete | 对称且较轻的软饱和，主要增加奇次谐波 | −18 dBFS 时约 0.00639% 总谐波失真；奇次主导断言通过 |
| Linear Phase | 四路有符号对称 FIR，静态核具有共同群延迟；动态衰减进入同一矩阵 FIR | 1.5 kHz、6 dB 范围实测衰减约 5.9996 dB；固定延迟 5120 样本 |

Tube / Discrete 是当前明确可测的音色模型，不是 EQ825、GML8200 电路复刻。四个入口、字体、图标、旋钮、布局、连续炫彩频谱保持既有设计。

## 核心修复

- Linear Phase 支持 Stereo / Left / Right / Mid / Side 混合，按槽位顺序相乘，不再把 Mid 当 Stereo 或漏掉 Side。
- Bell、Low Shelf、High Shelf 支持自动与手动阈值、0–24 dB 向下动态。Low Cut / High Cut 继续禁用动态。没有混入最小相位动态滤波来冒充线性相位。
- 动态检测以输入增益后的 Pre-EQ 信号为源；最小相位模式保留既有逐频段检测源。两者不是相同触发架构。
- 使用固定 64 原生采样控制时钟，25 ms 重建节流，分批生成完整四路核，再整体发布；卷积帧边界避开核生成峰值。新旧核使用同一输入历史交叉淡化。
- 缓存原型功率响应，避免逐频点反复计算对数与幂；与原模拟原型覆盖增益、Q、频率的等价性测试通过。
- GR 读取已应用核的控制量并随音频输出 FIFO 推进；不再直接显示尚未应用的检测目标。它仍是频段控制量，不是多段叠加后每个频率的实测电平。
- 旁通和临时监听期间引擎持续处理，解除后不会重放旧 FIFO。修正模式切换、重复初始化的检测采样率和延迟上报竞争。
- 废除矩阵元素约 ±36 dB 的静默限制，允许正常多段大增益叠加；非有限输入仍防护。
- 前三模式废除“每个宿主块逼近 25%”的参数平滑，改为内部 128 样本时钟和 15 ms 平滑，Cut 附加级也跟随截止频率。
- 自动增益原来仅评估左路或 Mid；已改为两输入矩阵能量估计并缓存，左右／中侧对称。它是静态曲线估计，不能保证任意音乐都严格等响，也不抵消动态压缩。

## 状态与兼容

- 仍为 323 个自动化参数，ID、顺序和枚举不变。增加根属性 `linearPhaseVersion` 与 `linearPhaseLegacyMask`，不占自动化参数。
- 无版本标记的旧工程和旧用户预设，Linear Phase 动态保持暂停，原动态参数完整保留；切回其它三个模式仍沿用原参数。
- 在 Linear Phase 中点击频段动态开关明确启用后，逐段清除兼容标记，支持保存、重开、撤销、重做。仅改变旧工程中的宿主自动化数值不会隐式清除标记。
- 新实例和工厂预设直接使用新能力。修正过的混合通道路由、自动增益和自动化平滑可能改变旧工程相应结果；不声称错误旧路径逐样本兼容。

## 验证结果

- 原生数值回归全部通过：矩阵路由、负号、核对称、未完成核不混用、动态检测和应用衰减、旧状态兼容、重开与撤销、三模式音色谐波。
- 动态 Bell / 两类 Shelf、自动与手动阈值覆盖 44.1 / 48 / 96 / 192 kHz，声道位置覆盖 ST / L / R / M / S。
- 固定参数事件的 Clean / Tube / Discrete 自动化和 Linear Phase 突发动态在 64 / 257 / 1024 样本宿主块下逐样本最大差值为 0。
- 自动补偿修复前，测试的 L/R、M/S 互换结果相差约 8 dB；修复后 L/R 相同，M/S 差异小于 0.01 dB。
- 网页第五轮回归、稳定选峰、频谱平滑、原生桥接及内嵌网页验证通过。原系列 UI 未重设计。
- AU `auval -v aufx SMQ6 SmAu` 通过；三种格式严格代码签名校验通过；本轮 AAX 的 PACE 签名和已安装包签名验证通过。

## 性能与尚未解决的发布边界

当前采用音频线程内的预分配、分批矩阵 FIR 参考实现，没有完成原方案提出的后台不可变缓存及队列路径；不要把该优化路径标记为已完成。下列为本机 Release 离线驱动回调测量，不等于宿主实时调度保证。

| 配置 | 最慢回调 / 预算 | 结论 |
| --- | --- | --- |
| 48 kHz / 64，单实例 24 段 | 0.286 / 1.333 ms | 本次通过 |
| 192 kHz / 64，单实例 24 段 | 0.284 / 0.333 ms | 本次通过，余量较小 |
| 48 kHz / 64，四实例各 24 段串行 | 1.121 / 1.333 ms | 本次通过 |
| 96 kHz / 64，四实例串行 | 1.113 / 0.667 ms | 超出预算 |
| 96 kHz / 256，四实例串行 | 2.217 / 2.667 ms | 本次通过 |
| 192 kHz / 256，四实例串行 | 2.190 / 1.333 ms | 超出预算 |

固定 8193 抽头仍存在低频窄带精度限制，不能声称 20 Hz–20 kHz、Q=0.1–40 全范围等精度。目标 +12 dB 的示例：

| 采样率 / 中心频率 / Q | 实测中心增益 |
| --- | --- |
| 48 kHz / 20 Hz / Q=1 | +9.959 dB |
| 48 kHz / 20 Hz / Q=4 | +6.000 dB |
| 192 kHz / 20 Hz / Q=40 | +0.028 dB |
| 192 kHz / 100 Hz / Q=4 | +6.436 dB |
| 192 kHz / 1.5 kHz / Q=4 | +11.822 dB |

这些不是已经解决的问题。要将 Linear Phase 定义为全范围高精度模式，需继续设计更长核或多分辨率引擎、相应延迟／质量契约，并重新核验动态响应与实时负载。当前界面频响为设计目标，并不是长核截断后每个频点的测量曲线。

尚未完成真实 DAW 的音乐试听、Pro Tools 扫描加载、工程重开与导出验证；签名通过不能代替这些项目。动态过渡的预振铃、极端快速扫动听感、内存分配钩子审计、跨模式瞬态也未被本次数值结论覆盖。外部侧链频谱与跨实例冲突检测仍未实现。

## 安装与回退

已安装以下本轮产物，主程序哈希与构建一致：

- `/Users/zhaojin/Library/Audio/Plug-Ins/Components/SanMu EQ6.component`
  - SHA-256：`b421a33ee05812fb3f6bd3ae5e0f226054df12febe30231a1c2902d5d100249f`
- `/Users/zhaojin/Library/Audio/Plug-Ins/VST3/SanMu EQ6.vst3`
  - SHA-256：`a0be8daaf44f6b0484d529c737d3ffaad4d3479efd2ca8837a11ee46d976611e`
- `/Library/Application Support/Avid/Audio/Plug-Ins/SanMu EQ6.aaxplugin`
  - SHA-256：`56a04314fa8c97f7fc7e1fcd30eeda6f70d25ef1411a0f0523445d17e523bcd8`

回退备份：`build/installation-backups/20260925-linear-final/`。构建已关闭自动复制，避免未验证或未签名 AAX 覆盖已安装版本。只签名本工程 EQ，没有触碰 Comp / Delay / Reverb 的包。

## 可复现入口

```sh
cmake --build build-make --target sanmu_eq_regression_tests sanmuq6_render_ui -j4
./build-make/source/sanmu_eq_regression_tests
./build-make/source/sanmu_eq_regression_tests --benchmark-linear
./build-make/source/sanmu_eq_regression_tests --benchmark-linear-multi
./build-make/source/sanmu_eq_regression_tests --measure-linear-low
./build-make/source/sanmuq6_render_ui --verify-web
node tests/verify_web_ui.cjs
node tests/verify_stable_peaks.mjs
node tests/verify_spectrum_motion.mjs
auval -v aufx SMQ6 SmAu
```

证据保存在同目录 `验证记录-20260925/`。`tests/test_linear_rebuild.h` 和 `tests/test_mode_integrity.h` 为新增契约测试；性能命令是测量命令，不用退出码 0 声称所有性能配置通过。
