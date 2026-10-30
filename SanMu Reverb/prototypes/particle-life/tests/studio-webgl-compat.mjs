import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile, mkdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { chromium } from 'playwright';
import { presets } from '../src/studio/presets.js';

// 验证真正发布的静态界面；图形分支必须实际渲染，而不能只检查初始化标志。
const current = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(process.env.SANMU_NATIVE_UI_DIR || path.join(current, '../../../build-release-20260928-make/source/native-ui-dist'));
const output = path.resolve(process.env.SANMU_WEBGL_TEST_OUTPUT || 'test-results/webgl-compat');
await mkdir(output, { recursive: true });
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
  executablePath: process.env.SANMU_CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome', headless: true,
});
const report = { passed: false, paths: [] };
try {
  for (const legacy of [false, true]) {
    const page = await browser.newPage({ viewport: { width: 1100, height: 750 }, deviceScaleFactor: 2 });
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    page.on('console', message => {
      if (message.type() === 'error' && /WebGL|shader|Shader|GL_INVALID/.test(message.text())) errors.push(message.text());
    });
    await page.addInitScript(({ initial, legacy }) => {
      if (legacy) {
        const getContext = HTMLCanvasElement.prototype.getContext;
        HTMLCanvasElement.prototype.getContext = function (name, ...options) {
          return name === 'webgl2' ? null : getContext.call(this, name, ...options);
        };
        window.WebGL2RenderingContext = undefined;
        // Catalina 同样没有原生弹窗，需要实际走 polyfill 的打开、定位和键盘关闭路径。
        const dialogPrototype = window.HTMLDialogElement.prototype;
        for (const name of ['showModal', 'show', 'close']) delete dialogPrototype[name];
        window.HTMLDialogElement = undefined;
      }
      const listeners = new Map();
      const notify = (name, value) => (listeners.get(name) || []).forEach(listener => listener(value));
      const state = { ...initial, values: { ...initial.values } };
      window.__graphicsBridge = { state, notify };
      window.__JUCE__ = { initialisationData: { sanmu: 'graphics-compat' }, backend: {
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
            if (Object.prototype.hasOwnProperty.call(data, 'mode')) state.mode = data.mode;
            if (Object.prototype.hasOwnProperty.call(data, 'bypass')) state.bypass = data.bypass;
          }
          const result = ['initialize', 'apply', 'patch', 'end'].includes(command) ? { ...state, values: { ...state.values } } : true;
          Promise.resolve().then(() => notify('__juce__complete', { promiseId: event.resultId, result }));
        },
      } };
    }, { initial: { values: presets[0].values, mode: 0, bypass: false, presets: [], ui: {}, sampleRate: 48000, channels: 2, build: 'graphics-compat' }, legacy });
    await page.goto(`http://127.0.0.1:${server.address().port}/studio.html`);
    await page.waitForFunction(() => window.__sanmuStudio?.nativeReady && window.__sanmuStudio.scene?.frameCount > 2);
    const webgl2 = await page.evaluate(() => window.__sanmuStudio.scene.renderer.getContext() instanceof WebGLRenderingContext === false);
    assert.equal(webgl2, !legacy, '现代环境继续使用 WebGL 2；禁用 WebGL 2 后必须使用 WebGL 1');
    await page.evaluate(() => {
      let index = 0;
      window.__signalTimer = setInterval(() => {
        const { state, notify } = window.__graphicsBridge;
        const signal = .02 + .015 * (1 + Math.sin(++index / 7));
        notify('sanmuFrame', { ...state, frames: [{
          dry: [signal, signal * .7, signal * .3, signal * .1], wet: [signal * .6, signal * .4, signal * .2, signal * .1],
          peaks: [.25, .2, .15, .1], duration: .033, dryRms: .1, wetRms: .05, peak: .25,
          pan: Math.sin(index / 10) * .2, wetPan: 0, correlation: .2,
        }] });
      }, 33);
    });
    const branch = { legacy, surfaces: [], scales: [] };
    for (const [theme, surface] of [['light', 'water'], ['dark', 'dust']]) {
      await page.evaluate(theme => window.__sanmuStudio.setTheme(theme), theme);
      await page.waitForTimeout(350);
      const rendered = await page.evaluate(() => {
        const { scene } = window.__sanmuStudio;
        const context = scene.renderer.getContext();
        scene.renderer.render(scene.scene, scene.camera);
        const pixels = new Uint8Array(context.drawingBufferWidth * context.drawingBufferHeight * 4);
        context.readPixels(0, 0, context.drawingBufferWidth, context.drawingBufferHeight, context.RGBA, context.UNSIGNED_BYTE, pixels);
        let visible = 0;
        for (let i = 3; i < pixels.length; i += 4) if (pixels[i] > 2) visible++;
        return { surface: scene.surface, visible, frames: scene.frameCount, particles: scene.life.count,
          phase: scene.water.uniforms.phase.value.toArray(), width: context.drawingBufferWidth, height: context.drawingBufferHeight };
      });
      assert.equal(rendered.surface, surface);
      assert(rendered.visible > 80, `${surface} 必须真正产生可见像素：${JSON.stringify(rendered)}`);
      if (surface === 'water') assert(rendered.phase.some((value, i) => value > [.1, 1.7, 3.1, 4.6][i]), '水波相位必须随宿主音频帧推进');
      await page.locator('#stage').screenshot({ path: path.join(output, `${legacy ? 'webgl1' : 'webgl2'}-${surface}.png`) });
      branch.surfaces.push(rendered);
    }
    for (const scale of [.85, 1, 1.15, 1.25, 1.5]) {
      await page.setViewportSize({ width: Math.round(1100 * scale), height: Math.round(750 * scale) });
      await page.evaluate(scale => window.__sanmuStudio.setUiScale(scale), scale);
      await page.waitForTimeout(100);
      const sizes = await page.locator('#knob-decay').evaluate(dial => ({
        display: dial.getBoundingClientRect().width,
        backing: dial.querySelector('canvas').width, dpr: devicePixelRatio,
      }));
      assert(Math.abs(sizes.backing - sizes.display * Math.min(2, sizes.dpr)) <= 1.1, '每档缩放必须按实际显示尺寸绘制旋钮位图');
      await page.locator('#settings').click();
      const panel = await page.locator('#settings-dialog').evaluate(dialog => {
        const box = dialog.getBoundingClientRect();
        return { left: box.left, right: box.right, top: box.top, bottom: box.bottom,
          width: box.width, height: box.height, viewportWidth: innerWidth, viewportHeight: innerHeight,
          parent: dialog.parentElement.tagName, position: getComputedStyle(dialog).position,
          zoom: getComputedStyle(dialog).zoom, scrollHeight: dialog.scrollHeight, clientHeight: dialog.clientHeight };
      });
      assert.equal(panel.parent, 'BODY', '弹窗必须处于缩放容器之外');
      assert.equal(panel.position, 'fixed', '旧弹窗也必须固定在视口内');
      assert(panel.left >= 0 && panel.top >= 0 && panel.right <= panel.viewportWidth + 1 && panel.bottom <= panel.viewportHeight + 1,
        `每档缩放的设置面板必须完整处于视口内：${JSON.stringify({ scale, ...panel })}`);
      if (scale === .85 || scale === 1.5) {
        await page.screenshot({ path: path.join(output, `${legacy ? 'legacy-dialog' : 'native-dialog'}-${Math.round(scale * 100)}.png`) });
      }
      await page.locator('#open-help').click();
      await page.waitForFunction(() => document.querySelector('#help-dialog').open);
      const help = await page.locator('#help-dialog').boundingBox();
      assert(help.x >= 0 && help.y >= 0 && help.x + help.width <= panel.viewportWidth + 1 && help.y + help.height <= panel.viewportHeight + 1,
        '帮助面板也不能越出视口');
      await page.keyboard.press('Escape');
      await page.waitForFunction(() => !document.querySelector('#help-dialog').open);
      assert(await page.locator('#settings-dialog').evaluate(dialog => dialog.open), '关闭帮助必须保留下层设置');
      await page.keyboard.press('Escape');
      await page.waitForFunction(() => !document.querySelector('#settings-dialog').open);
      await page.locator('#settings').click();
      await page.locator('#close-settings').click();
      assert.equal(await page.locator('#settings-dialog').evaluate(dialog => dialog.open), false, '关闭按钮必须正常工作');
      branch.scales.push({ scale, ...sizes, panel });
    }
    await page.locator('#knob-decay').focus();
    assert.equal(await page.locator('#knob-decay').evaluate(dial => getComputedStyle(dial).outlineStyle), 'none', '旋钮拖动不能出现系统方形焦点框');
    const phaseBefore = await page.evaluate(() => window.__sanmuStudio.scene.frameCount);
    await page.waitForTimeout(100);
    assert(await page.evaluate(() => window.__sanmuStudio.scene.frameCount) > phaseBefore, '缩放后图形循环仍应运行');
    assert.deepEqual(errors, []);
    branch.errors = errors; report.paths.push(branch);
    await page.close();
  }
  report.passed = true;
  console.log('现代 WebGL 2 与 Catalina WebGL 1 分支：水波/粒子实际像素、原生帧联动、五档旋钮清晰度、旧弹窗边界、键盘关闭和焦点框检查全部通过');
} finally {
  await writeFile(path.join(output, 'report.json'), JSON.stringify(report, null, 2));
  await browser.close();
  await new Promise(resolve => server.close(resolve));
}
