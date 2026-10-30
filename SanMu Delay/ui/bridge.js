// ==============================================================================
// bridge.js - SanMu Delay 离线 JUCE 请求协议与原生桥接
// 始终由原生参数系统持有声音状态与 DSP 权威
// ==============================================================================

const backend = window.__JUCE__?.backend;
export const isNative = !!window.__JUCE__?.initialisationData?.sanmuDelay;
let nextId = 0;
const pending = new Map();

if (isNative) {
    backend.addEventListener('__juce__complete', ({ promiseId, result }) => {
        const op = pending.get(promiseId);
        if (!op) return;
        clearTimeout(op.timer);
        pending.delete(promiseId);
        if (result?.error) op.reject(new Error(result.error));
        else op.resolve(result);
    });
}

export function callNative(command, data = {}) {
    if (!isNative) {
        return window.__delayPreview ? window.__delayPreview.call(command, data) : Promise.resolve({});
    }
    const resultId = nextId++;
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => {
            pending.delete(resultId);
            reject(new Error('插件连接超时，请重新打开窗口。'));
        }, 5000);
        pending.set(resultId, { resolve, reject, timer });
        backend.emitEvent('__juce__invoke', { name: 'sanmu', params: [command, data], resultId });
    });
}

export function onNativeFrame(listener) {
    if (isNative) {
        backend.addEventListener('sanmuFrame', listener);
    } else {
        window.addEventListener('delayPreviewFrame', e => listener(e.detail));
    }
}
