// 第四轮：真实鼠标操作、频谱选峰、自动阈值控件和所有尺寸的参数对齐。
const assert=require('node:assert/strict');
const {chromium}=require(process.env.SANMU_PLAYWRIGHT||'/Users/zhaojin/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
(async()=>{
  const browser=await chromium.launch({channel:'chrome',headless:true});
  const page=await browser.newPage({viewport:{width:1100,height:750},deviceScaleFactor:2});
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto('http://127.0.0.1:50614/index.html?demo=1');await page.waitForFunction(()=>window.eqApp);
  const idle=()=>page.waitForFunction(()=>eqApp.pendingEdits===0);
  const node=n=>page.locator(`.eq-node[data-band="${n}"]`);
  const show=async n=>{await node(n).hover();await page.waitForFunction(()=>!document.querySelector('#node-tools').hidden);};
  const choose=async(field,value,place='inspector')=>{await page.locator(`#${place}-${field}-open`).click();await page.locator(`#band-choice-menu [data-value="${value}"]`).click();await idle();};
  const set=async(id,value)=>{await page.evaluate(([id,value])=>{eqApp.begin();eqApp.set(id,value);eqApp.end();},[id,value]);await idle();};
  const shot=name=>page.screenshot({path:`docs/界面重设计/第四轮-${name}.png`});
  assert.equal(await page.locator('input[type=checkbox]').count(),0);
  assert.equal(await page.locator('#band-solo,#band-enable,#band-delete,#latency,#sample-rate').count(),0);
  assert.equal(await page.locator('#response').evaluate(e=>e.clientHeight),380);
  assert(await page.locator('#node-tools').isHidden());
  await show(3);assert.equal(await page.evaluate(()=>eqApp.selected),3);
  const card=await page.locator('#node-tools').boundingBox();assert(card.width<=184&&card.height<=70);
  await page.locator('#node-tools').hover();await page.waitForTimeout(230);assert(await page.locator('#node-tools').isVisible());await shot('小巧信息卡');
  await page.mouse.move(1000,65);await page.waitForTimeout(250);assert(await page.locator('#node-tools').isHidden());
  const nodeBounds=await node(3).boundingBox();await page.mouse.move(nodeBounds.x+12,nodeBounds.y+12);await page.mouse.down();
  assert(await page.locator('#node-tools').isVisible());const startCard=await page.locator('#node-tools').boundingBox();
  await page.mouse.move(nodeBounds.x+52,nodeBounds.y-22,{steps:5});assert(await page.locator('#node-tools').isVisible());const endCard=await page.locator('#node-tools').boundingBox();assert(endCard.x>startCard.x+25);
  assert.match(await page.locator('.node-gain').textContent(),/dB/);assert.match(await page.locator('.node-q').textContent(),/Q /);await page.mouse.up();await idle();
  await page.locator('#undo').click();await idle();await show(3);await page.locator('#node-more').click();await page.locator('[data-menu-action=invert]').click();await idle();assert(await page.evaluate(()=>eqApp.values.band3_gain>0));await page.locator('#undo').click();await idle();
  await show(3);await page.locator('#node-more').click();await page.locator('[data-menu-action=copy]').click();await idle();
  await show(4);await page.locator('#node-more').click();await page.locator('[data-menu-action=paste]').click();await idle();assert.equal(await page.evaluate(()=>eqApp.values.band4_freq),await page.evaluate(()=>eqApp.values.band3_freq));await page.locator('#undo').click();await idle();
  await show(3);await page.locator('#node-more').click();await page.locator('[data-menu-action=duplicate]').click();await idle();assert.equal(await page.evaluate(()=>eqApp.selected),7);assert.equal(await page.evaluate(()=>eqApp.values.band7_q),await page.evaluate(()=>eqApp.values.band3_q));await page.locator('#undo').click();await idle();assert.equal(await page.evaluate(()=>eqApp.values.band7_present),0);
  await show(3);await page.locator('#node-more').click();await page.locator('[data-menu-action=placement]').click();await page.locator('#band-choice-menu [data-value="2"]').click();await idle();assert.equal(await page.evaluate(()=>eqApp.values.band3_placement),2);
  await choose('placement',3);assert.equal(await node(3).locator('small').textContent(),'M');
  await choose('placement',0);assert(await node(3).locator('small').isHidden());
  await choose('type',3);assert.equal(await page.locator('#gain-knob .dial').getAttribute('aria-disabled'),'true');assert(await page.locator('#gain-knob').isVisible());
  const gain=await page.evaluate(()=>eqApp.values.band3_gain);await page.locator('#gain-knob .dial').click({modifiers:['Alt'],force:true});assert.equal(await page.evaluate(()=>eqApp.values.band3_gain),gain);
  for(const value of [1,2,3]){await page.locator('#slope-knob .dial').press('ArrowRight');await idle();assert.equal(await page.evaluate(()=>eqApp.values.band3_slope),value);assert.equal(await page.locator('#slope-knob input').inputValue(),String(12*(value+1)));}
  await page.locator('#slope-knob input').fill('24 dB/oct');await page.locator('#slope-knob input').press('Enter');await idle();assert.equal(await page.evaluate(()=>eqApp.values.band3_slope),1);
  await choose('type',0);assert.equal(await page.locator('#slope-knob .dial').getAttribute('aria-disabled'),'true');
  await page.locator('#band-dynamic').click();await idle();assert.equal(await page.locator('#dyn-auto').getAttribute('aria-pressed'),'true');assert(await page.locator('#threshold-knob input').isDisabled());
  await page.locator('#dyn-manual').click();await idle();assert(!await page.locator('#threshold-knob input').isDisabled());
  await page.locator('#threshold-knob input').fill('-31');await page.locator('#threshold-knob input').press('Enter');await idle();
  await page.locator('#dyn-auto').click();await idle();assert.equal(await page.evaluate(()=>eqApp.values.band3_dyn_threshold),-31);
  await page.locator('#dyn-manual').click();await idle();assert.equal(await page.locator('#threshold-knob input').inputValue(),'-31.0');
  await page.locator('#range-knob .dial').press('ArrowRight');await idle();assert(await page.evaluate(()=>eqApp.values.band3_dyn_range>6));await page.locator('#undo').click();await idle();assert.equal(await page.evaluate(()=>eqApp.values.band3_dyn_range),6);
  await page.locator('#dyn-auto').click();await idle();
  const colours=[];
  for(let mode=0;mode<4;mode++){
    await page.locator(`input[name=mode][value="${mode}"]`).check({force:true});await idle();
    assert.equal(await page.evaluate(()=>eqApp.values.eq_mode),mode===3?1:0);
    colours.push(await page.locator('#input-knob').evaluate(e=>getComputedStyle(e).getPropertyValue('--knob-accent')));
    if(mode===3)assert(!(await page.locator('#band-dynamic').isDisabled()));
  }
  assert.equal(new Set(colours).size,4);await page.locator('input[name=mode][value="1"]').check({force:true});await idle();
  for(const id of ['auto-gain','phase-invert']){assert(await page.locator(`.topbar #${id}`).count());await page.locator('#'+id).click();await idle();assert.equal(await page.locator('#'+id).getAttribute('aria-pressed'),'true');await page.locator('#'+id).click();await idle();}
  await page.locator('#q-knob .dial').click({modifiers:['Meta']});await idle();assert.equal(await page.evaluate(()=>eqApp.knobs[2].normalized()),.5);
  await set('band3_gain',5);await node(3).click({modifiers:['Alt']});await idle();assert.equal(await page.evaluate(()=>eqApp.values.band3_gain),0);
  await page.locator('#frequency-knob input').fill('1.5 kHz');await page.locator('#frequency-knob input').press('Enter');await idle();assert.equal(await page.evaluate(()=>eqApp.values.band3_freq),1500);
  await show(3);const listen=page.locator('#node-tools [data-action=headphones]'),r=await listen.boundingBox();
  await page.mouse.move(r.x+r.width/2,r.y+r.height/2);await page.mouse.down();assert.equal(await page.evaluate(()=>eqApp.auditionBand),3);
  await page.mouse.move(r.x+75,r.y+r.height/2,{steps:6});await idle();assert(await page.evaluate(()=>eqApp.values.band3_freq>1500));await page.mouse.up();await idle();assert.equal(await page.evaluate(()=>eqApp.auditionBand),-1);
  await show(3);await listen.focus();await page.keyboard.down('Space');assert.equal(await page.evaluate(()=>eqApp.auditionBand),3);await page.evaluate(()=>window.dispatchEvent(new Event('blur')));await page.keyboard.up('Space');assert.equal(await page.evaluate(()=>eqApp.auditionBand),-1);
  await page.mouse.move(1000,70);await page.waitForTimeout(250);
  await page.evaluate(()=>{eqApp.frame.spectrum=Array.from({length:500},(_,i)=>{const x=i/499;return -72+35*Math.exp(-Math.pow((x-.18)/.025,2))+29*Math.exp(-Math.pow((x-.52)/.035,2))+40*Math.exp(-Math.pow((x-.78)/.025,2));});eqApp.graph.motion.values=[];eqApp.graph.draw();});
  await page.locator('#spectrum-grab').click();await page.waitForTimeout(200);
  assert.equal(await page.locator('.spectrum-peak').count(),3);assert.equal(await page.locator('#nodes').evaluate(e=>getComputedStyle(e).opacity),'0');assert(await page.locator('#node-tools').isHidden());assert.equal(await page.locator('.candidate-node').count(),3);await shot('实时选峰');
  const before=Number(await page.locator('.spectrum-peak').first().getAttribute('data-frequency'));
  await page.evaluate(()=>{eqApp.frame.spectrum=Array.from({length:500},(_,i)=>-75+45*Math.exp(-Math.pow((i/499-.4)/.035,2)));window.liveTest=setInterval(()=>eqApp.graph.draw(),16);});await page.waitForTimeout(1400);
  const after=await page.locator('.spectrum-peak').evaluateAll(es=>es.map(e=>Number(e.dataset.frequency)));assert(after.some(f=>Math.abs(Math.log2(f/(20*Math.pow(1000,.4))))<.1));assert(!after.some(f=>Math.abs(Math.log2(f/before))<.1),'候选峰未随新频谱更新');await page.evaluate(()=>clearInterval(window.liveTest));
  const candidate=page.locator('.spectrum-peak').first(),rPeak=await candidate.boundingBox();const chosen=Number(await candidate.getAttribute('data-frequency'));
  await page.mouse.move(rPeak.x+rPeak.width/2,rPeak.y+rPeak.height-7);await page.mouse.down();await idle();
  assert.equal(await page.evaluate(()=>eqApp.selected),7);assert(await page.locator('#node-tools').isVisible());await page.mouse.move(rPeak.x+rPeak.width/2+36,rPeak.y+rPeak.height-47,{steps:5});await page.mouse.up();await idle();
  assert(await page.evaluate(()=>eqApp.values.band7_gain>1));assert(await page.evaluate(f=>eqApp.values.band7_freq>f,chosen));assert.equal(await page.locator('.spectrum-peak').count(),0);
  await page.evaluate(()=>eqApp.command('undo'));await idle();assert.equal(await page.evaluate(()=>eqApp.values.band7_present),0,'新增加拖动应一次撤销');
  await page.evaluate(()=>{eqApp.frame.spectrum=Array.from({length:500},(_,i)=>-65+30*Math.exp(-Math.pow((i-250)/20,2)));eqApp.graph.motion.values=[];eqApp.graph.draw();});
  const canvas=await page.locator('#response').boundingBox(),hoverX=canvas.x+canvas.width*.58,hoverY=canvas.y+canvas.height*.87;
  await page.mouse.move(hoverX,hoverY);await page.waitForTimeout(1450);assert(await page.evaluate(()=>eqApp.graph.focus.amount>0&&eqApp.graph.focus.amount<1));assert.equal(await page.locator('.spectrum-peak').count(),0);
  await page.mouse.move(hoverX,canvas.y+40);assert.equal(await page.evaluate(()=>eqApp.graph.focus.amount),0,'移出实际谱形应立即恢复');
  await page.mouse.move(hoverX,hoverY);await page.waitForTimeout(2850);assert.equal(await page.locator('.spectrum-peak').count(),1);
  await page.mouse.move(hoverX,canvas.y+40);assert.equal(await page.locator('.spectrum-peak').count(),0);assert.equal(await page.locator('#nodes').evaluate(e=>getComputedStyle(e).opacity),'1');
  // 不动鼠标持续有新帧时，实时选峰必须持续更新，不能再次冻结。
  await page.evaluate(async()=>{const {findSpectrumPeaks}=await import('./spectrum-focus.js');if(findSpectrumPeaks(Array(500).fill(-100)).length||findSpectrumPeaks(Array(500).fill(-30)).length)throw Error('静音或平坦谱不应生成峰');});
  await page.evaluate(()=>eqApp.select(3));await page.locator('#range').click();await page.locator('[data-range="24"]').click();await idle();assert.equal(await page.evaluate(()=>eqApp.graph.range),24);
  await page.locator('#gain-knob input').fill('abc');await page.locator('#gain-knob input').press('Enter');assert(await page.locator('#gain-knob').evaluate(e=>e.classList.contains('invalid')));await page.locator('#gain-knob input').press('Escape');assert(!await page.locator('#gain-knob').evaluate(e=>e.classList.contains('invalid')));
  await page.evaluate(()=>{eqApp.held=[3,4];eqApp.meters();});await page.locator('.eq-meters').click();assert(await page.evaluate(()=>eqApp.held.every(v=>v<0)));
  await show(3);await page.locator('#node-placement-open').click();await page.locator('#band-choice-menu [data-value="4"]').click();await idle();assert.equal(await page.evaluate(()=>eqApp.values.band3_placement),4);
  await page.locator('#settings').click();await page.locator('input[name=theme][value=dark]').check({force:true});await idle();await page.locator('#settings-dialog .close').click();
  for(const theme of ['light','dark'])for(const width of [935,1100,1650]){
    await page.evaluate(theme=>eqApp.updateUi({theme}),theme);await idle();await page.setViewportSize({width,height:Math.round(width*750/1100)});await page.waitForTimeout(120);
    const issues=await page.evaluate(()=>[...document.querySelectorAll('.knob')].filter(e=>e.getClientRects().length).flatMap(e=>{const d=e.querySelector('.dial').getBoundingClientRect(),i=e.querySelector('input').getBoundingClientRect(),r=e.getBoundingClientRect();return Math.abs(d.x+d.width/2-i.x-i.width/2)>1?[e.id+' 数字偏移']:r.bottom>innerHeight+1?[e.id+' 越界']:[];}));assert.deepEqual(issues,[]);await shot(`${theme}-${width}`);
  }
  await page.setViewportSize({width:1100,height:750});await page.evaluate(async()=>{for(let i=7;i<=24;i++)await eqApp.command('addBand',{freq:1000,gain:0});});await idle();assert.equal(await page.locator('.eq-node').count(),24);assert(await page.locator('#add-band').isDisabled());
  const overlaps=await page.locator('.eq-node').evaluateAll(es=>{const r=es.map(e=>e.getBoundingClientRect());return r.some((a,i)=>r.slice(i+1).some(b=>Math.abs(a.x-b.x)<20&&Math.abs(a.y-b.y)<20));});assert(!overlaps,'重叠节点不可分别操作');await shot('24频段');
  await page.evaluate(async()=>{for(const band of eqApp.bands())await eqApp.command('deleteBand',{band});});await idle();assert(await page.locator('#empty-state').isVisible());
  const unchanged=await page.evaluate(()=>{
    eqApp.frame.spectrum=Array(500).fill(-100);eqApp.graph.motion.values=[];eqApp.graph.focus.exit();eqApp.graph.draw();
    const before=document.querySelector('#response').toDataURL();eqApp.frame.inputSpectrum=Array(500).fill(-20);eqApp.frame.spectrumPeak=Array(500).fill(-10);eqApp.graph.draw();
    return before===document.querySelector('#response').toDataURL();
  });assert(unchanged,'已移除的输入对照或保持峰线仍在绘制');
  assert.deepEqual(errors,[]);await browser.close();console.log('第四轮网页验证通过：自动/手动、四档斜率、禁用增益、全局按钮、模式色、数值居中、悬停卡片、监听、实时选峰、新增撤销、双主题三档尺寸、24段与空状态');
})().catch(error=>{console.error(error);process.exit(1);});
