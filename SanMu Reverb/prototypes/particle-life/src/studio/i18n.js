// 文案只作用于显示；预设名称原值、参数与撤销快照不随语言切换改变。
const english = {
  '干': 'Dry', '湿': 'Wet', '声场显示': 'Visual display',
  '关闭动态图像，声音处理与电平表继续运行。': 'Turn off visuals while audio and meters keep running.',
  '干显示进入混响器的输入。湿显示经过 MIX 控制的湿信号。混合同时显示两者；MIX 为 100% 时仍可观察输入参考。设置中的声场显示开关只控制画面，声音处理与电平表继续运行。': 'Dry shows the input signal. Wet shows its wet contribution after Mix. Mix displays both; the input reference remains visible at 100% Mix. Visual display in Settings controls only the graphics; audio and meters keep running.',
  '三木混响': 'SanMu Audio Reverb', '三维生命声场': 'Living particle field', '上一个预设': 'Previous preset', '下一个预设': 'Next preset', '保存预设': 'Save preset', '已修改': 'Modified', '撤销': 'Undo', '重做': 'Redo', '前后对比': 'A/B comparison', '对比 A': 'Compare A', '对比 B': 'Compare B', '复制到 A': 'Copy to A', '复制到 B': 'Copy to B', '已复制到 A': 'Copied to A', '已复制到 B': 'Copied to B', '旁通': 'Bypass',
  '混响空间': 'Reverb space', '混响类型': 'Reverb type', '空间风格': 'Space character', '大厅': 'Hall', '板式': 'Plate', '房间': 'Room', '回声室': 'Chamber', '音乐厅': 'Concert Hall', '教堂': 'Cathedral', '现代丝滑': 'Modern Silk', '经典钢板': 'Classic Steel', '录音室': 'Studio Room', '现场房间': 'Live Room', '柔暖回声室': 'Velvet Chamber', '石质回声室': 'Stone Chamber',
  '生命声场与电平': 'Particle field and meters', '生命声场': 'Particle field', '观察信号': 'Signal view', '静候声音': 'Awaiting sound', '正在载入': 'Loading', '播放中': 'Playing', '尾音': 'Reverb tail', '原声': 'Dry', '混响': 'Reverb', '叠加': 'Overlay', '输入': 'Input', '输出': 'Output', '频段色谱': 'Frequency colors', '低频': 'Low', '中频': 'Mid', '高频': 'High', '专注声场': 'Focus on particles', '返回完整界面': 'Return to full interface', '输入输出电平': 'Input and output meters', '输入左声道': 'Input left', '输入右声道': 'Input right', '输出左声道': 'Output left', '输出右声道': 'Output right', '清除输入峰值': 'Clear input peak', '清除输出峰值': 'Clear output peak',
  '空间主参数': 'Space controls', '声音细节': 'Sound details', '频段阻尼': 'Damping', '扩散': 'Diffusion', '调制': 'Modulation', '输出滤波': 'Output EQ', '预延迟': 'Pre-delay', '衰减': 'Decay', '尺寸': 'Size', '建立': 'Attack', '混合': 'Mix', '低频分界': 'Bass Freq', '低频倍数': 'Bass Mult', '高频分界': 'High Freq', '高频阻尼': 'High Shelf', '早期扩散': 'Early', '晚期扩散': 'Late', '速率': 'Rate', '深度': 'Depth', '低切': 'Low Cut', '高切': 'High Cut', '秒': 's', '毫秒': 'ms', '赫兹': 'Hz', '分贝': 'dB', '倍': '×',
  '播放': 'Play', '停止输入，保留尾音': 'Stop input, keep the tail', '试听素材': 'Audition source', '和弦': 'Chords', '军鼓': 'Snare', '短音 · 220 赫兹': 'Tone · 220 Hz', '短音 · 880 赫兹': 'Tone · 880 Hz', '短音 · 6000 赫兹': 'Tone · 6000 Hz', '打开本地音频': 'Open audio file', '循环播放': 'Loop', '静音': 'Mute', '取消静音': 'Unmute', '监听': 'Monitor', '就绪': 'Ready', '立体声': 'Stereo', '纯湿': '100% wet', '干声': 'Dry', '插入': 'Insert',
  '预设': 'Presets', '关闭预设': 'Close presets', '搜索预设': 'Search presets', '预设分类': 'Preset categories', '全部': 'All', '我的': 'My presets', '取消保存': 'Cancel save', '名称': 'Name', '保存到我的预设': 'Save to My Presets', '没有匹配的预设': 'No matching presets', '我的预设已达到 100 个': 'My Presets has reached the 100-preset limit', '已保存到此浏览器的我的预设': 'Saved to My Presets in this browser', '保存失败': 'Could not save the preset',
  '设置': 'Settings', '关闭设置': 'Close settings', '外观': 'Appearance', '浅色': 'Light', '深色': 'Dark', '界面语言': 'Interface language', '窗口 · 界面缩放': 'Window · Interface scale', '恢复 100% 大小': 'Reset size to 100%', '增大窗口后可使用更高的缩放档位。': 'Enlarge the window to unlock higher scales.', '悬停操作提示': 'Tooltips', '跟随界面': 'Follow interface', '中文提示': 'Chinese', '英文提示': 'English', '关闭提示': 'Off', '操作帮助': 'Controls & shortcuts', '关于 SanMu Audio': 'About SanMu Audio', '关闭帮助': 'Close help', '关闭关于': 'Close about',
  '旋钮与数值': 'Knobs & values', '键盘与撤销': 'Keyboard & undo', '类型与预设': 'Types & presets', 'A/B 与 BYP': 'A/B & BYP', '试听与电平': 'Audition & meters',
  '上下拖动或滚轮调整。双击旋钮恢复参数默认；点击数值可直接输入，支持秒、毫秒、赫兹等单位。': 'Drag vertically or use the wheel. Double-click a knob to reset it. Click a value to type a number with units such as s, ms or Hz.',
  '聚焦旋钮后用方向键调整，Home / End 到达边界，回车编辑数值。⌘ / Ctrl + Z 撤销，加 Shift 重做。': 'Focus a knob and use the arrow keys. Home / End go to the limits; Enter edits its value. Use ⌘ / Ctrl + Z to undo, and add Shift to redo.',
  '切换类型、风格会保留参数；加载预设会应用其全部参数。预设列表标明纯湿或插入比例，保存的预设位于“我的”。': 'Changing the type or character keeps your parameters. Loading a preset applies all its values. Presets show their wet or insert mix; saved presets appear under My presets.',
  'A/B 保留两组声音设置，复制按钮把当前组复制到另一组。BYP 点亮时旁通混响；再次点击恢复处理。': 'A/B holds two sets of sound settings. Copy transfers the current set to the other slot. A lit BYP bypasses the reverb; click it again to restore processing.',
  '原声显示进入混响器的输入。混响显示经过 MIX 控制的湿声贡献。叠加同时显示两者；MIX 为 100% 时仍可观察输入参考。': 'Dry shows the signal entering the reverb. Reverb shows its wet contribution after Mix. Overlay shows both; the input reference remains visible at 100% Mix.',
  '停止只停止输入，保留尾音。监听音量不影响信号观察；点击电平峰值可清除保持。窗口、主题与提示设置不改变声音。': 'Stop ends the input and preserves the tail. Monitor volume does not affect visualization. Click a meter peak to clear it. Size, theme and tooltip settings do not change the sound.',
  '大厅、板式、房间、回声室。四类空间，八种风格。': 'Hall, Plate, Room and Chamber. Four spaces, eight characters.', '界面设计预览 · U12': 'Interface preview · U12',
  '画面暂停，正在恢复图形连接。': 'Visuals paused. Restoring graphics.', '未能启动三维画面，请使用支持图形加速的浏览器。': 'Unable to start the particle field. Please use a browser with graphics acceleration.', '载入失败，请重试': 'Could not load. Please try again.', '混响模块未能载入': 'Could not load the reverb module', '混响模块启动超时，请重新播放。': 'Reverb startup timed out. Please play again.', '声音处理已停止，请刷新页面重新载入。': 'Audio processing stopped. Reload the page to restart.', '请选择小于 100 MB 的音频文件。': 'Choose an audio file smaller than 100 MB.', '无法读取这段音频，请使用 WAV、MP3、AAC 或 FLAC 文件。': 'Could not read the audio. Please use WAV, MP3, AAC or FLAC.',
};
// 悬停提示使用短句解释功能，不把参数名本身改成另一套术语。
const tooltipEnglish = {
  '干：只显示输入信号': 'Dry: show only the input signal.',
  '湿：只显示经过 MIX 的湿信号': 'Wet: show only the wet signal after Mix.',
  '混合：同时显示干、湿信号，不改变声音比例': 'Mix: display dry and wet together without changing the audio balance.',
  '预延迟：直达声与混响开始之间的时间': 'Pre-delay: time before the reverb begins.',
  '衰减：混响尾音持续时间': 'Decay: length of the reverb tail.',
  '尺寸：模拟空间的相对大小': 'Size: relative scale of the simulated space.',
  '建立：控制混响从近到远的建立速度': 'Attack: how quickly the reverb moves from near to far.',
  '混合：干声与混响的比例': 'Mix: balance between dry and reverberated signal.',
  '低频分界：低频阻尼开始作用的频率': 'Bass Freq: frequency where bass damping begins.',
  '低频倍数：低频相对衰减时间': 'Bass Mult: relative decay time for low frequencies.',
  '高频分界：高频阻尼开始作用的频率': 'High Freq: frequency where high damping begins.',
  '高频阻尼：高频衰减量': 'High Shelf: amount of high-frequency damping.',
  '早期扩散：早期反射的密度': 'Early: density of early reflections.',
  '晚期扩散：晚期混响的密度': 'Late: density of the late reverb field.',
  '速率：调制变化速度': 'Rate: speed of the modulation movement.',
  '深度：调制变化幅度': 'Depth: amount of modulation movement.',
  '低切：混响输出的低频截止': 'Low Cut: removes low frequencies from the reverb output.',
  '高切：混响输出的高频截止': 'High Cut: removes high frequencies from the reverb output.',
  '预设：浏览并加载混响预设': 'Presets: browse and load reverb settings.',
  '原声：只显示输入信号': 'Dry: show only the input signal.',
  '混响：只显示经过 MIX 的湿声': 'Reverb: show only the wet signal after Mix.',
  '叠加：同时显示原声与混响': 'Overlay: show dry and reverb together.',
  '大厅：选择大厅混响空间': 'Hall: choose a large hall space.',
  '板式：选择板式混响空间': 'Plate: choose a dense plate space.',
  '房间：选择房间混响空间': 'Room: choose a room space.',
  '回声室：选择回声室混响空间': 'Chamber: choose a chamber space.',
  '音乐厅：选择当前空间的音乐厅风格': 'Concert Hall: choose the concert hall character.',
  '教堂：选择当前空间的教堂风格': 'Cathedral: choose the cathedral character.',
  '现代丝滑：选择当前空间的现代丝滑风格': 'Modern Silk: choose the smooth plate character.',
  '经典钢板：选择当前空间的经典钢板风格': 'Classic Steel: choose the textured steel plate character.',
  '录音室：选择当前空间的录音室风格': 'Studio Room: choose the studio room character.',
  '现场房间：选择当前空间的现场房间风格': 'Live Room: choose the live room character.',
  '柔暖回声室：选择当前空间的柔暖回声室风格': 'Velvet Chamber: choose the warm chamber character.',
  '石质回声室：选择当前空间的石质回声室风格': 'Stone Chamber: choose the stone chamber character.',
};
export const parameterTips = {
  '预延迟': '直达声与混响开始之间的时间', '衰减': '混响尾音持续时间', '尺寸': '模拟空间的相对大小', '建立': '控制混响从近到远的建立速度', '混合': '干声与混响的比例',
  '低频分界': '低频阻尼开始作用的频率', '低频倍数': '低频相对衰减时间', '高频分界': '高频阻尼开始作用的频率', '高频阻尼': '高频衰减量', '早期扩散': '早期反射的密度', '晚期扩散': '晚期混响的密度', '速率': '调制变化速度', '深度': '调制变化幅度', '低切': '混响输出的低频截止', '高切': '混响输出的高频截止',
};
const presetParts = {
  '丝滑板式': 'Modern Silk', '标准基准': 'Reference', '基准': 'Reference', '贴近人声': 'Intimate Vocal', '抒情人声': 'Vocal Ballad', '清晰钢琴': 'Clear Piano', '宽阔钢琴': 'Wide Piano', '柔和弦乐': 'Soft Strings', '室内乐融合': 'Chamber Ensemble', '温暖圣咏': 'Warm Choir', '深远穹顶': 'Distant Dome', '温暖人声': 'Warm Vocal', '军鼓短板': 'Short Snare Plate', '自然基准': 'Natural Reference', '开阔基准': 'Open Reference', '鼓组空间': 'Drum Space', '人声包裹': 'Vocal Embrace', '弦乐余韵': 'String Bloom', '军鼓轮廓': 'Snare Definition', '钢琴空间': 'Piano Space',
};
export function translate(text, language = 'en') {
  if (language !== 'en') return text;
  if (Object.hasOwn(tooltipEnglish, text)) return tooltipEnglish[text];
  if (Object.hasOwn(english, text)) return english[text];
  const value = text.match(/^(.*)数值$/);
  if (value && english[value[1]]) return `${english[value[1]]} value`;
  if (text === '声音处理已停止，请重新播放。') return 'Audio processing stopped. Please play again.';
  const scale = text.match(/^界面缩放 (\d+)%$/);
  if (scale) return `Interface scale ${scale[1]}%`;
  const invalid = text.match(/^(.*)需要有效数值$/);
  if (invalid) return `Enter a valid value for ${english[invalid[1]] || invalid[1]}`;
  if (text.startsWith('混响模块启动失败：')) return `Reverb startup failed: ${text.slice(9)}`;
  return text;
}
export function presetLabel(preset, language) {
  if (language !== 'en' || preset.user) return preset.name;
  return preset.name.split(' · ').map(part => presetParts[part] || translate(part, language)).join(' · ');
}
// 首次注册固定文本节点。切换只写这些节点，不重建旋钮、音频或用户输入。
export function bindTranslations(root) {
  const entries = [];
  const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT);
  for (let node = walker.nextNode(); node; node = walker.nextNode()) {
    if (node.parentElement.closest('script, style, #preset-name, #preset-list, #style-0, #style-1, #mix-badge, #status, #audio-state, #notification, #tooltip')) continue;
    const value = node.textContent.trim();
    if (Object.hasOwn(english, value)) entries.push(language => { node.textContent = node.textContent.replace(node.textContent.trim(), translate(value, language)); });
  }
  for (const element of root.querySelectorAll('[aria-label], [placeholder]')) {
    if (element.matches('.icon-button, .dial, .knob-readout input')) continue;
    for (const attribute of ['aria-label', 'placeholder']) {
      const value = element.getAttribute(attribute);
      if (value) entries.push(language => element.setAttribute(attribute, translate(value, language)));
    }
  }
  return language => entries.forEach(apply => apply(language));
}
