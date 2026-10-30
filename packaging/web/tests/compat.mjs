import assert from 'node:assert/strict';
import { mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { parse } from 'acorn';
import { lowerLegacyCss } from '../legacy-css.mjs';
import { prepareCompatibility } from '../build-ui.mjs';

const root = path.dirname(fileURLToPath(import.meta.url));
const compatDir = path.dirname(root);
const playwrightPath = path.resolve(root, '../../../SanMu Reverb/prototypes/particle-life/node_modules/playwright/index.js');
const playwright = (await import(playwrightPath)).default;
const { webkit } = playwright;

async function colourRegression(browser) {
  const modern = `
    :root { --red: #ff0000; --blue: rgb(0 0 255); --half: rgba(0, 255, 0, .5); }
    .sample { width: 2px; height: 2px; background: color-mix(in srgb, var(--red) 70%, var(--blue) 70%); }
    .transparent { width: 2px; height: 2px; background: color-mix(in srgb, var(--red) 13%, transparent); }
    .premultiplied { width: 2px; height: 2px; background: color-mix(in srgb, var(--half) 25%, var(--red) 75%); }
  `;
  const legacy = lowerLegacyCss(modern, 'colour-regression.css');
  assert.doesNotMatch(legacy, /color-mix\(|rgb\(var\(/, '旧版 CSS 仍包含新色彩函数');
  assert.match(legacy, /--red-r:\s*255/);
  assert.match(legacy, /max\(/, '完全透明源色必须有除零保护');

  const page = await browser.newPage();
  const sample = async css => {
    await page.setContent(`<style>${css}</style><i class="sample"></i><i class="transparent"></i><i class="premultiplied"></i>`);
    return page.evaluate(() => [...document.querySelectorAll('i')].map(element => {
      const context = document.createElement('canvas').getContext('2d');
      context.fillStyle = getComputedStyle(element).backgroundColor;
      context.fillRect(0, 0, 1, 1);
      return [...context.getImageData(0, 0, 1, 1).data];
    }));
  };
  const expected = await sample(modern);
  const actual = await sample(legacy);
  // 旧 WebKit 会在 rgba() 的非整数通道上先量化一次，再做预乘；允许一个
  // 量化单位的误差，但不允许透明度或颜色转换出现可见级别的偏差。
  const maxDelta = Math.max(...actual.flatMap((pixel, i) => pixel.map((value, channel) =>
    Math.abs(value - expected[i][channel]))));
  assert.ok(maxDelta <= 2, `现代/旧版颜色转换偏差过大（最大 ${maxDelta}）`);
  await page.close();
}

async function flexRegression(browser) {
  const source = await readFile(path.join(compatDir, 'flex-gap.js'), 'utf8');
  const browserSource = source.replace('export function installFlexGapFallback()', 'function installFlexGapFallback()') +
    '\nwindow.__installFlexGapFallback = installFlexGapFallback;';
  const page = await browser.newPage({ viewport: { width: 320, height: 240 } });
  await page.setContent(`
    <style>
      body { margin: 0; }
      [hidden] { display: none !important; }
      .flow { display: flex; flex-wrap: wrap; width: 100px; --sanmu-flex-gap-x: 10px; --sanmu-flex-gap-y: 8px; }
      .flow > b { display: block; flex: 0 0 45px; height: 20px; }
      .column { display: flex; flex-flow: column wrap; align-content: flex-start; width: 100px; height: 60px; --sanmu-flex-gap-x: 7px; --sanmu-flex-gap-y: 6px; }
      .column > b { display: block; flex: 0 0 20px; width: 20px; height: 20px; align-self: flex-start; }
      .auto-flow { display: flex; width: 200px; --sanmu-flex-gap-x: 10px; }
      .auto-flow > b { display: block; flex: 0 0 20px; height: 20px; }
      .auto-flow > #auto { margin-left: auto; }
    </style>
    <div class="flow"><b id="a"></b><b id="b"></b><b id="c"></b></div>
    <div class="column"><b id="d"></b><b id="e"></b><b id="f"></b><b id="g"></b></div>
    <div class="auto-flow"><b id="before-auto"></b><b id="auto"></b></div>
  `);
  await page.evaluate(() => { window.__SANMU_FORCE_LEGACY_FLEX_GAP = true; });
  await page.addScriptTag({ content: browserSource });
  await page.evaluate(() => window.__installFlexGapFallback());
  await page.waitForTimeout(40);
  const positions = ids => page.evaluate(ids => ids.map(id => {
    const rect = document.getElementById(id).getBoundingClientRect();
    return [id, Math.round(rect.left), Math.round(rect.top)];
  }), ids);
  assert.deepEqual(await positions(['a', 'b', 'c']), [['a', 0, 0], ['b', 55, 0], ['c', 0, 28]], 'Flex 换行主轴/交叉轴间距错误');
  assert.deepEqual(await positions(['d', 'e', 'f', 'g']), [['d', 0, 48], ['e', 0, 74], ['f', 27, 48], ['g', 27, 74]], 'column wrap 间距错误');
  assert.deepEqual(await page.evaluate(() => ['before-auto', 'auto'].map(id => Math.round(document.getElementById(id).getBoundingClientRect().left))), [0, 180], 'auto margin 被错误叠加 gap');
  await page.evaluate(() => { document.getElementById('b').hidden = true; });
  await page.waitForTimeout(40);
  assert.deepEqual(await page.evaluate(() => {
    const rect = document.getElementById('c').getBoundingClientRect();
    return [Math.round(rect.left), Math.round(rect.top), document.getElementById('b').style.marginLeft];
  }), [55, 0, ''], '隐藏元素后旧 margin 没有恢复并重新分行');
  await page.close();
}

async function buildRegression() {
  const directory = await mkdtemp(path.join(os.tmpdir(), 'sanmu-compat-'));
  try {
    await writeFile(path.join(directory, 'index.html'), '<!doctype html><html><head><link rel="stylesheet" href="style.css"></head><body><div id="app"></div><script type="module" src="app.js"></script></body></html>');
    await writeFile(path.join(directory, 'app.js'), 'const value = { nested: { value: 3 } }; document.body.dataset.value = value?.nested?.value ?? 0;');
    await writeFile(path.join(directory, 'style.css'), ':root{--red:#f00}.x{display:flex;gap:8px;background:color-mix(in srgb,var(--red) 20%,transparent)}');
    await prepareCompatibility(directory);
    const output = await readFile(path.join(directory, 'app.js'), 'utf8');
    parse(output, { ecmaVersion: 2019, sourceType: 'script' });
    assert.doesNotMatch(output, /\?\.|\?\?/);
    const runtime = await readFile(path.join(directory, 'sanmu-compat.js'), 'utf8');
    parse(runtime, { ecmaVersion: 2019, sourceType: 'script' });
    const css = await readFile(path.join(directory, 'style.css'), 'utf8');
    assert.doesNotMatch(css, /color-mix\(/);
    assert.match(css, /--sanmu-flex-gap-x/);
    const index = await readFile(path.join(directory, 'index.html'), 'utf8');
    assert.match(index, /sanmu-compat\.js/);
    assert.match(index, /sanmu-compat\.css/);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
}

const browser = await webkit.launch({ headless: true });
try {
  await colourRegression(browser);
  await flexRegression(browser);
} finally {
  await browser.close();
}
await buildRegression();
console.log('兼容回归通过：颜色预乘、Flex 换行/隐藏、Safari 13 语法与打包注入均已验证。');
