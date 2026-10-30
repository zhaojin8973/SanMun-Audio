// 每个提示描述当前控件实际效果；频段编号变化时按参数后缀复用说明。
const descriptions = {
  band_freq: ['Move the selected band along the frequency spectrum.', '调整所选频段在频谱上的位置。'],
  band_gain: ['Boost or cut the selected band. Cut filters have no gain control.', '提升或衰减所选频段；切除滤波器不使用增益旋钮。'],
  band_q: ['Set the band width: higher Q narrows the affected range.', '调整频段宽度：Q 值越高，作用范围越窄。'],
  band_slope: ['Set the Low Cut or High Cut slope from 12 to 48 dB/oct.', '设置低切或高切斜率，范围为 12～48 dB/倍频程。'],
  band_dyn_threshold: ['Manual detector threshold. AUTO follows the input signal and locks this knob.', '手动设置动态触发阈值；AUTO 会跟随输入信号并锁定此旋钮。'],
  band_dyn_range: ['Limit the maximum gain reduction of this dynamic band.', '限制当前动态频段的最大增益衰减量。'],
  input_gain_l: ['Adjust the input level before EQ processing; the right input follows.', '调整均衡处理前的输入电平；右声道输入同步变化。'],
  input_gain_r: ['Adjust the right input level before EQ processing.', '调整均衡处理前的右声道输入电平。'],
  output_gain_l: ['Adjust the left output level after EQ; Link can join the right output.', '调整均衡处理后的左声道输出电平；联动可同步右声道。'],
  output_gain_r: ['Adjust the right output level after EQ; Link can join the left output.', '调整均衡处理后的右声道输出电平；联动可同步左声道。'],
  '频谱': ['Show or hide the spectrum analyser; audio processing stays active.', '显示或隐藏频谱分析；音频处理保持运行。'],
  '频谱选峰': ['Hold over a stable spectrum peak to inspect and add a band.', '悬停在稳定谱峰上可查看并添加频段。'],
  '自动增益补偿': ['Compensate the output level when the EQ boosts or cuts.', '在均衡提升或衰减后补偿输出电平。'],
  '极性反转': ['Invert the signal polarity without changing the EQ curve.', '反转信号极性，不改变均衡曲线。'],
  '输出联动': ['Adjust both output channels together.', '同步调节左右输出声道。'],
  '整体旁通': ['Bypass all EQ processing while retaining the settings.', '旁通所有均衡处理，同时保留参数。'],
  '点击电平表清除峰值': ['Clear the held peaks on the output meters.', '清除输出电平表的峰值保持。'],
  '添加频段': ['Add a new EQ band, up to 24 active bands.', '添加均衡频段，最多可同时使用 24 段。'],
  '频段列表': ['Open the list of active bands and their controls.', '打开当前频段列表和各频段操作。'],
  '按住独听，左右拖动扫频': ['Hold to audition this band; drag sideways to sweep frequency, release to stop.', '按住可独听当前频段；左右拖动扫频，松开即停止。'],
  '自动阈值由系统跟随；切换到 MANUAL 后可调': ['AUTO follows the signal; choose MANUAL to edit the threshold.', 'AUTO 会跟随信号；切换到 MANUAL 后可调节阈值。'],
};
export function tooltipText(key, language) {
  const normalized = String(key || '').replace(/^band\d+_/, 'band_');
  const pair = descriptions[normalized];
  return pair?.[language === 'zh' ? 1 : 0];
}
