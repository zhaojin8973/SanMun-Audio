> 后续执行（2026-09-07）：用户已决定重做Room且只保留两种风格。0907C已实现并安装Studio Room与Live Room，待听审，详见`0907C_Room首稿试听说明.md`。以下保留决策前的评估内容。

# Room与Chamber开发优先级评估

2026-09-07。用户确认Plate两个模式暂定初稿，要求根据声学特质、环境与应用判断是否同时开发Room、Chamber。本文件是评估建议，不代表用户已决定开发路线。

## 建议

先做一个有明确用途的Room初稿；Chamber暂缓，不要求为凑齐分类而同时制作。现有Hall负责大空间，Modern Silk与Classic Steel提供两类板式尾音，当前最有价值的增量是近距离、短空间、早期反射与干声的自然结合。

Room首轮定位为自然录音室：给干录的人声和乐器增加周围空间，让鼓组和多轨录音更像处于同一环境，保持音头与稳定的声像。早期反射的时序、方向、频率吸收及其向晚期尾声的过渡，应作为核心设计对象；不能仅把Hall衰减缩短。

Chamber以后可以提供专用混响室的反射和拾音色彩，适用于人声、弦乐、鼓与具有特定录音室气质的制作。其价值取决于是否能做出当前Hall、Plate及未来Room无法方便实现的声学行为，而不只是更暗、更暖或更长的预设。Chamber并非一律暗色：VintageVerb的官方Chamber说明强调高密度、高扩散与较低染色。

## 首轮Room验证建议

- 先一个核心风格，暂不按旧菜单的三种材质名称全部铺开。
- 重点听干湿混合后的距离、贴合度、音头与共同空间感；100%湿声作为诊断补充。
- 以人声、军鼓/鼓组、木吉他及钢琴覆盖瞬态和持续音。
- 检查短尾的自然收束、左右平衡、单声道兼容及低中频箱体共振；保留已认可的Hall与两种Plate。

## 现有代码的地位

目前Room与Chamber已经有独立类。Room使用早期反射模块加四路反馈延迟网络，Chamber使用输入带通加六路反馈延迟网络；这些只是遗留实现，不能凭类名或“Capitol／Abbey Road”等标签认定真实空间或硬件复刻。两者尚未按本轮功能目标完成专门审计与听审。

## 官方资料

- [ValhallaRoom算法说明](https://valhalladsp.com/shop/reverb/valhalla-room/)：Room与Chamber在初期回声密度及密度建立方式上的设计差别；同一产品可覆盖多种空间尺度。
- [VintageVerb模式说明](https://valhalladsp.com/2023/02/10/valhallavintageverb-the-modes/)：Room与Chamber的具体实现具有不同扩散与染色取向，不能只按名称推断亮暗。
- [UA Capitol Chambers手册](https://help.uaudio.com/hc/en-us/articles/18741463946516-Capitol-Chambers-Manual)：混响室的声学结构、扬声器、麦克风与位置共同决定空间及频谱特征。

本轮只冻结Plate初稿并提出路线建议，没有修改DSP、默认参数或插件安装。
