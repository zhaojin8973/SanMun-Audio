import { chromium } from 'playwright';
import { PNG } from 'pngjs';
import { mkdirSync, writeFileSync } from 'node:fs';
import assert from 'node:assert/strict';

const directory = 'test-results/glass';
mkdirSync(directory, { recursive: true });
const browser = await chromium.launch({ executablePath: process.env.SANMU_CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  headless: true, args: ['--autoplay-policy=no-user-gesture-required'] });
const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
const report = { passed: false, errors: [], themes: [], layouts: [] };
page.on('pageerror', error => report.errors.push(error.message));
page.on('console', message => { if (message.type() === 'error') report.errors.push(message.text()); });
const difference = (first, second) => {
  const a = PNG.sync.read(first), b = PNG.sync.read(second);
  let changed = 0, maximum = 0;
  for (let y = 85; y < a.height - 65; y++) for (let x = 0; x < a.width; x++) {
    const i = (y * a.width + x) * 4;
    const delta = Math.max(...[0, 1, 2].map(ch => Math.abs(a.data[i + ch] - b.data[i + ch])));
    if (delta > 8) changed++;
    maximum = Math.max(maximum, delta);
  }
  return { changed, maximum };
};
const luminance = rgb => rgb.map(value => value / 255).map(value => value <= .04045 ? value / 12.92 : ((value + .055) / 1.055) ** 2.4)
  .reduce((sum, value, i) => sum + value * [.2126, .7152, .0722][i], 0);
try {
  await page.addInitScript(() => localStorage.setItem('sanmu-studio-language', 'zh'));
  await page.goto(process.env.SANMU_PROTOTYPE_URL || 'http://127.0.0.1:5173/studio.html');
  await page.getByRole('button', { name: '播放', exact: true }).click();
  await page.waitForFunction(() => window.__sanmuStudio.audio.latest?.wetRms > .01);
  await page.waitForTimeout(350);
  await page.mouse.move(1, 1);
  await page.evaluate(async () => { await window.__sanmuStudio.audio.context.suspend(); });
  await page.waitForTimeout(400);
  const original = await page.evaluate(() => {
    const { scene, snapshot } = window.__sanmuStudio;
    return { state: snapshot(), birth: Array.from(scene.life.birth), position: Array.from(scene.life.position),
      opacity: Array.from(scene.life.opacity), dry: Array.from(scene.dry.opacity) };
  });
  for (const appearance of ['dark', 'light']) {
    await page.evaluate(() => document.activeElement?.blur());
    await page.evaluate(theme => window.__sanmuStudio.setTheme(theme), appearance);
    await page.waitForTimeout(250);
    const state = await page.evaluate(() => {
      const { scene, snapshot } = window.__sanmuStudio;
      return { state: snapshot(), birth: Array.from(scene.life.birth), position: Array.from(scene.life.position),
        opacity: Array.from(scene.life.opacity), dry: Array.from(scene.dry.opacity) };
    });
    assert.deepEqual(state, original, '切换玻璃主题改变了参数或粒子状态');
    assert.equal(await page.evaluate(() => window.__sanmuStudio.scene.surface), appearance === 'light' ? 'water' : 'dust', '主题材质没有正确切换');
    const withSignal = await page.locator('#stage').screenshot();
    await page.evaluate(() => { const scene = window.__sanmuStudio.scene; scene.points.visible = scene.dryLines.visible = scene.water.group.visible = false; });
    const withoutSignal = await page.locator('#stage').screenshot();
    await page.evaluate(() => { const scene = window.__sanmuStudio.scene; scene.points.visible = scene.dryLines.visible = scene.surface === 'dust'; scene.water.group.visible = scene.surface === 'water'; });
    const visible = difference(withSignal, withoutSignal);
    // 尘埃以少量亮点显形，透明水膜以较大面积的微反差显形。
    assert(appearance === 'light' ? visible.changed > 2000 && visible.maximum > 18
      : visible.changed > 150 && visible.maximum > 30, JSON.stringify({ appearance, visible }));
    const labels = await page.evaluate(() => [...document.querySelectorAll('.knob-label, #preset-name, #status, .detail-group legend, .meter-titles span')].map(element => {
      const box = element.getBoundingClientRect();
      const colour = getComputedStyle(element).color.match(/[\d.]+/g).slice(0, 3).map(Number);
      element.style.visibility = 'hidden';
      return { name: element.textContent, colour, x: Math.floor(box.x + box.width / 2), y: Math.floor(box.y + box.height / 2) };
    }));
    const backing = PNG.sync.read(await page.screenshot());
    await page.evaluate(() => document.querySelectorAll('.knob-label, #preset-name, #status, .detail-group legend, .meter-titles span').forEach(element => { element.style.visibility = ''; }));
    const contrast = labels.map(label => {
      const offset = (label.y * backing.width + label.x) * 4;
      const text = luminance(label.colour), background = luminance([...backing.data.subarray(offset, offset + 3)]);
      return { name: label.name, ratio: (Math.max(text, background) + .05) / (Math.min(text, background) + .05) };
    });
    assert(contrast.every(item => item.ratio >= 4.5), JSON.stringify({ appearance, contrast }));
    report.themes.push({ appearance, visible, minimumTextContrast: Math.min(...contrast.map(item => item.ratio)) });
    for (const viewport of [{ width: 1440, height: 1000 }, { width: 1280, height: 800 }, { width: 1024, height: 768 }, { width: 768, height: 1024 }, { width: 375, height: 812 }, { width: 844, height: 390 }]) {
      await page.setViewportSize(viewport); await page.waitForTimeout(180);
      const layout = await page.evaluate(() => {
        const controls = [...document.querySelectorAll('.knob-readout, .knob-label, .brand, .preset-open, .tools, .families, .styles, .scene-caption, .scene-tools, .scene-legend, .spectrum-key')];
        const overflow = controls.filter(el => el.scrollWidth > el.clientWidth + 1).map(el => el.className);
        const overlap = (a, b) => { const x = document.querySelector(a).getBoundingClientRect(), y = document.querySelector(b).getBoundingClientRect(); return x.left < y.right && x.right > y.left && x.top < y.bottom && x.bottom > y.top; };
        return { width: document.documentElement.scrollWidth, viewport: innerWidth, overflow,
          collision: overlap('.scene-caption', '.scene-tools') || overlap('.scene-legend', '.spectrum-key') || overlap('.brand', '.tools') };
      });
      assert(layout.width <= viewport.width && !layout.overflow.length && !layout.collision, JSON.stringify(layout));
      await page.screenshot({ path: `${directory}/${appearance}-${viewport.width}.png`, fullPage: true });
      report.layouts.push({ appearance, viewport, ...layout });
    }
    await page.setViewportSize({ width: 1440, height: 1000 });
    await page.locator('#preset-open').click(); await page.waitForTimeout(180);
    await page.screenshot({ path: `${directory}/${appearance}-presets.png` });
    await page.keyboard.press('Escape');
  }
  await page.locator('input[name="view"][value="wet"]').check();
  await page.locator('#edit-mix').fill('0'); await page.locator('#edit-mix').press('Enter');
  await page.waitForTimeout(650);
  assert((await page.evaluate(() => window.__sanmuStudio.scene.wetGain)) < .001);
  await page.getByRole('button', { name: '撤销', exact: true }).click();
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.mix), 24.8);
  await page.emulateMedia({ reducedMotion: 'reduce' });
  await page.waitForTimeout(180);
  assert.equal(await page.evaluate(() => window.__sanmuStudio.scene.life.motion), false);
  assert.deepEqual(report.errors, []); report.passed = true;
  console.log('双主题玻璃合成、原声场状态保持、十二种布局与 MIX 检查通过');
} finally {
  writeFileSync(`${directory}/report.json`, JSON.stringify(report, null, 2));
  await browser.close();
}
