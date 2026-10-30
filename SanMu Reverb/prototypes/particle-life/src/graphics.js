import * as ModernThree from 'three';
import * as WebGL1Three from 'three-webgl1';

let backend;

export function graphicsBackend() {
  if (backend) return backend;
  // Catalina 的 WebKit 没有 WebGL 2；保留现代渲染器，旧驱动使用仍支持 WebGL 1 的同源实现。
  if (typeof document === 'undefined') return (backend = ModernThree);
  let context = null;
  if (typeof WebGL2RenderingContext !== 'undefined') {
    const probe = document.createElement('canvas');
    try { context = probe.getContext('webgl2', { antialias: false, powerPreference: 'low-power' }); }
    catch { /* 驱动拒绝 WebGL 2 时继续尝试 WebGL 1。 */ }
  }
  backend = context ? ModernThree : WebGL1Three;
  // 能力探测的临时上下文立即释放，避免多实例同时占用显卡上下文名额。
  context?.getExtension('WEBGL_lose_context')?.loseContext();
  return backend;
}
