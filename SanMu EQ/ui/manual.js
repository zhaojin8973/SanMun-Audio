// 帮助手册仅叙述已实现的 EQ 功能，独立于宿主授权与音频预览。
const sections = {
  zh: [
    ['频段与曲线', '点击频段节点或下方编号选择频段。拖动节点调整频率与增益，使用频率、增益、Q 旋钮精确调整；Q 越高范围越窄。可添加最多 24 段。低切与高切可选 12／24／36／48 dB/倍频程斜率，其增益旋钮不可编辑。'],
    ['动态均衡与监听', '开启动态后，向下模式压低超过阈值的信号，向上模式提升低于阈值的有效信号，并保护静音及底噪。范围限制最大增益变化。AUTO 在内部跟随输入，使用温和的比例和不超过 3 dB 的上限；切换到 MANUAL 后可调节阈值并使用完整范围。按住频段耳机键临时独听，左右拖动扫频，松开即退出。'],
    ['音色与线性相位', 'Clean、Tube、Discrete 是三种音色。在线性相位模式下，可在设置中选择日常、高精度、极限；该模式具有处理延迟，宿主应进行延迟补偿。动态均衡仍可使用，音色处理会暂停。'],
    ['操作与设置', '上下拖动旋钮或滚动鼠标调节；双击或按住 Option／Command 点击复位。聚焦旋钮后用方向键调节、Home／End 到达边界，Enter 输入数值。A/B 可比较声音，撤销／重做作用于参数修改。主题、语言、提示和显示设置会在本机按产品记忆，新实例和新工程自动沿用；声音参数仍由工程和预设保存。在频谱内停留片刻可捕捉固定峰值，点击或拖动候选添加频段，点击空白或按 Esc 退出。'],
    ['预设与内测', '通过顶部预设按钮浏览或保存预设。浏览器预览不处理宿主音频；请在 DAW 中检查声音、自动化、保存恢复及宿主兼容性。']
  ],
  en: [
    ['Bands and curve', 'Select a band by clicking its node or number. Drag a node to change frequency and gain; use Frequency, Gain and Q for precise adjustment. Higher Q narrows the affected range. Add up to 24 bands. Low Cut and High Cut offer 12/24/36/48 dB/oct slopes; Gain is unavailable for cut filters.'],
    ['Dynamic EQ and audition', 'Down reduces signals above the threshold. Up lifts valid signals below it, with protection against boosting silence and noise. Range limits the gain change. AUTO tracks internally with a gentle ratio and a 3 dB ceiling; MANUAL exposes the threshold and full range. Hold a band’s headphone control to audition, drag sideways to sweep frequency, and release to stop.'],
    ['Character and Linear Phase', 'Clean, Tube and Discrete provide three characters. In Linear Phase, choose Daily, High precision or Extreme in Settings. The mode adds latency that the host should compensate. Dynamic EQ remains available while character processing pauses.'],
    ['Controls and settings', 'Drag knobs vertically or scroll. Double-click or Option/Command-click to reset. Focus a knob and use arrows, Home/End or Enter for numeric entry. A/B compares sounds; Undo/Redo covers parameter edits. Appearance, language, tooltips and display preferences persist per product across instances and projects. Hover inside the spectrum to capture stable peaks, click or drag a candidate to add a band, and click blank space or press Escape to dismiss.'],
    ['Presets and testing', 'Use the top preset control to browse or save presets. Browser preview does not process host audio; test sound, automation, state recall and host compatibility in a DAW.']
  ]
};
export function renderManual(container, language) {
  container.replaceChildren();
  for (const [heading, text] of sections[language === 'en' ? 'en' : 'zh']) {
    const section = document.createElement('section');
    const title = document.createElement('h3');
    const body = document.createElement('p');
    title.textContent = heading;
    body.textContent = text;
    section.append(title, body);
    container.append(section);
  }
}
