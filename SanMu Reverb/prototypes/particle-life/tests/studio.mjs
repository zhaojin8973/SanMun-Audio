import { chromium } from 'playwright';
import { PNG } from 'pngjs';
import { mkdirSync, writeFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import assert from 'node:assert/strict';

const directory = 'test-results/studio';
mkdirSync(directory, { recursive: true });
const compiled = spawnSync('clang++', ['tests/suite-native.cpp', '-std=c++20', '-O2', '-o', `${directory}/suite-native`], { encoding: 'utf8' });
assert.equal(compiled.status, 0, compiled.stderr);
const native = spawnSync(`${directory}/suite-native`, [], { maxBuffer: 8 * 1024 * 1024 });
assert.equal(native.status, 0);
writeFileSync(`${directory}/native.f32`, native.stdout);
const browser = await chromium.launch({ executablePath: process.env.SANMU_CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  headless: true, args: ['--autoplay-policy=no-user-gesture-required'] });
const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
const report = { passed: false, errors: [], layouts: [] };
page.on('pageerror', error => report.errors.push(error.message));
page.on('console', message => { if (message.type() === 'error') report.errors.push(message.text()); });
function pixels(buffer) {
  const { width, height, data } = PNG.sync.read(buffer);
  let illuminated = 0;
  for (let y = 80; y < height - 60; y++) for (let x = 0; x < width; x++) {
    const i = (y * width + x) * 4;
    if (Math.max(data[i], data[i + 1], data[i + 2]) > 45) illuminated++;
  }
  return illuminated;
}
function sourceWav() {
  const rate = 48000, frames = rate * 8, bytes = Buffer.alloc(44 + frames * 4);
  bytes.write('RIFF'); bytes.writeUInt32LE(bytes.length - 8, 4); bytes.write('WAVEfmt ', 8);
  bytes.writeUInt32LE(16, 16); bytes.writeUInt16LE(3, 20); bytes.writeUInt16LE(1, 22);
  bytes.writeUInt32LE(rate, 24); bytes.writeUInt32LE(rate * 4, 28); bytes.writeUInt16LE(4, 32); bytes.writeUInt16LE(32, 34);
  bytes.write('data', 36); bytes.writeUInt32LE(frames * 4, 40);
  for (let i = 0; i < frames; i++) {
    const t = i / rate, e = Math.min(1, t * 200, (8 - t) * 200);
    bytes.writeFloatLE(e * (.11 * Math.sin(2 * Math.PI * 220 * t) + .055 * Math.sin(2 * Math.PI * 660 * t) + .014 * Math.sin(2 * Math.PI * 3200 * t)), 44 + i * 4);
  }
  return bytes;
}
try {
  await page.addInitScript(() => localStorage.setItem('sanmu-studio-language', 'zh'));
  await page.goto('http://127.0.0.1:5173/studio.html');
  await page.waitForFunction(() => window.__sanmuStudio?.scene);
  report.dsp = await page.evaluate(async () => {
    const { default: create } = await import('/audio/suite-dsp.js');
    const module = await WebAssembly.compile(await (await fetch('/audio/suite-dsp.wasm')).arrayBuffer());
    const dsp = await create({ instantiateWasm(imports, receive) { const i = new WebAssembly.Instance(module, imports); receive(i, module); return i.exports; } });
    const expected = new Float32Array(await (await fetch('/test-results/studio/native.f32')).arrayBuffer());
    const comparisons = [];
    let cursor = 0;
    for (let mode = 0; mode < 8; mode++) {
      dsp._suite_init(48000);
      const values = dsp.HEAPF32.subarray(dsp._suite_parameters() / 4, dsp._suite_parameters() / 4 + 14);
      values.set([2.4,20,6000,-18,300,1,100,30,100,100,.8,20,20,9000]); dsp._suite_commit(mode);
      const data = dsp.HEAPF32.subarray(dsp._suite_buffer() / 4, dsp._suite_buffer() / 4 + 512);
      let error = 0, energy = 0, finite = true;
      for (let start = 0; start < 48000; start += 128) {
        const count = Math.min(128, 48000 - start);
        data.fill(0, 0, 256);
        if (start === 0) { data[0] = .3; data[128 + 64] = .2; }
        dsp._suite_process(count);
        for (let i = 0; i < count; i++) for (let ch = 0; ch < 2; ch++) {
          const value = data[256 + ch * 128 + i];
          finite &&= Number.isFinite(value); energy += value * value;
          error = Math.max(error, Math.abs(value - expected[cursor++]));
        }
      }
      comparisons.push({ mode, error, energy, finite });
    }
    return comparisons;
  });
  for (const result of report.dsp) assert(result.finite && result.energy > .0001 && result.error < .00002, JSON.stringify(result));
  assert.equal(new Set(report.dsp.map(value => value.energy.toFixed(8))).size, 8, '风格未产生独立输出');

  const edit = async (id, value) => { await page.locator('#edit-' + id).fill(value); await page.locator('#edit-' + id).press('Enter'); };
  await edit('predelay', '.035秒');
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.predelay), 35);
  await page.getByRole('button', { name: '撤销', exact: true }).click();
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.predelay), 20);
  await page.getByRole('button', { name: '重做', exact: true }).click();
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.predelay), 35);
  await page.locator('#knob-predelay').dblclick();
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.predelay), 20);
  await edit('highCut', '8.5kHz');
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.highCut), 8500);
  await edit('decay', '错误');
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.decay), 3.99);
  await edit('mix', '200');
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.mix), 100);
  await edit('mix', '24.8');
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.mix), 24.8);

  const box = await page.locator('#knob-size').boundingBox();
  await page.evaluate(() => {
    window.__knobUpdates = Object.fromEntries([...document.querySelectorAll('.dial')].map(dial => [dial.id, 0]));
    window.__knobObserver = new MutationObserver(records => records.forEach(record => { window.__knobUpdates[record.target.id]++; }));
    document.querySelectorAll('.dial').forEach(dial => window.__knobObserver.observe(dial, { attributes: true, attributeFilter: ['aria-valuenow'] }));
  });
  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down(); await page.mouse.move(box.x + box.width / 2, box.y - 60);
  await page.mouse.move(box.x + box.width / 2, box.y - 45);
  const dragUpdates = await page.evaluate(() => { window.__knobObserver.disconnect(); return window.__knobUpdates; });
  await page.mouse.up();
  assert(dragUpdates['knob-size'] > 0 && Object.entries(dragUpdates).every(([id, count]) => id === 'knob-size' || count === 0), `拖动单个旋钮时不应刷新其他旋钮：${JSON.stringify(dragUpdates)}`);
  assert((await page.evaluate(() => window.__sanmuStudio.parameters.size)) < 100, '拖到边界后反向存在死区');
  await page.getByRole('button', { name: '撤销', exact: true }).click();
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.size), 100);
  await page.locator('#knob-size').focus(); await page.keyboard.press('Home');
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.size), 0);
  await page.keyboard.press('End');
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.size), 100);

  await edit('decay', '5.2');
  await page.getByRole('radio', { name: '对比 B', exact: true }).check();
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.decay), 3.99);
  await page.getByRole('radio', { name: '对比 A', exact: true }).check();
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.decay), 5.2);
  await page.getByRole('button', { name: '复制到 B', exact: true }).click();
  await page.getByRole('radio', { name: '对比 B', exact: true }).check();
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.decay), 5.2);

  await page.locator('#preset-open').click();
  assert.equal(await page.locator('.preset-item').count(), 23);
  await page.getByRole('searchbox', { name: '搜索预设' }).fill('钢琴');
  assert.equal(await page.locator('.preset-item').count(), 3);
  await page.getByRole('button', { name: '音乐厅 · 清晰钢琴', exact: true }).click();
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.decay), 2.15);
  await page.getByRole('button', { name: '保存预设', exact: true }).click();
  await page.locator('#save-name').fill('界面验证预设');
  await page.getByRole('button', { name: '保存到我的预设' }).click();
  await page.reload();
  await page.locator('#preset-open').click();
  await page.getByRole('radio', { name: '我的', exact: true }).check();
  await page.getByRole('button', { name: '界面验证预设', exact: true }).click();
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.decay), 2.15);

  await page.locator('#file').setInputFiles({ name: '持续和声.wav', mimeType: 'audio/wav', buffer: sourceWav() });
  await page.waitForFunction(() => window.__sanmuStudio.audio.latest?.wetRms > .01);
  for (const [family, style, expected] of [['大厅', '教堂', 1], ['板式', '现代丝滑', 2], ['板式', '经典钢板', 3], ['房间', '录音室', 4], ['房间', '现场房间', 5], ['回声室', '柔暖回声室', 6], ['回声室', '石质回声室', 7], ['大厅', '音乐厅', 0]]) {
    await page.getByRole('radio', { name: family, exact: true }).check();
    await page.getByRole('radio', { name: style, exact: true }).check();
    await page.waitForTimeout(160);
    assert.equal(await page.evaluate(() => window.__sanmuStudio.snapshot().mode), expected);
    assert(await page.evaluate(() => Number.isFinite(window.__sanmuStudio.audio.latest.wetRms)));
  }
  await page.getByRole('button', { name: '旁通', exact: true }).click();
  await page.waitForTimeout(500);
  assert(await page.evaluate(() => { const p = window.__sanmuStudio.audio.latest.peaks; return Math.abs(p[0] - p[2]) < .0001 && Math.abs(p[1] - p[3]) < .0001; }));
  assert((await page.evaluate(() => window.__sanmuStudio.scene.wetGain)) < .001);
  await page.getByRole('button', { name: '旁通', exact: true }).click();
  for (const view of ['湿', '混合']) {
    await page.getByRole('radio', { name: view, exact: true }).check(); await edit('mix', '0');
    await page.waitForTimeout(650);
    assert((await page.evaluate(() => window.__sanmuStudio.scene.wetGain)) < .001);
    await edit('mix', '100'); await page.waitForTimeout(650);
    assert((await page.evaluate(() => window.__sanmuStudio.scene.wetGain)) > .999);
    assert(pixels(await page.locator('#stage canvas').screenshot()) > 200);
  }
  await page.evaluate(() => window.__sanmuStudio.loadPreset(window.__sanmuStudio.presets[0]));
  await page.mouse.move(1, 1);
  for (const appearance of ['dark', 'light']) {
    await page.evaluate(theme => window.__sanmuStudio.setTheme(theme), appearance); await page.waitForTimeout(300);
    for (const viewport of [{ width: 1440, height: 1000 }, { width: 1280, height: 800 }, { width: 1024, height: 768 }, { width: 768, height: 1024 }, { width: 375, height: 812 }, { width: 844, height: 390 }]) {
      await page.setViewportSize(viewport); await page.waitForTimeout(160);
      const layout = await page.evaluate(() => {
        const controls = [...document.querySelectorAll('.knob-readout, .knob-label, .brand, .preset-open, .tools, .families, .styles, .scene-caption, .scene-tools, .scene-legend, .spectrum-key')];
        const overflow = controls.filter(el => el.scrollWidth > el.clientWidth + 1).map(el => el.className);
        const overlap = (a, b) => { const x = document.querySelector(a).getBoundingClientRect(), y = document.querySelector(b).getBoundingClientRect(); return x.left < y.right && x.right > y.left && x.top < y.bottom && x.bottom > y.top; };
        return { width: document.documentElement.scrollWidth, viewport: innerWidth, overflow, collision: overlap('.scene-caption', '.scene-tools') || overlap('.scene-legend', '.spectrum-key') || overlap('.brand', '.tools') };
      });
      assert(layout.width <= viewport.width && layout.overflow.length === 0 && !layout.collision, JSON.stringify({ viewport, ...layout }));
      assert(pixels(await page.locator('#stage canvas').screenshot()) > 60);
      await page.screenshot({ path: `${directory}/${appearance}-${viewport.width}.png`, fullPage: true });
      report.layouts.push({ appearance, viewport, ...layout });
    }
  }
  await page.setViewportSize({ width: 1440, height: 1000 });
  await page.emulateMedia({ reducedMotion: 'reduce' });
  await page.waitForTimeout(200);
  assert.equal(await page.evaluate(() => window.__sanmuStudio.scene.life.motion), false);
  await page.getByRole('button', { name: '专注声场', exact: true }).click();
  assert(await page.locator('.instrument.focused').count());
  await page.getByRole('button', { name: '返回完整界面', exact: true }).click();
  await page.locator('#preset-open').click();
  await page.screenshot({ path: `${directory}/presets.png` });
  await page.keyboard.press('Escape');
  assert(!(await page.locator('#preset-dialog').isVisible()));
  await page.getByRole('button', { name: '停止输入，保留尾音', exact: true }).click();
  await page.waitForTimeout(360);
  assert.equal(await page.evaluate(() => Math.max(...window.__sanmuStudio.scene.dry.opacity)), 0);
  assert((await page.evaluate(() => Math.max(...window.__sanmuStudio.scene.life.opacity))) > .01);
  assert.deepEqual(report.errors, []);
  report.passed = true;
  console.log('八种风格原生与浏览器输出、旋钮、预设、对比、旁通、声场和双主题布局检查通过');
} finally {
  writeFileSync(`${directory}/report.json`, JSON.stringify(report, null, 2));
  await browser.close();
}
