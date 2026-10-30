// 复用 Reverb 的离线 JUCE 请求协议；始终由原生参数系统持有声音状态。
const backend=window.__JUCE__?.backend;
export const isNative=!!window.__JUCE__?.initialisationData?.sanmuComp;
let nextId=0;const pending=new Map();
if(isNative)backend.addEventListener('__juce__complete',({promiseId,result})=>{
 const op=pending.get(promiseId);if(!op)return;clearTimeout(op.timer);pending.delete(promiseId);
 if(result?.error)op.reject(new Error(result.error));else op.resolve(result);
});
export function callNative(command,data={}){
 if(!isNative)return window.__compPreview.call(command,data);
 const resultId=nextId++;return new Promise((resolve,reject)=>{
  const timer=setTimeout(()=>{pending.delete(resultId);reject(new Error('插件连接超时，请重新打开窗口。'));},5000);
  pending.set(resultId,{resolve,reject,timer});backend.emitEvent('__juce__invoke',{name:'sanmu',params:[command,data],resultId});
 });
}
export function onNativeFrame(listener){if(isNative)backend.addEventListener('sanmuFrame',listener);else window.addEventListener('compPreviewFrame',e=>listener(e.detail));}
