// 直接对照现代源样式和实际发布样式，检查材质颜色与关键控件的布局差异。
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFileSync,existsSync,mkdirSync,writeFileSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import {resolve,extname,dirname} from 'node:path';
const workspace=resolve(dirname(fileURLToPath(import.meta.url)),'../..');
const compiledRoot=process.env.SANMU_UI_COMPILED_ROOT||resolve(workspace,'audit/catalina-20261005');
const outputs=process.env.SANMU_UI_AUDIT_OUTPUT||resolve(compiledRoot,'css-parity');
mkdirSync(outputs,{recursive:true});
const products={eq:['SanMu EQ',3],comp:['SanMu Comp',4],delay:['SanMu Delay',4]};
const types={'.html':'text/html','.css':'text/css','.js':'text/javascript','.json':'application/json'};
const server=createServer((request,response)=>{
 const route=decodeURIComponent(new URL(request.url,'http://localhost').pathname).split('/').filter(Boolean),source=route.shift()==='source',slug=route.shift(),product=products[slug];
 if(!product){response.writeHead(404).end();return;}
 const base=source?resolve(workspace,product[0],'ui'):resolve(compiledRoot,slug+'-ui'),file=resolve(base,...route);
 if(!file.startsWith(base+'/')||!existsSync(file)){response.writeHead(404).end();return;}
 response.setHeader('Content-Type',types[extname(file)]||'application/octet-stream');response.end(readFileSync(file));
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const address=`http://127.0.0.1:${server.address().port}`;
const {chromium,webkit}=await import(process.env.PLAYWRIGHT_MODULE||'/Users/zhaojin/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright/index.mjs');
const browserName=process.env.SANMU_UI_BROWSER||'chromium';
const browser=await (browserName==='webkit'?webkit.launch({headless:true}):chromium.launch({executablePath:process.env.CHROME_PATH||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true}));
const report={passed:false,browser:browserName,cases:0,colorDifferences:[],layoutDifferences:[],errors:[]};
const signature=async(page,{product,theme,mode})=>{
 await page.evaluate(({product,theme,mode})=>{
  if(product==='eq'){const app=eqApp;app.ui.theme=theme;app.ui.language='en';app.values.character_mode=mode;app.select(3);app.refresh();app.resize();app.tools.show(3);}
  else if(product==='comp'){const app=__sanmuComp;app.ui.theme=theme;app.ui.language='en';app.values.mode=mode;app.refresh();window.dispatchEvent(new Event('resize'));}
  else{const app=sanmuApp;app.ui.theme=theme;app.ui.language='en';app.values.engine=mode;app.applyTheme(theme);app.applyLanguage('en');app.updateAll();window.dispatchEvent(new Event('resize'));}
 },{product,theme,mode});
 await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
 await page.locator('#settings').click();
 await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
 const result=await page.evaluate(()=>{
  const canvas=document.createElement('canvas');canvas.width=canvas.height=1;const context=canvas.getContext('2d');
  const pixel=color=>{context.clearRect(0,0,1,1);context.fillStyle='rgba(0,0,0,0)';context.fillStyle=color;context.fillRect(0,0,1,1);return [...context.getImageData(0,0,1,1).data];};
  const selectors=['.instrument','.topbar','.preset-control','.families','.topbar .tools','.modebar .listen-button','#level-range','#gr-range','.layers','.history-duration','.engine-controls .knob','.mode-routing','.echo-precision','.eq-node[data-band="3"]','#node-tools','#inspector-type-open','#settings-dialog','#settings-dialog .theme-options input:checked + span','.knob-readout input'];
  const items={};
  for(const selector of selectors){const element=document.querySelector(selector);if(!element)continue;const style=getComputedStyle(element),rect=element.getBoundingClientRect();
   items[selector]={rect:[rect.x,rect.y,rect.width,rect.height],colors:Object.fromEntries(['color','background-color','border-top-color','--knob-top','--knob-bottom','--knob-track','--panel'].map(property=>[property,pixel(style.getPropertyValue(property).trim())]))};
  }
  return items;
 });
 await page.locator('#settings-dialog .close').click();return result;
};
try{
 for(const legacy of [false,true])for(const [product,[,modeCount]] of Object.entries(products)){
  const source=await browser.newPage({viewport:{width:1100,height:750},deviceScaleFactor:2}),compiled=await browser.newPage({viewport:{width:1100,height:750},deviceScaleFactor:2});
  compiled.on('pageerror',e=>report.errors.push(e.message));source.on('pageerror',e=>report.errors.push(e.message));
  if(legacy)await compiled.addInitScript(()=>{delete HTMLDialogElement.prototype.showModal;delete HTMLDialogElement.prototype.show;delete HTMLDialogElement.prototype.close;window.__SANMU_FORCE_LEGACY_FLEX_GAP=true;});
  await source.goto(`${address}/source/${product}/index.html?demo=1`);await compiled.goto(`${address}/compiled/${product}/index.html?demo=1`);
  const ready=page=>page.waitForFunction(product=>!!(product==='eq'?window.eqApp:product==='comp'?window.__sanmuComp?.ready:window.sanmuApp?.knobs?.bpm),product);
  await Promise.all([ready(source),ready(compiled)]);
  for(const theme of ['light','dark'])for(let mode=0;mode<modeCount;mode++){
   const [expected,actual]=await Promise.all([signature(source,{product,theme,mode}),signature(compiled,{product,theme,mode})]);
   for(const [selector,reference] of Object.entries(expected)){
    const tested=actual[selector];if(!tested){report.layoutDifferences.push({product,legacy,theme,mode,selector,missing:true});continue;}
    for(const [property,rgba] of Object.entries(reference.colors))if(rgba.some((value,index)=>Math.abs(value-tested.colors[property][index])>2))report.colorDifferences.push({product,legacy,theme,mode,selector,property,expected:rgba,actual:tested.colors[property]});
    if(reference.rect.some((value,index)=>Math.abs(value-tested.rect[index])>1.5))report.layoutDifferences.push({product,legacy,theme,mode,selector,expected:reference.rect,actual:tested.rect});
   }
   report.cases++;
  }
  await source.close();await compiled.close();
 }
 report.passed=report.colorDifferences.length===0&&report.layoutDifferences.length===0&&report.errors.length===0;
}finally{writeFileSync(`${outputs}/report.json`,JSON.stringify(report,null,2));await browser.close();await new Promise(resolve=>server.close(resolve));}
console.log(JSON.stringify({...report,colorDifferences:report.colorDifferences.slice(0,12),layoutDifferences:report.layoutDifferences.slice(0,12)},null,2));
assert(report.passed,'发布样式与现代样式仍有差异，详细数据见报告');
