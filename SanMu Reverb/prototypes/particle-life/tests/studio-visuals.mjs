import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'node:fs';
import assert from 'node:assert/strict';

const directory = 'test-results/visuals';
mkdirSync(directory, { recursive: true });
const browser = await chromium.launch({ executablePath: '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome', headless: true,
  args: ['--autoplay-policy=no-user-gesture-required'] });
const page = await browser.newPage({ viewport: { width: 1100, height: 700 } });
const report = { passed: false, errors: [], themes: [] };
page.on('pageerror', error => report.errors.push(error.message));
try {
  await page.goto(process.env.SANMU_PROTOTYPE_URL || 'http://127.0.0.1:5173/studio.html');
  await page.waitForFunction(() => window.__sanmuStudio?.scene);
  await page.evaluate(() => document.documentElement.classList.add('native-plugin'));
  assert.deepEqual(await page.locator('.views label span').allTextContents(), ['Dry', 'Wet', 'Mix']);
  for (const theme of ['light', 'dark']) {
    await page.evaluate(value => window.__sanmuStudio.setTheme(value), theme);
    const markers = await page.evaluate(() => [...document.querySelectorAll('.signal-mark')].map(mark => {
      const shape = mark.querySelector(document.documentElement.dataset.theme === 'light' ? '.water-mark' : '.dust-mark');
      return { display: getComputedStyle(shape).display, color: getComputedStyle(mark).color, width: mark.getBoundingClientRect().width };
    }));
    assert(markers.every(mark => mark.display !== 'none' && mark.color !== 'rgba(0, 0, 0, 0)' && mark.width > 0));
    await page.locator('.scene-legend').screenshot({ path: `${directory}/${theme}-legend.png` });
    await page.locator('#settings').click();
    const box = await page.locator('#settings-dialog').boundingBox();
    assert(box.y >= 0 && box.y + box.height <= 701, '默认窗口的设置面板溢出');
    await page.screenshot({ path: `${directory}/${theme}-settings.png` });
    report.themes.push({ theme, markers, settings: box });
    await page.locator('#close-settings').click();
  }
  await page.evaluate(() => window.__sanmuStudio.setLanguage('zh'));
  assert.deepEqual(await page.locator('.views label span').allTextContents(), ['干', '湿', '混合']);
  await page.locator('[name=view][value=wet]').check();
  assert(await page.locator('#input-legend').isHidden());
  assert(await page.locator('#wet-legend').isVisible());
  await page.locator('[name=view][value=overlay]').check();
  await page.evaluate(() => window.__sanmuStudio.audio.play());
  await page.waitForFunction(() => window.__sanmuStudio.audio.latest?.wetRms > .001);
  const original = await page.evaluate(() => JSON.stringify(window.__sanmuStudio.snapshot()));
  await page.locator('#settings').click();
  const toggle = page.getByRole('switch', { name: '声场显示', exact: true });
  await toggle.uncheck();
  const stoppedAt = await page.evaluate(() => window.__sanmuStudio.scene.frameCount);
  await page.waitForTimeout(200);
  const off = await page.evaluate(() => ({ frames: window.__sanmuStudio.scene.frameCount,
    hidden: document.querySelector('#stage canvas').hidden, audio: window.__sanmuStudio.audio.context.state,
    input: window.__sanmuStudio.audio.latest.dryRms, state: JSON.stringify(window.__sanmuStudio.snapshot()) }));
  assert.equal(off.frames, stoppedAt); assert(off.hidden); assert.equal(off.audio, 'running'); assert(off.input > 0); assert.equal(off.state, original);
  // 页面重新可见不能误开启用户已经关闭的声场。
  await page.evaluate(() => document.dispatchEvent(new Event('visibilitychange')));
  await page.waitForTimeout(100);
  assert.equal(await page.evaluate(() => window.__sanmuStudio.scene.frameCount), stoppedAt);
  await page.reload();
  await page.waitForFunction(() => window.__sanmuStudio?.scene);
  assert.equal(await page.evaluate(() => window.__sanmuStudio.scene.enabled), false);
  await page.locator('#settings').click();
  await toggle.focus(); await page.keyboard.press('Space');
  await page.waitForFunction(() => window.__sanmuStudio.scene.enabled && window.__sanmuStudio.scene.frameCount > 0);
  assert.deepEqual(report.errors, []);
  report.passed = true;
  console.log('双主题图例、中英文名称、显示开关、声音隔离、键盘操作与偏好恢复通过');
} finally {
  writeFileSync(`${directory}/report.json`, JSON.stringify(report, null, 2));
  await browser.close();
}
