// ==============================================================================
// controls.js - SanMu Delay 控件与交互契约
// 标准画布旋钮、高灵敏度手势、音符时值与数显交互卡片
// ==============================================================================

import { drawKnob } from './shared/draw-knob.js';

export const clamp = (x, min = 0, max = 1) => Math.max(min, Math.min(max, x));

export const normal = (d, value) => {
    const norm = clamp((value - d.min) / (d.max - d.min));
    return Math.pow(norm, d.skew || 1);
};

export function denormal(d, n) {
    const v = d.min + (d.max - d.min) * Math.pow(clamp(n), 1 / (d.skew || 1));
    return clamp(d.step ? d.min + Math.round((v - d.min) / d.step) * d.step : v, d.min, d.max);
}

const labels = {
    time_l:           ['Time (L)', '延迟时间 (L)', 'ms'],
    time_r:           ['Time (R)', '延迟时间 (R)', 'ms'],
    feedback:         ['Feedback', '反馈深度', '%'],
    manual_bpm:       ['BPM', '节拍速度', 'BPM'],
    damp_lowcut:      ['Low Cut', '低切阻尼', 'Hz'],
    damp_highcut:     ['High Cut', '高切阻尼', 'Hz'],
    mix:              ['Mix', '干湿比', '%'],
    output_gain:      ['Output', '输出增益', 'dB'],
    mod_rate:         ['Mod Rate', '调制速率', 'Hz'],
    mod_depth:        ['Mod Depth', '调制深度', '%'],
    tape_drive:       ['Tape Drive', '磁带饱和', '%'],
    tape_wow:         ['Tape Wow', '磁带抖晃', '%'],
    bbd_warmth:       ['BBD Warmth', '模拟暖色', '%'],
    diffusion_amount: ['Diffusion', '空间扩散', '%'],
    duck_threshold:   ['Threshold', '避让门限', 'dB'],
    duck_amount:      ['Amount', '避让深度', '%'],
    duck_release:     ['Release', '避让释放', 'ms']
};

export function info(id) {
    if (labels[id]) return labels[id];
    return [id, id, ''];
}

export class Knob {
    constructor(container, id, app, { size = '', ends = null, labelOverride = null, domId = id } = {}) {
        this.id = id;
        this.app = app;
        this.container = container;
        this.labelOverride = labelOverride;
        this.domId = domId;
        this.d = app.schema[id] || { min: 0, max: 100, initial: 50, step: 0.1, skew: 1 };
        this.draw = drawKnob;

        container.className = 'knob ' + size;
        container.dataset.parameter = id;
        container.dataset.tip = 'tip:' + id;
        const [name, , unit] = info(id);

        container.innerHTML = `
            <label class="knob-label" for="edit-${domId}">${name}</label>
            <div class="dial" id="knob-${domId}" role="slider" tabindex="0">
                <canvas aria-hidden="true"></canvas>
            </div>
            <div class="scale-ends"><span></span><span></span></div>
            <div class="knob-readout">
                <input id="edit-${domId}" type="text" inputmode="decimal" autocomplete="off" spellcheck="false">
                <span>${unit}</span>
            </div>
        `;

        this.dial = container.querySelector('.dial');
        this.canvas = container.querySelector('canvas');
        this.input = container.querySelector('input');
        this.getValue = () => id === 'manual_bpm' && app.values.sync_mode === 0 ? (app.hostBpm || 120) : app.values[id];
        this.normalized = () => normal(this.d, this.getValue());

        this.dial.setAttribute('aria-valuemin', this.d.min);
        this.dial.setAttribute('aria-valuemax', this.d.max);

        const range = ends || [this.formatEnd(this.d.min), this.formatEnd(this.d.max)];
        container.querySelectorAll('.scale-ends span').forEach((s, i) => s.textContent = range[i]);

        const reset = () => {
            if (this.isReadOnly()) return;
            app.begin();
            this.set(this.d.initial);
            app.end();
        };

        this.dial.addEventListener('dblclick', reset);
        this.dial.addEventListener('pointerdown', e => {
            if (e.button !== 0) return;
            if (this.isReadOnly()) { e.preventDefault(); return; }
            e.preventDefault();
            this.dial.focus();
            if (e.metaKey || e.ctrlKey) { reset(); return; }
            app.begin();
            this.pointer = e.pointerId;
            this.y = e.clientY;
            this.drag = this.normalized();
            this.dial.setPointerCapture(e.pointerId);
            container.classList.add('dragging');
        });

        this.dial.addEventListener('pointermove', e => {
            if (this.isReadOnly()) { e.preventDefault(); this.cancelGesture(); return; }
            if (this.pointer !== e.pointerId) return;
            const delta = (this.y - e.clientY) / (240 * (app.scale || 1));
            this.y = e.clientY;
            this.drag = clamp(this.drag + delta);
            const val = denormal(this.d, this.drag);
            this.set(val);

            // 左右时间联动同步
            if (app.values['time_link'] > 0.5 && (this.id === 'time_l' || this.id === 'time_r')) {
                const other = this.id === 'time_l' ? 'time_r' : 'time_l';
                app.set(other, val);
            }
        });

        const finish = e => {
            if (this.pointer !== e.pointerId) return;
            this.pointer = null;
            container.classList.remove('dragging');
            app.end();
        };

        for (const event of ['pointerup', 'pointercancel', 'lostpointercapture']) {
            this.dial.addEventListener(event, finish);
        }

        this.dial.addEventListener('wheel', e => {
            e.preventDefault();
            if (this.isReadOnly()) return;
            app.begin();
            const delta = -Math.sign(e.deltaY || e.deltaX) * 0.01;
            const n = clamp(this.normalized() + delta);
            const val = denormal(this.d, n);
            this.set(val);
            if (app.values['time_link'] > 0.5 && (this.id === 'time_l' || this.id === 'time_r')) {
                const other = this.id === 'time_l' ? 'time_r' : 'time_l';
                app.set(other, val);
            }
            app.end();
        }, { passive: false });

        this.dial.addEventListener('keydown', e => {
            if (!['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Home', 'End', 'Enter'].includes(e.key)) return;
            e.preventDefault();
            if (this.isReadOnly()) return;
            if (e.key === 'Enter') {
                this.input.focus();
                this.input.select();
                return;
            }
            app.begin();
            let n = this.normalized();
            if (e.key === 'Home') n = 0;
            else if (e.key === 'End') n = 1;
            else n += ['ArrowUp', 'ArrowRight'].includes(e.key) ? 0.01 : -0.01;
            const val = denormal(this.d, clamp(n));
            this.set(val);
            if (app.values['time_link'] > 0.5 && (this.id === 'time_l' || this.id === 'time_r')) {
                const other = this.id === 'time_l' ? 'time_r' : 'time_l';
                app.set(other, val);
            }
            app.end();
        });

        this.input.addEventListener('focus', () => {
            this.editing = true;
            container.classList.remove('invalid');
            this.input.value = this.numeric().toFixed(this.digits());
            this.input.select();
        });

        this.input.addEventListener('blur', () => {
            this.editing = false;
            this.update();
        });

        this.input.addEventListener('change', () => this.commit());
        this.input.addEventListener('keydown', e => {
            if (e.key === 'Enter') {
                e.preventDefault();
                this.commit();
            }
            if (e.key === 'Escape') {
                e.preventDefault();
                this.editing = false;
                this.input.blur();
                container.classList.remove('invalid');
                this.update();
            }
        });

        this.observer = new ResizeObserver(() => this.draw());
        this.observer.observe(this.dial);
        this.update();
    }

    formatEnd(val) {
        if (this.d.step >= 1) return Math.round(val);
        return val.toFixed(1);
    }

    numeric() {
        return this.getValue();
    }

    digits() {
        if (this.d.step < 0.1) return 2;
        if (this.d.step < 1) return 1;
        return 0;
    }

    set(v) {
        if (this.isReadOnly()) return;
        this.app.set(this.id, v);
        this.update();
    }

    commit() {
        if (this.isReadOnly()) return;
        if (!this.editing) return;
        const text = this.input.value.trim();
        const match = text.match(/^([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*(BPM|kHz|Hz|ms|s|dB|%|千赫兹|赫兹|毫秒|秒|分贝)?$/i);
        const unit = info(this.id)[2];
        let v = match ? Number(match[1]) : NaN;
        const suffix = match?.[2]?.toLowerCase();

        const valid = { BPM: ['bpm'], Hz: ['hz', 'khz', '赫兹', '千赫兹'], ms: ['ms', 's', '毫秒', '秒'], dB: ['db', '分贝'], '%': ['%'], '': [] };
        if (suffix && !valid[unit]?.includes(suffix)) v = NaN;
        if (suffix === 'khz' || suffix === '千赫兹') v *= 1000;
        if (unit === 'ms' && (suffix === 's' || suffix === '秒')) v *= 1000;

        if (!Number.isFinite(v) || v < this.d.min || v > this.d.max) {
            this.container.classList.add('invalid');
            this.app.notify(this.app.t('Enter a value within the parameter range.'));
            return;
        }

        this.editing = false;
        this.app.begin();
        this.set(denormal(this.d, normal(this.d, v)));
        if (this.app.values['time_link'] > 0.5 && (this.id === 'time_l' || this.id === 'time_r')) {
            const other = this.id === 'time_l' ? 'time_r' : 'time_l';
            this.app.set(other, this.getValue());
        }
        this.app.end();
        this.input.blur();
        this.update();
    }

    isReadOnly() { return this.id === 'manual_bpm' && this.app.values.sync_mode !== 1; }

    update() {
        const key = JSON.stringify([this.getValue(), this.app.ui.language, this.app.ui.theme, this.app.values.engine, this.app.values.sync_mode, this.editing]);
        if (this.renderKey === key) return;
        this.renderKey = key;

        const [en, zh, unit] = info(this.id);
        const name = this.labelOverride
            ? (this.app.ui.language === 'zh' ? this.labelOverride.zh : this.labelOverride.en)
            : this.id === 'manual_bpm' && this.app.values.sync_mode === 0
                ? (this.app.ui.language === 'zh' ? '宿主速度' : 'Host tempo')
                : (this.app.ui.language === 'zh' ? zh : en);
        this.container.querySelector('.knob-label').textContent = name;
        this.dial.setAttribute('aria-label', name);
        this.input.setAttribute('aria-label', name);
        this.input.disabled = this.isReadOnly();
        this.dial.setAttribute('aria-disabled', String(this.isReadOnly()));
        this.container.classList.toggle('tempo-inactive', this.id === 'manual_bpm' && this.app.values.sync_mode === 2);

        const value = this.numeric();
        let text = value.toFixed(this.digits());
        if (this.id === 'damp_highcut' && value >= 20000) text = '20k';

        this.container.querySelector('.knob-readout span').textContent = unit;
        if (!this.editing) this.input.value = text;
        this.dial.setAttribute('aria-valuenow', String(value));
        this.dial.setAttribute('aria-valuetext', `${text} ${unit}`);
        this.draw();
    }

    cancelGesture() {
        if (this.pointer != null) {
            const pointer = this.pointer;
            this.pointer = null;
            this.container.classList.remove('dragging');
            if (this.dial.hasPointerCapture(pointer)) this.dial.releasePointerCapture(pointer);
            this.app.end();
        }
    }

    dispose() {
        this.cancelGesture();
        this.observer.disconnect();
    }
}

// Modern 没有独立染色参数；这个界面宏只保存用户对数字回声“清晰度”的视觉偏好，绝不伪造 DSP 参数。
export class VirtualKnob extends Knob {
    constructor(container, id, app, options = {}) {
        app.ui.virtualValues ??= {};
        app.ui.virtualValues[id] ??= options.initial ?? 50;
        // Knob 的首次绘制发生在 super() 内；先提供临时值，避免虚拟控件把空值格式化成 NaN。
        app.values[id] = app.ui.virtualValues[id];
        super(container, id, app, options);
        this.getValue = () => app.ui.virtualValues[id];
        this.set = value => {
            app.ui.virtualValues[id] = Math.max(this.d.min, Math.min(this.d.max, value));
            this.update();
        };
        this.update();
    }
}

// ==============================================================================
// 交互数显卡片 (TempoTimeCard)
// 统一管理：Sync 模式节拍选择、BPM 模式数字调节与 Free 毫秒输入
// ==============================================================================
export class TempoTimeCard {
    constructor(container, id, app, { role = 'L', isRhythm = true } = {}) {
        this.container = container;
        this.id = id;
        this.app = app;
        this.role = role;
        this.isRhythm = isRhythm;
        this.divisions = [
            "1/1", "1/2D", "1/2", "1/2T", "1/4D", "1/4", "1/4T",
            "1/8D", "1/8", "1/8T", "1/16D", "1/16", "1/16T", "1/32"
        ];
        this.init();
    }

    init() {
        this.container.className = 'tempo-time-card';
        this.container.innerHTML = `
            <div class="card-header">
                <span class="card-label">TIME</span>
                <span class="card-sub-info"></span>
            </div>
            <div class="card-body">
                <button class="card-step-btn prev" data-tip="Decrease">－</button>
                <div class="card-display" tabindex="0" role="spinbutton">
                    <span class="card-text">500.0</span>
                    <input class="card-input" type="text" style="display:none" autocomplete="off">
                    <span class="card-unit">ms</span>
                </div>
                <button class="card-step-btn next" data-tip="Increase">＋</button>
            </div>
            <div class="card-division-pills" style="display:none"></div>
        `;

        this.labelSpan = this.container.querySelector('.card-label');
        this.subInfoSpan = this.container.querySelector('.card-sub-info');
        this.display = this.container.querySelector('.card-display');
        this.textSpan = this.container.querySelector('.card-text');
        this.input = this.container.querySelector('.card-input');
        this.unitSpan = this.container.querySelector('.card-unit');
        this.btnPrev = this.container.querySelector('.card-step-btn.prev');
        this.btnNext = this.container.querySelector('.card-step-btn.next');
        this.pillsContainer = this.container.querySelector('.card-division-pills');

        // 构建音符切分标签
        this.divisions.forEach((div, idx) => {
            if (![5,7,8,9,11].includes(idx)) return;
            const pill = document.createElement('button');
            pill.type = 'button';
            pill.className = 'div-pill';
            pill.textContent = div;
            pill.dataset.index = idx;
            pill.addEventListener('click', () => {
                const paramId = this.role === 'R' ? 'division_r' : 'division';
                this.app.begin();
                this.app.set(paramId, idx);
                if (this.app.values['time_link'] > 0.5) {
                    this.app.set('division_r', idx);
                    this.app.set('division', idx);
                }
                this.app.end();
            });
            this.pillsContainer.appendChild(pill);
        });

        // 步进按钮
        this.btnPrev.addEventListener('click', () => this.step(-1));
        this.btnNext.addEventListener('click', () => this.step(1));

        // 鼠标上下拖动调速
        let pointer = null, startY = 0, startVal = 0;
        this.display.addEventListener('pointerdown', e => {
            if (e.button !== 0 || this.editing || (this.id === 'bpm' && this.app.values.sync_mode === 0)) return;
            e.preventDefault();
            this.display.focus();
            pointer = e.pointerId;
            startY = e.clientY;
            startVal = this.getCurrentValue();
            this.display.setPointerCapture(pointer);
            this.container.classList.add('dragging');
            this.app.begin();
        });

        this.display.addEventListener('pointermove', e => {
            if (pointer !== e.pointerId) return;
            const deltaY = (startY - e.clientY) / (this.app.scale || 1);
            const syncMode = Math.round(this.app.values['sync_mode'] || 0);

            if (syncMode === 0 || syncMode === 1) { // Sync or BPM
                // 调节 BPM 或音符
                if (this.id === 'bpm') {
                    const step = 1.0;
                    const newVal = clamp(startVal + deltaY * (step / 5), 20, 300);
                    this.app.set('manual_bpm', newVal);
                } else {
                    const idxDelta = Math.floor(deltaY / 15);
                    const newIdx = clamp(startVal - idxDelta, 0, this.divisions.length - 1);
                    const paramId = this.role === 'R' ? 'division_r' : 'division';
                    this.app.set(paramId, newIdx);
                    if (this.app.values['time_link'] > 0.5) {
                        this.app.set('division_r', newIdx);
                        this.app.set('division', newIdx);
                    }
                }
            } else { // Free mode ms
                const step = 5.0;
                const newVal = clamp(startVal + deltaY * step, 1.0, 2000.0);
                const paramId = this.role === 'R' ? 'time_r' : 'time_l';
                this.app.set(paramId, newVal);
                if (this.app.values['time_link'] > 0.5) {
                    this.app.set('time_r', newVal);
                    this.app.set('time_l', newVal);
                }
            }
        });

        const finishDrag = e => {
            if (pointer !== e.pointerId) return;
            pointer = null;
            this.container.classList.remove('dragging');
            this.app.end();
        };

        for (const evt of ['pointerup', 'pointercancel', 'lostpointercapture']) {
            this.display.addEventListener(evt, finishDrag);
        }

        this.display.addEventListener('keydown', e => {
            if (this.editing) return;
            if (e.key === 'Enter') { e.preventDefault(); this.startEdit(); }
            if (['ArrowLeft', 'ArrowDown', 'ArrowRight', 'ArrowUp'].includes(e.key)) {
                e.preventDefault(); this.step(['ArrowRight', 'ArrowUp'].includes(e.key) ? 1 : -1);
            }
        });

        // 双击直接进入键盘键入
        this.display.addEventListener('dblclick', () => {
            this.startEdit();
        });

        this.input.addEventListener('keydown', e => {
            if (e.key === 'Enter') {
                e.preventDefault();
                this.commitEdit();
            } else if (e.key === 'Escape') {
                e.preventDefault();
                this.cancelEdit();
            }
        });

        this.input.addEventListener('blur', () => {
            if (this.editing) this.commitEdit();
        });
    }

    getCurrentValue() {
        const syncMode = Math.round(this.app.values['sync_mode'] || 0);
        if (this.id === 'bpm') {
            return this.app.values['manual_bpm'] || 120.0;
        }
        if (syncMode === 0 || syncMode === 1) {
            const paramId = this.role === 'R' ? 'division_r' : 'division';
            return Math.round(this.app.values[paramId] ?? 5);
        }
        const paramId = this.role === 'R' ? 'time_r' : 'time_l';
        return this.app.values[paramId] ?? 500.0;
    }

    step(dir) {
        if (this.id === 'bpm' && this.app.values.sync_mode === 0) return;
        const syncMode = Math.round(this.app.values['sync_mode'] || 0);
        this.app.begin();
        if (this.id === 'bpm') {
            const cur = this.app.values['manual_bpm'] || 120.0;
            const delta = 1.0 * dir;
            this.app.set('manual_bpm', clamp(cur + delta, 20.0, 300.0));
        } else if (syncMode === 0 || syncMode === 1) {
            const paramId = this.role === 'R' ? 'division_r' : 'division';
            const curIdx = Math.round(this.app.values[paramId] ?? 5);
            const newIdx = clamp(curIdx - dir, 0, this.divisions.length - 1);
            this.app.set(paramId, newIdx);
            if (this.app.values['time_link'] > 0.5) {
                this.app.set('division_r', newIdx);
                this.app.set('division', newIdx);
            }
        } else {
            const paramId = this.role === 'R' ? 'time_r' : 'time_l';
            const cur = this.app.values[paramId] ?? 500.0;
            const delta = 10.0 * dir;
            const newVal = clamp(cur + delta, 1.0, 2000.0);
            this.app.set(paramId, newVal);
            if (this.app.values['time_link'] > 0.5) {
                this.app.set('time_r', newVal);
                this.app.set('time_l', newVal);
            }
        }
        this.app.end();
        this.update();
    }

    startEdit() {
        if (this.id === 'bpm' && this.app.values.sync_mode === 0) return;
        this.editing = true;
        this.textSpan.style.display = 'none';
        this.unitSpan.style.display = 'none';
        this.input.style.display = 'inline-block';
        this.input.value = this.textSpan.textContent.trim();
        this.input.focus();
        this.input.select();
    }

    cancelEdit() {
        this.editing = false;
        this.input.style.display = 'none';
        this.textSpan.style.display = 'inline';
        this.unitSpan.style.display = 'inline';
    }

    commitEdit() {
        if (!this.editing) return;
        const text = this.input.value.trim();
        const syncMode = Math.round(this.app.values['sync_mode'] || 0);

        const bpm = this.id === 'bpm';
        let value;
        if (!bpm && syncMode !== 2) value = this.divisions.findIndex(d => d.toLowerCase() === text.toLowerCase());
        else {
            const match = text.match(/^([+]?(?:\d+(?:\.\d*)?|\.\d+))\s*(ms|s|bpm)?$/i);
            value = match ? Number(match[1]) : NaN;
            const unit = match?.[2]?.toLowerCase();
            if (bpm && unit && unit !== 'bpm') value = NaN;
            if (!bpm && unit === 'bpm') value = NaN;
            if (!bpm && unit === 's') value *= 1000;
        }
        const min = bpm ? 20 : syncMode === 2 ? 1 : 0;
        const max = bpm ? 300 : syncMode === 2 ? 2000 : 13;
        if (!Number.isFinite(value) || value < min || value > max) {
            this.app.notify(this.app.t('Enter a value within the parameter range.'));
            this.input.focus(); return;
        }
        const id = bpm ? 'manual_bpm' : syncMode === 2 ? (this.role === 'R' ? 'time_r':'time_l') : (this.role === 'R' ? 'division_r':'division');
        this.app.begin(); this.app.set(id, value);
        if (!bpm && this.app.values.time_link > .5) this.app.set(syncMode === 2 ? (this.role === 'R' ? 'time_l':'time_r') : (this.role === 'R' ? 'division':'division_r'), value);
        this.app.end();
        this.cancelEdit();
        this.update();
    }

    update() {
        const key = JSON.stringify([this.app.values, this.app.ui.language, this.app.hostBpm, this.editing]);
        if (this.renderKey === key) return;
        this.renderKey = key;
        const syncMode = Math.round(this.app.values['sync_mode'] || 0);
        const isBpm = this.id === 'bpm';
        const isZh = this.app.ui.language === 'zh';

        if (isBpm) {
            this.labelSpan.textContent = syncMode === 0 ? (isZh ? '宿主速度' : 'Host tempo') : (isZh ? '手动速度' : 'Manual tempo');
            this.display.setAttribute('aria-readonly', String(syncMode === 0));
            this.display.setAttribute('aria-label', this.labelSpan.textContent);
            this.input.setAttribute('aria-label', this.labelSpan.textContent);
            const curBpm = (this.app.values['manual_bpm'] || 120.0).toFixed(1);
            this.subInfoSpan.textContent = syncMode === 0 ? (isZh ? '跟随宿主' : 'Follow host') : '';
            this.textSpan.textContent = syncMode === 0 ? (this.app.hostBpm || 120.0).toFixed(1) : curBpm;
            this.unitSpan.textContent = 'BPM';
            this.pillsContainer.style.display = 'none';
            this.btnPrev.disabled = syncMode === 0;
            this.btnNext.disabled = syncMode === 0;
            return;
        }

        const paramId = this.role === 'R' ? 'division_r' : 'division';
        const timeParamId = this.role === 'R' ? 'time_r' : 'time_l';
        const isLinked = this.app.values['time_link'] > 0.5;

        let label = 'TIME';
        if (!isLinked && this.app.values['topology'] === 0) {
            label = this.role === 'L' ? (isZh ? '时间 (L)' : 'TIME (L)') : (isZh ? '时间 (R)' : 'TIME (R)');
        }
        this.labelSpan.textContent = (isZh ? '时间 ' : 'Time ') + ((isLinked || this.app.values.topology === 1) ? 'L / R' : this.role);
        this.display.setAttribute('aria-label', this.labelSpan.textContent);
        this.input.setAttribute('aria-label', this.labelSpan.textContent);

        if (syncMode === 2) {
            // Free mode (毫秒)
            this.pillsContainer.style.display = 'none';
            const ms = (this.app.values[timeParamId] || 500.0).toFixed(1);
            this.textSpan.textContent = ms;
            this.unitSpan.textContent = 'ms';
            this.subInfoSpan.textContent = isLinked ? '(LINKED)' : (this.role === 'L' ? '(LEFT)' : '(RIGHT)');
            this.btnPrev.disabled = false;
            this.btnNext.disabled = false;
        } else {
            // Sync or BPM mode (音符切分)
            const divIdx = Math.round(this.app.values[paramId] ?? 5);
            this.textSpan.textContent = this.divisions[divIdx] || '1/4';
            this.unitSpan.textContent = '';
            const effBpm = syncMode === 0 ? (this.app.hostBpm || 120.0) : (this.app.values['manual_bpm'] || 120.0);
            const beatValues = [4,3,2,4/3,1.5,1,2/3,.75,.5,1/3,.375,.25,1/6,.125];
            this.subInfoSpan.textContent = `${Math.min(5000, beatValues[divIdx]*60000/effBpm).toFixed(1)} ms`;
            this.pillsContainer.style.display = 'flex';
            this.btnPrev.disabled = false;
            this.btnNext.disabled = false;

            // 更新 pills 选中状态
            this.pillsContainer.querySelectorAll('.div-pill').forEach(pill => {
                pill.classList.toggle('active', Number(pill.dataset.index) === divIdx);
            });
        }
    }
}
