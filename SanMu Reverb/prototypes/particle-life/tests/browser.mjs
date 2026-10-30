import { chromium } from 'playwright';
import { PNG } from 'pngjs';
import assert from 'node:assert/strict';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('..', import.meta.url));
const artifacts = root + '/test-results';
mkdirSync(artifacts, { recursive: true });
const compiled = spawnSync('clang++', ['-std=c++20', '-O3', 'tests/native-reference.cpp', '-o', `${artifacts}/native-reference`], { cwd: root, encoding: 'utf8' });
assert.equal(compiled.status, 0, compiled.stderr);
const browser = await chromium.launch({ executablePath: process.env.SANMU_CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome', headless: true,
  args: ['--autoplay-policy=no-user-gesture-required'] });
const report = { passed: false, audio: [], visual: [], errors: [] };
const url = process.env.SANMU_PROTOTYPE_URL || 'http://127.0.0.1:5173';
const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
page.on('pageerror', error => report.errors.push(error.message));
page.on('console', message => { if (message.type() === 'error') report.errors.push(message.text()); });

function pixels(buffer) {
  const png = PNG.sync.read(buffer);
  let active = 0, colour = 0;
  for (let i = 0; i < png.data.length; i += 4) {
    const [r, g, b] = png.data.subarray(i, i + 3);
    if (Math.max(r, g, b) > 44) active++;
    if (Math.max(r, g, b) - Math.min(r, g, b) > 24 && Math.max(r, g, b) > 50) colour++;
  }
  return { active, colour, total: png.width * png.height };
}

try {
  await page.goto(url);
  await page.waitForFunction(() => !!window.__sanmu?.scene);
  for (const rate of [44100, 48000]) {
    for (const mode of [0, 1]) {
      const file = `${artifacts}/reference-${rate}-${mode}.f32`;
      assert.equal(spawnSync(`${artifacts}/native-reference`, [file, String(mode), String(rate)]).status, 0);
      const reference = readFileSync(file).toString('base64');
      const result = await page.evaluate(async ({ reference, rate, mode }) => {
        const { default: createHall } = await import('/audio/hall-dsp.js');
        const module = await WebAssembly.compile(await (await fetch('/audio/hall-dsp.wasm')).arrayBuffer());
        const dsp = await createHall({ instantiateWasm(imports, receive) {
          const instance = new WebAssembly.Instance(module, imports); receive(instance); return instance.exports;
        } });
        dsp._hall_init(rate);
        const p = dsp._hall_parameters() / 4;
        const values = mode ? [1.7, 100, 4000, -12, 220, .85, 35, 85, 0, 25, 1.3, 0, 120, 5100]
          : [3.99, 20, 6000, -24, 300, 1.5, 100, 50, 100, 100, 2.53, 38, 10, 8010];
        dsp.HEAPF32.set(values, p); dsp._hall_commit();
        const bytes = Uint8Array.from(atob(reference), character => character.charCodeAt(0));
        const expected = new Float32Array(bytes.buffer);
        const offset = dsp._hall_buffer() / 4;
        let peak = 0, maxError = 0, first = -1;
        for (let start = 0; start < rate * 3; start += 128) {
          const count = Math.min(128, rate * 3 - start);
          dsp.HEAPF32.fill(0, offset, offset + 256);
          const impulse = Math.floor(rate / 10) - start;
          if (impulse >= 0 && impulse < count) { dsp.HEAPF32[offset + impulse] = .25; dsp.HEAPF32[offset + 128 + impulse] = .2; }
          dsp._hall_process(count);
          for (let i = 0; i < count; i++) for (let channel = 0; channel < 2; channel++) {
            const actual = dsp.HEAPF32[offset + 256 + channel * 128 + i];
            if (!Number.isFinite(actual)) throw new Error('音频出现非有限值');
            peak = Math.max(peak, Math.abs(actual));
            if (Math.abs(actual) > 1e-8 && first < 0) first = start + i;
            maxError = Math.max(maxError, Math.abs(actual - expected[(start + i) * 2 + channel]));
          }
        }
        return { rate, mode, maxError, peak, first };
      }, { reference, rate, mode });
      assert(result.maxError < .00001, JSON.stringify(result));
      assert(result.peak > .001 && result.peak < 1, JSON.stringify(result));
      assert(result.first >= Math.floor(rate * (.1 + (mode ? .1 : .02) + .002)) - 1);
      report.audio.push(result);
      console.log('声音核对通过', result);
    }
  }

  const silent = pixels(await page.locator('canvas').screenshot());
  await page.getByRole('button', { name: '播放', exact: true }).click();
  await page.mouse.move(8, 8);
  await page.waitForFunction(() => window.__sanmu.audio.latest?.wetRms > .005 && window.__sanmu.scene.life.count > 500, { timeout: 12000 });
  await page.waitForTimeout(2800);
  const activeBuffer = await page.locator('canvas').screenshot();
  const active = pixels(activeBuffer);
  assert(active.colour > silent.colour + 500, JSON.stringify({ silent, active }));
  await page.screenshot({ path: `${artifacts}/desktop.png` });
  const positionsBefore = await page.evaluate(() => Array.from(window.__sanmu.scene.life.position.slice(0, 300)));
  await page.waitForTimeout(300);
  const positionsAfter = await page.evaluate(() => Array.from(window.__sanmu.scene.life.position.slice(0, 300)));
  assert.notDeepEqual(positionsAfter, positionsBefore);
  assert(!activeBuffer.equals(await page.locator('canvas').screenshot()));
  report.visual.push({ viewport: '1440×900', silent, active });

  await page.getByRole('button', { name: '停止输入，保留尾音', exact: true }).click();
  await page.waitForTimeout(100);
  const born = await page.evaluate(() => window.__sanmu.scene.life.bornTotal);
  await page.waitForTimeout(400);
  assert.equal(await page.evaluate(() => window.__sanmu.scene.life.bornTotal), born, '停止输入后仍生成新粒子');
  assert((await page.evaluate(() => window.__sanmu.audio.latest.wetRms)) > 0, '停止输入误切断尾音');
  assert.equal(await page.evaluate(() => Math.max(...window.__sanmu.scene.dry.opacity)), 0, '纯混响尾音点亮了原声层');
  await page.getByRole('tab', { name: '调制', exact: true }).click();
  await page.locator('#param-mix').fill('0');
  await page.waitForTimeout(900);
  const hiddenWet = await page.evaluate(() => ({ analysis: Math.max(...window.__sanmu.scene.life.opacity), gain: window.__sanmu.scene.wetGain }));
  assert(hiddenWet.analysis > .001, '零 MIX 清空了内部混响状态');
  assert(hiddenWet.gain < .001, '叠加的混响没有跟随 MIX 消隐');
  await page.getByRole('button', { name: '恢复默认参数', exact: true }).click();
  assert.equal(await page.evaluate(() => window.__sanmu.parameters.mix), 100);
  await page.getByRole('tab', { name: '空间', exact: true }).click();
  await page.getByRole('button', { name: '收起参数', exact: true }).click();
  assert(await page.locator('#parameters').isHidden());
  await page.getByRole('button', { name: '显示参数', exact: true }).click();
  await page.locator('#source').selectOption('snare');
  await page.getByRole('button', { name: '播放', exact: true }).click();
  await page.waitForTimeout(2200);
  await page.screenshot({ path: `${artifacts}/snare.png` });

  for (const viewport of [{ width: 390, height: 844 }, { width: 375, height: 667 }, { width: 844, height: 390 }, { width: 1920, height: 1080 }]) {
    await page.setViewportSize(viewport);
    await page.mouse.move(1, 1);
    await page.waitForTimeout(250);
    const bounds = await page.evaluate(() => ({ width: document.documentElement.scrollWidth, viewport: innerWidth,
      collision: [...document.querySelectorAll('[role="tabpanel"]:not([hidden]) .parameter-label')].some(element => {
        const label = element.querySelector('label').getBoundingClientRect(), value = element.querySelector('output').getBoundingClientRect();
        return label.right > value.left;
      }) }));
    assert(bounds.width <= bounds.viewport && !bounds.collision, JSON.stringify({ viewport, bounds }));
    await page.screenshot({ path: `${artifacts}/${viewport.width}x${viewport.height}.png`, fullPage: true });
    report.visual.push({ viewport, bounds, pixels: pixels(await page.locator('canvas').screenshot()) });
  }
  await page.emulateMedia({ reducedMotion: 'reduce' });
  await page.waitForTimeout(100);
  assert.equal(await page.evaluate(() => window.__sanmu.scene.life.motion), false);
  await page.emulateMedia({ reducedMotion: 'no-preference' });

  const wav = Buffer.alloc(44 + 48000 * 4);
  wav.write('RIFF'); wav.writeUInt32LE(wav.length - 8, 4); wav.write('WAVEfmt ', 8);
  wav.writeUInt32LE(16, 16); wav.writeUInt16LE(3, 20); wav.writeUInt16LE(1, 22);
  wav.writeUInt32LE(48000, 24); wav.writeUInt32LE(192000, 28); wav.writeUInt16LE(4, 32); wav.writeUInt16LE(32, 34);
  wav.write('data', 36); wav.writeUInt32LE(wav.length - 44, 40);
  for (let n = 0; n < 48000; n++) {
    const t = n / 48000;
    wav.writeFloatLE(.16 * Math.sin(t * Math.PI * 2 * 640) * Math.min(1, t * 200) * Math.exp(-t * 8), 44 + n * 4);
  }
  await page.locator('#file').setInputFiles({ name: '本地短音.wav', mimeType: 'audio/wav', buffer: wav });
  await page.waitForFunction(() => window.__sanmu.audio.sourceId === 'file' && !!window.__sanmu.audio.source);
  assert.equal(await page.locator('#source option:checked').textContent(), '本地短音.wav');
  await page.locator('#file').setInputFiles({ name: '无效音频.wav', mimeType: 'audio/wav', buffer: Buffer.from('无效音频') });
  await page.waitForFunction(() => !document.querySelector('#error').hidden);
  assert.equal(await page.evaluate(() => window.__sanmu.audio.sourceId), 'file');
  await page.getByRole('button', { name: '停止输入，保留尾音', exact: true }).click();
  await page.getByRole('button', { name: '播放', exact: true }).click();
  await page.waitForFunction(() => !document.querySelector('#play').disabled);
  assert.equal(await page.locator('#error').isHidden(), true);
  await page.getByRole('button', { name: '循环播放', exact: true }).click();
  await page.waitForFunction(() => !window.__sanmu.audio.source);

  const mobile = await browser.newPage({ viewport: { width: 390, height: 844 }, deviceScaleFactor: 2, isMobile: true, hasTouch: true });
  mobile.on('pageerror', error => report.errors.push(error.message));
  await mobile.goto(url);
  await mobile.getByRole('button', { name: '播放', exact: true }).click();
  await mobile.waitForFunction(() => window.__sanmu.scene.life.count > 700);
  await mobile.waitForTimeout(800);
  const mobilePixels = pixels(await mobile.locator('canvas').screenshot());
  assert(mobilePixels.colour > 400, JSON.stringify(mobilePixels));
  assert.equal(await mobile.evaluate(() => window.__sanmu.scene.life.capacity), 4500);
  await mobile.screenshot({ path: `${artifacts}/mobile.png`, fullPage: true });
  report.visual.push({ viewport: '独立手机页面', pixels: mobilePixels, capacity: 4500 });
  await mobile.close();
  assert.deepEqual(report.errors, []);
  report.passed = true;
  console.log('画面、交互、尾音与尺寸检查通过');
} finally {
  writeFileSync(`${artifacts}/report.json`, JSON.stringify(report, null, 2));
  await browser.close();
}
