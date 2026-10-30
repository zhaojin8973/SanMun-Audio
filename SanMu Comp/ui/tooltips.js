// 文案描述当前实现，不使用硬件型号或快捷键代替功能说明。
const tips = {
  vox_comp: ['Turn up for stronger levelling.', '调大可加强电平平准。'],
  vox_gain: ['Adjust the level after levelling.', '调整平准后的音量。'],
  vox_gate: ['Turn up to reduce more quiet background sound.', '调高门限，压低更多安静段的底噪。'],
  vca_threshold: ['Lower the threshold for more compression.', '阈值越低，越容易产生压缩。'],
  vca_ratio: ['Higher ratios compress above-threshold sound more strongly.', '比例越高，超过阈值的声音压得越多。'],
  vca_attack: ['Shorter times catch attacks; longer times let more through.', '越短越能压住起音，越长越保留起音。'],
  vca_release: ['Set how quickly compression recovers; AUTO adapts to the signal.', '控制压缩恢复速度；AUTO 随信号调整。'],
  vca_makeup: ['Restore the compressed signal’s level.', '补回压缩后降低的音量。'],
  fet_input: ['Turn clockwise for more input drive and compression.', '顺时针提高输入，增加压缩。'],
  fet_output: ['Turn clockwise to raise the compressed signal’s level.', '顺时针提高压缩后的音量。'],
  fet_attack: ['Set how quickly compression starts; 7 is fastest.', '控制压缩介入速度；7 最快。'],
  fet_release: ['Set how quickly compression recovers; 7 is fastest.', '控制压缩恢复速度；7 最快。'],
  fet_ratio: ['Higher ratios compress harder; ALL is the most aggressive.', '比例越高压缩越强；ALL 最激烈。'],
  headroom: ['Higher headroom leaves more level before compression.', '余量越大，越不容易触发压缩。'],
  opto_gain: ['Adjust the level after optical compression.', '调整光电压缩后的音量。'],
  opto_peak_reduction: ['Turn up for more optical compression.', '调大可增加光电压缩量。'],
  opto_is_limit: ['COMPRESS is gentler; LIMIT restrains loud parts more firmly.', '压缩更温和，限制更强地约束大声部分。'],
  vox_smooth: ['Smooth gain changes with 12 ms latency; off keeps zero latency.', '以 12 毫秒延迟平滑增益；关闭为零延迟。'],
  in_out_link: ['Changing input adjusts output in the opposite direction.', '调整输入时反向调整输出。'],
  sidechain_hp: ['Reduce low-frequency triggering without cutting bass from the audio.', '减少低频触发压缩，不切除声音中的低频。'],
  stereo_link: ['Shared settings; 0% responds independently, 100% links detection.', '左右共用参数；0% 各自响应，100% 联动检测。'],
  mix: ['Blend original and compressed sound; 100% is fully compressed.', '混合原声与压缩声；100% 为全压缩声。'],
  output_trim: ['Adjust the final level after the blend.', '调整混合后的最终音量。'],
  mode_0: ['LEV · Adaptive levelling for more even levels.', 'LEV · 自适应平准，让音量更均匀。'],
  mode_1: ['VCA · Bus compression with adjustable timing.', 'VCA · 总线压缩，可细调响应速度。'],
  mode_2: ['FET · Fast compression for attacks and punch.', 'FET · 快速压缩，塑造起音与冲击感。'],
  mode_3: ['Opto · Gentle optical compression with gradual recovery.', 'Opto · 温和的光电压缩，恢复更舒缓。'],
  analyse: ['Measure 5 seconds and suggest settings for your target on this compressor.', '采集 5 秒，为当前压缩器测算目标参数。'],
  history: ['The graph shows the last 2 seconds, from left to right.', '曲线从左到右显示最近 2 秒。'],
  level_range: ['Zoom IN and OUT together: ×2 makes level differences twice as tall.', '同步放大 IN／OUT；×2 让输入输出的电平差显示为两倍高度。'],
  gr_range: ['Zoom the GR curve: 6 dB enlarges it, 24 dB shows more range.', '缩放 GR 曲线：6 dB 放大显示，24 dB 显示更大范围；不改变压缩量。'],
  input: ['Show the input level before processing.', '显示处理前的输入电平。'],
  output: ['Show the final output level.', '显示处理后的输出电平。'],
  gr: ['Show how many decibels the compressor reduces.', '显示压缩器压低了多少分贝。'],
};

export function tooltip(key, language) {
  const id = key.replace(/^tip:/, '');
  const value = tips[id] || ['sidechain_hp', 'stereo_link', 'output_trim', 'mix'].map(s => id.endsWith(s) ? tips[s] : null).find(Boolean);
  return value?.[language === 'zh' ? 1 : 0];
}
