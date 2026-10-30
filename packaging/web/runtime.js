import dialogPolyfill from 'dialog-polyfill';
import ResizeObserverFallback from 'resize-observer-polyfill';
import { installFlexGapFallback } from './flex-gap.js';

// 编译器只转换语法；旧版 WebKit 缺失的接口在入口执行前补齐。
// Catalina 的 WebKit 没有 Object.hasOwn；统一回退到 hasOwnProperty，避免页面
// 已经绘制后在翻译/初始化阶段抛错，导致旋钮和宿主桥接都停止。
if (!Object.hasOwn) Object.hasOwn = (object, property) => Object.prototype.hasOwnProperty.call(object, property);
// Catalina 也没有 crypto.randomUUID；仅用于本地预设 ID，不参与音频或授权。
if (window.crypto && !window.crypto.randomUUID) window.crypto.randomUUID = () => 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, character => {
  const random = Math.random() * 16 | 0;
  const value = character === 'x' ? random : (random & 3 | 8);
  return value.toString(16);
});
for (const prototype of [Element.prototype, DocumentFragment.prototype]) {
  if (!prototype.replaceChildren) Object.defineProperty(prototype, 'replaceChildren', {
    configurable: true, writable: true,
    value: function (...nodes) {
      const fragment = document.createDocumentFragment();
      for (const node of nodes) fragment.appendChild(node instanceof Node ? node : document.createTextNode(String(node)));
      while (this.firstChild) this.removeChild(this.firstChild);
      this.appendChild(fragment);
    }
  });
}
if (!Array.prototype.at) Object.defineProperty(Array.prototype, 'at', {
  configurable: true, writable: true,
  value: function (index) {
    const length = this.length;
    let offset = Math.trunc(Number(index) || 0);
    if (offset < 0) offset += length;
    return offset < 0 || offset >= length ? undefined : this[offset];
  }
});
if (!window.structuredClone) window.structuredClone = value => JSON.parse(JSON.stringify(value));
if (!window.queueMicrotask) window.queueMicrotask = callback => {
  Promise.resolve().then(callback).catch(error => setTimeout(() => { throw error; }, 0));
};
if (!window.ResizeObserver) window.ResizeObserver = ResizeObserverFallback;

// Catalina 尚无原生 dialog；静态和动态插入的弹窗都需要注册。
const registerDialogs = root => {
  if (root.matches && root.matches('dialog')) dialogPolyfill.registerDialog(root);
  if (root.querySelectorAll) root.querySelectorAll('dialog').forEach(dialog => dialogPolyfill.registerDialog(dialog));
};
registerDialogs(document);
if (typeof document.createElement('dialog').showModal !== 'function') {
  new MutationObserver(records => records.forEach(record => record.addedNodes.forEach(registerDialogs)))
    .observe(document.documentElement, { childList: true, subtree: true });
}

// 启动异常必须可见，避免只留下无提示的空白窗口。
window.addEventListener('error', event => {
  if (!event.message) return;
  document.documentElement.classList.remove('native-loading');
  let message = document.getElementById('sanmu-startup-error');
  if (!message) {
    message = document.createElement('p'); message.id = 'sanmu-startup-error';
    message.setAttribute('role', 'alert'); document.body.appendChild(message);
  }
  message.textContent = '界面加载异常，请记录此信息并重新打开插件：' + event.message;
});
window.__sanmuCompatibility = { target: 'safari13', installed: true };

if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', installFlexGapFallback, { once: true });
else installFlexGapFallback();
