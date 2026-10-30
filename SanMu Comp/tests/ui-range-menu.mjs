import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync} from 'node:fs';
const {chromium}=await import(process.env.SANMU_PLAYWRIGHT_MODULE||'playwright');
const dir=process.cwd()+'/docs/verification/gr-menu-2026-09-12/';mkdirSync(dir,{recursive:true});
const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
const page=await browser.newPage({viewport:{width:1100,height:750},deviceScaleFactor:2});const errors=[];page.on('pageerror',e=>errors.push(e.message));
const report={themes:[],layouts:0,keyboard:false,parametersUnchanged:false,errors};
try{
 await page.goto('http://127.0.0.1:50585/index.html');await page.waitForFunction(()=>window.__sanmuComp?.ready);
 const wait=()=>page.waitForFunction(()=>window.__sanmuComp.pending===0);
 const before=await page.evaluate(()=>JSON.stringify(window.__sanmuComp.values));
 for(const theme of ['light','dark']){
  await page.evaluate(theme=>window.__sanmuComp.preference('theme',theme),theme);await wait();
  for(const scale of [.85,1,1.5]){
   await page.setViewportSize({width:Math.round(1100*scale),height:Math.round(750*scale)});
   await page.locator('#gr-range').click();
   const b=await page.locator('#gr-range-menu').boundingBox();assert.ok(b.x>=0&&b.y>=0&&b.x+b.width<=1100*scale&&b.y+b.height<=750*scale);
   await page.locator('#gr-range-menu [value="24"]').click();await wait();
   assert.equal(await page.evaluate(()=>window.__sanmuComp.ui.grRange),24);
   assert.equal(await page.locator('#gr-range').getAttribute('aria-expanded'),'false');report.layouts++;
  }
  await page.setViewportSize({width:1100,height:750});await page.locator('#gr-range').click();
  report.themes.push(await page.locator('#gr-range-menu').evaluate(e=>({theme:document.documentElement.dataset.theme,background:getComputedStyle(e).backgroundColor,color:getComputedStyle(e).color})));
  await page.screenshot({path:dir+theme+'.png'});
  await page.locator('.stage').screenshot({path:dir+theme+'-detail.png'});
  await page.keyboard.press('Escape');assert.equal(await page.locator('#gr-range').evaluate(e=>document.activeElement===e),true);
 }
 assert.notEqual(report.themes[0].background,report.themes[1].background);
 await page.locator('#gr-range').focus();await page.keyboard.press('Enter');await page.keyboard.press('Home');await page.keyboard.press('Enter');await wait();assert.equal(await page.evaluate(()=>window.__sanmuComp.ui.grRange),6);
 await page.keyboard.press('ArrowDown');await page.keyboard.press('ArrowDown');await page.keyboard.press('Space');await wait();assert.equal(await page.evaluate(()=>window.__sanmuComp.ui.grRange),12);
 await page.keyboard.press('ArrowUp');await page.keyboard.press('End');await page.keyboard.press('Escape');assert.equal(await page.evaluate(()=>window.__sanmuComp.ui.grRange),12);
 await page.keyboard.press('Enter');await page.keyboard.press('Tab');assert.equal(await page.locator('#gr-range-menu').isVisible(),false);assert.equal(await page.locator('#pause').evaluate(e=>document.activeElement===e),true);
 await page.locator('#gr-range').click();await page.locator('#listen').click();assert.equal(await page.locator('#gr-range-menu').isVisible(),false);await page.locator('#listen-dialog .close').click();
 report.keyboard=true;assert.equal(await page.evaluate(()=>JSON.stringify(window.__sanmuComp.values)),before);report.parametersUnchanged=true;assert.deepEqual(errors,[]);
}finally{writeFileSync(dir+'report.json',JSON.stringify(report,null,2));await browser.close();}
console.log(JSON.stringify(report,null,2));
