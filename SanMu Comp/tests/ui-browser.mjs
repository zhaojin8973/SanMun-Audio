import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
const {chromium}=await import(process.env.SANMU_PLAYWRIGHT_MODULE||'playwright');
const dir=fileURLToPath(new URL('../docs/verification/ui-details-2026-09-12/',import.meta.url));mkdirSync(dir,{recursive:true});
const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
const page=await browser.newPage({viewport:{width:1100,height:750},deviceScaleFactor:2});const errors=[];page.on('pageerror',e=>errors.push(e.message));
const report={layouts:0,interactions:[],errors,passed:false};
try{
 await page.goto('http://127.0.0.1:50585/index.html');await page.waitForFunction(()=>window.__sanmuComp?.ready);
 const wait=()=>page.waitForFunction(()=>window.__sanmuComp.pending===0);
 const change=async(id,value)=>{await page.evaluate(([id,value])=>window.__sanmuComp.change(id,value),[id,value]);await wait();};
 const read=id=>page.evaluate(id=>window.__sanmuComp.values[id],id);
 for(const theme of ['light','dark'])for(let mode=0;mode<4;mode++){
  await page.evaluate(theme=>window.__sanmuComp.preference('theme',theme),theme);await wait();await change('mode',mode);
  for(const scale of [.85,1,1.15,1.25,1.5]){
   await page.setViewportSize({width:Math.round(1100*scale),height:Math.round(750*scale)});await page.waitForTimeout(60);
   const layout=await page.evaluate(()=>{const root=document.querySelector('.instrument').getBoundingClientRect();const controls=[...document.querySelectorAll('.knob,.fet-ratios,.opto-mode,.smooth-control,.topbar>.tools,.preset-control,.details,.plot-tools')];return {overflow:controls.filter(e=>{const b=e.getBoundingClientRect();return b.left<root.left||b.right>root.right+1||b.bottom>root.bottom+1||b.top<root.top;}).map(e=>e.className),scroll:document.documentElement.scrollWidth>innerWidth+1||document.documentElement.scrollHeight>innerHeight+1};});
   assert.deepEqual(layout.overflow,[],JSON.stringify({theme,mode,scale,layout}));assert.equal(layout.scroll,false);report.layouts++;
  }
  await page.setViewportSize({width:1100,height:750});
  await page.evaluate(()=>{const app=window.__sanmuComp;app.visual.frames=[];for(let i=0;i<60;i++){const age=(i%15)/30;app.visual.feed({time:i*33,input:-23+12*Math.exp(-age/.15),output:-23+12*Math.exp(-age/.15)-(2+3*Math.exp(-age/.18))+3,gr:2+3*Math.exp(-age/.18)});}});
  await page.screenshot({path:dir+theme+'-'+['lev','vca','fet','opto'][mode]+'.png'});
 }
 await change('mode',2);await change('fet_attack',3);
 const dial=page.locator('#knob-fet_attack');let b=await dial.boundingBox();
 await page.mouse.move(b.x+b.width/2,b.y+b.height/2);await page.mouse.down();await page.mouse.move(b.x+b.width/2,b.y+b.height/2-48,{steps:5});await page.mouse.up();await wait();assert.ok(Math.abs((await read('fet_attack'))-4.2)<.11);report.interactions.push('垂直拖动');
 await page.keyboard.down('Shift');b=await dial.boundingBox();await page.mouse.move(b.x+b.width/2,b.y+b.height/2);await page.mouse.down();await page.mouse.move(b.x+b.width/2,b.y+b.height/2-48,{steps:5});await page.mouse.up();await page.keyboard.up('Shift');await wait();assert.ok(Math.abs((await read('fet_attack'))-5.4)<.11);assert.equal(await dial.evaluate(e=>getComputedStyle(e).outlineStyle),'none');report.interactions.push('Shift 保持普通步进且无外围圆环');
 await change('fet_attack',7);await page.mouse.move(b.x+b.width/2,b.y+b.height/2);await page.mouse.down();await page.mouse.move(b.x+b.width/2,b.y+b.height/2-35);await page.mouse.move(b.x+b.width/2,b.y+b.height/2-15);await page.mouse.up();await wait();assert.ok((await read('fet_attack'))<7);report.interactions.push('边界反向立即响应');
 const input=page.locator('#edit-fet_attack');await input.fill('6.2');await input.press('Enter');await wait();assert.ok(Math.abs((await read('fet_attack'))-6.2)<.001);
 await input.fill('99');await input.press('Enter');await wait();assert.ok(Math.abs((await read('fet_attack'))-6.2)<.001);assert.ok(await page.locator('[data-parameter=fet_attack]').evaluate(e=>e.classList.contains('invalid')));
 await input.fill('2.0');await input.press('Escape');await wait();assert.ok(Math.abs((await read('fet_attack'))-6.2)<.001);report.interactions.push('数值输入、非法值、Esc');
 await change('in_out_link',1);await change('fet_input',22);const old=await read('fet_output');await change('fet_input',23);assert.equal(await read('fet_output'),old-1);report.interactions.push('FET 反向联动');
 await change('mode',0);await change('vox_comp',-12);b=await page.locator('#knob-vox_comp').boundingBox();await page.mouse.move(b.x+b.width/2,b.y+b.height/2);await page.mouse.down();await page.mouse.move(b.x+b.width/2,b.y+b.height/2-24);await page.mouse.up();await wait();assert.ok((await read('vox_comp'))<-12);report.interactions.push('LEV 压缩量方向');
 await page.locator('#settings').click();await page.locator('[name=language][value=zh]').check({force:true});await wait();await page.locator('[name=theme][value=light]').check({force:true});await wait();await page.screenshot({path:dir+'settings-zh.png'});await page.locator('#settings-dialog .close').click();
 await page.locator('#listen').click();assert.equal(await page.locator('#listen-start').isDisabled(),true);assert.ok(await page.locator('#listen-preview').isVisible());await page.locator('#listen-target').fill('4');await page.screenshot({path:dir+'listen-zh.png'});await page.locator('#listen-dialog .close').click();
 const before=await page.evaluate(()=>JSON.stringify(window.__sanmuComp.values));await page.locator('#pause').click();await page.locator('#gr-range').click();await page.locator('#gr-range-menu [value="24"]').click();await page.locator('.layers input[value=input]').check({force:true});await wait();assert.equal(await page.evaluate(()=>JSON.stringify(window.__sanmuComp.values)),before);report.interactions.push('显示交互不改声音参数');
 const page2=await browser.newPage();await page2.goto('http://127.0.0.1:50585/index.html');await page2.waitForFunction(()=>window.__sanmuComp?.ready);await page2.evaluate(()=>window.__sanmuComp.preference('theme','dark'));await page2.waitForFunction(()=>window.__sanmuComp.pending===0);assert.equal(await page.evaluate(()=>document.documentElement.dataset.theme),'light');report.interactions.push('实例主题隔离');
 assert.deepEqual(errors,[]);report.passed=true;
}finally{writeFileSync(dir+'browser-report.json',JSON.stringify(report,null,2));await browser.close();}
console.log(JSON.stringify(report,null,2));
