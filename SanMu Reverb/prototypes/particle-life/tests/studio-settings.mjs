import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'node:fs';
import assert from 'node:assert/strict';

const directory = 'test-results/settings';
mkdirSync(directory, { recursive: true });
const browser = await chromium.launch({ executablePath: process.env.SANMU_CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome', headless: true });
const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
const report = { passed: false, errors: [], layouts: [] };
page.on('pageerror', error => report.errors.push(error.message));
page.on('console', message => { if (message.type() === 'error') report.errors.push(message.text()); });
const url = process.env.SANMU_PROTOTYPE_URL || 'http://127.0.0.1:5173/studio.html';
const frozen = () => page.evaluate(() => {
  const { snapshot, scene } = window.__sanmuStudio;
  return { state: snapshot(), birth: Array.from(scene.life.birth), position: Array.from(scene.life.position) };
});
const layout = async () => {
  await page.waitForTimeout(120);
  const result = await page.evaluate(() => {
    const box = selector => document.querySelector(selector).getBoundingClientRect();
    const a = box('.brand'), b = box('.tools'), d = box('#settings-dialog');
    const collision = a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
    const overflow = [...document.querySelectorAll('.knob-readout, .brand, .families, .styles')].filter(el => el.scrollWidth > el.clientWidth + 1).map(el => el.className);
    return { width: innerWidth, scale: Number(document.getElementById('app').style.zoom), scrollWidth: document.documentElement.scrollWidth, collision, overflow,
      dialogVisible: d.left >= 0 && d.right <= innerWidth + 1 && d.top >= 0 && d.bottom <= innerHeight + 1,
      mixDiameter: box('#knob-mix').width };
  });
  report.layouts.push(result);
  assert(!result.collision && !result.overflow.length && result.scrollWidth <= result.width + 1 && result.dialogVisible, JSON.stringify(result));
  assert(Math.abs(result.mixDiameter - 92 * result.scale) < 1, '窗口缩放没有实际改变控件大小');
};
try {
  await page.addInitScript(() => localStorage.setItem('sanmu-studio-language', 'zh'));
  await page.goto(url);
  assert.equal(await page.locator('.brand-mark').count(), 0, '已移除的标志仍显示');
  await page.getByRole('button', { name: '播放', exact: true }).click();
  await page.waitForFunction(() => window.__sanmuStudio.audio.latest?.wetRms > .01);
  await page.evaluate(() => window.__sanmuStudio.audio.context.suspend());
  await page.waitForTimeout(400);
  const original = await frozen();
  await page.getByRole('button', { name: '设置', exact: true }).click();
  for (const [theme, label] of [['dark', '深色'], ['light', '浅色']]) {
    await page.getByRole('radio', { name: label, exact: true }).check();
    for (const scale of [85, 100, 115, 125, 150]) {
      await page.getByRole('radio', { name: `界面缩放 ${scale}%`, exact: true }).check();
      await layout();
      assert.deepEqual(await frozen(), original, '辅助设置改变了声音或声场生命状态');
    }
    await page.getByRole('button', { name: '恢复 100% 大小' }).click();
    assert(await page.getByRole('radio', { name: '界面缩放 100%', exact: true }).isChecked());
    await page.waitForTimeout(180);
    await page.screenshot({ path: `${directory}/${theme}-settings.png` });
  }
  await page.getByRole('button', { name: '操作帮助', exact: true }).click();
  assert(await page.getByRole('dialog', { name: '操作帮助', exact: true }).isVisible());
  await page.screenshot({ path: `${directory}/help.png` });
  await page.keyboard.press('Escape');
  assert(await page.getByRole('dialog', { name: '设置', exact: true }).isVisible());
  await page.getByRole('button', { name: '关于 SanMu Audio', exact: true }).click();
  assert(await page.getByRole('dialog', { name: 'SanMu Audio', exact: true }).isVisible());
  await page.keyboard.press('Escape');
  await page.getByRole('radio', { name: '关闭提示', exact: true }).check();
  await page.getByRole('button', { name: '关闭设置' }).click();
  await page.getByRole('button', { name: '旁通', exact: true }).hover();
  await page.waitForTimeout(600);
  assert(await page.locator('#tooltip').isHidden());
  await page.getByRole('button', { name: '旁通', exact: true }).click();
  assert.equal(await page.locator('#bypass').getAttribute('aria-pressed'), 'true');
  assert.equal(await page.locator('#bypass').textContent(), 'BYP');
  assert.deepEqual(await page.evaluate(() => window.__sanmuStudio.parameters), original.state.values);
  await page.getByRole('button', { name: '撤销', exact: true }).click();
  assert.deepEqual((await frozen()).state, original.state);
  await page.getByRole('button', { name: '设置', exact: true }).click();
  for (const width of [475, 375, 320]) {
    await page.setViewportSize({ width, height: 812 });
    for (const scale of [85, 100, 115, 125, 150]) {
      const option = page.getByRole('radio', { name: `界面缩放 ${scale}%`, exact: true });
      if (!(await option.isEnabled())) continue;
      await option.check(); await layout();
    }
    await page.getByRole('button', { name: '恢复 100% 大小' }).click();
    await page.waitForTimeout(180);
    await page.screenshot({ path: `${directory}/settings-${width}.png` });
  }
  await page.setViewportSize({ width: 1440, height: 1000 });
  await page.getByRole('radio', { name: '界面缩放 115%', exact: true }).check();
  await page.getByRole('radio', { name: '深色', exact: true }).check();
  await page.reload();
  await page.getByRole('button', { name: '设置', exact: true }).click();
  assert(await page.getByRole('radio', { name: '界面缩放 115%', exact: true }).isChecked());
  assert(await page.getByRole('radio', { name: '深色', exact: true }).isChecked());
  assert(await page.getByRole('radio', { name: '关闭提示', exact: true }).isChecked());
  assert.deepEqual(report.errors, []);
  report.passed = true;
  console.log('设置、实际缩放、窄窗口约束、偏好保持、帮助、BYP 与声场状态检查通过');
} finally {
  writeFileSync(`${directory}/report.json`, JSON.stringify(report, null, 2));
  await browser.close();
}
