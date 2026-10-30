// ==============================================================================
// preview.js - 仅用于独立浏览器开发预览，不进入插件资源包
// ==============================================================================

export async function installPreview() {
    const original = await (await fetch('./parameter-schema.json')).json();
    const state = structuredClone(original);
    const categoryFor = engine => ['Modern', 'Tape', 'BBD', 'Diffused'][engine] || 'Modern';
    state.presets = (state.presets || []).map(p => ({ ...p, category: p.category || categoryFor(p.values?.engine) }));
    const extra = [
        ['Tape Slapback', 1, { sync_mode: 2, time_l: 105, time_r: 118, time_link: 0, feedback: 12, tape_drive: 24, tape_wow: 8, damp_highcut: 9000, mix: 28 }],
        ['BBD Chorus Echo', 2, { sync_mode: 0, division: 8, division_r: 8, time_link: 1, feedback: 32, bbd_warmth: 72, mod_rate: .8, mod_depth: 24, damp_highcut: 7200, mix: 34 }],
        ['Diffused Plate Wash', 3, { sync_mode: 0, division: 5, division_r: 7, time_link: 0, feedback: 58, diffusion_amount: 74, damp_lowcut: 120, damp_highcut: 10500, mod_rate: .5, mod_depth: 16, mix: 42 }]
    ];
    const template = state.presets[0];
    extra.forEach(([name, engine, values], i) => state.presets.push({ id: `factory:${8 + i}`, name, category: categoryFor(engine), user: false, values: { ...template.values, engine, ...values } }));
    state.canUndo = false;
    state.canRedo = false;
    state.ab = 'A';
    let start = null;
    const undo = [], redo = [], ab = { A: structuredClone(state.values), B: structuredClone(state.values) };
    const snapshot = () => structuredClone(state.values);
    const response = () => ({ ...structuredClone(state), canUndo: !!undo.length, canRedo: !!redo.length });

    window.__delayPreview = {
        async call(cmd, data = {}) {
            if (cmd === 'initialize' || cmd === 'state') return response();
            if (cmd === 'begin') { start = snapshot(); return true; }
            if (cmd === 'set') {
                const d = state.schema[data.id];
                if (!d || !Number.isFinite(data.value) || data.value < d.min || data.value > d.max) {
                    throw Error('参数值超出范围');
                }
                state.values[data.id] = data.value;
                state.dirty = true;
            }
            if (cmd === 'end' && start) {
                if (JSON.stringify(start) !== JSON.stringify(state.values)) {
                    undo.push(start);
                    redo.length = 0;
                }
                start = null;
            }
            if (cmd === 'ui') Object.assign(state.ui, data);
            if (cmd === 'undo' && undo.length) { redo.push(snapshot()); state.values = undo.pop(); }
            if (cmd === 'redo' && redo.length) { undo.push(snapshot()); state.values = redo.pop(); }
            if (cmd === 'ab' && ['A', 'B'].includes(data.slot)) {
                ab[state.ab] = snapshot();
                state.ab = data.slot;
                state.values = structuredClone(ab[data.slot]);
            }
            if (cmd === 'copyAB') ab[state.ab === 'A' ? 'B' : 'A'] = snapshot();
            if (cmd === 'loadPreset') {
                const p = state.presets.find(item => item.id === data.id);
                if (p) { undo.push(snapshot()); redo.length=0; state.values=structuredClone(p.values); state.preset=p.name; state.dirty=false; }
            }
            if (cmd === 'savePreset') {
                state.preset = data.name;
                state.presets.push({ id: 'user:' + data.name, name: data.name, user: true, values: snapshot() });
            }
            return response();
        }
    };

    // 模拟 30Hz 遥测帧
    setInterval(() => {
        const time = performance.now();
        const demo = new URLSearchParams(location.search).get('meters') === '1';
        const outputL = demo ? (-18 + 8 * Math.sin(time / 200)) : -100;
        const outputR = demo ? (-20 + 7 * Math.cos(time / 250)) : -100;
        const wetL = demo ? (-24 + 10 * Math.sin(time / 400)) : -100;
        const wetR = demo ? (-26 + 10 * Math.cos(time / 400)) : -100;
        const duckGr = 0;

        window.dispatchEvent(new CustomEvent('delayPreviewFrame', {
            detail: {
                ...response(),
                meter: {
                    time,
                    outputL,
                    outputR,
                    wetL,
                    wetR,
                    duckGr,
                    bpm: state.bpm || 120.0
                }
            }
        }));
    }, 33);
}
