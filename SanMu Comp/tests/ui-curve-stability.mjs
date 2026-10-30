import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync} from 'node:fs';
const {chromium}=await import(process.env.SANMU_PLAYWRIGHT_MODULE||'playwright');
const dir=process.cwd()+'/docs/verification/curve-stability-2026-09-14/';mkdirSync(dir,{recursive:true});
const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
const page=await browser.newPage({viewport:{width:1100,height:750},deviceScaleFactor:2});
const errors=[];page.on('pageerror',e=>errors.push(e.message));const report={cases:[],errors,passed:false};
try{
 await page.goto('http://127.0.0.1:50585/index.html');await page.waitForFunction(()=>window.__sanmuComp?.ready);
 const sound=await page.evaluate(()=>JSON.stringify(window.__sanmuComp.values));
 for(const theme of ['light','dark'])for(const scale of [.85,1,1.5]){
  await page.setViewportSize({width:Math.round(1100*scale),height:Math.round(750*scale)});
  await page.evaluate(theme=>window.__sanmuComp.preference('theme',theme),theme);
  await page.waitForFunction(()=>window.__sanmuComp.pending===0);
  await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
  const result=await page.evaluate(()=>{
   const a=window.__sanmuComp,v=a.visual;v.frames=[];v.paused=false;v.history(true);
   const g=v.canvas.getContext('2d'),move=g.moveTo.bind(g),line=g.lineTo.bind(g),stroke=g.stroke.bind(g);
   const colour=getComputedStyle(document.querySelector('.instrument')).getPropertyValue('--trace-gr').trim();
   let first,last,t=0,seed=31;const edges=[],images=new Set();
   g.moveTo=(x,y)=>{first=[x,y];move(x,y);};g.lineTo=(x,y)=>{last=[x,y];line(x,y);};
   g.stroke=()=>{if(g.strokeStyle===colour)edges.push([first[0],last[0]]);stroke();};
   // 模拟宿主消息间隔 23～46 毫秒：静音完整窗口必须逐像素完全一致。
   for(let i=0;i<180;i++){seed=(seed*16807)%2147483647;t+=23+seed%24;v.feed({time:t,input:-100,output:-100,gr:0});if(i>90){v.paint();images.add(v.canvas.toDataURL());}}
   g.moveTo=move;g.lineTo=line;g.stroke=stroke;
   const border=getComputedStyle(document.querySelector('.engine-controls')).borderBottomWidth;
   // 原始瞬态必须继续送到峰值表，不受显示平滑影响。
   v.feed({time:t+=33,input:-6,output:-12,gr:6});
   const peak={raw:v.latest.gr,held:v.holds.gr,shown:Number(document.querySelector('#meter-gr').textContent),display:v.frames.at(-1).display.gr};
   const saved=JSON.stringify(v.frames);v.paused=true;v.feed({time:t+5000,input:-100,output:-100,gr:0});v.paint();const frozen=JSON.stringify(v.frames)===saved;
   v.paused=false;v.feed({time:t+10000,input:-20,output:-23,gr:3});const restarted=v.frames.length===1;
   v.feed({time:t+10000,input:-18,output:-21,gr:3});const duplicate=v.frames.length===1;
   v.feed({time:NaN,input:-18,output:-21,gr:3});const invalid=v.frames.length===1;
   // 对帧间快速波动做轻度减振，并保持 IN/OUT 的固定 3 dB 差值。
   v.frames=[];t+=11000;const raw=[],shown=[];let maxDeltaError=0;
   for(let i=0;i<90;i++){const value=-18+(i%2?.8:-.8);v.feed({time:t+=33,input:value,output:value-3,gr:3});if(i>30){raw.push(value);shown.push(v.frames.at(-1).display.input);maxDeltaError=Math.max(maxDeltaError,Math.abs(v.frames.at(-1).display.input-v.frames.at(-1).display.output-3));}}
   const spread=values=>Math.max(...values)-Math.min(...values);
   return {border,edges,images:images.size,width:v.canvas.clientWidth,peak,frozen,restarted,duplicate,invalid,rippleRatio:spread(shown)/spread(raw),maxDeltaError};
  });
  assert.equal(result.border,'0px');assert.equal(result.images,1,'静音图像仍在变化');
  assert.ok(result.edges.every(([left,right])=>left===40&&right===result.width-16),'固定时间窗端点伸缩');
  assert.deepEqual([result.peak.raw,result.peak.held,result.peak.shown],[6,6,-6]);assert.ok(result.peak.display>0&&result.peak.display<6);
  assert.ok(result.frozen&&result.restarted&&result.duplicate&&result.invalid);
  assert.ok(result.rippleRatio<.5&&result.rippleRatio>.25);assert.ok(result.maxDeltaError<1e-10);
  report.cases.push({theme,scale,...result,edges:result.edges[0]});
 }
 await page.setViewportSize({width:1100,height:750});
 for(const theme of ['light','dark']){
  await page.evaluate(theme=>window.__sanmuComp.preference('theme',theme),theme);await page.waitForFunction(()=>window.__sanmuComp.pending===0);
  await page.evaluate(()=>{const v=window.__sanmuComp.visual;v.frames=[];v.paused=false;for(let i=0;i<150;i++){const level=-27+12*Math.exp(-((i%25)/25)*3);v.feed({time:100000+i*33,input:level,output:level-3,gr:3});}v.paused=true;v.paint();});
  await page.waitForTimeout(500);await page.screenshot({path:dir+theme+'.png'});
 }
 assert.equal(await page.evaluate(()=>JSON.stringify(window.__sanmuComp.values)),sound);assert.deepEqual(errors,[]);report.passed=true;
}finally{writeFileSync(dir+'report.json',JSON.stringify(report,null,2));await browser.close();}
console.log(JSON.stringify(report,null,2));
