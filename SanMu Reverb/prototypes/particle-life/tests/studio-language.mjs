import { chromium } from 'playwright';
import { mkdirSync, writeFileSync } from 'node:fs';
import assert from 'node:assert/strict';

const directory = 'test-results/language';
mkdirSync(directory, { recursive: true });
const browser = await chromium.launch({ executablePath: process.env.SANMU_CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome', headless: true,
  args: ['--autoplay-policy=no-user-gesture-required'] });
const page = await browser.newPage({ viewport: { width: 1440, height: 1080 } });
const report = { passed: false, errors: [], layouts: [] };
page.on('pageerror', error => report.errors.push(error.message));
page.on('console', message => { if (message.type() === 'error') report.errors.push(message.text()); });
const url = process.env.SANMU_PROTOTYPE_URL || 'http://127.0.0.1:5173/studio.html';
const state = () => page.evaluate(() => {
  const { scene, snapshot } = window.__sanmuStudio;
  return { state: snapshot(), birth: Array.from(scene.life.birth), position: Array.from(scene.life.position),
    opacity: Array.from(scene.life.opacity), dry: Array.from(scene.dry.opacity), knobs: [...document.querySelectorAll('.dial canvas')].map(canvas => canvas.toDataURL()) };
});
const onlyEnglish = async selector => {
  const text = await page.locator(selector).innerText();
  assert(!/[\u3400-\u9fff]/.test(text.replaceAll('中文', '')), `英文界面仍含未翻译文案：${text}`);
};
const expectTip = async (locator, expected) => {
  await page.mouse.move(1, 1);
  await locator.hover();
  await page.locator('#tooltip').waitFor({ state: 'visible' });
  const text = await page.locator('#tooltip').innerText();
  assert(text.startsWith(expected), `悬停提示不匹配：${text}，预期：${expected}`);
  if (!/[\u3400-\u9fff]/.test(expected)) assert(!/[\u3400-\u9fff]/.test(text), `英文提示含中文：${text}`);
};
try {
  await page.goto(url);
  assert.equal(await page.locator('html').getAttribute('lang'), 'en');
  assert.equal(await page.locator('.brand-mark').count(), 0);
  await onlyEnglish('.instrument');
  await page.getByRole('button', { name: 'Play', exact: true }).click();
  await page.waitForFunction(() => window.__sanmuStudio.audio.latest?.wetRms > .01);
  await page.waitForTimeout(350);
  await page.evaluate(() => window.__sanmuStudio.audio.context.suspend());
  await page.waitForTimeout(400);
  await page.mouse.move(1, 1);
  const original = await state();
  await page.getByRole('button', { name: 'Settings', exact: true }).click();
  await onlyEnglish('#settings-dialog');
  await page.getByRole('radio', { name: '中文', exact: true }).check();
  assert.equal(await page.locator('html').getAttribute('lang'), 'zh-CN');
  assert.equal(await page.locator('#preset-name').textContent(), '音乐厅 · 标准基准');
  assert.equal(await page.locator('#knob-attack').getAttribute('aria-label'), '建立');
  await page.getByRole('radio', { name: 'English', exact: true }).check();
  assert.deepEqual(await state(), original, '语言切换改变了参数、粒子或旋钮绘制');
  assert.equal(await page.locator('#knob-attack').getAttribute('aria-label'), 'Attack');
  await page.getByRole('button', { name: 'Controls & shortcuts', exact: true }).click();
  await onlyEnglish('#help-dialog');
  await page.screenshot({ path: `${directory}/help-en.png` });
  await page.keyboard.press('Escape');
  await page.getByRole('button', { name: 'About SanMu Audio', exact: true }).click();
  await onlyEnglish('#about-dialog');
  await page.keyboard.press('Escape');
  // 提示可独立于界面选择中文、英文或关闭。
  for (const [value, expected] of [['zh', '旁通'], ['en', 'Bypass'], ['off', null], ['auto', 'Bypass']]) {
    await page.locator(`[name="tooltip-language"][value="${value}"]`).check();
    await page.locator('#close-settings').click();
    await page.locator('#bypass').hover();
    await page.waitForTimeout(520);
    if (expected) assert.equal(await page.locator('#tooltip').innerText(), expected);
    else assert(await page.locator('#tooltip').isHidden());
    await page.locator('#settings').click();
  }
  // 中文界面独立选用英文注解，实际悬停覆盖全部旋钮、类型与八种风格。
  await page.getByRole('radio', { name: '中文', exact: true }).check();
  await page.locator('[name="tooltip-language"][value="en"]').check();
  await page.locator('#close-settings').click();
  for (const [id, name] of [
    ['predelay', 'Pre-delay'], ['decay', 'Decay'], ['size', 'Size'], ['attack', 'Attack'], ['mix', 'Mix'],
    ['bassHz', 'Bass Freq'], ['bassMult', 'Bass Mult'], ['highHz', 'High Freq'], ['highDb', 'High Shelf'],
    ['early', 'Early'], ['late', 'Late'], ['rate', 'Rate'], ['depth', 'Depth'], ['lowCut', 'Low Cut'], ['highCut', 'High Cut'],
  ]) await expectTip(page.locator(`#knob-${id}`), `${name}:`);
  await page.screenshot({ path: `${directory}/tooltip-en-on-zh.png` });
  for (const [family, name, modes] of [
    [0, 'Hall', ['Concert Hall', 'Cathedral']], [1, 'Plate', ['Modern Silk', 'Classic Steel']],
    [2, 'Room', ['Studio Room', 'Live Room']], [3, 'Chamber', ['Velvet Chamber', 'Stone Chamber']],
  ]) {
    const radio = page.locator(`[name="family"][value="${family}"]`);
    await radio.check();
    await expectTip(radio.locator('..'), `${name}:`);
    for (const [style, mode] of modes.entries()) await expectTip(page.locator(`#style-${style}`), `${mode}:`);
    const missing = await page.evaluate(async () => {
      const { translate } = await import('/src/studio/i18n.js');
      return [...document.querySelectorAll('[data-tip]')].map(el => el.dataset.tip)
        .filter(text => /[\u3400-\u9fff]/.test(translate(text, 'en')));
    });
    assert.deepEqual(missing, [], '存在未翻译的悬停注解');
  }
  for (const [value, name] of [['dry', 'Dry'], ['wet', 'Wet'], ['overlay', 'Mix']]) {
    await expectTip(page.locator(`[name="view"][value="${value}"]`).locator('..'), `${name}:`);
  }
  await expectTip(page.locator('#preset-open'), 'Presets:');
  assert.equal(await page.getByRole('radio', { name: '湿', exact: true }).count(), 1, '提示说明覆盖了控件名称');
  await page.locator('#settings').click();
  await page.locator('[name="tooltip-language"][value="auto"]').check();
  await page.locator('#close-settings').click();
  await expectTip(page.locator('#knob-decay'), '衰减：');
  await page.evaluate(() => window.__sanmuStudio.setLanguage('en'));
  await expectTip(page.locator('#knob-decay'), 'Decay:');
  await page.locator('#preset-open').click();
  assert.equal(await page.locator('.preset-item').count(), 23);
  await onlyEnglish('#preset-dialog');
  await page.locator('#preset-search').fill('人声');
  assert((await page.locator('.preset-item').count()) >= 4);
  await page.locator('#preset-search').fill('Vocal');
  assert((await page.locator('.preset-item').count()) >= 4);
  await page.locator('#preset-search').fill('不存在的预设');
  assert.equal(await page.locator('.empty').innerText(), 'No matching presets');
  await page.locator('#close-presets').click();
  for (const [id, value, expected] of [['predelay', '0.025 s', 25], ['decay', '2500 ms', 2.5], ['highCut', '8.5 kHz', 8500], ['bassMult', '1.2 ×', 1.2]]) {
    await page.locator(`#edit-${id}`).fill(value); await page.locator(`#edit-${id}`).press('Enter');
    assert.equal(await page.evaluate(id => window.__sanmuStudio.parameters[id], id), expected);
  }
  await page.locator('#edit-decay').fill('abc'); await page.locator('#edit-decay').press('Enter');
  assert.equal(await page.locator('#notification').innerText(), 'Enter a valid value for Decay');
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.decay), 2.5);
  await page.evaluate(() => window.__sanmuStudio.loadPreset(window.__sanmuStudio.presets[0]));
  // 英文模式名称较长，逐一覆盖四类八种风格和六档窗口。
  for (const width of [1440, 1024, 844, 768, 375, 320]) {
    await page.setViewportSize({ width, height: 1080 });
    for (let family = 0; family < 4; family++) {
      await page.locator(`[name="family"][value="${family}"]`).check();
      for (const style of [0, 1]) {
        await page.locator(`[name="style"][value="${style}"]`).check();
        await page.waitForTimeout(40);
        await onlyEnglish('.instrument');
        const layout = await page.evaluate(() => {
          const selectors = ['.brand', '.tools', '.families', '.styles', '.scene-tools', '.knob-label', '.knob-readout'];
          const overflow = [...document.querySelectorAll(selectors.join(','))].filter(el => el.scrollWidth > el.clientWidth + 1).map(el => el.className);
          const a = document.querySelector('.styles').getBoundingClientRect(), b = document.querySelector('#mix-badge').getBoundingClientRect();
          return { width: innerWidth, scrollWidth: document.documentElement.scrollWidth, overflow, collision: a.right > b.left + 1 && a.left < b.right && a.top < b.bottom && a.bottom > b.top };
        });
        assert(layout.scrollWidth <= width + 1 && !layout.overflow.length && !layout.collision, JSON.stringify({ family, style, ...layout }));
        report.layouts.push({ family, style, ...layout });
      }
    }
    if ([1440, 768, 375].includes(width)) await page.screenshot({ path: `${directory}/chamber-en-${width}.png`, fullPage: true });
  }
  await page.setViewportSize({ width: 1440, height: 1080 });
  await page.evaluate(() => window.__sanmuStudio.loadPreset(window.__sanmuStudio.presets[0]));
  for (const language of ['en', 'zh']) for (const theme of ['dark', 'light']) {
    await page.evaluate(({ language, theme }) => { window.__sanmuStudio.setLanguage(language); window.__sanmuStudio.setTheme(theme); }, { language, theme });
    await page.mouse.move(1, 1); await page.waitForTimeout(180);
    await page.screenshot({ path: `${directory}/${theme}-${language}.png` });
  }
  // 用户命名不随翻译重写，刷新后仍保留语言与提示偏好。
  await page.locator('#save').click(); await page.locator('#save-name').fill('我的人声 · Vocal');
  await page.locator('.save-submit').click();
  await page.evaluate(() => window.__sanmuStudio.setLanguage('en'));
  assert.equal(await page.locator('#preset-name').textContent(), '我的人声 · Vocal');
  await page.locator('#settings').click();
  await page.locator('[name="tooltip-language"][value="en"]').check();
  await page.getByRole('radio', { name: '中文', exact: true }).check();
  await page.waitForTimeout(180);
  await page.screenshot({ path: `${directory}/settings-zh.png` });
  await page.reload();
  assert.equal(await page.locator('html').getAttribute('lang'), 'zh-CN');
  await page.locator('#settings').click();
  assert(await page.locator('[name="tooltip-language"][value="en"]').isChecked());
  await page.locator('#close-settings').click();
  await expectTip(page.locator('#knob-decay'), 'Decay:');
  assert.equal(await page.locator('html').getAttribute('lang'), 'zh-CN');
  await page.locator('#settings').click();
  await page.getByRole('radio', { name: 'English', exact: true }).check();
  await page.waitForTimeout(180);
  await page.screenshot({ path: `${directory}/settings-en.png` });
  await page.locator('#close-settings').click();
  await page.locator('#preset-open').click();
  await page.getByRole('radio', { name: 'My presets', exact: true }).check();
  assert.equal(await page.locator('.preset-item').innerText(), '我的人声 · Vocal\n3.99 s · Insert 24.8%');
  await page.locator('#close-presets').click();
  await page.setViewportSize({ width: 320, height: 900 });
  for (const theme of ['dark', 'light']) {
    await page.evaluate(theme => window.__sanmuStudio.setTheme(theme), theme);
    await expectTip(page.locator('#knob-bassHz'), 'Bass Freq:');
    const box = await page.locator('#tooltip').boundingBox();
    assert(box.x >= 8 && box.x + box.width <= 312 && box.y >= 8 && box.y + box.height <= 892, '英文提示未保留窗口边距');
    await page.screenshot({ path: `${directory}/tooltip-${theme}-320.png` });
  }
  assert.deepEqual(report.errors, []);
  report.passed = true;
  console.log('默认英文、全部悬停注解双语、独立提示及刷新恢复、窄屏提示、用户命名、数值单位、声场保持与四十八种英文布局通过');
} finally {
  writeFileSync(`${directory}/report.json`, JSON.stringify(report, null, 2));
  await browser.close();
}
