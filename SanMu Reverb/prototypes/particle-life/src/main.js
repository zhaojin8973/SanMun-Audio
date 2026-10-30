import { createElement, Play, Square, Upload, Repeat2, Volume2, VolumeX, RotateCcw, Maximize2, Minimize2, LoaderCircle } from 'lucide';
import { parameterGroups, parameterList, defaults, normalizedValue, parameterValue, formatValue } from './parameters.js';
import { AudioEngine } from './audio.js';
import { LifeScene } from './scene.js';
import './style.css';

const parameters = { ...defaults };
const app = document.querySelector('#app');
app.innerHTML = `
  <header class="masthead">
    <h1>三木<span class="slash">/</span><span class="title-secondary">生命声场</span></h1>
    <div class="scene-name">音乐厅<span class="mode-dot"></span><span>01</span></div>
    <button id="focus" class="icon-button" aria-label="收起参数" data-tooltip="收起参数" aria-pressed="false"></button>
  </header>
  <section id="stage" class="stage" aria-label="生命声场">
    <div class="stage-status"><span id="status-dot" class="status-dot"></span><span id="status">静候声音</span></div>
    <fieldset class="view-switch" aria-describedby="view-note">
      <legend class="sr-only">观察信号</legend>
      <span class="view-label" aria-hidden="true">画面</span>
      <div class="view-options">
        <label><input type="radio" name="signal-view" value="dry" /><span>原声</span></label>
        <label><input type="radio" name="signal-view" value="wet" /><span>混响</span></label>
        <label><input type="radio" name="signal-view" value="overlay" checked /><span>叠加</span></label>
      </div>
      <span id="view-note" title="原声显示输入参考；混响和叠加中的尘埃均跟随 MIX。切换视图不改变试听声音。">仅切换画面</span>
    </fieldset>
    <p id="error" class="error" role="alert" hidden></p>
    <div class="layer-key" aria-live="polite">
      <span id="dry-key"><i class="thread-mark"></i>中央细丝 · 输入参考</span>
      <span id="wet-key"><i class="dust-mark"></i><span id="wet-key-label">混响 · 跟随 MIX</span></span>
    </div>
    <div class="spectrum-key" aria-label="频段色谱">
      <span><i class="swatch low"></i>低频</span><span><i class="swatch mid"></i>中频</span><span><i class="swatch high-mid"></i>中高频</span><span><i class="swatch high"></i>高频</span>
    </div>
  </section>
  <section class="workbench" aria-label="试听与参数">
    <div class="transport">
      <div class="source-controls">
        <button id="play" class="icon-button primary" aria-label="播放" data-tooltip="播放"></button>
        <label class="sr-only" for="source">试听素材</label>
        <select id="source"><option value="chord">和弦</option><option value="snare">鼓击</option><option value="tone">短音 · 220 赫兹</option><option value="tone-mid">短音 · 880 赫兹</option><option value="tone-high">短音 · 6000 赫兹</option></select>
        <button id="upload" class="icon-button" aria-label="打开本地音频" data-tooltip="打开本地音频"></button>
        <input id="file" type="file" accept="audio/*,.wav,.mp3,.m4a,.flac,.aiff" hidden />
        <button id="loop" class="icon-button active" aria-label="循环播放" data-tooltip="循环播放" aria-pressed="true"></button>
      </div>
      <div class="output-controls">
        <button id="mute" class="icon-button" aria-label="静音" data-tooltip="静音" aria-pressed="false"></button>
        <label class="sr-only" for="volume">试听音量</label>
        <input id="volume" type="range" min="0" max="1" step=".01" value=".55" />
        <output id="volume-value" for="volume">55%</output>
      </div>
    </div>
    <div id="parameters" class="parameters">
      <div class="parameter-navigation">
        <div role="tablist" aria-label="参数分组">${parameterGroups.map((group, i) => `<button id="tab-${group.id}" role="tab" aria-controls="panel-${group.id}" aria-selected="${i === 0}" tabindex="${i === 0 ? 0 : -1}" data-group="${group.id}">${group.name}</button>`).join('')}</div>
        <button id="reset" class="icon-button small" aria-label="恢复默认参数" data-tooltip="恢复默认参数"></button>
      </div>
      ${parameterGroups.map((group, groupIndex) => `<div id="panel-${group.id}" role="tabpanel" aria-labelledby="tab-${group.id}" class="parameter-grid" ${groupIndex ? 'hidden' : ''}>${group.items.map(item => `
        <div class="parameter">
          <div class="parameter-label"><label for="param-${item[0]}">${item[1]}</label><output id="value-${item[0]}" for="param-${item[0]}">${formatValue(item, item[5])}</output></div>
          <input id="param-${item[0]}" type="range" min="0" max="1" step=".0001" value="${normalizedValue(item, item[5])}" aria-valuetext="${formatValue(item, item[5])}" />
        </div>`).join('')}</div>`).join('')}
    </div>
  </section>
`;

const $ = id => document.getElementById(id);
function icon(id, component) { $(id).replaceChildren(createElement(component, { width: 18, height: 18, 'stroke-width': 1.6, 'aria-hidden': 'true' })); }
function name(id, text) { $(id).setAttribute('aria-label', text); $(id).dataset.tooltip = text; }
for (const [id, component] of [['play', Play], ['upload', Upload], ['loop', Repeat2], ['mute', Volume2], ['reset', RotateCcw], ['focus', Maximize2]]) icon(id, component);
let playing = false, busy = false, scene;
function error(text) { $('error').textContent = text; $('error').hidden = !text; }
const audio = new AudioEngine(parameters, metrics => scene?.ingest(metrics), value => {
  playing = value;
  icon('play', value ? Square : Play);
  name('play', value ? '停止输入，保留尾音' : '播放');
}, error);
try {
  scene = new LifeScene($('stage'), parameters, () => audio.context?.currentTime ?? performance.now() / 1000, state => {
    if (state.error !== undefined) { error(state.error); return; }
    const tail = state.wet > .00003;
    $('status').textContent = busy ? '正在载入' : playing ? '播放中' : tail ? '尾音' : '静候声音';
    $('status-dot').classList.toggle('live', playing || tail);
  });
} catch { error('当前浏览器未能启动三维画面，请使用支持图形加速的浏览器。'); }

for (const input of document.querySelectorAll('input[name="signal-view"]')) {
  input.addEventListener('change', () => {
    if (!input.checked) return;
    scene?.setView(input.value);
    $('dry-key').hidden = input.value === 'wet';
    $('wet-key').hidden = input.value === 'dry';
  });
}

async function run(operation) {
  if (busy) return;
  busy = true; error('');
  $('play').disabled = $('upload').disabled = $('source').disabled = true;
  icon('play', LoaderCircle);
  $('play').classList.add('loading');
  try { await operation(); }
  catch (failure) { error(failure.message || '未能载入，请重试。'); }
  finally {
    busy = false;
    $('play').disabled = $('upload').disabled = $('source').disabled = false;
    $('play').classList.remove('loading');
    icon('play', playing ? Square : Play);
    name('play', playing ? '停止输入，保留尾音' : '播放');
  }
}
$('play').addEventListener('click', () => playing ? audio.stop() : run(() => audio.play()));
$('source').addEventListener('change', () => run(async () => {
  const restart = playing;
  audio.stop(); audio.clear(); scene?.clear();
  audio.sourceId = $('source').value;
  if (restart) await audio.play();
}));
$('loop').addEventListener('click', () => {
  audio.setLoop(!audio.loop);
  $('loop').classList.toggle('active', audio.loop);
  $('loop').setAttribute('aria-pressed', String(audio.loop));
});
$('mute').addEventListener('click', () => {
  audio.muted = !audio.muted; audio.setVolume();
  icon('mute', audio.muted ? VolumeX : Volume2);
  name('mute', audio.muted ? '取消静音' : '静音');
  $('mute').setAttribute('aria-pressed', String(audio.muted));
});
$('volume').addEventListener('input', () => {
  audio.setVolume(Number($('volume').value));
  $('volume-value').value = `${Math.round(audio.volume * 100)}%`;
});
$('upload').addEventListener('click', () => $('file').click());
$('file').addEventListener('change', () => {
  const file = $('file').files[0];
  if (!file) return;
  run(async () => {
    await audio.loadFile(file);
    scene?.clear();
    let option = $('source').querySelector('option[value="file"]');
    if (!option) { option = document.createElement('option'); option.value = 'file'; $('source').append(option); }
    option.textContent = file.name;
    $('source').value = 'file';
    $('source').title = file.name;
    await audio.play();
  });
  $('file').value = '';
});

function syncParameter(item) {
  const id = item[0], value = parameters[id];
  $('param-' + id).value = normalizedValue(item, value);
  $('param-' + id).style.setProperty('--fill', `${normalizedValue(item, value) * 100}%`);
  $('param-' + id).setAttribute('aria-valuetext', formatValue(item, value));
  $('value-' + id).value = formatValue(item, value);
}
for (const item of parameterList) {
  syncParameter(item);
  $('param-' + item[0]).addEventListener('input', event => {
    parameters[item[0]] = parameterValue(item, Number(event.target.value));
    syncParameter(item);
    audio.setParameters();
  });
}
$('reset').addEventListener('click', () => {
  Object.assign(parameters, defaults);
  parameterList.forEach(syncParameter);
  audio.setParameters();
});
function selectGroup(id) {
  for (const group of parameterGroups) {
    const selected = group.id === id;
    $('tab-' + group.id).setAttribute('aria-selected', String(selected));
    $('tab-' + group.id).tabIndex = selected ? 0 : -1;
    $('panel-' + group.id).hidden = !selected;
  }
}
for (const [i, group] of parameterGroups.entries()) {
  $('tab-' + group.id).addEventListener('click', () => selectGroup(group.id));
  $('tab-' + group.id).addEventListener('keydown', event => {
    if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
    event.preventDefault();
    const next = event.key === 'Home' ? 0 : event.key === 'End' ? 2 : (i + (event.key === 'ArrowRight' ? 1 : 2)) % 3;
    selectGroup(parameterGroups[next].id);
    $('tab-' + parameterGroups[next].id).focus();
  });
}
$('focus').addEventListener('click', () => {
  const hidden = !$('parameters').hidden;
  $('parameters').hidden = hidden;
  app.classList.toggle('focused', hidden);
  icon('focus', hidden ? Minimize2 : Maximize2);
  name('focus', hidden ? '显示参数' : '收起参数');
  $('focus').setAttribute('aria-pressed', String(hidden));
});

// 只在开发服务器暴露检查入口，正式构建不携带调试对象。
if (import.meta.env.DEV) window.__sanmu = { audio, scene, parameters, set(name, value) {
  const item = parameterList.find(item => item[0] === name);
  if (!item) return;
  parameters[name] = Math.max(item[2], Math.min(item[3], value));
  syncParameter(item); audio.setParameters();
} };
