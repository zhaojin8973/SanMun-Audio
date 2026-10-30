import {mountDialogLayer,positionDialogLayer} from './shared/dialog-layer.js';
// ==============================================================================
// app.js - SanMu Delay 前端主应用程序
// 统管参数双向同步、引擎与拓扑刷新、回声视窗与立体声电平、预设与双语设置
// ==============================================================================

import { callNative, onNativeFrame, isNative } from './bridge.js';
import { Knob, VirtualKnob } from './controls.js';
import { EchoVisualizer, DuckMeter } from './visualizer.js';
import { icon } from './shared/icons.js';

function renderIcons(root = document) {
    root.querySelectorAll('[data-icon]').forEach(el => {
        el.innerHTML = icon(el.dataset.icon);
    });
}
import { tooltips } from './tooltips.js';
import { manual } from './manual.js';

class App {
    constructor() {
        this.values = {};
        this.schema = {};
        this.presets = [];
        this.ui = {
            theme: 'light',
            language: 'en',
            scale: 1,
            tips: 'follow',
            history: true
        };
        this.hostBpm = 120.0;
        this.knobs = {};
        this.pendingSetValues = new Map();
        this.setFlushScheduled = false;
        this.setFlushInFlight = false;
        this.setDrainPromise = Promise.resolve();

        this.i18nDict = {
            'Settings': '设置',
            'Appearance': '外观',
            'Light': '浅色',
            'Dark': '深色',
            'Visual display': '视觉显示',
            'Turn off dynamic display while audio and meters keep running.': '关闭动态图像，声音处理与电平表继续运行。',
            'Hide the decay diagram while time editing, audio and meters remain active.': '隐藏衰减示意图，时间编辑、声音处理与电平表继续运行。',
            'Interface language': '界面语言',
            'Window · Interface scale': '窗口 · 界面缩放',
            'Reset size to 100%': '恢复 100% 大小',
            'Tooltips': '悬停提示',
            'Follow interface': '跟随界面',
            'Chinese tips': '中文提示',
            'English tips': '英文提示',
            'Turn off': '关闭提示',
            'Help manual': '操作帮助',
            'About SanMu Audio': '关于 SanMu Audio',
            'Back to settings': '返回设置',
            'Clear peaks': '清除峰值',
            'Presets': '预设库',
            'Save preset': '保存预设',
            'Save': '保存',
            'Time & Rhythm': '节奏与时间',
            'Time mode': '时间模式',
            'Tone & Feedback': '音色与反馈',
            'Dynamics & Output': '动态与输出',
            'Damping': '阻尼衰减',
            'Modulation': '空间调制',
            'Ducking': '动态避让',
            'Output': '输出主控',
            'DUCKING': '避让 Ducking',
            'Clean repeats': '清晰回声',
            'Shape the repeats with damping and modulation.': '用阻尼调整明暗，用调制增加流动感。',
            'Enter a value within the parameter range.': '请输入在参数范围内的有效数值。',
            'Enter a valid preset name without special characters.': '请输入不含特殊字符的有效预设名称。',
            'Four delay engines. Three spatial topologies. One unified workspace.': '四种延迟音色，三种立体声回声方式。'
        };
    }

    t(key) {
        if (this.ui.language === 'zh' && this.i18nDict[key]) {
            return this.i18nDict[key];
        }
        return key;
    }

    async init() {
        mountDialogLayer();
        if (!isNative) {
            const { installPreview } = await import('./preview.js');
            await installPreview();
        }
        const data = await callNative('initialize');
        this.schema = data.schema || {};
        this.values = data.values || {};
        this.presets = data.presets || [];
        if (data.ui) Object.assign(this.ui, data.ui);
        this.hostBpm = data.bpm || 120.0;

        document.documentElement.dataset.theme = this.ui.theme;

        this.initVisualizer();
        this.initControls();
        this.initTopbar();
        this.initModebar();
        this.initDialogs();
        this.initTooltips();
        this.applyTheme(this.ui.theme);
        this.applyLanguage(this.ui.language);

        renderIcons(document);

        // 响应式自适应缩放 (根据宿主窗口实际 innerWidth / innerHeight 动态缩放居中 #app 并重定位对话框)
        const resize = () => {
            this.scale = Math.min(window.innerWidth / 1100, window.innerHeight / 750);
            const app = document.querySelector('#app');
            if (app) {
                app.style.transform = `scale(${this.scale})`;
                app.style.left = `${Math.max(0, (window.innerWidth - 1100 * this.scale) / 2)}px`;
                app.style.top = `${Math.max(0, (window.innerHeight - 750 * this.scale) / 2)}px`;
            }
            this.positionDialogs();
            Object.values(this.knobs).forEach(knob => knob.draw());
        };
        window.addEventListener('resize', resize);
        resize();

        // 监听来自 C++ 原生的高频帧同步（音频电平、走带 BPM、Ducking 衰减）
        onNativeFrame(packet => this.onFrame(packet));

        this.syncState(data);

        const params = new URLSearchParams(window.location.search);
        if (params.get('lang')) this.applyLanguage(params.get('lang'));
        if (params.get('theme')) this.applyTheme(params.get('theme'));
        if (params.get('settings')) this.open('settings-dialog');
        if (params.get('presets')) this.open('presets-dialog');
    }

    initVisualizer() {
        const canvas = document.querySelector('#echo-editor');
        this.visualizer = new EchoVisualizer(canvas, this);

        this.duckMeter = new DuckMeter(document.querySelector('.duck-meter'));
    }

    initControls() {
        // 1. 节拍速度主旋钮；时间由编辑器轨道完成
        this.knobs.bpm = new Knob(document.querySelector('#knob-bpm'), 'manual_bpm', this, { size: 'large' });

        // 2. 核心 Hero 旋钮 (Feedback & Mix)
        this.knobs.feedback = new Knob(document.querySelector('#knob-feedback'), 'feedback', this, { size: 'large' });
        this.knobs.mix = new Knob(document.querySelector('#knob-mix'), 'mix', this, { size: 'large' });

        // 专属特性旋钮 (Engine Character)
        this.knobChar1Container = document.querySelector('#knob-engine-char-1');
        this.knobChar2Container = document.querySelector('#knob-engine-char-2');
        this.modernModAmountContainer = document.querySelector('#knob-modern-clarity');
        this.rebuildEngineCharKnobs();

        // 3. 次级细节抽屉 (Details Drawer) - 全部统一使用 small (52px)
        this.knobs.lowcut = new Knob(document.querySelector('#knob-damp-lowcut'), 'damp_lowcut', this, { size: 'small' });
        this.knobs.highcut = new Knob(document.querySelector('#knob-damp-highcut'), 'damp_highcut', this, { size: 'small' });

        this.knobs.modRate = new Knob(document.querySelector('#knob-mod-rate'), 'mod_rate', this, { size: 'small' });
        this.knobs.modDepth = new Knob(document.querySelector('#knob-mod-depth'), 'mod_depth', this, { size: 'small' });

        this.knobs.duckThresh = new Knob(document.querySelector('#knob-duck-threshold'), 'duck_threshold', this, { size: 'small' });
        this.knobs.duckAmount = new Knob(document.querySelector('#knob-duck-amount'), 'duck_amount', this, { size: 'small' });
        this.knobs.duckRelease = new Knob(document.querySelector('#knob-duck-release'), 'duck_release', this, { size: 'small' });

        this.knobs.outGain = new Knob(document.querySelector('#knob-output-gain'), 'output_gain', this, { size: 'small' });
    }

    rebuildEngineCharKnobs() {
        const engine = Math.round(this.values['engine'] || 0);
        if (this.knobs.char1) this.knobs.char1.dispose();
        if (this.knobs.char2) this.knobs.char2.dispose();
        if (this.knobs.modernModAmount) this.knobs.modernModAmount.dispose();

        delete this.knobs.char1;
        delete this.knobs.char2;
        delete this.knobs.modernModAmount;
        this.characterEngine = engine;
        this.knobChar1Container.innerHTML = '';
        this.knobChar2Container.innerHTML = '';
        this.knobChar2Container.style.display = 'none';
        this.modernModAmountContainer.innerHTML = '';
        this.modernModAmountContainer.style.display = 'none';
        document.querySelector('#knob-mod-depth').style.display = engine === 0 ? 'none' : '';

        if (engine === 0) { // Modern
            this.knobChar1Container.style.display = 'none';
            this.knobChar2Container.style.display = 'none';
            this.modernModAmountContainer.style.display = 'block';
            this.knobs.modernModAmount = new Knob(this.modernModAmountContainer, 'mod_depth', this, {
                labelOverride: { en: 'Mod Amount', zh: '调制量' },
                domId: 'modern-mod-amount'
            });
            this.modernModAmountContainer.dataset.tip = 'tip:modern_mod_mix';
        } else if (engine === 1) { // Tape
            this.knobChar1Container.style.display = 'block';
            this.knobChar2Container.style.display = 'block';
            this.knobs.char1 = new Knob(this.knobChar1Container, 'tape_drive', this);
            this.knobs.char2 = new Knob(this.knobChar2Container, 'tape_wow', this);
        } else if (engine === 2) { // BBD
            this.knobChar1Container.style.display = 'block';
            this.knobs.char1 = new Knob(this.knobChar1Container, 'bbd_warmth', this);
        } else if (engine === 3) { // Diffused
            this.knobChar1Container.style.display = 'block';
            this.knobs.char1 = new Knob(this.knobChar1Container, 'diffusion_amount', this);
        }
    }

    initTopbar() {
        // 预设
        document.querySelector('#preset-prev').addEventListener('click', () => this.stepPreset(-1));
        document.querySelector('#preset-next').addEventListener('click', () => this.stepPreset(1));
        document.querySelector('#preset-open').addEventListener('click', () => this.openPresetDialog());
        document.querySelector('#preset-save').addEventListener('click', () => this.openSaveDialog());

        // 撤销 / 重做
        document.querySelector('#undo').addEventListener('click', () => callNative('undo').then(s => this.syncState(s)));
        document.querySelector('#redo').addEventListener('click', () => callNative('redo').then(s => this.syncState(s)));

        // A/B
        document.querySelectorAll('input[name="ab"]').forEach(radio => {
            radio.addEventListener('change', e => {
                callNative('ab', { slot: e.target.value }).then(s => this.syncState(s));
            });
        });
        document.querySelector('#copy-ab').addEventListener('click', () => {
            callNative('copyAB').then(s => this.syncState(s));
            this.notify(this.t('Copied to other slot'));
        });

        // 旁通 BYP
        const btnBypass = document.querySelector('#bypass');
        btnBypass.addEventListener('click', () => {
            const cur = (this.values['global_bypass'] || 0) > 0.5;
            this.begin();
            this.set('global_bypass', cur ? 0 : 1);
            this.end();
            btnBypass.setAttribute('aria-pressed', String(!cur));
        });

        // 设置
        document.querySelector('#settings').addEventListener('click', () => {
            this.open('settings-dialog');
        });
    }

    initModebar() {
        // 引擎选择
        document.querySelectorAll('input[name="engine"]').forEach(radio => {
            radio.addEventListener('change', e => {
                const val = Number(e.target.value);
                this.begin();
                this.set('engine', val);
                this.end();
                document.querySelector('.instrument').dataset.engine = String(val);
            });
        });

        // 拓扑选择
        document.querySelectorAll('input[name="topology"]').forEach(radio => {
            radio.addEventListener('change', e => {
                const val = Number(e.target.value);
                this.begin();
                this.set('topology', val);
                this.end();
                this.updateLayoutByTopology();
            });
        });

        // 时钟模式选择
        document.querySelectorAll('input[name="sync_mode"]').forEach(radio => {
            radio.addEventListener('change', e => {
                const val = Number(e.target.value);
                this.begin();
                this.set('sync_mode', val);
                this.end();
                this.updateLayoutBySyncMode();
            });
        });

        // 左右联动 LINK
        const btnLink = document.querySelector('#btn-time-link');
        btnLink.addEventListener('click', () => {
            const cur = (this.values['time_link'] || 0) > 0.5;
            this.begin();
            this.set('time_link', cur ? 0 : 1);
            if (!cur) {
                // 开启联动时将 R 强制对齐 L
                this.set('time_r', this.values['time_l']);
                this.set('division_r', this.values['division']);
            }
            this.end();
            btnLink.setAttribute('aria-pressed', String(!cur));
            this.updateLayoutByTopology();
        });

        // 冻结 FREEZE
        const btnFreeze = document.querySelector('#btn-freeze');
        btnFreeze.addEventListener('click', () => {
            const cur = (this.values['freeze'] || 0) > 0.5;
            this.begin();
            this.set('freeze', cur ? 0 : 1);
            this.end();
            btnFreeze.setAttribute('aria-pressed', String(!cur));
        });

        // Ducking 启用与源切换
        const duckEn = document.querySelector('#duck-enable');
        duckEn.addEventListener('change', e => {
            this.begin();
            this.set('duck_enable', e.target.checked ? 1 : 0);
            this.end();
        });

        document.querySelectorAll('input[name="duck_source"]').forEach(radio => {
            radio.addEventListener('change', e => {
                this.begin();
                this.set('duck_source', Number(e.target.value));
                this.end();
            });
        });
    }

    updateLayoutByTopology() {
        const pingPong = Math.round(this.values.topology || 0) === 1;
        const btnLink = document.querySelector('#btn-time-link');
        btnLink.disabled = pingPong;
        btnLink.setAttribute('aria-pressed', String(pingPong || this.values.time_link > .5));
    }

    updateLayoutBySyncMode() {
        this.updateLayoutByTopology();
        this.knobs.bpm?.update();
    }

    open(id) {
        for (const d of document.querySelectorAll('dialog[open]')) d.close();
        const target = document.getElementById(id);
        if (target) {
            target.showModal();
            this.positionDialogs();
        }
    }

    positionDialogs() {
        positionDialogLayer(this.scale);
    }

    initDialogs() {
        // 关闭按钮与点击背景关闭绑定
        document.querySelectorAll('dialog').forEach(d => {
            d.querySelectorAll('.close, .back-settings').forEach(btn => {
                btn.addEventListener('click', () => d.close());
            });
            d.addEventListener('click', e => {
                if (e.target === d) {
                    const b = d.getBoundingClientRect();
                    if (e.clientX < b.left || e.clientX > b.right || e.clientY < b.top || e.clientY > b.bottom) {
                        d.close();
                    }
                }
            });
        });

        // 设置面板绑定
        document.querySelectorAll('input[name="theme"]').forEach(r => {
            r.addEventListener('change', e => {
                this.applyTheme(e.target.value);
                callNative('ui', { theme: e.target.value });
            });
        });

        document.querySelectorAll('input[name="language"]').forEach(r => {
            r.addEventListener('change', e => {
                this.applyLanguage(e.target.value);
                callNative('ui', { language: e.target.value });
            });
        });

        const chkVisual = document.querySelector('#history-enabled');
        chkVisual.checked = this.ui.history !== false;
        chkVisual.addEventListener('change', e => {
            this.ui.history = e.target.checked;
            callNative('ui', { history: e.target.checked });
        });

        document.querySelectorAll('input[name="scale"]').forEach(r => {
            r.addEventListener('change', e => {
                const s = parseFloat(e.target.value);
                this.ui.scale = s;
                callNative('ui', { scale: s });
            });
        });

        document.querySelector('#reset-window').addEventListener('click', () => {
            this.ui.scale = 1.0;
            const r = document.querySelector('input[name="scale"][value="1"]');
            if (r) r.checked = true;
            callNative('ui', { scale: 1.0 });
        });

        document.querySelectorAll('input[name="tips"]').forEach(r => {
            r.addEventListener('change', e => {
                this.ui.tips = e.target.value;
                callNative('ui', { tips: e.target.value });
            });
        });

        // 打开手册
        document.querySelector('#help').addEventListener('click', () => {
            this.renderManual();
            this.open('help-dialog');
        });

        // 打开关于
        document.querySelector('#about').addEventListener('click', () => {
            this.open('about-dialog');
        });

        // 返回设置
        document.querySelectorAll('.back-settings').forEach(btn => {
            btn.addEventListener('click', () => {
                this.open('settings-dialog');
            });
        });

        // 预设保存表单
        const saveForm = document.querySelector('#save-form');
        saveForm.addEventListener('submit', async e => {
            e.preventDefault();
            const nameInput = document.querySelector('#save-name');
            const errP = document.querySelector('#save-error');
            const name = nameInput.value.trim();
            if (!name || name.length > 60 || /[\\/:]/.test(name)) {
                errP.textContent = this.t('Enter a valid preset name without special characters.');
                return;
            }
            try {
                const res = await callNative('savePreset', { name });
                if (res.error) {
                    errP.textContent = res.error;
                    return;
                }
                this.syncState(res);
                document.querySelector('#save-dialog').close();
                this.notify(this.t('Saved preset') + ': ' + name);
            } catch (err) {
                errP.textContent = err.message;
            }
        });

        // 预设搜索
        document.querySelector('#preset-search').addEventListener('input', e => {
            this.renderPresetList(e.target.value.toLowerCase());
        });
    }

    renderManual() {
        const lang = this.ui.language === 'zh' ? 'zh' : 'en';
        const doc = manual[lang];
        const container = document.querySelector('#manual-content');
        container.innerHTML = `
            <h3>${doc.title}</h3>
            <p>${doc.intro}</p>
            ${doc.sections.map(s => `
                <details class="manual-section">
                    <summary>${s.title}</summary>
                    <div>${s.body}</div>
                </details>
            `).join('')}
        `;
    }

    openPresetDialog() {
        this.renderPresetList();
        this.open('presets-dialog');
    }

    openSaveDialog() {
        const input = document.querySelector('#save-name');
        input.value = this.currentPresetName || 'My Delay Preset';
        document.querySelector('#save-error').textContent = '';
        this.open('save-dialog');
        input.focus();
        input.select();
    }

    renderPresetList(filter = '') {
        const listContainer = document.querySelector('#preset-list');
        listContainer.innerHTML = '';
        const items = this.presets.filter(p => !filter || p.name.toLowerCase().includes(filter));
        const categories = ['Modern', 'Tape', 'BBD', 'Diffused'];
        const categoryNames = this.ui.language === 'zh' ? { Modern: 'Modern 数字', Tape: 'Tape 磁带', BBD: 'BBD 桶链', Diffused: 'Diffused 扩散' } : {};
        categories.forEach(category => {
            const groupItems = items.filter(p => (p.category || 'Modern') === category);
            if (!groupItems.length) return;
            const heading = document.createElement('h3');
            heading.className = 'preset-category';
            heading.textContent = categoryNames[category] || category;
            listContainer.appendChild(heading);
            groupItems.forEach(p => {
            const item = document.createElement('button');
            item.type = 'button';
            item.className = 'preset-item' + (p.name === this.currentPresetName ? ' active' : '');
            const name = document.createElement('span');
            name.className = 'preset-item-name'; name.textContent = p.name;
            const tag = document.createElement('span'); tag.className = 'preset-item-tag';
            tag.textContent = p.user ? (this.ui.language === 'zh' ? '用户' : 'USER') : (this.ui.language === 'zh' ? '出厂' : 'FACTORY');
            item.append(name, tag);
            item.addEventListener('click', async () => {
                const res = await callNative('loadPreset', { id: p.id });
                if (!res.error) {
                    this.syncState(res);
                    document.querySelector('#presets-dialog').close();
                }
            });
            listContainer.appendChild(item);
            });
        });
    }

    stepPreset(dir) {
        if (!this.presets.length) return;
        const curIdx = this.presets.findIndex(p => p.name === this.currentPresetName);
        let nextIdx = curIdx + dir;
        if (nextIdx < 0) nextIdx = this.presets.length - 1;
        if (nextIdx >= this.presets.length) nextIdx = 0;
        callNative('loadPreset', { id: this.presets[nextIdx].id }).then(s => this.syncState(s));
    }

    initTooltips() {
        const tooltipBox = document.querySelector('#tooltip');
        document.body.append(tooltipBox);
        let timer = null;

        document.addEventListener('pointerover', e => {
            if (this.ui.tips === 'off') return;
            const target = e.target.closest('[data-tip]');
            if (!target) return;
            clearTimeout(timer);
            const tipKey = target.dataset.tip;
            const lang = this.ui.tips === 'follow' ? this.ui.language : this.ui.tips;
            const text = tooltips[tipKey]?.[lang] || tooltips[tipKey]?.en || tipKey;

            timer = setTimeout(() => {
                tooltipBox.textContent = text;
                tooltipBox.hidden = false;
                const r = target.getBoundingClientRect();
                tooltipBox.style.left = `${Math.min(window.innerWidth - 220, Math.max(10, r.left + r.width / 2 - 100))}px`;
                tooltipBox.style.top = `${Math.min(window.innerHeight - tooltipBox.offsetHeight - 8, r.bottom + 8)}px`;
            }, 300);
        });

        document.addEventListener('pointerdown', () => { clearTimeout(timer); tooltipBox.hidden = true; });
        document.addEventListener('pointerout', e => {
            if (e.target.closest('[data-tip]')) {
                clearTimeout(timer);
                tooltipBox.hidden = true;
            }
        });
    }

    notify(msg) {
        const notif = document.querySelector('#notification');
        notif.textContent = msg;
        notif.hidden = false;
        setTimeout(() => { notif.hidden = true; }, 2500);
    }

    applyTheme(theme) {
        this.ui.theme = theme;
        document.documentElement.dataset.theme = theme;
        const r = document.querySelector(`input[name="theme"][value="${theme}"]`);
        if (r) r.checked = true;
        this.updateAll();
    }

    applyLanguage(lang) {
        this.ui.language = lang;
        document.documentElement.lang = lang;
        const r = document.querySelector(`input[name="language"][value="${lang}"]`);
        if (r) r.checked = true;

        document.querySelectorAll('[data-i18n]').forEach(el => {
            const key = el.dataset.i18n;
            el.textContent = this.t(key);
        });

        this.updateAll();
    }

    // 同一手势内立即绘制本地值；结束后再接受原生量化结果。
    scheduleSetDrain() {
        if (this.setFlushScheduled || this.setFlushInFlight || !this.pendingSetValues.size) return;
        this.setFlushScheduled = true;
        const run = () => { this.setFlushScheduled = false; this.flushPendingSets(); };
        if (typeof requestAnimationFrame === 'function') requestAnimationFrame(run); else setTimeout(run, 0);
    }

    queueSet(id, value) {
        this.pendingSetValues.set(id, value);
        this.scheduleSetDrain();
    }

    flushPendingSets() {
        if (this.setFlushInFlight || !this.pendingSetValues.size) return;
        const entries = [...this.pendingSetValues];
        this.pendingSetValues.clear();
        this.setFlushInFlight = true;
        let work = this.commandQueue || Promise.resolve();
        for (const [id, value] of entries) work = work.then(() => callNative('set', { id, value }).catch(e => this.notify(e.message)));
        this.commandQueue = work.catch(e => this.notify(e.message));
        this.setDrainPromise = this.commandQueue.finally(() => {
            this.setFlushInFlight = false;
            if (this.pendingSetValues.size) this.scheduleSetDrain();
        });
    }

    drainPendingSets() {
        if (!this.setFlushInFlight && this.pendingSetValues.size) this.flushPendingSets();
        return this.setDrainPromise.then(() => this.pendingSetValues.size ? this.drainPendingSets() : undefined);
    }

    begin() {
        this.gestureActive = true;
        this.gestureVersion = (this.gestureVersion || 0) + 1;
        this.commandQueue = (this.commandQueue || Promise.resolve()).then(() => callNative('begin'));
    }

    end() {
        const version = this.gestureVersion;
        this.commandQueue = this.drainPendingSets().then(() => callNative('end')).then(s => {
            if (version === this.gestureVersion) { this.gestureActive = false; this.syncState(s); }
        }).catch(e => { this.gestureActive = false; this.notify(e.message); });
    }

    set(id, value) {
        const d = this.schema[id];
        if (!d || !Number.isFinite(value)) return;
        value = Math.max(d.min, Math.min(d.max, value));
        if (d.step) value = d.min + Math.round((value - d.min) / d.step) * d.step;
        this.values[id] = value;
        if (this.gestureActive) this.queueSet(id, value);
        else this.commandQueue = (this.commandQueue || Promise.resolve()).then(() => callNative('set', { id, value })).catch(e => this.notify(e.message));
        this.updateAll();
    }

    syncState(state) {
        if (!state) return;
        if (state.values) Object.assign(this.values, state.values);
        if (state.presets) this.presets = state.presets;
        if (state.ui) Object.assign(this.ui, state.ui);
        if (state.preset) {
            this.currentPresetName = state.preset;
            document.querySelector('#preset-name').textContent = state.preset;
        }
        document.querySelector('#preset-dirty').hidden = !state.dirty;

        // A/B
        if (state.ab) {
            const radio = document.querySelector(`input[name="ab"][value="${state.ab}"]`);
            if (radio) radio.checked = true;
        }

        // 撤销 / 重做可用性
        document.querySelector('#undo').disabled = !state.canUndo;
        document.querySelector('#redo').disabled = !state.canRedo;

        this.updateAll();
    }

    onFrame(packet) {
        if (!packet) return;
        if (packet.values && !this.gestureActive) {
            const key = JSON.stringify([packet.values, packet.ui, packet.preset, packet.dirty, packet.ab, packet.canUndo, packet.canRedo]);
            if (key !== this.frameKey) { this.frameKey = key; this.syncState(packet); }
        }
        if (packet.meter) {
            this.duckMeter.update(packet.meter.duckGr);
            const bpm = Number(packet.meter.bpm);
            if (Number.isFinite(bpm) && bpm > 0 && Math.abs(bpm - this.hostBpm) > 0.05) {
                this.hostBpm = bpm;
                this.knobs.bpm.update();
                this.visualizer.render();
            }
        }
    }

    updateAll() {
        const currentEngine = Math.round(this.values.engine ?? 0);
        document.querySelector('.instrument').dataset.engine = String(currentEngine);
        if (this.knobChar1Container && this.characterEngine !== currentEngine) this.rebuildEngineCharKnobs();
        document.documentElement.dataset.theme = this.ui.theme;
        document.documentElement.lang = this.ui.language;
        document.querySelectorAll('[data-i18n]').forEach(el => {
            const value = this.t(el.dataset.i18n);
            if (el.textContent !== value) el.textContent = value;
        });
        document.querySelectorAll('button[data-tip]').forEach(button => {
            const tip = tooltips[button.dataset.tip];
            if (tip) button.setAttribute('aria-label', tip[this.ui.language] || tip.en);
        });
        // 更新旋钮
        Object.values(this.knobs).forEach(k => k.update());

        // 更新模式按钮状态
        const engine = Math.round(this.values['engine'] || 0);
        const rEng = document.querySelector(`input[name="engine"][value="${engine}"]`);
        if (rEng) rEng.checked = true;
        document.querySelector('.instrument').dataset.engine = String(engine);

        const topology = Math.round(this.values['topology'] || 0);
        const rTopo = document.querySelector(`input[name="topology"][value="${topology}"]`);
        if (rTopo) rTopo.checked = true;

        const syncMode = Math.round(this.values['sync_mode'] ?? 1);
        const rSync = document.querySelector(`input[name="sync_mode"][value="${syncMode}"]`);
        if (rSync) rSync.checked = true;

        const isLink = (this.values['time_link'] || 0) > 0.5;
        document.querySelector('#btn-time-link').setAttribute('aria-pressed', String(isLink));

        const isFreeze = (this.values['freeze'] || 0) > 0.5;
        document.querySelector('#btn-freeze').setAttribute('aria-pressed', String(isFreeze));


        const isByp = (this.values['global_bypass'] || 0) > 0.5;
        document.querySelector('#bypass').setAttribute('aria-pressed', String(isByp));

        const duckEn = (this.values['duck_enable'] || 0) > 0.5;
        document.querySelector('#duck-enable').checked = duckEn;

        const duckSrc = Math.round(this.values['duck_source'] || 0);
        const rSrc = document.querySelector(`input[name="duck_source"][value="${duckSrc}"]`);
        if (rSrc) rSrc.checked = true;

        // 同步设置面板控件
        for (const key of ['theme', 'language', 'tips', 'scale']) {
            document.querySelectorAll(`input[name="${key}"]`).forEach(e => {
                e.checked = key === 'scale'
                    ? Math.abs(Number(e.value) - (this.ui.scale || 1)) < 0.001
                    : e.value === this.ui[key];
            });
        }
        const chkVisual = document.querySelector('#history-enabled');
        if (chkVisual) chkVisual.checked = this.ui.history !== false;

        this.updateLayoutBySyncMode();
        this.visualizer?.render();
    }
}

window.addEventListener('DOMContentLoaded', () => {
    window.sanmuApp = new App();
    window.sanmuApp.init();
});
