import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { chromium } from 'playwright';
import { presets } from '../src/studio/presets.js';

// 用真正发布进 AAX 的静态资源复现旧 WebKit 缺失的接口，并模拟 JUCE 原生桥接。
const current = path.dirname(fileURLToPath(import.meta.url));
const root = process.env.SANMU_NATIVE_UI_DIR
  ? path.resolve(process.env.SANMU_NATIVE_UI_DIR)
  : path.resolve(current, '../../../build-release-20260928-make/source/native-ui-dist');
const mime = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css' };
const server = createServer(async (request, response) => {
  const filename = path.resolve(root, '.' + decodeURIComponent(new URL(request.url, 'http://localhost').pathname));
  if (!filename.startsWith(root + path.sep)) { response.writeHead(403).end(); return; }
  try {
    const bytes = await readFile(filename);
    response.writeHead(200, { 'Content-Type': mime[path.extname(filename)] || 'application/octet-stream' }).end(bytes);
  } catch { response.writeHead(404).end(); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const browser = await chromium.launch({
  executablePath: process.env.SANMU_CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  headless: true,
});
try {
  const page = await browser.newPage({ viewport: { width: 1100, height: 700 } });
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.addInitScript(initial => {
    // 保持现代 Chrome 的渲染器，只剔除 Catalina 没有的接口。
    Object.hasOwn = undefined;
    Array.prototype.at = undefined;
    window.ResizeObserver = undefined;
    window.structuredClone = undefined;
    window.queueMicrotask = undefined;
    Element.prototype.replaceChildren = undefined;
    DocumentFragment.prototype.replaceChildren = undefined;
    const listeners = new Map();
    const notify = (name, value) => (listeners.get(name) || []).forEach(listener => listener(value));
    const state = { ...initial, values: { ...initial.values } };
    window.__legacyBridge = { state, notify };
    window.__JUCE__ = {
      initialisationData: { sanmu: 'catalina-test' },
      backend: {
        addEventListener(name, listener) {
          if (!listeners.has(name)) listeners.set(name, []);
          listeners.get(name).push(listener);
        },
        emitEvent(name, event) {
          if (name !== '__juce__invoke') return;
          const [command, data] = event.params;
          if (command === 'apply') Object.assign(state, { values: { ...data.values }, mode: data.mode, bypass: data.bypass });
          if (command === 'patch') {
            Object.assign(state.values, data.values);
            if (Object.hasOwn(data, 'mode')) state.mode = data.mode;
            if (Object.hasOwn(data, 'bypass')) state.bypass = data.bypass;
          }
          const result = ['initialize', 'apply', 'patch', 'end'].includes(command) ? { ...state, values: { ...state.values } } : true;
          Promise.resolve().then(() => notify('__juce__complete', { promiseId: event.resultId, result }));
        },
      },
    };
  }, { values: presets[0].values, mode: 0, bypass: false, presets: [], ui: {}, sampleRate: 48000, channels: 2, build: 'compat-test' });
  await page.goto(`http://127.0.0.1:${server.address().port}/studio.html`);
  await page.waitForFunction(() => window.__sanmuStudio?.nativeReady === true, { timeout: 10000 });
  assert.equal(await page.locator('[data-knob]').count(), 15, '旧接口环境中 15 个旋钮应全部初始化');
  assert.equal(await page.locator('#sanmu-startup-error').count(), 0, '旧接口环境不能出现启动错误');
  assert(await page.evaluate(() => Object.hasOwn({ decay: 1 }, 'decay')));

  const before = await page.evaluate(() => window.__legacyBridge.state.values.decay);
  await page.locator('#knob-decay').focus();
  await page.keyboard.press('ArrowUp');
  await page.waitForFunction(previous => window.__legacyBridge.state.values.decay > previous, before);
  assert.equal(await page.locator('#knob-decay').getAttribute('aria-valuenow'), String(await page.evaluate(() => window.__legacyBridge.state.values.decay)));

  await page.evaluate(() => {
    const bridge = window.__legacyBridge;
    bridge.state.values.decay = 7.5;
    bridge.notify('sanmuFrame', { ...bridge.state, frames: [{
      dry: [.1, .2, .1, .2], wet: [.05, .08, .02, .02], peaks: [.25, .2, .15, .1],
      duration: .01, dryRms: .1, wetRms: .05, peak: .25, pan: 0, wetPan: 0, correlation: 0,
    }] });
  });
  await page.waitForFunction(() => window.__sanmuStudio.parameters.decay === 7.5);
  assert(Number(await page.locator('#meter-0').evaluate(element => element.parentElement.getAttribute('aria-valuenow'))) > -60,
    '原生帧必须驱动电平表');
  assert.equal(errors.length, 0, errors.join('\n'));
  console.log('Catalina 缺失接口环境：旋钮写入、宿主参数回传、电平帧和启动无错误，全部通过');
} finally {
  await browser.close();
  await new Promise(resolve => server.close(resolve));
}
