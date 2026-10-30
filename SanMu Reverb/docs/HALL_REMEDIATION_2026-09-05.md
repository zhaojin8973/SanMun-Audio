# Hall / Concert Hall 整改记录

> 历史记录：本页的八线整改方案及其完成声明已被否定，不能作为当前架构或验收依据。当前独立设计、错误复盘及验证边界见 [Hall F1 设计](HALL_F1_DESIGN.md)。

## 已锁定的参考条件

- ValhallaVintageVerb 4.0.5 ARM VST3，REAPER 插入效果。
- Mode：Concert Hall；Color：1970s。
- Mix 24.8%、PreDelay 20 ms、Decay 3.99 s、HighFreq 6000 Hz、HighShelf -24 dB、BassFreq 300 Hz、BassMult 1.50x、Size 100%、Attack 50%、Early 100%、Late 100%、ModRate 2.53 Hz、ModDepth 38%、HighCut 8010 Hz、LowCut 10 Hz。
- 另测四种扩散组合：`0/0`、`100/0`、`0/100`、`100/100`。比较时保持所有其他参数与输入一致。

## 本轮重构与已修复

1. Hall 已移除原来的“4 级 Attack 全通 + 4 级输入全通 + 双支路大延迟 + 输出全通”串联。该结构的直接项和大延迟抽头会在 Early/Late 提高时形成可听的梳状相位与孤立回声，不能继续作为 Concert Hall 基线。
2. 新 Hall 使用“预延迟 + 5 个早期反射抽头 + 8 条互质延迟线正交反馈网络”。Late=100 只增加反馈向量的正交混合；Early=100 只改变早期抽头比例和输入扩散。
3. 每条延迟线独立使用 HallLoopDamping，按目标 Decay、BassMult、HighFreq、HighShelf 计算每圈频率衰减；反馈余量固定为 0.995，避免重复计算 RT60。
4. Attack 改为输入进入反馈场的平滑比例，并将首圈能量转移到反馈场；它不再通过移动多级全通延迟改变相位。
5. Hall 的反馈阻尼改为根据目标 RT60 和 BassMult 计算绝对增益；输出 LowCut/HighCut 不再直接决定反馈环路的滤波。
6. 顶层模式切换使用两个独立引擎 bank，避免同一状态在一个 block 内被处理两次。
7. 顶层支持宿主超大 block 分片、单声道湿声折叠、reset 清理尾音、非有限输入隔离，以及等功率 Mix 递推。
8. 新增 `sanmu_hall_contracts`：验证四象限扩散确实改变响应、100% wet 的 t=0 不泄露、block size 不改变结果、Attack 早期减弱而后期保留。
9. 新增 `tools/render_hall.cpp` 和 `tools/render_reference.py`，输出 WAV、参数 manifest、客观 metrics、10 ms 包络 CSV；不自动归一化、不自动时间对齐、不把客观指标解释为听审结论。
10. 立体声输出使用左右不同的延迟线权重和正交反馈状态，不使用 `L-R` 反相制造宽度；Early=100 时不再存在专用输出全通去相关层。

## 当前测量与听审边界

用同一 48 kHz、双声道脉冲和 128-sample block 离线渲染后，新拓扑在用户给定参数下的首个湿声约 `24.5 ms`，测得 RT60 约 `3.8 s`，左右相关性约 `0.71`，能量从 20~80 ms 向 80~500 ms 平滑分散。相位相关性和密度指标只能证明结构没有明显反相或单一回声，不能代替 REAPER 中的人工听感验收。

参考插件的 `Mix=24.8%` 通过独立离线宿主导出时没有出现原始干声电平；因此该渲染只用于检查湿声算法和参数写入，不能代替 REAPER 插入态的干湿路由。插入态的 Mix、旁通、延迟补偿和电平必须在 REAPER 中用同一输入再次核验。

四组候选的客观文件位于 `build-make/audit/2026-09-05/current/`。这些文件只证明渲染可复现；最终方向仍需要在 REAPER 中以同一输入、同一 Mix 和同一监听电平进行人工听审。

## 当前判断

八线正交反馈网络现在作为 Concert Hall 的新基线。下一步只在固定参考条件下听审 100/100、0/0、100/0、0/100 四组，分别记录密度、暖度、宽度、起音和相位感受；在 Concert Hall 通过听审前，不扩展 Scoring Stage、Cathedral、Plate、Room、Chamber 的相似度验收。
