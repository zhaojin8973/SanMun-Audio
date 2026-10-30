// 第三轮：真实鼠标操作、频谱选峰、自动阈值控件和所有尺寸的参数对齐。
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
  const shot=name=>page.screenshot({path:`docs/界面重设计/第三轮-${name}.png`});
  assert.equal(await page.locator('input[type=checkbox]').count(),0);
  assert.equal(await page.locator('#band-solo,#band-enable,#band-delete,#latency,#sample-rate').count(),0);
  assert.equal(await page.locator('#response').evaluate(e=>e.clientHeight),380);
  assert(await page.locator('#node-tools').isHidden());
  await show(3);assert.equal(await page.evaluate(()=>eqApp.selected),3);
  const card=await page.locator('#node-tools').boundingBox();assert(card.width<=235&&card.height<80);
  await page.locator('#node-tools').hover();await page.waitForTimeout(230);assert(await page.locator('#node-tools').isVisible());await shot('小巧信息卡');
  await page.mouse.move(1000,65);await page.waitForTimeout(250);assert(await page.locator('#node-tools').isHidden());
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
  await page.evaluate(()=>{eqApp.frame.spectrum=Array.from({length:500},(_,i)=>{const x=i/499;return -72+35*Math.exp(-Math.pow((x-.18)/.025,2))+29*Math.exp(-Math.pow((x-.52)/.035,2))+40*Math.exp(-Math.pow((x-.78)/.025,2));});eqApp.graph.draw();});
  await page.locator('#spectrum-grab').click();await page.waitForTimeout(700);
  assert.equal(await page.locator('.spectrum-peak').count(),3);assert.equal(await page.locator('#nodes').evaluate(e=>getComputedStyle(e).opacity),'0');assert(await page.locator('#node-tools').isHidden());await shot('频谱选峰');
  const frozen=await page.locator('.spectrum-peak').first().textContent();await page.evaluate(()=>{eqApp.frame.spectrum.fill(-100);eqApp.graph.draw();});assert.equal(await page.locator('.spectrum-peak').first().textContent(),frozen);
  const chosen=Number(await page.locator('.spectrum-peak').first().getAttribute('data-frequency'));await page.locator('.spectrum-peak').first().click();await idle();await page.waitForTimeout(250);assert.equal(await page.evaluate(()=>eqApp.selected),7);assert(Math.abs(await page.evaluate(()=>eqApp.values.band7_freq)-chosen)<1.1);assert.equal(await page.evaluate(()=>eqApp.values.band7_gain),0);
  await page.evaluate(()=>eqApp.command('undo'));await idle();assert.equal(await page.evaluate(()=>eqApp.values.band7_present),0);
  await page.evaluate(()=>{eqApp.frame.spectrum=Array.from({length:500},(_,i)=>-65+30*Math.exp(-Math.pow((i-250)/20,2)));eqApp.graph.draw();});
  const canvas=await page.locator('#response').boundingBox();await page.mouse.move(canvas.x+canvas.width*.58,canvas.y+canvas.height*.87);await page.waitForTimeout(1900);assert.equal(await page.locator('.spectrum-peak').count(),1);
  await page.mouse.move(1000,65);await page.waitForTimeout(250);assert.equal(await page.locator('.spectrum-peak').count(),0);assert.equal(await page.locator('#nodes').evaluate(e=>getComputedStyle(e).opacity),'1');
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
  assert.deepEqual(errors,[]);await browser.close();console.log('第三轮网页验证通过：自动/手动、四档斜率、禁用增益、全局按钮、模式色、数值居中、悬停卡片、监听、冻结选峰、新增撤销、双主题三档尺寸、24段与空状态');
})().catch(error=>{console.error(error);process.exit(1);});
