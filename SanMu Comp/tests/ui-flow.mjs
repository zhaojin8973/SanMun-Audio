import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync} from 'node:fs';
const {chromium}=await import(process.env.SANMU_PLAYWRIGHT_MODULE||'playwright');
const dir=process.cwd()+'/docs/verification/levels-2026-09-13/';mkdirSync(dir,{recursive:true});
const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
const page=await browser.newPage({viewport:{width:1100,height:750},deviceScaleFactor:2});const errors=[];page.on('pageerror',e=>errors.push(e.message));
const report={curve:[],clicks:0,errors,passed:false};
const wait=()=>page.waitForFunction(()=>window.__sanmuComp?.ready&&window.__sanmuComp.pending===0);
try{
 await page.goto('http://127.0.0.1:50585/index.html');await wait();
 await page.evaluate(()=>{const a=window.__sanmuComp;a.visual.frames=[{time:0,input:-18,output:-21,gr:3},{time:2000,input:-18,output:-21,gr:3}];a.visual.paused=true;a.visual.draw();});
 const sound=await page.evaluate(()=>JSON.stringify(window.__sanmuComp.values));
 for(const theme of ['light','dark']){
  await page.evaluate(theme=>window.__sanmuComp.preference('theme',theme),theme);await wait();
  const offsets=[];
  for(const range of [6,12,24]){
   await page.locator('#gr-range').click();await page.locator(`#gr-range-menu [value="${range}"]`).click();await wait();
   const trace=await page.evaluate(async()=>{
    const a=window.__sanmuComp,v=a.visual,g=v.canvas.getContext('2d');await new Promise(requestAnimationFrame);
    const line=g.lineTo.bind(g),stroke=g.stroke.bind(g);let points=[],grPoints=[];const colour=getComputedStyle(document.querySelector('.instrument')).getPropertyValue('--trace-gr').trim();
    g.lineTo=(x,y)=>{points.push([x,y]);line(x,y);};g.stroke=()=>{if(g.strokeStyle===colour)grPoints=[...points];points=[];stroke();};v.paint();g.lineTo=line;g.stroke=stroke;
    const xy=grPoints.at(-1),expectedY=xy[1],dpr=v.canvas.width/v.canvas.clientWidth;
    // 同时读取栅格像素，避免只验证数值或标注。
    const bytes=Array.from(g.getImageData(Math.round(300*dpr),Math.floor(expectedY*dpr),1,1).data);
    return {range:a.ui.grRange,y:expectedY,bytes,frames:JSON.stringify(v.frames)};
   });
   assert.ok(trace.bytes[3]>120,'曲线位置没有实际绘制像素');offsets.push(trace.y-14);report.curve.push({theme,...trace});
   await page.locator('#stage').screenshot({path:dir+`${theme}-gr-${range}.png`});
  }
  assert.ok(Math.abs(offsets[0]/offsets[1]-2)<.001);assert.ok(Math.abs(offsets[1]/offsets[2]-2)<.001);
 }
 // 以同一帧的 IN=-18、OUT=-21 验证两条实际曲线同时放大，GR 独立。
 report.levelZoom=[];
 for(const range of [60,30]){
  await page.locator('#level-range').click();await page.locator(`#level-range-menu [value="${range}"]`).click();await wait();
  const points=await page.evaluate(()=>{
   const a=window.__sanmuComp,v=a.visual,g=v.canvas.getContext('2d'),line=g.lineTo.bind(g),stroke=g.stroke.bind(g),style=getComputedStyle(document.querySelector('.instrument'));let path=[],curves={};
   g.lineTo=(x,y)=>{path.push([x,y]);line(x,y);};g.stroke=()=>{for(const [key,colour] of [['input','--trace-in'],['output','--trace-out'],['gr','--trace-gr']])if(g.strokeStyle===style.getPropertyValue(colour).trim())curves[key]=path.at(-1)?.[1];path=[];stroke();};v.paint();g.lineTo=line;g.stroke=stroke;return curves;
  });
  report.levelZoom.push({range,...points});
 }
 assert.ok(Math.abs((report.levelZoom[1].output-report.levelZoom[1].input)/(report.levelZoom[0].output-report.levelZoom[0].input)-2)<.001);
 assert.equal(report.levelZoom[0].gr,report.levelZoom[1].gr);
 await page.locator('#level-range').click();await page.keyboard.press('Home');await page.keyboard.press('Enter');await wait();
 assert.equal(await page.evaluate(()=>window.__sanmuComp.ui.levelRange),60);
 assert.equal(await page.evaluate(()=>JSON.stringify(window.__sanmuComp.values)),sound);
 const saved=await page.evaluate(()=>JSON.stringify(window.__sanmuComp.visual.frames));
 await page.evaluate(()=>window.__sanmuComp.visual.feed({time:80000,input:-12,output:-15,gr:3}));
 assert.equal(await page.evaluate(()=>JSON.stringify(window.__sanmuComp.visual.frames)),saved,'暂停后断流清空曲线');
 await page.locator('#listen').click();assert.ok(await page.locator('#listen-preview').isVisible());assert.ok(await page.locator('#listen-start').isDisabled());await page.locator('#listen-dialog .close').click();
 // 模拟原生协议的消息频率和延迟，只测界面响应；音频采集另由 C++ 集成回归验证。
 const schema=await (await fetch('http://127.0.0.1:50585/parameter-schema.json')).json();
 await page.addInitScript(state=>{
  state.ui={...state.ui,theme:'light',language:'zh',history:true,scale:1};state.listen={status:'idle',seconds:0,progress:0,target:4};
  const listeners={};window.__flow={commands:[],state,fail:false};
  const emit=(name,data)=>listeners[name]?.forEach(fn=>fn(data));
  window.__JUCE__={initialisationData:{sanmuComp:true},backend:{addEventListener(name,fn){(listeners[name]??=[]).push(fn);},emitEvent(name,message){
   const [cmd,data]=message.params;window.__flow.commands.push(cmd);
   setTimeout(()=>{
    if(cmd==='listenStart'&&window.__flow.fail){emit('__juce__complete',{promiseId:message.resultId,result:{error:'测试连接失败'}});return;}
    if(cmd==='listenStart')state.listen={...state.listen,status:'waiting',target:data.target};
    if(cmd==='listenCancel')state.listen.status='idle';if(cmd==='ui')Object.assign(state.ui,data);
    emit('__juce__complete',{promiseId:message.resultId,result:structuredClone(state)});
   },cmd==='listenStart'?350:5);
  }}};
  setInterval(()=>emit('sanmuFrame',{...structuredClone(state),meter:{time:performance.now(),input:-18,output:-21,gr:3}}),33);
 },schema);
 await page.reload();await wait();
 for(const width of [506,935,1100]){
  await page.setViewportSize({width,height:Math.round(width*750/1100)});
  await page.waitForFunction(()=>Math.abs(window.__sanmuComp.scale-Math.min(innerWidth/1100,innerHeight/750))<.0001);
  await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));
  for(let i=0;i<10;i++){
   const before=await page.evaluate(()=>window.__sanmuComp.visual.paused);const box=await page.locator('#pause').boundingBox();
   await page.mouse.move(box.x+box.width/2,box.y+box.height/2);await page.mouse.down();await page.waitForTimeout(90);await page.mouse.up();
   assert.equal(await page.evaluate(()=>window.__sanmuComp.visual.paused),!before,JSON.stringify({width,i,box}));report.clicks++;
  }
  for(let i=0;i<4;i++){
   await page.locator('#listen').click();assert.ok(await page.locator('#listen-dialog').isVisible());
   await page.locator('#listen-target').fill('4');
   const box=await page.locator('#listen-start').boundingBox();await page.mouse.move(box.x+box.width/2,box.y+box.height/2);await page.mouse.down();await page.waitForTimeout(90);await page.mouse.up();
   assert.equal(await page.evaluate(()=>window.__sanmuComp.listenRequest),'starting');await wait();
   assert.equal(await page.evaluate(()=>window.__sanmuComp.listen.status),'waiting');
   assert.ok((await page.locator('#listen-progress').textContent()).includes('请在宿主播放'));
   await page.locator('#listen-cancel').click();await wait();await page.locator('#listen-dialog .close').click();report.clicks++;
  }
 }
 await page.setViewportSize({width:1100,height:750});await page.locator('#listen').click();
 await page.locator('#listen-target').fill('25');await page.locator('#listen-target').press('Enter');assert.ok((await page.locator('#listen-error').textContent()).includes('24'));
 await page.locator('#listen-target').fill('6');await page.locator('#listen-target').press('Enter');await wait();assert.equal(await page.evaluate(()=>window.__sanmuComp.listen.status),'waiting');
 await page.screenshot({path:dir+'waiting.png'});await page.locator('#listen-cancel').click();await wait();
 await page.evaluate(()=>window.__flow.fail=true);await page.locator('#listen-start').click();await wait();await page.waitForFunction(()=>!window.__sanmuComp.listenRequest);assert.ok((await page.locator('#listen-error').textContent()).includes('测试连接失败'));assert.ok(await page.locator('#listen-start').isEnabled());
 assert.deepEqual(errors,[]);report.passed=true;
}finally{writeFileSync(dir+'report.json',JSON.stringify(report,null,2));await browser.close();}
console.log(JSON.stringify(report,null,2));
