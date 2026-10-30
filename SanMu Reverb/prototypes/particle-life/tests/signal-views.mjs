import { chromium } from 'playwright';
import { PNG } from 'pngjs';
import assert from 'node:assert/strict';
import { mkdirSync, writeFileSync } from 'node:fs';

const directory = 'test-results/signal-views';
mkdirSync(directory, { recursive: true });
const browser = await chromium.launch({ executablePath: process.env.SANMU_CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  headless: true, args: ['--autoplay-policy=no-user-gesture-required'] });
const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
const report = { passed: false, modes: [], layouts: [], errors: [] };
page.on('pageerror', error => report.errors.push(error.message));
page.on('console', message => { if (message.type() === 'error') report.errors.push(message.text()); });

function sample() {
  const rate = 48000, frames = rate * 8, wav = Buffer.alloc(44 + frames * 4);
  wav.write('RIFF'); wav.writeUInt32LE(wav.length - 8, 4); wav.write('WAVEfmt ', 8);
  wav.writeUInt32LE(16, 16); wav.writeUInt16LE(3, 20); wav.writeUInt16LE(1, 22);
  wav.writeUInt32LE(rate, 24); wav.writeUInt32LE(rate * 4, 28); wav.writeUInt16LE(4, 32); wav.writeUInt16LE(32, 34);
  wav.write('data', 36); wav.writeUInt32LE(frames * 4, 40);
  for (let i = 0; i < frames; i++) {
    const t = i / rate, envelope = Math.min(1, t * 200, (8 - t) * 200);
    wav.writeFloatLE(envelope * (.1 * Math.sin(t * Math.PI * 440) + .05 * Math.sin(t * Math.PI * 1320)
      + .015 * Math.sin(t * Math.PI * 6400)), 44 + i * 4);
  }
  return wav;
}

function regions(buffer) {
  const image = PNG.sync.read(buffer);
  let inner = 0, outer = 0, colored = 0;
  for (let y = 85; y < image.height - 80; y++) for (let x = 0; x < image.width; x++) {
    const offset = (y * image.width + x) * 4;
    const r = image.data[offset], g = image.data[offset + 1], b = image.data[offset + 2];
    if (Math.max(r, g, b) < 42) continue;
    const radius = Math.hypot((x - image.width / 2) / (image.width * .12), (y - image.height / 2) / (image.height * .24));
    if (radius < 1) inner++;
    if (radius > 1.1) outer++;
    if (Math.max(r, g, b) - Math.min(r, g, b) > 15) colored++;
  }
  return { inner, outer, colored };
}

try {
  await page.goto(process.env.SANMU_PROTOTYPE_URL || 'http://127.0.0.1:5173/');
  await page.locator('#file').setInputFiles({ name: '持续和声.wav', mimeType: 'audio/wav', buffer: sample() });
  await page.waitForFunction(() => window.__sanmu.audio.latest?.wetRms > .03);
  await page.evaluate(() => {
    const { audio, parameters } = window.__sanmu;
    window.signalProbe = { source: audio.source, parameters: JSON.stringify(parameters), calls: 0 };
    const send = audio.setParameters.bind(audio);
    audio.setParameters = () => { window.signalProbe.calls++; send(); };
  });
  for (const [view, label] of [['dry', '原声'], ['wet', '混响'], ['overlay', '叠加']]) {
    await page.getByRole('radio', { name: label, exact: true }).check();
    await page.mouse.move(1, 1);
    await page.waitForTimeout(550);
    const state = await page.evaluate(() => {
      const { scene, audio, parameters } = window.__sanmu;
      return { view: scene.view, dryGain: scene.dryGain, wetGain: scene.wetGain,
        inputWindow: audio.latest.duration, sameSource: audio.source === window.signalProbe.source,
        sameParameters: JSON.stringify(parameters) === window.signalProbe.parameters, calls: window.signalProbe.calls,
        dry: Math.max(...scene.dry.opacity), wet: Math.max(...scene.life.opacity) };
    });
    assert.equal(state.view, view);
    assert(state.sameSource && state.sameParameters && state.calls === 0, '观察切换改变了声音或播放');
    assert(state.dry > .1 && state.wet > .1, '隐藏的信号层没有继续更新');
    assert(state.inputWindow < .012);
    if (view === 'dry') assert(state.wetGain < .001);
    if (view === 'wet') assert(state.dryGain < .001);
    const measured = regions(await page.locator('canvas').screenshot());
    if (view === 'dry') { assert(measured.inner > 100); assert(measured.outer < 10, JSON.stringify(measured)); }
    else assert(measured.outer > 150, JSON.stringify(measured));
    await page.screenshot({ path: `${directory}/${view}.png` });
    report.modes.push({ ...state, pixels: measured });
  }

  // 固定同一音频时刻比较 MIX：排除素材起伏，检查只改变渲染权重。
  await page.evaluate(async () => { await window.__sanmu.audio.context.suspend(); });
  await page.waitForTimeout(400);
  const fixed = await page.evaluate(() => {
    const { scene } = window.__sanmu;
    window.mixBaseline = {
      birth: Array.from(scene.life.birth), loss: Array.from(scene.life.loss),
      position: Array.from(scene.life.position), dryOpacity: Array.from(scene.dry.opacity),
      wetOpacity: Array.from(scene.life.opacity), born: scene.life.bornTotal,
    };
    return { dryGain: scene.dryGain, wetGain: scene.wetGain };
  });
  assert(Math.abs(fixed.dryGain - 1) < .000001);
  await page.getByRole('tab', { name: '调制', exact: true }).click();
  report.mix = [];
  for (const mix of [100, 25, 0, 100]) {
    await page.locator('#param-mix').fill(String(mix / 100));
    await page.mouse.move(1, 1);
    await page.waitForTimeout(600);
    const state = await page.evaluate(() => {
      const { scene, parameters } = window.__sanmu;
      const saved = window.mixBaseline;
      const same = (a, b) => a.every((value, i) => value === b[i]);
      return { mix: parameters.mix, dryGain: scene.dryGain, wetGain: scene.wetGain,
        sameBirth: same(scene.life.birth, saved.birth), sameLoss: same(scene.life.loss, saved.loss),
        samePosition: same(scene.life.position, saved.position), sameDry: same(scene.dry.opacity, saved.dryOpacity),
        sameWet: same(scene.life.opacity, saved.wetOpacity), sameBorn: scene.life.bornTotal === saved.born };
    });
    assert(Math.abs(state.wetGain - Math.sin(mix * Math.PI / 200)) < .001, JSON.stringify(state));
    assert(Math.abs(state.dryGain - 1) < .000001);
    assert(state.sameBirth && state.sameLoss && state.samePosition && state.sameDry && state.sameWet && state.sameBorn,
      'MIX 改变了粒子的状态或中央输入参考');
    const measured = regions(await page.locator('canvas').screenshot());
    report.mix.push({ ...state, pixels: measured });
    if (mix === 0) { assert(measured.inner > 100); assert.equal(measured.outer, 0); }
    await page.screenshot({ path: `${directory}/mix-${mix}.png` });
  }
  assert(report.mix[1].pixels.outer < report.mix[0].pixels.outer * .7, '调小 MIX 后外围没有明显变淡');
  for (const key of ['inner', 'outer', 'colored'])
    assert(Math.abs(report.mix[0].pixels[key] - report.mix[3].pixels[key]) < 10, '恢复 MIX 后同一时刻的画面发生变化');

  await page.getByRole('radio', { name: '混响', exact: true }).check();
  report.wetMix = [];
  for (const mix of [0, 25, 100]) {
    await page.locator('#param-mix').fill(String(mix / 100));
    await page.waitForTimeout(600);
    const state = await page.evaluate(() => {
      const { scene, parameters } = window.__sanmu;
      const saved = window.mixBaseline;
      const same = (a, b) => a.every((value, i) => value === b[i]);
      return { mix: parameters.mix, dryGain: scene.dryGain, wetGain: scene.wetGain,
        sameBirth: same(scene.life.birth, saved.birth), sameLoss: same(scene.life.loss, saved.loss),
        samePosition: same(scene.life.position, saved.position), sameDry: same(scene.dry.opacity, saved.dryOpacity),
        sameWet: same(scene.life.opacity, saved.wetOpacity), sameBorn: scene.life.bornTotal === saved.born };
    });
    assert(Math.abs(state.wetGain - Math.sin(mix * Math.PI / 200)) < .001, JSON.stringify(state));
    assert(state.dryGain < .001, '混响视图没有隐藏输入参考');
    assert(state.sameBirth && state.sameLoss && state.samePosition && state.sameDry && state.sameWet && state.sameBorn,
      '混响视图中的 MIX 改变了内部声场');
    const measured = regions(await page.locator('canvas').screenshot());
    const overlay = report.mix.find(value => value.mix === mix);
    assert(Math.abs(measured.outer - overlay.pixels.outer) < 10, '相同 MIX 下两个视图的混响强度不一致');
    if (mix === 0) {
      assert.equal(measured.outer, 0);
      // 暂时排除无信号时的中央定位圆，确认画布中没有残留细丝或尘埃。
      await page.evaluate(() => { window.__sanmu.scene.source.visible = false; });
      try {
        const background = regions(await page.locator('canvas').screenshot());
        assert.equal(background.inner, 0);
        assert.equal(background.outer, 0);
      } finally {
        await page.evaluate(() => { window.__sanmu.scene.source.visible = true; });
      }
    }
    report.wetMix.push({ ...state, pixels: measured });
    await page.screenshot({ path: `${directory}/wet-mix-${mix}.png` });
  }
  await page.locator('#param-mix').fill('0');
  await page.getByRole('radio', { name: '原声', exact: true }).check();
  await page.waitForTimeout(600);
  assert((await page.evaluate(() => window.__sanmu.scene.dryGain)) > .999);
  await page.getByRole('button', { name: '恢复默认参数', exact: true }).click();
  await page.getByRole('tab', { name: '空间', exact: true }).click();
  await page.getByRole('radio', { name: '叠加', exact: true }).check();
  await page.evaluate(async () => { await window.__sanmu.audio.context.resume(); });
  await page.waitForTimeout(200);

  await page.getByRole('button', { name: '停止输入，保留尾音', exact: true }).click();
  await page.waitForTimeout(360);
  const tail = await page.evaluate(() => ({ dry: Math.max(...window.__sanmu.scene.dry.opacity),
    wet: Math.max(...window.__sanmu.scene.life.opacity) }));
  assert.equal(tail.dry, 0);
  assert(tail.wet > .1);
  report.tail = tail;
  await page.screenshot({ path: `${directory}/tail.png` });
  await page.getByRole('radio', { name: '原声', exact: true }).focus();
  await page.keyboard.press('ArrowRight');
  assert(await page.getByRole('radio', { name: '混响', exact: true }).isChecked());

  for (const viewport of [{ width: 390, height: 844 }, { width: 375, height: 667 }, { width: 844, height: 390 }]) {
    await page.setViewportSize(viewport);
    const layout = await page.evaluate(() => {
      const status = document.querySelector('.stage-status').getBoundingClientRect();
      const modes = document.querySelector('.view-switch').getBoundingClientRect();
      const layers = document.querySelector('.layer-key').getBoundingClientRect();
      const bands = document.querySelector('.spectrum-key').getBoundingClientRect();
      const overlap = (a, b) => a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
      return { width: document.documentElement.scrollWidth, viewport: innerWidth, overlap: overlap(status, modes) || overlap(layers, bands) };
    });
    assert(layout.width <= layout.viewport && !layout.overlap, JSON.stringify(layout));
    await page.getByRole('radio', { name: '叠加', exact: true }).check();
    await page.getByRole('button', { name: '播放', exact: true }).click();
    await page.waitForTimeout(650);
    await page.mouse.move(1, 1);
    await page.screenshot({ path: `${directory}/${viewport.width}.png`, fullPage: true });
    await page.getByRole('tab', { name: '调制', exact: true }).click();
    await page.locator('#param-mix').fill('0');
    await page.waitForTimeout(600);
    assert((await page.evaluate(() => window.__sanmu.scene.wetGain)) < .001);
    assert((await page.evaluate(() => window.__sanmu.scene.dryGain)) > .999);
    await page.screenshot({ path: `${directory}/${viewport.width}-mix-0.png`, fullPage: true });
    await page.getByRole('radio', { name: '混响', exact: true }).check();
    await page.waitForTimeout(600);
    assert((await page.evaluate(() => window.__sanmu.scene.wetGain)) < .001);
    assert((await page.evaluate(() => window.__sanmu.scene.dryGain)) < .001);
    await page.screenshot({ path: `${directory}/${viewport.width}-wet-mix-0.png`, fullPage: true });
    await page.getByRole('button', { name: '恢复默认参数', exact: true }).click();
    await page.getByRole('tab', { name: '空间', exact: true }).click();
    await page.getByRole('button', { name: '停止输入，保留尾音', exact: true }).click();
    report.layouts.push({ viewport, ...layout });
  }
  assert.deepEqual(report.errors, []);
  report.passed = true;
  console.log('三种信号视图、叠加 MIX 联动、输入参考稳定与尾音留存检查通过');
} finally {
  writeFileSync(`${directory}/report.json`, JSON.stringify(report, null, 2));
  await browser.close();
}
