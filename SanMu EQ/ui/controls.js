import { drawKnob } from './shared/draw-knob.js';
export const clamp = (v, min = 0, max = 1) => Math.max(min, Math.min(max, v));
export const normal = (d, v) => Math.pow(clamp((v - d.min) / (d.max - d.min)), d.skew || 1);
export const denormal = (d, n) => {
  const v = d.min + (d.max - d.min) * Math.pow(clamp(n), 1 / (d.skew || 1));
  return clamp(d.step ? d.min + Math.round((v - d.min) / d.step) * d.step : v, d.min, d.max);
};
export class Knob {
  constructor(container, app, id, label, unit) {
    Object.assign(this, { container, app, id, label, unit, draw: drawKnob });
    container.className = 'knob';
    container.dataset.tip = id;
    container.innerHTML = '<label class="knob-label"></label><div class="dial" role="slider" tabindex="0"><canvas aria-hidden="true"></canvas></div><div class="knob-readout"><input type="text" inputmode="decimal" autocomplete="off" spellcheck="false"><span></span></div>';
    this.dial = container.querySelector('.dial'); this.canvas = container.querySelector('canvas'); this.input = container.querySelector('input');
    // Q 的显示中点固定为 1；只改变旋钮映射，宿主参数范围和自动化保持原样。
    this.normalized = () => this.id.endsWith('_q') ? (app.values[this.id] <= 1 ? .5 + .5 * Math.log(app.values[this.id]) / Math.log(10) : .5 + .5 * Math.log(app.values[this.id]) / Math.log(40)) : normal(app.schema[this.id], this.displayValue ?? app.values[this.id]);
    this.fromNormalized = n => this.id.endsWith('_q') ? Math.round((n <= .5 ? Math.pow(10, (clamp(n) - .5) * 2) : Math.pow(40, (clamp(n) - .5) * 2)) * 100) / 100 : denormal(app.schema[this.id], n);
    const reset = () => { if(this.disabled)return; app.begin(); this.set(app.schema[this.id].initial); app.end(); };
    this.dial.addEventListener('dblclick', reset);
    this.dial.addEventListener('pointerdown', e => {
      if (e.button) return;
      // 即使旋钮当前不可编辑，也必须截断默认拖拽行为，否则浏览器会把
      // 拖动路径上的所有文字标记为选中，留下大片原生蓝色选区。
      e.preventDefault();
      if (this.disabled) {
        if (this.id.endsWith('_dyn_threshold') && this.app.values?.[`band${this.app.selected}_dyn_auto`])
          this.app.notify(this.app.t('自动阈值由系统跟随；切换到 MANUAL 后可调'));
        return;
      }
      this.dial.focus();
      if (e.altKey || e.metaKey || e.ctrlKey) { reset(); return; }
      app.begin(); this.pointer = e.pointerId; this.y = e.clientY; this.n = this.normalized();
      this.dial.setPointerCapture(e.pointerId); container.classList.add('dragging');
    });
    this.dial.addEventListener('pointermove', e => {
      if (this.pointer !== e.pointerId) return;
      this.n = clamp(this.n + (this.y - e.clientY) / (240 * app.scale)); this.y = e.clientY;
      this.set(this.fromNormalized(this.n));
    });
    const finish = e => { if (e.pointerId !== this.pointer) return; this.pointer = null; container.classList.remove('dragging'); app.end(); };
    for (const name of ['pointerup', 'pointercancel', 'lostpointercapture']) this.dial.addEventListener(name, finish);
    this.dial.addEventListener('wheel', e => { e.preventDefault(); if(this.disabled)return; app.begin(); this.set(this.fromNormalized(this.normalized() - Math.sign(e.deltaY) * (this.id.endsWith('_slope')?1/3:.01))); app.end(); }, { passive: false });
    this.dial.addEventListener('keydown', e => {
      if (e.key === 'Enter' || ['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(e.key)) e.preventDefault();
      if(this.disabled)return;
      if (e.key === 'Enter') { this.input.focus(); this.input.select(); return; }
      if (!['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(e.key)) return;
      app.begin(); this.set(this.fromNormalized(e.key === 'Home' ? 0 : e.key === 'End' ? 1 : this.normalized() + (['ArrowUp', 'ArrowRight'].includes(e.key) ? 1 : -1)*(this.id.endsWith('_slope')?1/3:.01))); app.end();
    });
    this.input.addEventListener('focus', () => { this.editing = true; this.input.value = this.id.endsWith('_slope')?String((app.values[this.id]+1)*12):app.values[this.id].toFixed(this.unit === 'Hz' ? 0 : 2); this.container.querySelector('.knob-readout span').textContent = this.unit; this.input.select(); });
    this.input.addEventListener('blur', () => { if (this.editing) this.commit(); this.editing = false; container.classList.remove('invalid'); this.update(); });
    this.input.addEventListener('keydown', e => { if (e.key === 'Escape') { this.editing = false; container.classList.remove('invalid'); this.input.blur(); this.update(); } if (e.key === 'Enter') { this.commit(); if (!container.classList.contains('invalid')) this.input.blur(); } });
    this.observer = new ResizeObserver(() => this.draw()); this.observer.observe(this.dial); this.update();
  }
  setDisabled(disabled) { this.disabled=!!disabled;this.container.classList.toggle('disabled',this.disabled);this.dial.setAttribute('aria-disabled',this.disabled);this.dial.tabIndex=this.disabled?-1:0;this.input.disabled=this.disabled;if(this.disabled)this.cancel(); }
  set(value) { this.app.set(this.id, value); this.update(); }
  commit() {
    if (!this.editing) return;
    const match = this.input.value.trim().match(/^([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*(kHz|Hz|dB\/oct|dB)?$/i);
    let value = match ? Number(match[1]) : NaN;
    const suffix = match?.[2]?.toLowerCase();
    if (suffix && !(this.unit === 'Hz' ? ['hz', 'khz'] : this.unit === 'dB' ? ['db'] : this.unit==='dB/oct'?['db/oct']:[]).includes(suffix)) value = NaN;
    if (suffix === 'khz') value *= 1000;
    if(this.id.endsWith('_slope'))value=[12,24,36,48].includes(value)?value/12-1:NaN;
    const d = this.app.schema[this.id];
    if (!Number.isFinite(value) || value < d.min || value > d.max) { this.container.classList.add('invalid'); return; }
    this.editing = false; this.container.classList.remove('invalid'); this.app.begin(); this.set(value); this.app.end();
  }
  rebind(id) { if (id === this.id) return; this.cancel(); this.editing = false; this.container.classList.remove('invalid'); this.input.blur(); this.id = id; this.container.dataset.tip = id; this.update(); }
  cancel() { if (this.pointer != null) { const p = this.pointer; this.pointer = null; this.container.classList.remove('dragging'); if (this.dial.hasPointerCapture(p)) this.dial.releasePointerCapture(p); this.app.end(); } }
  update() {
    const value = this.displayValue ?? this.app.values[this.id], d = this.app.schema[this.id]; if (!d) return;
    const name = this.app.t(this.label); this.container.querySelector('label').textContent = name;
    this.dial.setAttribute('aria-label', name); this.input.setAttribute('aria-label', name);
    this.dial.setAttribute('aria-valuemin', d.min); this.dial.setAttribute('aria-valuemax', d.max); this.dial.setAttribute('aria-valuenow', value);
    let unit = this.unit, text = value.toFixed(this.unit === 'Hz' ? 0 : this.unit === 'dB' ? 1 : 2);
    if(this.id.endsWith('_slope')){text=String((value+1)*12);this.dial.setAttribute('aria-valuetext',`${text} dB/oct`);}
    if (this.unit === 'Hz' && value >= 1000) { text = (value / 1000).toFixed(2); unit = 'kHz'; }
    if (!this.editing) this.input.value = text;
    this.input.style.width=`${Math.max(3,(this.editing?this.input.value:text).length+.3)}ch`;
    const unitLabel=this.container.querySelector('.knob-readout span');unitLabel.textContent=this.editing?this.unit:unit;this.container.querySelector('.knob-readout').classList.toggle('unitless',!unitLabel.textContent);unitLabel.style.marginLeft=`${this.input.offsetWidth/2+4}px`;this.draw();
  }
  dispose() { this.cancel(); this.observer.disconnect(); }
}
