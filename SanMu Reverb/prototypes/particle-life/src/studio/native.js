const backend = window.__JUCE__?.backend;
export const isNative = !!window.__JUCE__?.initialisationData?.sanmu;
let nextId = 0;
const pending = new Map();
if (isNative) backend.addEventListener('__juce__complete', ({ promiseId, result }) => {
  const operation = pending.get(promiseId);
  if (!operation) return;
  clearTimeout(operation.timer); pending.delete(promiseId);
  if (result?.error) operation.reject(new Error(result.error));
  else operation.resolve(result);
});
export function callNative(command, data = {}) {
  if (!isNative) return Promise.resolve(null);
  const resultId = nextId++;
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => { pending.delete(resultId); reject(new Error('插件连接超时，请重新打开窗口。')); }, 5000);
    pending.set(resultId, { resolve, reject, timer });
    backend.emitEvent('__juce__invoke', { name: 'sanmu', params: [command, data], resultId });
  });
}
export function onNativeFrame(listener) {
  if (isNative) backend.addEventListener('sanmuFrame', listener);
}
export class NativeAudio {
  constructor() {
    this.context = { sampleRate: 48000, get currentTime() { return performance.now() / 1000; } };
    this.ready = true;
  }
  setParameters() {}
}
