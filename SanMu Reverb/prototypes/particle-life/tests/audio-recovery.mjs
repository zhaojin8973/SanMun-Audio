import { chromium } from 'playwright';
import assert from 'node:assert/strict';
import { mkdirSync, writeFileSync } from 'node:fs';

const browser = await chromium.launch({
  executablePath: process.env.SANMU_CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  headless: true, args: ['--autoplay-policy=no-user-gesture-required'],
});
const page = await browser.newPage();
const report = { passed: false, errors: [], scenarios: [] };
page.on('pageerror', error => report.errors.push(error.message));
const url = process.env.SANMU_PROTOTYPE_URL || 'http://127.0.0.1:5173/studio.html';
async function readyPage() {
  await page.goto(url);
  await page.waitForFunction(() => window.__sanmuStudio?.audio);
}
async function verifyRecovery(label) {
  await page.waitForFunction(() => !document.querySelector('#play').disabled);
  assert.deepEqual(await page.evaluate(() => {
    const a = window.__sanmuStudio.audio;
    return { ready: !!a.ready, context: a.context, output: a.output, worklet: a.worklet };
  }), { ready: false, context: null, output: null, worklet: null });
  await page.locator('#mute').click();
  await page.locator('#volume').fill('0.3');
  await page.locator('#edit-highCut').fill('8.5 kHz');
  await page.locator('#edit-highCut').press('Enter');
  await page.unrouteAll();
  await page.evaluate(() => { window.__sanmuStudio.audio.configuration.worklet = '/audio/suite-worklet.js'; });
  await page.locator('#play').click();
  await page.waitForFunction(() => window.__sanmuStudio.audio.source && window.__sanmuStudio.audio.ready);
  assert.equal(await page.evaluate(() => window.__sanmuStudio.parameters.highCut), 8500);
  await page.locator('#play').click();
  report.scenarios.push(label);
}
try {
  await page.addInitScript(() => localStorage.setItem('sanmu-studio-language', 'zh'));
  await readyPage();
  await page.route('**/audio/suite-dsp.wasm', route => route.fulfill({ status: 503, body: '故障注入' }));
  await page.locator('#play').click();
  await page.waitForFunction(() => document.querySelector('#notification').textContent.includes('模块'));
  await verifyRecovery('模块下载失败后操作控件并重试');

  await readyPage();
  await page.evaluate(() => {
    window.__sanmuStudio.audio.configuration.worklet = URL.createObjectURL(new Blob([
      `registerProcessor('sanmu-suite', class extends AudioWorkletProcessor {
      constructor() { super(); this.port.postMessage({ type: 'failure', message: '测试启动失败' }); }
      process() { return true; }
    });`], { type: 'application/javascript' }));
  });
  await page.locator('#play').click();
  await page.waitForFunction(() => document.querySelector('#notification').textContent.includes('测试启动失败'));
  await verifyRecovery('工作节点创建后启动失败，静音和音量操作无空对象错误');

  await readyPage();
  await page.evaluate(() => {
    window.__sanmuStudio.audio.configuration.worklet = URL.createObjectURL(new Blob([
      `registerProcessor('sanmu-suite', class extends AudioWorkletProcessor {
      constructor() { super(); this.frames = 0; this.port.postMessage({ type: 'ready' }); }
      process() { if (++this.frames > 80) throw new Error('测试运行中故障'); return true; }
    });`], { type: 'application/javascript' }));
  });
  await page.locator('#play').click();
  await page.waitForFunction(() => document.querySelector('#notification').textContent.includes('声音处理已停止'));
  await verifyRecovery('真实音频工作线程抛错后重新创建并播放');
  assert.deepEqual(report.errors, []);
  report.passed = true;
  console.log('浏览器真实音频故障恢复检查通过');
} finally {
  mkdirSync('test-results/audio-recovery', { recursive: true });
  writeFileSync('test-results/audio-recovery/report.json', JSON.stringify(report, null, 2));
  await browser.close();
}
