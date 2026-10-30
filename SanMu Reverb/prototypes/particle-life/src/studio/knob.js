import { normalizedValue, parameterValue } from '../parameters.js';
import { translate, parameterTips } from './i18n.js';

export class Knob {
  constructor(container, definition, getValue, change, begin, end) {
    this.definition = definition;
    this.language = 'zh';
    this.getValue = getValue;
    const [id, name, min, max, step, initial, unit] = definition;
    container.innerHTML = `<label class="knob-label" for="edit-${id}">${name}</label>
      <div class="dial" role="slider" tabindex="0" aria-label="${name}" aria-valuemin="${min}" aria-valuemax="${max}" id="knob-${id}">
        <canvas aria-hidden="true"></canvas>
      </div><div class="knob-readout"><input id="edit-${id}" aria-label="${name}数值" inputmode="decimal" spellcheck="false" autocomplete="off" /><span>${unit}</span></div>`;
    this.container = container;
    this.dial = container.querySelector('.dial');
    this.canvas = container.querySelector('canvas');
    this.input = container.querySelector('input');
    const clamp = value => Math.min(max, Math.max(min, value));
    const set = value => change(id, clamp(value));
    const reset = () => { begin(); set(initial); end(); };
    this.dial.addEventListener('dblclick', reset);
    this.dial.addEventListener('pointerdown', event => {
      if (event.button !== 0) return;
      event.preventDefault(); this.dial.focus();
      if (event.metaKey || event.ctrlKey) { reset(); return; }
      begin();
      this.pointer = event.pointerId;
      this.lastY = event.clientY;
      this.dragValue = normalizedValue(definition, getValue());
      this.dial.setPointerCapture(event.pointerId);
      container.classList.add('dragging');
    });
    this.dial.addEventListener('pointermove', event => {
      if (this.pointer !== event.pointerId) return;
      const delta = (this.lastY - event.clientY) / 240;
      this.lastY = event.clientY;
      // 在边界处钳位累计量，反向拖动会立即响应。
      this.dragValue = Math.max(0, Math.min(1, this.dragValue + delta));
      set(parameterValue(definition, this.dragValue));
    });
    const finish = event => {
      if (this.pointer !== event.pointerId) return;
      this.pointer = null; container.classList.remove('dragging'); end();
    };
    this.dial.addEventListener('pointerup', finish);
    this.dial.addEventListener('pointercancel', finish);
    this.dial.addEventListener('lostpointercapture', finish);
    this.dial.addEventListener('wheel', event => {
      event.preventDefault(); begin();
      const direction = -Math.sign(event.deltaY || event.deltaX);
      const value = normalizedValue(definition, getValue()) + direction * .01;
      set(parameterValue(definition, Math.min(1, Math.max(0, value)))); end();
    }, { passive: false });
    this.dial.addEventListener('keydown', event => {
      if (!['ArrowUp', 'ArrowRight', 'ArrowDown', 'ArrowLeft', 'Home', 'End', 'Enter'].includes(event.key)) return;
      event.preventDefault();
      if (event.key === 'Enter') { this.input.focus(); this.input.select(); return; }
      begin();
      if (event.key === 'Home') set(min);
      else if (event.key === 'End') set(max);
      else {
        const direction = ['ArrowUp', 'ArrowRight'].includes(event.key) ? 1 : -1;
        set(getValue() + direction * step * 10);
      }
      end();
    });
    this.input.addEventListener('focus', () => { this.editStart = getValue(); this.input.select(); });
    const commit = () => {
      const match = this.input.value.trim().match(/^([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*(千赫兹|千赫|kHz|赫兹|Hz|毫秒|ms|秒|s|分贝|dB|倍|%|X|×)?$/i);
      const suffix = match?.[2]?.toLowerCase();
      let value = match ? Number(match[1]) : NaN;
      const frequencies = ['赫兹', 'hz', '千赫兹', '千赫', 'khz'];
      if (suffix && !([unit.toLowerCase(), ...(unit === '赫兹' ? frequencies : unit === '秒' ? ['s', 'ms', '毫秒'] : unit === '毫秒' ? ['ms', 's', '秒'] : unit === '分贝' ? ['db'] : unit === '倍' ? ['x', '×'] : [])].includes(suffix))) value = NaN;
      if (['khz', '千赫兹', '千赫'].includes(suffix)) value *= 1000;
      if (unit === '毫秒' && ['s', '秒'].includes(suffix)) value *= 1000;
      if (unit === '秒' && ['ms', '毫秒'].includes(suffix)) value /= 1000;
      if (Number.isFinite(value)) { begin(); set(Math.round(clamp(value) / step) * step); end(); }
      else container.dispatchEvent(new CustomEvent('invalid-value', { bubbles: true, detail: `${name}需要有效数值` }));
      this.input.blur(); this.update();
    };
    this.input.addEventListener('change', commit);
    this.input.addEventListener('keydown', event => {
      if (event.key === 'Enter') { event.preventDefault(); commit(); }
      if (event.key === 'Escape') { this.input.value = String(this.editStart); this.input.blur(); this.update(); }
    });
    this.observer = new ResizeObserver(() => this.draw());
    this.observer.observe(this.dial);
    this.update();
  }
  setLanguage(language) {
    this.language = language;
    const [, name, , , , , unit] = this.definition;
    this.container.querySelector('.knob-label').textContent = translate(name, language);
    this.container.querySelector('.knob-readout span').textContent = translate(unit, language);
    this.dial.setAttribute('aria-label', translate(name, language));
    this.dial.dataset.tip = parameterTips[name] ? `${name}：${parameterTips[name]}` : name;
    this.input.setAttribute('aria-label', translate(`${name}数值`, language));
    this.update();
  }
  update() {
    const value = this.getValue(), definition = this.definition;
    const digits = definition[4] < .1 ? 2 : definition[4] < 1 ? 1 : 0;
    if (document.activeElement !== this.input) this.input.value = value.toFixed(digits);
    this.dial.setAttribute('aria-valuenow', String(value));
    this.dial.setAttribute('aria-valuetext', `${value.toFixed(digits)} ${translate(definition[6], this.language)}`);
    this.draw();
  }
  draw() {
    const size = this.dial.clientWidth;
    if (!size) return;
    // 放大时按屏幕上的实际尺寸分配像素，避免只拉伸原来 100% 的旋钮位图。
    const displaySize = this.dial.getBoundingClientRect().width || size;
    const backingSize = Math.max(1, Math.round(displaySize * Math.min(2, devicePixelRatio || 1)));
    const ratio = backingSize / size;
    if (this.canvas.width !== backingSize || this.canvas.height !== backingSize) {
      this.canvas.width = backingSize; this.canvas.height = backingSize;
    }
    const context = this.canvas.getContext('2d');
    context.setTransform(ratio, 0, 0, ratio, 0, 0);
    context.clearRect(0, 0, size, size);
    const style = getComputedStyle(this.container);
    const colour = name => style.getPropertyValue(name).trim();
    const centre = size / 2, radius = size * .335, ring = size * .43;
    const start = Math.PI * .75, end = Math.PI * 2.25;
    const angle = start + normalizedValue(this.definition, this.getValue()) * (end - start);
    context.lineCap = 'round';
    const arc = (from, to, stroke, width) => {
      context.beginPath(); context.arc(centre, centre, ring, from, to);
      context.strokeStyle = stroke; context.lineWidth = width; context.stroke();
    };
    // 盘面保持中性材质；外圈与指针使用当前类型的旋钮色，不直接沿用胶囊颜色。
    arc(start, end, colour('--knob-track') || colour('--track'), 1.5);
    if (angle > start + .001) arc(start, angle, colour('--knob-accent') || colour('--accent'), 1.6);
    context.strokeStyle = colour('--knob-tick') || colour('--tick'); context.lineWidth = 1;
    for (let i = 0; i <= 8; i++) {
      const a = start + i / 8 * (end - start);
      context.beginPath();
      context.moveTo(centre + Math.cos(a) * (ring + 4), centre + Math.sin(a) * (ring + 4));
      context.lineTo(centre + Math.cos(a) * (ring + (i % 4 === 0 ? 7 : 5.5)), centre + Math.sin(a) * (ring + (i % 4 === 0 ? 7 : 5.5)));
      context.stroke();
    }
    context.shadowColor = colour('--knob-shadow'); context.shadowBlur = 5; context.shadowOffsetY = 2;
    const gradient = context.createLinearGradient(0, centre - radius, size, centre + radius);
    gradient.addColorStop(0, colour('--knob-top')); gradient.addColorStop(1, colour('--knob-bottom'));
    context.beginPath(); context.arc(centre, centre, radius, 0, Math.PI * 2);
    context.fillStyle = gradient; context.fill();
    context.shadowColor = 'transparent'; context.shadowBlur = 0; context.shadowOffsetY = 0;
    context.strokeStyle = colour('--knob-edge'); context.lineWidth = .8; context.stroke();
    // 固定的材质短弧，没有最佳参数范围含义；浅、深主题分别定义可见的弧线颜色。
    context.beginPath(); context.arc(centre, centre, radius - 2.5, Math.PI * 1.1, Math.PI * 1.8);
    context.strokeStyle = colour('--knob-shine'); context.lineWidth = .7; context.stroke();
    context.beginPath();
    context.moveTo(centre + Math.cos(angle) * radius * .52, centre + Math.sin(angle) * radius * .52);
    context.lineTo(centre + Math.cos(angle) * radius * .86, centre + Math.sin(angle) * radius * .86);
    context.strokeStyle = colour('--knob-pointer') || colour('--pointer'); context.lineWidth = size > 90 ? 2.5 : 2; context.stroke();
  }
}
