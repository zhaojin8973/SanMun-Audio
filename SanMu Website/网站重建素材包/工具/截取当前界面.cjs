// 只读取插件网页源码；截图中的演示曲线不代表宿主音频。
const {chromium}=require('/Users/zhaojin/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const fs=require('node:fs');const path=require('node:path');
const pack=path.resolve(__dirname,'..');const out=path.join(pack,'03-插件实图/当前-2026-09-21');fs.mkdirSync(out,{recursive:true});
(async()=>{const browser=await chromium.launch({channel:'chrome',headless:true});const rows=[];
try{for(const slug of ['eq6','comp','delay','reverb']){
 const port={eq6:50721,comp:50722,delay:50723,reverb:50724}[slug];const url=`http://127.0.0.1:${port}/${slug==='reverb'?'studio.html':'index.html'}${slug==='eq6'?'?demo=1':''}`;
 const page=await browser.newPage({viewport:{width:1100,height:750},deviceScaleFactor:2});const errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto(url);
 await page.waitForFunction(s=>s==='eq6'?!!window.eqApp?.graph:s==='comp'?window.__sanmuComp?.ready:s==='delay'?!!window.sanmuApp?.knobs?.bpm:!!window.__sanmuStudio,slug);
 for(const theme of ['light','dark'])for(let mode=0;mode<4;mode++){
  const modeNames={eq6:['clean','tube','discrete','linear-phase'],comp:['lev','vca','fet','opto'],delay:['modern','tape','bbd','diffused'],reverb:['hall','plate','room','chamber']};
  if(slug==='eq6'){await page.evaluate(t=>{eqApp.updateUi({theme:t,language:'en',tips:'off'});eqApp.select(3);},theme);await page.locator(`input[name=mode][value="${mode}"]`).check({force:true});await page.waitForFunction(()=>eqApp.pendingEdits===0);}
  if(slug==='comp'){await page.evaluate(async({theme,mode})=>{const app=__sanmuComp;await app.preference('theme',theme);await app.preference('language','en');await app.preference('tips','off');await app.change('mode',mode);},{theme,mode});await page.waitForFunction(()=>__sanmuComp.pending===0);}
  if(slug==='delay'){await page.evaluate(async t=>{await __delayPreview.call('ui',{theme:t,language:'en',tips:'off'});sanmuApp.applyTheme(t);sanmuApp.applyLanguage('en');},theme);await page.locator(`input[name=engine][value="${mode}"]`).check({force:true});await page.evaluate(()=>sanmuApp.commandQueue);}
  if(slug==='reverb'){await page.evaluate(t=>{__sanmuStudio.setTheme(t);__sanmuStudio.setLanguage('en');},theme);await page.locator(`input[name=family][value="${mode}"]`).check({force:true});}
  await page.mouse.move(1095,745);await page.waitForTimeout(350);await page.evaluate(()=>document.fonts.ready);
  const file=`${slug}-${modeNames[slug][mode]}-${theme}-en.png`;await page.locator('.instrument').screenshot({path:path.join(out,file)});rows.push({file,product:slug,mode:modeNames[slug][mode],theme,language:'en',url,source:'当前工程网页界面',audio:slug==='eq6'?'内置演示频谱':'无宿主音频',errors:[...errors]});console.log(file);
 }
 await page.close();}
}finally{await browser.close();fs.writeFileSync(path.join(out,'截图来源.json'),JSON.stringify(rows,null,2)+'\n');}})().catch(e=>{console.error(e);process.exit(1)});
