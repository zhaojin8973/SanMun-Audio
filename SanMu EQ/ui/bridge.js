const backend = window.__JUCE__?.backend;
export const isNative = !!window.__JUCE__?.initialisationData?.sanmuEq;
let nextId = 0; const pending = new Map();
if (isNative) backend.addEventListener('__juce__complete', ({promiseId, result}) => {
  const op = pending.get(promiseId); if (!op) return;
  clearTimeout(op.timer); pending.delete(promiseId);
  result?.error ? op.reject(new Error(result.error)) : op.resolve(result);
});
export async function callNative(command, data = {}) {
  if (!isNative) { const {preview} = await import('./preview.js'); return preview(command, data); }
  const resultId = nextId++;
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => { pending.delete(resultId); reject(new Error('插件连接超时')); }, 5000);
    pending.set(resultId, {resolve, reject, timer});
    backend.emitEvent('__juce__invoke', {name: 'sanmu', params: [command, data], resultId});
  });
}
export function onFrame(listener) { if (isNative) backend.addEventListener('sanmuFrame', listener); }
