export type Lang = 'zh' | 'en';

export const productList = [
  {
    slug: 'eq6', name: 'SanMu EQ6', version: '0.1.5', accent: '#27A88A', imageWidth: 1836, imageHeight: 1264, darkImageWidth: 1836, darkImageHeight: 1264, promoWidth: 1800, promoHeight: 1200, image: '/images/music-capture/eq6-light.png',
    darkImage: '/images/music-capture/eq6-dark.png', promo: '/images/promo/real-response-2026-09-27/eq6-light.png',
    zh: { kicker: '频率 · 动态', short: '最多 24 个槽位的动态参量均衡器。', features: ['最多 24 个频段槽位', 'Zero Latency 与设置中的 Linear Phase', 'Pre/Post、实时选峰、Mid/Side'], captureNote: '真实音乐播放捕捉 · Clean / 动态频段 03 · 轨道名已隐藏', body: 'SanMu EQ6 在同一工作区提供频谱、频点、动态范围和 Clean、Tube、Discrete 音色模式。' },
    en: { kicker: 'FREQUENCY · DYNAMICS', short: 'A dynamic parametric EQ with up to 24 slots.', features: ['Up to 24 band slots', 'Zero Latency and Linear Phase in Settings', 'Pre/Post, peak focus, Mid/Side'], captureNote: 'Real playback capture · Clean / dynamic band 03 · track name hidden', body: 'SanMu EQ6 combines spectrum, band controls, dynamic range and Clean, Tube and Discrete character modes in one workspace.' }
  },
  {
    slug: 'comp', name: 'SanMu Comp', version: '0.1.17', accent: '#D4933D', imageWidth: 1836, imageHeight: 1264, darkImageWidth: 1836, darkImageHeight: 1264, promoWidth: 1800, promoHeight: 1200, image: '/images/music-capture/comp-light.png', darkImage: '/images/music-capture/comp-dark.png', promo: '/images/promo/real-response-2026-09-27/comp-light.png',
    zh: { kicker: '动态 · 四种模式', short: 'LEV、VCA、FET、Opto 四种压缩模式。', features: ['LEV 自适应平准', 'VCA 总线胶水 / FET 快速抓击', 'Opto 光电温润；LISTEN 五秒测算'], captureNote: '浅色为 VCA 实录 · 深色为 LEV 实录 · 两种状态分别标注', body: 'SanMu Comp 提供 LEV、VCA、FET 和 Opto 四种压缩模式，并显示输入、输出、增益衰减和监听测算结果。' },
    en: { kicker: 'DYNAMICS · FOUR MODES', short: 'Four compression modes: LEV, VCA, FET and Opto.', features: ['LEV adaptive levelling', 'VCA bus glue / FET fast capture', 'Opto warmth; five-second LISTEN measurement'], captureNote: 'Light is VCA capture · dark is LEV capture · states are labelled separately', body: 'SanMu Comp provides LEV, VCA, FET and Opto modes with input, output, gain-reduction and listening measurements.' }
  },
  {
    slug: 'delay', name: 'SanMu Delay', version: '0.1.5', accent: '#3D6FD8', imageWidth: 1730, imageHeight: 1258, darkImageWidth: 1740, darkImageHeight: 1206, promoWidth: 1800, promoHeight: 1200, image: '/images/music-capture/delay-light.png', darkImage: '/images/music-capture/delay-dark.png', promo: '/images/promo/real-response-2026-09-27/delay-light.png',
    zh: { kicker: '时间 · 路由', short: '带 Echo time editor 的多路由延迟。', features: ['Modern / Tape / BBD / Diffused', 'Independent / Ping-Pong / Cross', 'SYNC / BPM / FREE；Ducking GR'], captureNote: '真实音乐播放捕捉 · 右侧衰减图为参数模型，不是实时波形', body: 'SanMu Delay 将四种引擎、L/R 时间节点和 Independent、Ping-Pong、Cross 三种路由放在同一编辑器中。' },
    en: { kicker: 'TIME · ROUTING', short: 'A multi-routing delay with an Echo time editor.', features: ['Modern / Tape / BBD / Diffused', 'Independent / Ping-Pong / Cross', 'SYNC / BPM / FREE; Ducking GR'], captureNote: 'Real playback capture · the decay chart is a parameter model, not a live waveform', body: 'SanMu Delay combines four engines, L/R timing nodes and Independent, Ping-Pong and Cross routings in one editor.' }
  },
  {
    slug: 'reverb', name: 'SanMu Reverb', version: '1.0.4', accent: '#8170C8', imageWidth: 2152, imageHeight: 1458, darkImageWidth: 2152, darkImageHeight: 1458, promoWidth: 1800, promoHeight: 1200, image: '/images/music-capture/reverb-light.png', darkImage: '/images/music-capture/reverb-dark.png', promo: '/images/promo/real-response-2026-09-27/reverb-light.png',
    zh: { kicker: '空间 · 早晚反射', short: '包含四类空间与早晚反射控制的混响。', features: ['Plate / Room / Hall / Chamber', 'Time、Damping、Shape、Diff、Mod、EQ', '干 / 湿 / 混合声场视图'], captureNote: '真实音乐播放捕捉 · Plate / Modern Silk · 水波与粒子来自当前声场界面', body: 'SanMu Reverb 提供四类空间，并分别控制 Predelay、Decay、阻尼、Size、扩散、调制和 Output EQ。' },
    en: { kicker: 'SPACE · REFLECTIONS', short: 'A reverb with four spaces and early/late reflection controls.', features: ['Plate / Room / Hall / Chamber', 'Time, Damping, Shape, Diff, Mod, EQ', 'Dry / wet / mix sound-field views'], captureNote: 'Real playback capture · Plate / Modern Silk · water and particles come from the current sound-field UI', body: 'SanMu Reverb provides four spaces with separate Predelay, Decay, damping, Size, diffusion, modulation and Output EQ controls.' }
  }
] as const;

export const getProduct = (slug: string) => productList.find((p) => p.slug === slug) ?? productList[0];
