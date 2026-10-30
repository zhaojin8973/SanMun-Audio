import { createElement, ChevronDown, ChevronLeft, ChevronRight, Undo2, Redo2, Copy, Save, Settings,
  Maximize2, Minimize2, Play, Square, Upload, Repeat2, Volume2, VolumeX, X, Search, Check } from 'lucide';
import { AudioEngine } from '../audio.js';
import { LifeScene } from '../scene.js';
import { parameterList } from '../parameters.js';
import { Knob } from './knob.js';
import { families, presets, validatePreset } from './presets.js';
import { translate, presetLabel, bindTranslations } from './i18n.js';
import './style.css';
import { isNative, callNative, onNativeFrame, NativeAudio } from './native.js';
import { NativeStateQueue } from './native-state.js';

let nativeReady = false;
const nativeSync = new NativeStateQueue(callNative,
  state => { if (!transaction) applyHostState(state); }, error => notify(error.message));
if (isNative) document.documentElement.classList.add('native-plugin', 'native-loading');

const definitions = parameterList.map(item => {
  const result = [...item];
  if (item[6] === '%' || item[0] === 'predelay') result[4] = .1;
  if (['highHz', 'highCut'].includes(item[0])) result[4] = 1;
  return result;
});
const initial = presets[0];
const parameters = { ...initial.values };
let mode = 0, bypass = false, presetId = initial.id, presetName = initial.name, presetValues = JSON.stringify(parameters);
let playing = false, busy = false, scene, audio, theme = 'light', slot = 'A', transaction = null;
const scales = [.85, 1, 1.15, 1.25, 1.5];
let uiScale = 1, language = 'en', tooltipLanguage = 'auto', visualsEnabled = true;
const t = value => translate(value, language);
let users = [];
const history = [], future = [], knobs = [], knobById = new Map();
const snapshot = () => ({ values: { ...parameters }, mode, bypass, presetId, presetName, presetValues });
const slots = { A: snapshot(), B: snapshot() };
try {
  const stored = JSON.parse(localStorage.getItem('sanmu-studio-presets') || '[]');
  if (Array.isArray(stored)) users = stored.filter(validatePreset).slice(0, 100);
  theme = localStorage.getItem('sanmu-studio-theme') === 'dark' ? 'dark' : 'light';
  visualsEnabled = localStorage.getItem('sanmu-studio-visuals') !== 'off';
  const storedScale = Number(localStorage.getItem('sanmu-studio-scale'));
  if (scales.includes(storedScale)) uiScale = storedScale;
  language = localStorage.getItem('sanmu-studio-language') === 'zh' ? 'zh' : 'en';
  const savedTips = localStorage.getItem('sanmu-studio-tooltip-language');
  if (['auto', 'en', 'zh', 'off'].includes(savedTips)) tooltipLanguage = savedTips;
  else if (localStorage.getItem('sanmu-studio-tips') === 'off') tooltipLanguage = 'off';
} catch { /* 本地偏好不可用时仍可操作界面。 */ }

const button = (id, label, className = '', extra = '') => `<button id="${id}" class="icon-button ${className}" aria-label="${label}" data-tip="${label}" ${extra}></button>`;
const knob = (id, size = '') => `<div class="knob ${size}" data-knob="${id}"></div>`;
document.querySelector('#app').innerHTML = `
  <article class="instrument" data-family="0" aria-label="三木混响">
    <header class="topbar">
      <div class="brand" aria-label="SanMu Audio Reverb">
        <span class="brand-wordmark" aria-hidden="true"><strong>SanMu</strong><span class="brand-product">Reverb</span></span>
        <span class="brand-audio" aria-hidden="true"><span>A</span><span>u</span><span>d</span><span>i</span><span>o</span></span>
      </div>
      <div class="preset-control">
        ${button('previous', '上一个预设')}
        <button id="preset-open" class="preset-open" aria-label="预设" aria-haspopup="dialog" data-tip="预设：浏览并加载混响预设"><span id="preset-name"></span><span id="preset-dirty" aria-label="已修改" hidden>·</span><i data-icon="down"></i></button>
        ${button('next', '下一个预设')}${button('save', '保存预设')}
      </div>
      <div class="tools">
        <div class="history-tools">${button('undo', '撤销', '', 'disabled')}${button('redo', '重做', '', 'disabled')}</div>
        <fieldset class="compare"><legend class="sr-only">前后对比</legend>
          <label><input type="radio" name="compare" value="A" checked aria-label="对比 A" /><span>A</span></label>
          <label><input type="radio" name="compare" value="B" aria-label="对比 B" /><span>B</span></label>
        </fieldset>${button('copy', '复制到 B')}
        <span class="tool-divider"></span><button id="bypass" class="icon-button power" aria-label="旁通" data-tip="旁通" aria-pressed="false">BYP</button>${button('settings', '设置', '', 'aria-haspopup="dialog"')}
      </div>
    </header>
    <nav class="modebar" aria-label="混响空间">
      <fieldset class="families"><legend class="sr-only">混响类型</legend>${families.map((family, i) => `<label data-tip="${family.name}：选择${family.name}混响空间"><input type="radio" name="family" value="${i}" ${i === 0 ? 'checked' : ''} /><span>${family.name}</span></label>`).join('')}</fieldset>
      <span class="mode-divider"></span><fieldset class="styles"><legend class="sr-only">空间风格</legend>
        <label data-tip="音乐厅：选择当前空间的音乐厅风格"><input type="radio" name="style" value="0" checked /><span id="style-0">音乐厅</span></label>
        <label data-tip="教堂：选择当前空间的教堂风格"><input type="radio" name="style" value="1" /><span id="style-1">教堂</span></label>
      </fieldset><span id="mix-badge" class="mix-badge">插入</span>
    </nav>
    <section class="sound-section" aria-label="生命声场与电平">
      <div id="stage" class="stage" aria-label="生命声场">
        <div class="scene-caption"><span class="status-dot"></span><span id="status">静候声音</span></div>
        <div class="scene-tools"><fieldset class="views"><legend class="sr-only">观察信号</legend>
          ${[['dry', '干', '干：只显示输入信号'], ['wet', '湿', '湿：只显示经过 MIX 的湿信号'], ['overlay', '混合', '混合：同时显示干、湿信号，不改变声音比例']].map(([id, label, tip]) => `<label data-tip="${tip}"><input type="radio" name="view" value="${id}" ${id === 'overlay' ? 'checked' : ''} /><span>${label}</span></label>`).join('')}
        </fieldset>${button('focus', '专注声场')}</div>
        <div class="scene-legend"><span id="input-legend">
          <svg class="signal-mark dry-mark" viewBox="0 0 18 14" aria-hidden="true"><path class="dust-mark" d="M3 11 15 3"/><ellipse class="water-mark" cx="9" cy="7" rx="4" ry="2.5"/></svg>干</span>
          <span id="wet-legend"><svg class="signal-mark wet-mark" viewBox="0 0 18 14" aria-hidden="true"><g class="dust-mark"><circle cx="4" cy="4" r="1.7"/><circle cx="9" cy="10" r="1.7"/><circle cx="14" cy="5" r="1.7"/></g><g class="water-mark"><ellipse cx="9" cy="7" rx="7.5" ry="5.5"/><ellipse cx="9" cy="7" rx="4.5" ry="3"/><ellipse cx="9" cy="7" rx="1.4" ry="1"/></g></svg>湿</span></div>
        <div class="spectrum-key" aria-label="频段色谱"><span><i class="low"></i>低频</span><span><i class="mid"></i>中频</span><span><i class="high"></i>高频</span></div>
      </div>
      <aside class="meters" aria-label="输入输出电平">
        <div class="meter-titles"><span>输入</span><span>输出</span></div>
        <div class="meter-body"><div class="meter-scale">${[0, -12, -24, -36, -48, -60].map(value => `<span>${value}</span>`).join('')}</div>
          ${[0, 1].map(pair => `<div class="meter-pair">${[0, 1].map(channel => `<div class="meter" role="meter" aria-label="${pair ? '输出' : '输入'}${channel ? '右' : '左'}声道" aria-valuemin="-60" aria-valuemax="0" aria-valuenow="-60"><i class="meter-fill" id="meter-${pair * 2 + channel}"></i><i class="peak-line" id="peak-${pair * 2 + channel}"></i></div>`).join('')}</div>`).join('')}
        </div><div class="meter-peaks"><button id="input-peak" aria-label="清除输入峰值" data-tip="清除输入峰值">−∞</button><button id="output-peak" aria-label="清除输出峰值" data-tip="清除输出峰值">−∞</button></div>
      </aside>
    </section>
    <section class="main-controls" aria-label="空间主参数">
      ${knob('predelay')}${knob('decay', 'large')}${knob('size')}${knob('attack')}${knob('mix', 'large')}
    </section>
    <section class="details" aria-label="声音细节">
      <fieldset class="detail-group damping"><legend>频段阻尼</legend><div>${['bassHz', 'bassMult', 'highHz', 'highDb'].map(id => knob(id, 'small')).join('')}</div></fieldset>
      <fieldset class="detail-group"><legend>扩散</legend><div>${['early', 'late'].map(id => knob(id, 'small')).join('')}</div></fieldset>
      <fieldset class="detail-group"><legend>调制</legend><div>${['rate', 'depth'].map(id => knob(id, 'small')).join('')}</div></fieldset>
      <fieldset class="detail-group"><legend>输出滤波</legend><div>${['lowCut', 'highCut'].map(id => knob(id, 'small')).join('')}</div></fieldset>
    </section>
    <footer class="transport">
      <div class="source-controls">${button('play', '播放', 'play')}
        <label for="source" class="sr-only">试听素材</label><select id="source"><option value="chord">和弦</option><option value="snare">军鼓</option><option value="tone">短音 · 220 赫兹</option><option value="tone-mid">短音 · 880 赫兹</option><option value="tone-high">短音 · 6000 赫兹</option></select>
        ${button('upload', '打开本地音频')}${button('loop', '循环播放', 'active', 'aria-pressed="true"')}
        <input id="file" type="file" accept="audio/*,.wav,.mp3,.m4a,.flac,.aiff" hidden />
      </div><span id="audio-state" class="audio-state">就绪</span>
      <div class="monitor">${button('mute', '静音', '', 'aria-pressed="false"')}<label for="volume">监听</label><input id="volume" type="range" min="0" max="1" step=".01" value=".55" /><output id="volume-value">55%</output></div>
    </footer>
  </article>
  <p id="notification" role="status" hidden></p>
  <dialog id="preset-dialog" aria-labelledby="preset-heading">
    <div class="dialog-heading"><h2 id="preset-heading">预设</h2>${button('close-presets', '关闭预设')}</div>
    <div class="preset-search"><i data-icon="search"></i><input id="preset-search" type="search" placeholder="搜索预设" aria-label="搜索预设" /></div>
    <fieldset class="preset-filters"><legend class="sr-only">预设分类</legend>${['全部', ...families.map(f => f.name), '我的'].map((label, i) => `<label><input type="radio" name="preset-filter" value="${i - 1}" ${i === 0 ? 'checked' : ''} /><span>${label}</span></label>`).join('')}</fieldset>
    <div id="preset-list" class="preset-list"></div>
  </dialog>
  <dialog id="save-dialog" aria-labelledby="save-heading"><form id="save-form">
    <div class="dialog-heading"><h2 id="save-heading">保存预设</h2>${button('close-save', '取消保存', '', 'type="button"')}</div>
    <label for="save-name">名称</label><input id="save-name" required maxlength="60" autocomplete="off" />
    <p id="save-error" role="alert" hidden></p><button type="submit" class="save-submit">保存到我的预设</button>
  </form></dialog>
  <dialog id="settings-dialog" aria-labelledby="settings-heading">
    <div class="dialog-heading"><h2 id="settings-heading">设置</h2>${button('close-settings', '关闭设置')}</div>
    <fieldset class="setting-group"><legend>外观</legend><div class="setting-options theme-options">
      <label><input type="radio" name="appearance" value="light" /><span><i class="theme-swatch light"></i>浅色</span></label>
      <label><input type="radio" name="appearance" value="dark" /><span><i class="theme-swatch dark"></i>深色</span></label>
    </div></fieldset>
    <div class="setting-group visual-setting"><label for="visuals-enabled">声场显示</label><input id="visuals-enabled" type="checkbox" role="switch" checked aria-describedby="visuals-note" /><p id="visuals-note" class="setting-note">关闭动态图像，声音处理与电平表继续运行。</p></div>
    <fieldset class="setting-group"><legend>界面语言</legend><div class="setting-options language-options">
      <label><input type="radio" name="language" value="en" /><span>English</span></label>
      <label><input type="radio" name="language" value="zh" /><span>中文</span></label>
    </div></fieldset>
    <fieldset class="setting-group"><legend>窗口 · 界面缩放</legend><div class="setting-options scale-options">
      ${scales.map(value => `<label><input type="radio" name="scale" value="${value}" aria-label="界面缩放 ${Math.round(value * 100)}%" /><span>${Math.round(value * 100)}%</span></label>`).join('')}
    </div><button id="reset-window" class="setting-link">恢复 100% 大小</button><p id="scale-note" class="setting-note" hidden>增大窗口后可使用更高的缩放档位。</p></fieldset>
    <fieldset class="setting-group tooltip-group"><legend>悬停操作提示</legend><div class="setting-options tooltip-options">
      ${[['auto', '跟随界面'], ['zh', '中文提示'], ['en', '英文提示'], ['off', '关闭提示']].map(([value, label]) => `<label><input type="radio" name="tooltip-language" value="${value}" /><span>${label}</span></label>`).join('')}
    </div></fieldset>
    <div class="setting-links"><button id="open-help">操作帮助<span aria-hidden="true">↗</span></button><button id="open-about">关于 SanMu Audio<span aria-hidden="true">↗</span></button></div>
  </dialog>
  <dialog id="help-dialog" aria-labelledby="help-heading">
    <div class="dialog-heading"><h2 id="help-heading">操作帮助</h2>${button('close-help', '关闭帮助')}</div>
    <dl class="help-list">
      <div><dt>旋钮与数值</dt><dd>上下拖动或滚轮调整。双击旋钮恢复参数默认；点击数值可直接输入，支持秒、毫秒、赫兹等单位。</dd></div>
      <div><dt>键盘与撤销</dt><dd>聚焦旋钮后用方向键调整，Home / End 到达边界，回车编辑数值。⌘ / Ctrl + Z 撤销，加 Shift 重做。</dd></div>
      <div><dt>类型与预设</dt><dd>切换类型、风格会保留参数；加载预设会应用其全部参数。预设列表标明纯湿或插入比例，保存的预设位于“我的”。</dd></div>
      <div><dt>A/B 与 BYP</dt><dd>A/B 保留两组声音设置，复制按钮把当前组复制到另一组。BYP 点亮时旁通混响；再次点击恢复处理。</dd></div>
      <div><dt>观察信号</dt><dd>干显示进入混响器的输入。湿显示经过 MIX 控制的湿信号。混合同时显示两者；MIX 为 100% 时仍可观察输入参考。设置中的声场显示开关只控制画面，声音处理与电平表继续运行。</dd></div>
      <div><dt>试听与电平</dt><dd>停止只停止输入，保留尾音。监听音量不影响信号观察；点击电平峰值可清除保持。窗口、主题与提示设置不改变声音。</dd></div>
    </dl>
  </dialog>
  <dialog id="about-dialog" aria-labelledby="about-heading">
    <div class="dialog-heading"><h2 id="about-heading">SanMu Audio</h2>${button('close-about', '关闭关于')}</div>
    <div class="about-brand"><strong>Reverb</strong></div>
    <p>大厅、板式、房间、回声室。四类空间，八种风格。</p><p class="about-note">界面设计预览 · U12</p>
  </dialog>
  <div id="tooltip" role="tooltip" hidden></div>
`;
// 弹窗与提示位于缩放容器外，保持屏幕坐标、可读尺寸与键盘焦点稳定。
for (const element of document.querySelectorAll('#app > dialog, #tooltip, #notification')) {
  if (element.tagName === 'DIALOG') element.classList.add('viewport-dialog');
  document.body.append(element);
}
const $ = id => document.getElementById(id);
const icon = (id, component) => $(id).replaceChildren(createElement(component, { width: 16, height: 16, 'stroke-width': 1.5, 'aria-hidden': 'true' }));
const rename = (id, name) => { $(id).setAttribute('aria-label', t(name)); $(id).dataset.tip = name; };
for (const [id, component] of [['previous', ChevronLeft], ['next', ChevronRight], ['save', Save], ['undo', Undo2], ['redo', Redo2], ['copy', Copy], ['settings', Settings], ['focus', Maximize2], ['play', Play], ['upload', Upload], ['loop', Repeat2], ['mute', Volume2], ['close-presets', X], ['close-save', X], ['close-settings', X], ['close-help', X], ['close-about', X]]) icon(id, component);
for (const [name, component] of [['down', ChevronDown], ['search', Search]]) document.querySelector(`[data-icon="${name}"]`).replaceChildren(createElement(component, { width: 15, height: 15, 'stroke-width': 1.5 }));

let notificationTimer;
function notify(message) {
  $('notification').textContent = t(message); $('notification').hidden = false;
  clearTimeout(notificationTimer); notificationTimer = setTimeout(() => { $('notification').hidden = true; }, 3500);
}
function begin() {
  if (transaction) return;
  transaction = snapshot();
  if (nativeReady) void nativeSync.command('begin');
}
function end(changes = null) {
  if (transaction && JSON.stringify(transaction) !== JSON.stringify(snapshot())) {
    history.push(transaction); if (history.length > 100) history.shift(); future.length = 0;
  }
  transaction = null; renderState();
  if (nativeReady) {
    if (changes) void nativeSync.patch(changes);
    void nativeSync.command('end');
  }
  persistNativeUi();
}
function restore(state) {
  Object.assign(parameters, state.values); mode = state.mode; bypass = state.bypass;
  presetId = state.presetId; presetName = state.presetName; presetValues = state.presetValues;
  audio?.setParameters(); renderState();
  sendNativeState();
}
function setParameter(id, value) {
  parameters[id] = value; audio?.setParameters();
  if (transaction) {
    knobById.get(id)?.update();
    $('preset-dirty').hidden = presetValues === JSON.stringify(parameters);
    if (id === 'mix') $('mix-badge').textContent = t(parameters.mix === 100 ? '纯湿' : parameters.mix === 0 ? '干声' : '插入');
  } else renderState();
  if (nativeReady) void nativeSync.patch({ values: { [id]: value } });
}
function loadPreset(preset) {
  begin(); Object.assign(parameters, preset.values); mode = preset.mode;
  presetId = preset.id; presetName = preset.name; presetValues = JSON.stringify(parameters);
  audio?.setParameters(); end(snapshot());
}
function changeMode(value) {
  begin(); mode = value; presetId = ''; presetName = families[Math.floor(mode / 2)].modes[mode % 2];
  presetValues = ''; audio?.setParameters(); end({ mode });
}
function renderState() {
  const family = Math.floor(mode / 2);
  document.querySelector('.instrument').dataset.family = family;
  document.querySelector(`input[name="family"][value="${family}"]`).checked = true;
  document.querySelector(`input[name="style"][value="${mode % 2}"]`).checked = true;
  for (let i = 0; i < 2; i++) {
    const name = families[family].modes[i], label = $('style-' + i);
    label.textContent = t(name);
    label.parentElement.dataset.tip = `${name}：选择当前空间的${name}风格`;
  }
  $('preset-name').textContent = displayPresetName();
  $('preset-dirty').hidden = presetValues === JSON.stringify(parameters);
  $('mix-badge').textContent = t(parameters.mix === 100 ? '纯湿' : parameters.mix === 0 ? '干声' : '插入');
  $('undo').disabled = history.length === 0; $('redo').disabled = future.length === 0;
  $('bypass').setAttribute('aria-pressed', String(bypass));
  document.querySelector('.instrument').classList.toggle('bypassed', bypass);
  for (const control of knobs) control.update();
}
const fieldAppearance = () => ({ exposure: 1.4, saturation: 1.4, pearl: .025, surface: theme === 'light' ? 'water' : 'dust' });
function setTheme(value) {
  if (!['dark', 'light'].includes(value)) return;
  theme = value; document.documentElement.dataset.theme = theme;
  scene?.setAppearance(fieldAppearance());
  document.querySelector(`input[name="appearance"][value="${theme}"]`).checked = true;
  try { localStorage.setItem('sanmu-studio-theme', theme); } catch { /* 外观切换不依赖本地存储。 */ }
  for (const control of knobs) control.draw();
}
setTheme(theme);
for (const container of document.querySelectorAll('[data-knob]')) {
  const definition = definitions.find(item => item[0] === container.dataset.knob);
  const control = new Knob(container, definition, () => parameters[definition[0]], setParameter, begin, end);
  knobs.push(control); knobById.set(definition[0], control);
}
const applyTranslations = bindTranslations(document.body);
function displayPresetName() {
  const preset = [...presets, ...users].find(item => item.id === presetId);
  return preset ? presetLabel(preset, language) : t(presetName);
}
function setLanguage(value) {
  if (!['en', 'zh'].includes(value)) return;
  language = value;
  document.documentElement.lang = value === 'zh' ? 'zh-CN' : 'en';
  document.title = value === 'zh' ? '三木 · 混响' : 'SanMu Audio · Reverb';
  applyTranslations(value);
  document.querySelector(`input[name="language"][value="${value}"]`).checked = true;
  for (const element of document.querySelectorAll('.icon-button[data-tip]')) element.setAttribute('aria-label', t(element.dataset.tip));
  for (const control of knobs) control.setLanguage(value);
  scene?.renderer.domElement.setAttribute('aria-label', t('三维生命声场'));
  $('audio-state').textContent = audio?.context ? audioStateLabel() : t('就绪');
  $('status').textContent = t(busy ? '正在载入' : bypass ? '旁通' : playing ? '播放中' : (scene?.life.metrics.wetRms || 0) > .00003 ? '尾音' : '静候声音');
  $('notification').hidden = true;
  hideTip();
  renderState();
  if ($('preset-dialog').open) renderPresets();
  requestAnimationFrame(positionSettings);
  try { localStorage.setItem('sanmu-studio-language', value); } catch { /* 语言切换不依赖持久化。 */ }
}
function audioStateLabel() {
  return `${Math.round(audio.context.sampleRate / 100) / 10} ${language === 'en' ? 'kHz' : '千赫兹'} · ${t('立体声')}`;
}
document.addEventListener('invalid-value', event => notify(event.detail));

let meterCurrent = [0, 0, 0, 0], held = [0, 0, 0, 0], holdUntil = [0, 0, 0, 0], maxPeaks = [0, 0];
const levelDb = level => 20 * Math.log10(Math.max(1e-6, level));
const meterHeight = level => Math.max(0, Math.min(1, (levelDb(level) + 60) / 60));
function paintMeters() {
  for (let i = 0; i < 4; i++) {
    $('meter-' + i).style.clipPath = `inset(${(1 - meterHeight(meterCurrent[i])) * 100}% 0 0 0)`;
    $('meter-' + i).parentElement.setAttribute('aria-valuenow', String(Math.max(-60, Math.min(0, levelDb(meterCurrent[i])))));
    $('peak-' + i).style.bottom = `${meterHeight(held[i]) * 100}%`;
    $('peak-' + i).style.opacity = held[i] > .001 ? '1' : '0';
  }
  for (const [i, id] of ['input-peak', 'output-peak'].entries()) {
    $(id).textContent = maxPeaks[i] < .00001 ? '−∞' : levelDb(maxPeaks[i]).toFixed(1);
    $(id).classList.toggle('clipped', maxPeaks[i] >= 1);
  }
}
function meters(metrics) {
  const now = performance.now();
  for (let i = 0; i < 4; i++) {
    const peak = metrics.peaks?.[i] || 0;
    meterCurrent[i] = Math.max(peak, meterCurrent[i] * Math.exp(-metrics.duration / .25));
    if (peak >= held[i]) { held[i] = peak; holdUntil[i] = now + 1000; }
    else if (now > holdUntil[i]) held[i] *= Math.exp(-metrics.duration / .6);
    maxPeaks[Math.floor(i / 2)] = Math.max(maxPeaks[Math.floor(i / 2)], peak);
  }
  // 原生帧到达时立即反映到辅助功能值和画面，避免 WebView 暂停 RAF 时电平停在 -60 dB。
  paintMeters();
}
let meterTime = 0;
function drawMeters(now) {
  requestAnimationFrame(drawMeters);
  if (now - meterTime < 40) return;
  meterTime = now;
  paintMeters();
}
requestAnimationFrame(drawMeters);
for (const [i, id] of ['input-peak', 'output-peak'].entries()) $(id).onclick = () => { maxPeaks[i] = 0; };

audio = isNative ? new NativeAudio() : new AudioEngine(parameters, metrics => { scene?.ingest(metrics); meters(metrics); }, value => {
  playing = value; icon('play', value ? Square : Play); rename('play', value ? '停止输入，保留尾音' : '播放');
}, notify, { module: '/audio/suite-dsp.wasm', worklet: '/audio/suite-worklet.js', processor: 'sanmu-suite', state: () => ({ mode, bypass }) });
try {
  const visualParameters = new Proxy(parameters, { get: (target, key) => key === 'mix' && bypass ? 0 : target[key] });
  scene = new LifeScene($('stage'), visualParameters, () => audio.context?.currentTime ?? performance.now() / 1000, state => {
    if (state.error !== undefined) { notify(state.error); return; }
    $('status').textContent = t(busy ? '正在载入' : bypass ? '旁通' : playing ? '播放中' : state.wet > .00003 ? '尾音' : '静候声音');
    document.querySelector('.status-dot').classList.toggle('live', playing || state.wet > .00003);
  }, { transparent: true, ...fieldAppearance() });
  scene.renderer.domElement.setAttribute('aria-label', t('三维生命声场'));
} catch { notify('未能启动三维画面，请使用支持图形加速的浏览器。'); }

async function run(operation) {
  if (busy) return;
  busy = true; $('play').disabled = $('upload').disabled = $('source').disabled = true;
  try { await operation(); $('audio-state').textContent = audioStateLabel(); }
  catch (error) { notify(error.message || '载入失败，请重试'); }
  finally { busy = false; $('play').disabled = $('upload').disabled = $('source').disabled = false; }
}
$('play').onclick = () => playing ? audio.stop() : run(() => audio.play());
$('source').onchange = () => { audio.sourceId = $('source').value; if (playing) run(() => audio.play()); };
$('upload').onclick = () => $('file').click();
$('file').onchange = () => {
  const file = $('file').files[0]; if (!file) return;
  run(async () => {
    await audio.loadFile(file); scene?.clear();
    let option = $('source').querySelector('[value="file"]');
    if (!option) { option = new Option('', 'file'); $('source').append(option); }
    option.textContent = file.name; $('source').value = 'file'; await audio.play();
  }); $('file').value = '';
};
$('loop').onclick = () => { audio.setLoop(!audio.loop); $('loop').setAttribute('aria-pressed', String(audio.loop)); $('loop').classList.toggle('active', audio.loop); };
$('mute').onclick = () => {
  audio.muted = !audio.muted; audio.setVolume(); $('mute').setAttribute('aria-pressed', String(audio.muted));
  icon('mute', audio.muted ? VolumeX : Volume2); rename('mute', audio.muted ? '取消静音' : '静音');
};
$('volume').oninput = () => { audio.setVolume(Number($('volume').value)); $('volume-value').textContent = `${Math.round(audio.volume * 100)}%`; };
$('bypass').onclick = () => { begin(); bypass = !bypass; audio.setParameters(); end({ bypass }); };
$('focus').onclick = () => {
  const focused = document.querySelector('.instrument').classList.toggle('focused');
  icon('focus', focused ? Minimize2 : Maximize2); rename('focus', focused ? '返回完整界面' : '专注声场');
};
document.querySelectorAll('[name="family"]').forEach(input => input.onchange = () => changeMode(Number(input.value) * 2));
document.querySelectorAll('[name="style"]').forEach(input => input.onchange = () => changeMode(Math.floor(mode / 2) * 2 + Number(input.value)));
function setSignalView(value) {
  if (!['dry', 'wet', 'overlay'].includes(value)) return;
  scene?.setView(value);
  document.querySelector(`[name="view"][value="${value}"]`).checked = true;
  $('input-legend').hidden = value === 'wet'; $('wet-legend').hidden = value === 'dry';
}
document.querySelectorAll('[name="view"]').forEach(input => input.onchange = () => setSignalView(input.value));
function setVisualsEnabled(value) {
  visualsEnabled = value === true;
  scene?.setEnabled(visualsEnabled);
  $('stage').classList.toggle('visuals-off', !visualsEnabled);
  $('visuals-enabled').checked = visualsEnabled;
  if (!visualsEnabled && document.querySelector('.instrument').classList.contains('focused')) $('focus').click();
  try { localStorage.setItem('sanmu-studio-visuals', visualsEnabled ? 'on' : 'off'); } catch { /* 显示开关不依赖本地存储。 */ }
}
$('visuals-enabled').onchange = () => setVisualsEnabled($('visuals-enabled').checked);
setVisualsEnabled(visualsEnabled);
$('undo').onclick = () => { if (history.length) { future.push(snapshot()); restore(history.pop()); } };
$('redo').onclick = () => { if (future.length) { history.push(snapshot()); restore(future.pop()); } };
document.querySelectorAll('[name="compare"]').forEach(input => input.onchange = () => {
  slots[slot] = snapshot(); slot = input.value; restore(slots[slot]); history.length = future.length = 0;
  rename('copy', `复制到 ${slot === 'A' ? 'B' : 'A'}`); renderState();
});
$('copy').onclick = () => { const destination = slot === 'A' ? 'B' : 'A'; slots[destination] = snapshot(); notify(`已复制到 ${destination}`); };
const allPresets = () => [...presets, ...users];
for (const [id, offset] of [['previous', -1], ['next', 1]]) $(id).onclick = () => {
  const list = allPresets(); const index = list.findIndex(p => p.id === presetId);
  loadPreset(list[(Math.max(0, index) + offset + list.length) % list.length]);
};
function renderPresets() {
  const filter = Number(document.querySelector('[name="preset-filter"]:checked').value);
  const search = $('preset-search').value.trim().toLocaleLowerCase();
  const list = allPresets().filter(p => (filter === -1 || (filter === 4 ? p.user : Math.floor(p.mode / 2) === filter)) && [p.name, presetLabel(p, 'en')].some(name => name.toLocaleLowerCase().includes(search)));
  $('preset-list').replaceChildren();
  for (const preset of list) {
    const item = document.createElement('button'); item.className = 'preset-item';
    const title = document.createElement('span'); title.textContent = presetLabel(preset, language);
    const detail = document.createElement('span'); detail.className = 'preset-meta';
    detail.textContent = `${preset.values.decay.toFixed(2)} ${t('秒')} · ${preset.values.mix === 100 ? t('纯湿') : `${t('插入')} ${preset.values.mix}%`}`;
    item.append(title, detail);
    item.setAttribute('aria-label', presetLabel(preset, language)); item.setAttribute('aria-current', String(preset.id === presetId));
    if (preset.id === presetId) item.append(createElement(Check, { width: 14, height: 14 }));
    item.onclick = () => { loadPreset(preset); $('preset-dialog').close(); };
    $('preset-list').append(item);
  }
  if (!list.length) { const empty = document.createElement('p'); empty.className = 'empty'; empty.textContent = t('没有匹配的预设'); $('preset-list').append(empty); }
}
$('preset-open').onclick = () => { renderPresets(); $('preset-dialog').showModal(); $('preset-search').focus(); };
$('close-presets').onclick = () => $('preset-dialog').close();
$('preset-search').oninput = renderPresets;
document.querySelectorAll('[name="preset-filter"]').forEach(input => input.onchange = renderPresets);
$('save').onclick = () => { $('save-name').value = displayPresetName(); $('save-error').hidden = true; $('save-dialog').showModal(); $('save-name').select(); };
$('close-save').onclick = () => $('save-dialog').close();
$('save-form').onsubmit = async event => {
  event.preventDefault(); const name = $('save-name').value.trim();
  if (!name) { $('save-name').focus(); return; }
  if (isNative) {
    try {
      await nativeSync.tail;
      const result = await callNative('savePreset', { name });
      updateNativePresets(result.presets);
      presetId = result.id; presetName = result.name; presetValues = JSON.stringify(parameters);
      $('save-dialog').close(); renderState(); persistNativeUi(); notify('预设已保存');
    } catch (error) { $('save-error').textContent = error.message; $('save-error').hidden = false; }
    return;
  }
  const preset = { id: crypto.randomUUID(), name, mode, values: { ...parameters }, user: true };
  try {
    const next = [...users, preset]; if (next.length > 100) throw new Error('我的预设已达到 100 个');
    localStorage.setItem('sanmu-studio-presets', JSON.stringify(next)); users = next;
    presetId = preset.id; presetName = name; presetValues = JSON.stringify(parameters);
    $('save-dialog').close(); renderState(); notify('已保存到此浏览器的我的预设');
  } catch (error) { $('save-error').textContent = t(error.message || '保存失败'); $('save-error').hidden = false; }
};
for (const dialog of document.querySelectorAll('dialog')) dialog.addEventListener('click', event => {
  if (event.target !== dialog) return;
  const box = dialog.getBoundingClientRect();
  if (event.clientX < box.left || event.clientX > box.right || event.clientY < box.top || event.clientY > box.bottom) dialog.close();
});

function positionSettings() {
  const dialog = $('settings-dialog'); if (!dialog.open) return;
  // 弹窗位于未缩放的 body 下，以下尺寸和定位值统一使用视口实际像素。
  const anchor = $('settings').getBoundingClientRect(), box = dialog.getBoundingClientRect();
  dialog.style.left = `${Math.max(8, Math.min(innerWidth - box.width - 8, anchor.right - box.width))}px`;
  dialog.style.top = `${Math.max(8, Math.min(innerHeight - box.height - 8, anchor.bottom + 12))}px`;
}
function setUiScale(value) {
  if (!scales.includes(value)) return;
  const maximum = Math.max(1, innerWidth / 320);
  uiScale = Math.max(...scales.filter(scale => scale <= value && scale <= maximum));
  $('app').style.zoom = String(uiScale);
  for (const input of document.querySelectorAll('input[name="scale"]')) {
    input.checked = Number(input.value) === uiScale;
    input.disabled = Number(input.value) > maximum;
  }
  $('scale-note').hidden = maximum >= scales.at(-1);
  try { localStorage.setItem('sanmu-studio-scale', String(uiScale)); } catch { /* 窗口缩放不依赖持久化。 */ }
  requestAnimationFrame(() => {
    // CSS 缩放不一定触发旧 ResizeObserver，主动重建按实际像素尺寸绘制的画布。
    for (const control of knobs) control.draw();
    scene?.resize();
    positionSettings();
  });
}
$('settings').onclick = () => { hideTip(); $('settings-dialog').showModal(); positionSettings(); };
for (const id of ['settings', 'help', 'about']) $(`close-${id}`).onclick = () => $(`${id}-dialog`).close();
for (const input of document.querySelectorAll('input[name="appearance"]')) input.onchange = () => setTheme(input.value);
for (const input of document.querySelectorAll('input[name="language"]')) input.onchange = () => { hideTip(); setLanguage(input.value); };
for (const input of document.querySelectorAll('input[name="scale"]')) input.onchange = () => setUiScale(Number(input.value));
$('reset-window').onclick = () => setUiScale(1);
for (const id of ['help', 'about']) $(`open-${id}`).onclick = () => { $(`${id}-dialog`).showModal(); };
window.addEventListener('resize', () => setUiScale(uiScale));
setUiScale(uiScale);
document.querySelector(`input[name="tooltip-language"][value="${tooltipLanguage}"]`).checked = true;
for (const input of document.querySelectorAll('input[name="tooltip-language"]')) input.onchange = () => {
  tooltipLanguage = input.value; hideTip();
  try { localStorage.setItem('sanmu-studio-tooltip-language', tooltipLanguage); } catch { /* 提示语言立即生效。 */ }
};

let tipTimer;
function hideTip() { clearTimeout(tipTimer); $('tooltip').hidden = true; }
document.addEventListener('pointerover', event => {
  const target = event.target.closest('[data-tip]'); if (tooltipLanguage === 'off' || !target || target.disabled) return;
  hideTip(); tipTimer = setTimeout(() => {
    const tip = $('tooltip'), box = target.getBoundingClientRect();
    tip.textContent = translate(target.dataset.tip, tooltipLanguage === 'auto' ? language : tooltipLanguage); tip.hidden = false;
    tip.style.left = `${Math.max(8, Math.min(innerWidth - tip.offsetWidth - 8, box.left + box.width / 2 - tip.offsetWidth / 2))}px`;
    tip.style.top = `${Math.max(8, box.bottom + tip.offsetHeight + 12 > innerHeight ? box.top - tip.offsetHeight - 8 : box.bottom + 8)}px`;
  }, 450);
});
document.addEventListener('pointerout', hideTip); document.addEventListener('pointerdown', hideTip);
document.addEventListener('keydown', event => {
  if (event.key === 'Escape') hideTip();
  if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === 'z' && !['INPUT', 'TEXTAREA'].includes(event.target.tagName) && !document.querySelector('dialog[open]')) {
    event.preventDefault(); $(event.shiftKey ? 'redo' : 'undo').click();
  }
});
setLanguage(language);
window.__sanmuStudio = { audio, scene, parameters, snapshot, loadPreset, presets, setTheme, setUiScale, setLanguage };

function updateNativePresets(items) {
  if (!Array.isArray(items)) return;
  const valid = items.filter(validatePreset);
  presets.splice(0, presets.length, ...valid.filter(item => !item.user));
  users = valid.filter(item => item.user);
}
function sameValues(a, b) {
  return a && b && definitions.every(([id]) => Number.isFinite(a[id]) && Number.isFinite(b[id]) && Math.abs(a[id] - b[id]) < .0002);
}
function applyHostState(state) {
  if (!state?.values || !Number.isInteger(state.mode) || state.mode < 0 || state.mode > 7) return;
  if (sameValues(parameters, state.values) && mode === state.mode && bypass === !!state.bypass) return;
  Object.assign(parameters, state.values); mode = state.mode; bypass = !!state.bypass;
  const preset = allPresets().find(item => item.mode === mode && sameValues(item.values, parameters));
  if (preset) { presetId = preset.id; presetName = preset.name; presetValues = JSON.stringify(parameters); }
  else { presetId = ''; presetName = families[Math.floor(mode / 2)].modes[mode % 2]; presetValues = ''; }
  renderState();
}
function sendNativeState() {
  if (!nativeReady) return Promise.resolve();
  return nativeSync.command('apply', snapshot());
}
function persistNativeUi() {
  if (!nativeReady) return;
  const state = { snapshot: snapshot(), slots, slot, history, future,
    theme, language, tooltipLanguage, uiScale, visualsEnabled, view: scene?.view || 'overlay' };
  void callNative('ui', state).catch(error => notify(error.message));
}
async function initializeNative() {
  try {
    const initial = await callNative('initialize');
    updateNativePresets(initial.presets);
    const ui = initial.ui || {};
    const validSnapshot = value => value && validatePreset({ ...value, name: value.presetName || '预设', id: value.presetId || 'current' }) && typeof value.bypass === 'boolean';
    if (validSnapshot(ui.snapshot)) {
      presetId = ui.snapshot.presetId; presetName = ui.snapshot.presetName; presetValues = ui.snapshot.presetValues;
    }
    applyHostState(initial);
    for (const name of ['A', 'B']) slots[name] = validSnapshot(ui.slots?.[name]) ? ui.slots[name] : snapshot();
    slot = ['A', 'B'].includes(ui.slot) ? ui.slot : 'A';
    document.querySelector(`input[name="compare"][value="${slot}"]`).checked = true;
    if (Array.isArray(ui.history)) history.push(...ui.history.filter(validSnapshot).slice(-100));
    if (Array.isArray(ui.future)) future.push(...ui.future.filter(validSnapshot).slice(-100));
    setTheme(['light', 'dark'].includes(ui.theme) ? ui.theme : 'light');
    setVisualsEnabled(typeof ui.visualsEnabled === 'boolean' ? ui.visualsEnabled : true);
    if (['auto', 'zh', 'en', 'off'].includes(ui.tooltipLanguage)) tooltipLanguage = ui.tooltipLanguage;
    document.querySelector(`input[name="tooltip-language"][value="${tooltipLanguage}"]`).checked = true;
    setLanguage(['zh', 'en'].includes(ui.language) ? ui.language : language);
    if (scales.includes(ui.uiScale)) setUiScale(ui.uiScale);
    if (['dry', 'wet', 'overlay'].includes(ui.view)) {
      setSignalView(ui.view);
    }
    $('audio-state').textContent = `${initial.sampleRate / 1000} kHz · ${initial.channels === 1 ? 'Mono' : 'Stereo'} · Pro Tools / DAW`;
    document.querySelector('.about-note').textContent = `AAX · AU · VST3 · ${initial.build}`;
    nativeReady = true;
    document.documentElement.classList.remove('native-loading');
    renderState(); persistNativeUi();
    window.__sanmuStudio.nativeReady = true;
  } catch (error) {
    document.documentElement.classList.remove('native-loading'); notify(error.message);
  }
}
if (isNative) {
  document.querySelector('.transport').replaceChildren($('audio-state'));
  let lastSignal = performance.now();
  onNativeFrame(state => {
    if (!nativeReady) return;
    if (!nativeSync.pending && !transaction) applyHostState(state);
    audio.context.sampleRate = state.sampleRate;
    $('audio-state').textContent = `${state.sampleRate / 1000} kHz · ${state.channels === 1 ? 'Mono' : 'Stereo'} · DAW`;
    const now = performance.now() / 1000;
    const frames = state.frames || [];
    if (frames.length) {
      let remaining = frames.reduce((total, frame) => total + frame.duration, 0);
      for (const frame of frames) {
        remaining -= frame.duration; frame.time = now - remaining;
        audio.latest = frame; scene?.ingest(frame); meters(frame);
      }
      playing = frames.some(frame => frame.dryRms > .00003); lastSignal = performance.now();
    } else if (performance.now() - lastSignal > 80) {
      playing = false;
      const silent = { time: now, duration: 1 / 30, dry: [0,0,0,0], wet: [0,0,0,0], dryRms: 0, wetRms: 0, peak: 0, peaks: [0,0,0,0], pan: 0, wetPan: 0, correlation: 0 };
      scene?.ingest(silent); meters(silent);
    }
  });
  for (const type of ['change', 'click']) document.addEventListener(type, () => queueMicrotask(persistNativeUi));
  void initializeNative();
}
