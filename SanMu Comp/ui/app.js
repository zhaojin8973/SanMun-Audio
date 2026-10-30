import {mountDialogLayer,positionDialogLayer} from './shared/dialog-layer.js';
import {RangeMenu} from './range-menu.js';
import {renderManual} from './manual.js';
import {tooltip} from './tooltips.js';
import {icon} from './shared/icons.js';
import {callNative,isNative,onNativeFrame} from './bridge.js';
import {Knob,clamp,denormal,normal} from './controls.js';
import {Visualizer} from './visualizer.js';
const $=s=>document.querySelector(s);
const setText=(element,value)=>{if(element.textContent!==value)element.textContent=value;};
const zh={'Previous preset':'上一个预设','Browse presets':'浏览预设','Next preset':'下一个预设','Save preset':'保存预设','Undo':'撤销','Redo':'重做','Copy to other slot':'复制到另一组','Bypass':'旁通','Settings':'设置','Close':'关闭','Bus compressor':'总线压缩器','Fast limiting amplifier':'快速压限放大器','Optical levelling amplifier':'光电平准放大器','Adaptive leveller':'自适应平准器','Freeze history':'暂停历史','Resume history':'恢复历史','Clear peaks':'清除峰值','History hidden · meters remain active':'历史已隐藏 · 电平表继续工作','Appearance':'外观','Light':'浅色','Dark':'深色','History display':'历史曲线','Interface language':'界面语言','Interface scale':'界面缩放','Tooltips':'悬停提示','Follow language':'跟随界面','Off':'关闭','Help manual':'帮助手册','Controls and shortcuts':'操作与快捷键','Presets':'预设','Save':'保存','Target compression':'目标压缩量','0.5–24 dB · measure 5 seconds of the current track':'0.5–24 dB · 采集当前音轨 5 秒','Reuse recording':'复用录音','Analyse now':'开始分析','Start analysis':'开始并等待播放','Waiting for playback…':'等待播放输入…','Cancel':'取消','Apply result':'应用方案','Restore previous settings':'恢复应用前设置','Drag vertically to adjust.':'垂直拖动调节。','Double-click or Command/Ctrl-click to reset. Enter edits a value; Escape cancels.':'双击或按住 Command/Ctrl 点击复位。回车编辑数值，Esc 取消。','FET timing uses 1–7: clockwise is faster. INPUT and OUTPUT retain the attenuation scale.':'FET 时间使用 1–7 刻度，顺时针更快。输入与输出保留衰减刻度。','History shows sample peaks and gain reduction. Freezing the graph does not freeze the meters or sound.':'历史显示采样峰值与增益衰减。暂停历史不影响实时电平和声音。','Enter a value within the parameter range.':'请输入参数范围内的有效数值。','Compression':'压缩量','Gain':'增益','Ratio':'比例','Compress':'压缩','Limit':'限制','Smooth':'平滑','On':'开启','Zero latency':'零延迟','12 ms · smoother gain transitions':'12 毫秒 · 平滑增益变化','Link input / output':'联动输入与输出','Now':'现在','Factory presets':'工厂预设','User presets':'用户预设','Search presets':'搜索预设','No matching presets':'没有匹配的预设','Measuring target compression…':'正在测算目标压缩量…','Cancelling…':'正在取消…','Recording input':'正在采集输入','Current compressor':'当前压缩器','Applied':'已应用','Ready to apply':'待应用','Try another passage':'请换一个片段','Target must be between 0.5 and 24 dB.':'目标必须在 0.5 至 24 dB 之间。','Set a target, click Start, then play audio.':'设定目标后点“开始并等待播放”，再播放音频。','Input detected — collecting 5 seconds…':'已检测到输入，正在采集 5 秒…','Audio captured — calculating…':'采集完成，正在计算…','Analysis is a starting point; refine the result by ear in the mix.':'测算结果仅供参考，请在混音中继续凭听感微调。','Parameters changed during measurement; your settings were preserved.':'测算期间参数发生变化，已保留你的设置。',};
Object.assign(zh,{"ANALYZE": "测算", "Target analysis": "目标测算", "Start analysis": "开始测算", "Last 2 s": "最近 2 秒", "Last 6 s": "最近 6 秒", "Last 10 s": "最近 10 秒", "Visual display": "图像显示", "Turn off the graph while audio and meters keep running.": "关闭曲线，声音处理与电平表继续运行。", "Window · Interface scale": "窗口 · 界面缩放", "Reset size to 100%": "恢复 100% 大小", "Follow interface": "跟随界面", "Chinese": "中文", "English": "English", "About SanMu Audio": "关于 SanMu Audio", "Back to settings": "返回设置", "Four compressors. One workspace.": "四种压缩器，统一的操作界面。", "Knobs and values": "旋钮与数值", "Drag vertically or scroll to adjust. Double-click or Command/Ctrl-click to reset.": "上下拖动或滚轮调节。双击或按住 Command/Ctrl 点击旋钮复位。", "Keyboard": "键盘操作", "Arrow keys adjust a focused knob. Home / End reach its limits. Enter edits or confirms a value; Escape cancels.": "旋钮聚焦后用方向键调节，Home / End 到达边界。回车编辑或确认数值，Esc 取消。", "FET scales": "FET 刻度", "A/B and bypass": "A/B 与旁通", "A/B keeps two sound settings. Copy transfers the current set to the other slot. BYP bypasses compression.": "A/B 保留两组声音设置；复制按钮将当前组复制到另一组。BYP 旁通压缩处理。", "Reading the graph": "曲线读法", "IN is input, OUT is output, and GR is gain reduction. The graph shows the last 2 seconds. GR 6/12/24 dB changes only its vertical scale.": "IN 为输入，OUT 为输出，GR 为压低的分贝数。曲线默认显示最近 6 秒（点击时间标签可切换 6 秒 / 10 秒 / 2 秒）；GR 6/12/24 dB 调整压缩量刻度；IN/OUT ×2 同步放大输入输出的电平差，显示 0 至 −30 dBFS。", "Display and peaks": "显示与峰值", "Curves use light visual smoothing; meters and hover values show raw readings. Freezing the graph leaves audio and meters running. Clear peaks resets held peaks and clip indicators.": "曲线经过轻度显示平滑；电平表与悬停数值保留原始读数。暂停曲线不影响声音和实时电平。清除峰值会重置保持值与过载标记。", "Choose a target and play 5 seconds. Analysis uses the current compressor; review the result before applying it. Reuse recording recalculates the same passage.": "设定目标后播放 5 秒素材。使用当前压缩器测算，查看结果后再应用；复用录音可用同一片段重算。"});
Object.assign(zh,{
 'Start analysis':'开始测算',
 'Starting…':'正在启动…', 'Waiting for input':'等待输入',
 'Play audio in your DAW. Capture begins when input arrives.':'请在宿主播放音频，收到输入后开始采集。',
 'Set a target and click Start analysis. Then play audio in your DAW; if already playing, capture starts immediately.':'设定目标并点击“开始测算”，再在宿主播放音频；如果已在播放，会直接开始采集。',
 'Capturing':'正在采集',
 'This preview cannot receive DAW audio. Use analysis in the installed plugin.':'此预览无法接收宿主音频，请在已安装的插件中使用测算。',
 'Ready to start':'准备测算',
 'When ready, settings are applied automatically if you have not adjusted them during analysis.':'测算完成后自动应用；若期间手动调过参数，则保留你的设置，等待手动应用。',
 'Waiting for curve data':'等待曲线数据',
 'Enable the curve demo above to preview GR zoom.':'开启上方“显示曲线示意”可查看 GR 缩放。',
 'Frozen':'已暂停',
 'IN/OUT ×2 doubles the visual level difference, using a shared 0 to −30 dBFS range.':'IN/OUT ×2 让电平差显示为两倍高度，两条线共用 0 至 −30 dBFS 刻度。',
 'Choose a louder, representative passage for more useful results.':'建议播放电平较高、具有代表性的片段，测算结果更有参考价值。',
});
const modeNames=['LEV','VCA','FET','Opto'];
export class App{
 constructor(){this.values={};this.schema={};this.ui={language:'en',theme:'light',history:true,historySpan:6000,levelRange:60,grRange:12,layers:{input:true,output:true,gr:true},tips:'follow'};this.knobs=[];this.pending=0;this.queue=Promise.resolve();this.mode=-1;this.scale=1;this.lastManualRelease=150;this.visual=new Visualizer(this);this.rangeMenu=new RangeMenu(this);this.levelMenu=new RangeMenu(this,'levelRange','level-range',value=>'IN/OUT ×'+(60/value));this.gestureActive=false;this.pendingSetValues=new Map();this.setFlushScheduled=false;this.setFlushInFlight=false;this.setDrainPromise=Promise.resolve();}
 t(s,lang=this.ui.language){return tooltip(s,lang)||(lang==='zh'?(zh[s]||s):s);}
 notify(s){$('#notification').textContent=s;$('#notification').hidden=false;clearTimeout(this.notificationTimer);this.notificationTimer=setTimeout(()=>$('#notification').hidden=true,4500);}
 send(cmd,data={}){this.pending++;const work=this.queue.then(()=>callNative(cmd,data));this.queue=work.catch(()=>{});return work.then(r=>{this.pending--;if(r?.schema)this.schema=r.schema;if(r?.presets)this.presets=r.presets;if(this.pending===0&&r?.values)this.sync(r);return r;},e=>{this.pending--;this.notify(e.message);throw e;});}
 scheduleSetDrain(){if(this.setFlushScheduled||this.setFlushInFlight||!this.pendingSetValues.size)return;this.setFlushScheduled=true;const run=()=>{this.setFlushScheduled=false;this.flushPendingSets();};if(typeof requestAnimationFrame==='function')requestAnimationFrame(run);else setTimeout(run,0);}
 queueSet(id,value){this.pendingSetValues.set(id,value);this.scheduleSetDrain();}
 flushPendingSets(){if(this.setFlushInFlight||!this.pendingSetValues.size)return;const entries=[...this.pendingSetValues];this.pendingSetValues.clear();this.setFlushInFlight=true;let work=Promise.resolve();for(const [id,value] of entries)work=work.then(()=>this.send('set',{id,value}).catch(()=>{}));this.setDrainPromise=work.finally(()=>{this.setFlushInFlight=false;if(this.pendingSetValues.size)this.scheduleSetDrain();});}
 drainPendingSets(){if(!this.setFlushInFlight&&this.pendingSetValues.size)this.flushPendingSets();return this.setDrainPromise.then(()=>this.pendingSetValues.size?this.drainPendingSets():undefined);}
 begin(){this.gestureActive=true;this.send('begin').catch(()=>{});}
 end(){this.gestureActive=false;this.drainPendingSets().then(()=>this.send('end')).catch(()=>{});}
 set(id,value){const previous=this.values[id];this.values[id]=value;if(['fet_input','fet_output'].includes(id)&&this.values.in_out_link>.5){const other=id==='fet_input'?'fet_output':'fet_input';this.values[other]=clamp(this.values[other]-(value-previous),0,48);}if(this.gestureActive)this.queueSet(id,value);else this.send('set',{id,value}).catch(()=>{});this.refresh();}
 change(id,value){this.begin();this.set(id,value);this.end();}
 preference(key,value){this.ui[key]=value;this.refresh();this.positionDialogs();this.send('ui',{[key]:value}).catch(()=>{});}
 sync(data){
  const key=JSON.stringify([data.values,data.ui,data.preset,data.dirty,data.canUndo,data.canRedo,data.ab]);
  this.values={...data.values};this.ui={...this.ui,...data.ui};this.listen=data.listen;this.state=data;
  if(key!==this.controlState){this.controlState=key;this.refresh();}else this.updateListen();
 }
 refresh(){
  const mode=Math.round(this.values.mode??1);if(mode!==this.mode){this.mode=mode;this.layout();this.visual.frames=[];}
  document.documentElement.dataset.theme=this.ui.theme;document.documentElement.lang=this.ui.language;$('.instrument').dataset.mode=modeNames[mode].toLowerCase();
  document.querySelectorAll('[data-i18n]').forEach(e=>setText(e,this.t(e.dataset.i18n)));document.querySelectorAll('[data-tip]').forEach(e=>{const lang=this.ui.tips==='zh'?'zh':this.ui.tips==='en'?'en':this.ui.language;e.setAttribute(e.dataset.tip.startsWith('tip:')?'aria-description':'aria-label',this.t(e.dataset.tip,lang));});
  document.querySelectorAll('[name=mode]').forEach(e=>e.checked=Number(e.value)===mode);document.querySelectorAll('[name=ab]').forEach(e=>e.checked=e.value===(this.state?.ab||'A'));
  $('#bypass').setAttribute('aria-pressed',this.values.global_bypass>.5);$('#undo').disabled=!this.state?.canUndo;$('#redo').disabled=!this.state?.canRedo;$('#preset-name').textContent=this.state?.preset||'Default Settings';$('#preset-dirty').hidden=!this.state?.dirty;
  for(const k of this.knobs)k.update();document.querySelectorAll('[data-set-id]').forEach(e=>e.setAttribute('aria-pressed',Math.abs(this.values[e.dataset.setId]-Number(e.dataset.setValue))<.005));
  if(this.mode===2){const all=this.values.fet_ratio===4;document.querySelectorAll('.fet-ratios button').forEach(e=>e.setAttribute('aria-pressed',all||Number(e.dataset.setValue)===this.values.fet_ratio));$('#fet-link').setAttribute('aria-pressed',this.values.in_out_link>.5);}
  if(this.mode===0)$('#smooth-description').textContent=this.t(this.values.vox_smooth>.5?'12 ms · smoother gain transitions':'Zero latency');
  if(this.mode===1){$('#vca-auto').setAttribute('aria-pressed',this.values.vca_release>=1200);if(this.values.vca_release<1200)this.lastManualRelease=this.values.vca_release;}
  for(const key of ['theme','language','tips','scale'])document.querySelectorAll(`[name=${key}]`).forEach(e=>e.checked=key==='scale'?Math.abs(Number(e.value)-(this.ui.scale||1))<.001:e.value===this.ui[key]);
  $('#history-enabled').checked=this.ui.history;this.rangeMenu.update(Number(this.ui.grRange)||12);this.levelMenu.update(Number(this.ui.levelRange)||60);document.querySelectorAll('.layers input').forEach(e=>e.checked=this.ui.layers?.[e.value]!==false);
  const spanSec=(Number(this.ui.historySpan)||6000)/1000,durEl=$('.history-duration');if(durEl){durEl.dataset.i18n=`Last ${spanSec} s`;setText(durEl,this.t(`Last ${spanSec} s`));}
  this.visual.history(this.ui.history);const pause=$('#pause');pause.setAttribute('aria-pressed',this.visual.paused);const pauseName=this.visual.paused?'play':'pause';if(pause.dataset.icon!==pauseName){pause.innerHTML=icon(pauseName);pause.dataset.icon=pauseName;}pause.dataset.tip=this.visual.paused?'Resume history':'Freeze history';this.visual.draw();this.updateListen();
 }
 knob(parent,id,size='',ends=null){const element=document.createElement('div');parent.append(element);this.knobs.push(new Knob(element,id,this,{size,ends}));return element;}
 button(parent,id,value,text){const b=document.createElement('button');b.dataset.tip='tip:'+id;b.dataset.setId=id;b.dataset.setValue=value;b.textContent=text;b.onclick=()=>this.change(id,value);parent.append(b);return b;}
 layout(){
  this.knobs.forEach(k=>k.dispose());this.knobs=[];const main=$('#controls'),details=$('#details');main.replaceChildren();details.replaceChildren();const row=document.createElement('div');main.append(row);
  if(this.mode===1){row.className='vca-layout';for(const [id,size,values]of [['vca_threshold','large'],['vca_ratio','',[[2,'2'],[4,'4'],[10,'10']]],['vca_attack','',[[.1,'.1'],[1,'1'],[10,'10'],[30,'30']]],['vca_release',''],['vca_makeup','large']]){const cell=document.createElement('div');row.append(cell);this.knob(cell,id,size);if(values){const q=document.createElement('div');q.className='quick-values';cell.append(q);values.forEach(([v,s])=>this.button(q,id,v,s));}if(id==='vca_release'){const q=document.createElement('div');q.className='quick-values';cell.append(q);const b=document.createElement('button');b.textContent='AUTO';b.id='vca-auto';b.dataset.tip='tip:vca_release';b.onclick=()=>this.change(id,this.values[id]>=1200?this.lastManualRelease:2000);q.append(b);}}}
  if(this.mode===2){row.className='fet-layout';const pair=document.createElement('div');pair.className='fet-pair';row.append(pair);this.knob(pair,'fet_input','large');this.knob(pair,'fet_output','large');const link=document.createElement('button');link.id='fet-link';link.className='link';link.innerHTML=icon('link')+'<span>LINK</span>';link.dataset.tip='tip:in_out_link';link.onclick=()=>this.change('in_out_link',this.values.in_out_link>.5?0:1);pair.append(link);const timing=document.createElement('div');timing.className='fet-times';row.append(timing);this.knob(timing,'fet_attack','compact');this.knob(timing,'fet_release','compact');const ratio=document.createElement('div');ratio.className='fet-ratios';ratio.innerHTML='<span class="control-label" data-i18n="Ratio">Ratio</span>';row.append(ratio);[[3,'20'],[2,'12'],[1,'8'],[0,'4'],[4,'ALL']].forEach(([v,s])=>this.button(ratio,'fet_ratio',v,s));}
  if(this.mode===3){row.className='opto-layout';this.knob(row,'opto_gain','huge',['0','100']);const middle=document.createElement('div');middle.className='opto-mode';middle.innerHTML='<span class="control-label">MODE</span><div class="mode-switch"></div>';row.append(middle);this.button(middle.lastElementChild,'opto_is_limit',0,this.t('Compress'));this.button(middle.lastElementChild,'opto_is_limit',1,this.t('Limit'));this.knob(row,'opto_peak_reduction','huge',['0','100']);}
  if(this.mode===0){row.className='lev-layout';this.knob(row,'vox_comp','huge');const middle=document.createElement('div');middle.className='smooth-control';middle.innerHTML='<span class="control-label" data-i18n="Smooth">Smooth</span><div class="mode-switch"></div><small id="smooth-description"></small>';row.append(middle);this.button(middle.querySelector('.mode-switch'),'vox_smooth',0,this.t('Off'));this.button(middle.querySelector('.mode-switch'),'vox_smooth',1,this.t('On'));this.knob(row,'vox_gain','large',['−36','0']);}
  const foot=[['vox_gate','vox_mix'],['vca_sidechain_hp','vca_stereo_link','vca_mix','vca_output_trim'],['headroom','fet_sidechain_hp','fet_stereo_link','fet_mix','fet_output_trim'],['opto_sidechain_hp','opto_stereo_link','opto_mix','opto_output_trim']][this.mode];foot.forEach(id=>this.knob(details,id,'small'));
 }
 open(id){this.rangeMenu.close();this.levelMenu.close();if(id==='help-dialog')renderManual($('#manual-content'),this.ui.language,this.mode);this.hideTip?.();for(const d of document.querySelectorAll('dialog[open]'))d.close();this.end();$('#'+id).showModal();this.positionDialogs();}
 positionDialogs(){positionDialogLayer(this.scale);}

 showPresets(){const filter=$('#preset-search').value.toLowerCase();const list=$('#preset-list');list.replaceChildren();for(const user of [false,true]){const items=(this.presets||[]).filter(p=>p.user===user&&(p.mode===this.mode||p.mode<0)&&p.name.toLowerCase().includes(filter));if(!items.length)continue;const h=document.createElement('h3');h.textContent=this.t(user?'User presets':'Factory presets');list.append(h);for(const p of items){const b=document.createElement('button');b.textContent=p.name;b.onclick=()=>this.send('loadPreset',{id:p.id}).then(()=>$('#presets-dialog').close()).catch(()=>{});list.append(b);}}if(!list.children.length)list.textContent=this.t('No matching presets');}
 updateListen(){
  if(!this.listen)return;
  const s=this.listen,status=this.listenRequest||s.status||'idle';
  const busy=['starting','waiting','capturing','analysing','cancelling','applying','restoring'].includes(status);
  const button=$('#listen');button.classList.toggle('active',busy||s.applied);
  const title=status==='starting'?this.t('Starting…'):status==='waiting'?this.t('Waiting for input'):status==='capturing'?Math.max(0,5-s.seconds).toFixed(1)+' s':status==='analysing'?this.t('ANALYZE')+'…':s.applied?this.t('ANALYZE')+' ✓':this.t('ANALYZE');
  setText(button.querySelector('span'),title);
  setText($('#listen-intro'),this.t('Current compressor')+' · '+modeNames[this.mode]);
  $('#listen-preview').hidden=isNative;
  const progress=$('#listen-progress');progress.hidden=!busy;
  const bar=progress.querySelector('progress');bar.value=status==='capturing'?s.progress:0;
  if(['analysing','starting','cancelling','applying','restoring'].includes(status))bar.removeAttribute('value');
  setText(progress.querySelector('p'),status==='starting'?this.t('Starting…'):status==='waiting'?this.t('Play audio in your DAW. Capture begins when input arrives.'):status==='capturing'?this.t('Capturing')+` · ${Number(s.seconds||0).toFixed(1)} / 5.0 s`:status==='cancelling'?this.t('Cancelling…'):this.t('Measuring target compression…'));
  $('#listen-start').hidden=busy;$('#listen-start').disabled=!isNative;
  $('#listen-reuse').hidden=busy;$('#listen-reuse').disabled=!isNative||!s.hasCapture;
  $('#listen-stop').hidden=status!=='capturing';$('#listen-stop').disabled=Number(s.seconds)<1;
  $('#listen-cancel').hidden=!busy;$('#listen-cancel').disabled=!!this.listenRequest||status==='cancelling';
  $('#listen-apply').hidden=busy||!s.ready||s.applied;
  $('#listen-restore').hidden=busy||!s.canRestore;$('#listen-target').disabled=busy;
  const result=$('#listen-result');result.hidden=!['ready','applied','review','error'].includes(status);
  if(s.ready)setText(result,this.t(s.applied?'Applied':'Ready to apply')+` · ${modeNames[s.mode]} · GR ${s.gr?.toFixed(1)} dB · Δ ${s.level?.toFixed(1)} dB`+(s.applied?'':'\n'+this.t('Parameters changed during measurement; your settings were preserved.')));
  else setText(result,this.ui.language==='zh'?s.message||'':this.t('Try another passage'));
  const layoutKey=[status,this.ui.language,s.ready,s.canRestore,isNative].join(':');
  if(this.listenLayout!==layoutKey){this.listenLayout=layoutKey;this.positionDialogs();}
 }
 async listenCommand(command,data={},status='starting'){
  if(this.listenRequest)return;
  this.listenRequest=status;setText($('#listen-error'),'');this.updateListen();
  try{await this.send(command,data);}catch(e){setText($('#listen-error'),e.message);}
  finally{this.listenRequest=null;this.updateListen();this.positionDialogs();}
 }
 async init(){
  mountDialogLayer();
  document.querySelectorAll('[data-icon]').forEach(e=>e.innerHTML=icon(e.dataset.icon));if(!isNative){const {installPreview}=await import('./preview.js');await installPreview();}
  const state=await this.send('initialize');this.schema=state.schema;this.presets=state.presets;this.sync(state);this.ready=true;this.nativeReady=isNative;
  const resize=()=>{this.scale=Math.min(innerWidth/1100,innerHeight/750);$('#app').style.transform=`scale(${this.scale})`;$('#app').style.left=(innerWidth-1100*this.scale)/2+'px';$('#app').style.top=(innerHeight-750*this.scale)/2+'px';this.positionDialogs();this.knobs.forEach(k=>k.draw());this.visual.draw();};window.addEventListener('resize',resize);resize();
  document.querySelectorAll('[name=mode]').forEach(e=>e.onchange=()=>this.change('mode',Number(e.value)));document.querySelectorAll('[name=ab]').forEach(e=>e.onchange=()=>this.send('ab',{slot:e.value}).catch(()=>{}));
  $('#copy-ab').onclick=()=>this.send('copyAB').catch(()=>{});$('#undo').onclick=()=>this.send('undo').catch(()=>{});$('#redo').onclick=()=>this.send('redo').catch(()=>{});$('#bypass').onclick=()=>this.change('global_bypass',this.values.global_bypass>.5?0:1);
  $('#settings').onclick=()=>this.open('settings-dialog');for(const key of ['theme','language','tips','scale'])document.querySelectorAll(`[name=${key}]`).forEach(e=>e.onchange=()=>{this.hideTip?.();this.preference(key,key==='scale'?Number(e.value):e.value);if(key==='language'){this.layout();this.refresh();}});$('#reset-window').onclick=()=>this.preference('scale',1);$('#history-enabled').onchange=e=>this.preference('history',e.target.checked);$('#help').onclick=()=>this.open('help-dialog');$('#about').onclick=()=>this.open('about-dialog');document.querySelectorAll('.back-settings').forEach(e=>e.onclick=()=>this.open('settings-dialog'));
  document.querySelectorAll('dialog').forEach(d=>{d.querySelector('.close').onclick=()=>d.close();d.addEventListener('click',e=>{if(e.target===d){const b=d.getBoundingClientRect();if(e.clientX<b.left||e.clientX>b.right||e.clientY<b.top||e.clientY>b.bottom)d.close();}});});
  $('#preset-open').onclick=()=>{this.showPresets();$('#preset-search').placeholder=this.t('Search presets');this.open('presets-dialog');};$('#preset-search').oninput=()=>this.showPresets();
  const stepPreset=delta=>{const items=this.presets.filter(p=>!p.user&&p.mode===this.mode);if(!items.length)return;let index=items.findIndex(p=>p.name===this.state.preset);index=(index+delta+items.length)%items.length;this.send('loadPreset',{id:items[index].id}).catch(()=>{});};$('#preset-prev').onclick=()=>stepPreset(-1);$('#preset-next').onclick=()=>stepPreset(1);
  $('#preset-save').onclick=()=>{$('#save-name').value=this.state.preset||'';$('#save-error').textContent='';this.open('save-dialog');$('#save-name').select();};$('#save-form').onsubmit=e=>{e.preventDefault();this.send('savePreset',{name:$('#save-name').value}).then(()=>$('#save-dialog').close()).catch(e=>$('#save-error').textContent=e.message);};
  $('#clear-peaks').onclick=()=>this.visual.clear();$('#pause').onclick=()=>{this.visual.pause();this.refresh();};document.querySelectorAll('.layers input').forEach(e=>e.onchange=()=>this.preference('layers',{...this.ui.layers,[e.value]:e.checked}));
  const durBtn=$('.history-duration');if(durBtn){durBtn.style.cursor='pointer';durBtn.onclick=()=>{const spans=[6000,10000,2000],cur=Number(this.ui.historySpan)||6000,next=spans[(spans.indexOf(cur)+1)%spans.length];this.preference('historySpan',next);};}
  $('#listen').onclick=()=>{$('#listen-target').value=Number(this.listen?.target||4).toFixed(1);$('#listen-error').textContent='';this.open('listen-dialog');this.updateListen();};
  const start=reuse=>{
   if(!isNative||this.listenRequest||['waiting','capturing','analysing','cancelling'].includes(this.listen?.status))return;
   const target=Number($('#listen-target').value.trim());
   if(!Number.isFinite(target)||target<.5||target>24){setText($('#listen-error'),this.t('Target must be between 0.5 and 24 dB.'));this.positionDialogs();return;}
   this.listenCommand('listenStart',{target,reuse},'starting');
  };
  $('#listen-start').onclick=()=>start(false);$('#listen-reuse').onclick=()=>start(true);
  $('#listen-target').onkeydown=e=>{if(e.key==='Enter'){e.preventDefault();e.stopPropagation();start(false);}};
  for(const [id,cmd,status]of [['stop','listenStop','analysing'],['cancel','listenCancel','cancelling'],['apply','listenApply','applying'],['restore','listenRestore','restoring']])$('#listen-'+id).onclick=()=>this.listenCommand(cmd,{},status);
  let tooltipTimer, tipTarget;
  this.hideTip=()=>{clearTimeout(tooltipTimer);$('#tooltip').hidden=true;tipTarget=null;};
  document.addEventListener('pointerover',e=>{
   const el=e.target.closest('[data-tip]');if(!el||this.ui.tips==='off'||el===tipTarget)return;
   this.hideTip();tipTarget=el;
   tooltipTimer=setTimeout(()=>{
    if(!el.isConnected||this.ui.tips==='off')return;
    const tip=$('#tooltip'), host=el.closest('dialog')||$('#app');host.append(tip);
    tip.textContent=this.t(el.dataset.tip,this.ui.tips==='follow'?this.ui.language:this.ui.tips);tip.hidden=false;
    const b=el.getBoundingClientRect(),a=host.getBoundingClientRect();
    const scale=this.scale;
    const width=tip.getBoundingClientRect().width;
    const left=Math.max(a.left+8,Math.min(a.right-width-8,b.left));
    const height=tip.getBoundingClientRect().height;
    const top=b.bottom+8+height<Math.min(a.bottom,innerHeight)?b.bottom+8:b.top-height-8;
    tip.style.left=(left-a.left)/scale+'px';tip.style.top=Math.max(4,(top-a.top)/scale)+'px';
   },550);
  });
  document.addEventListener('pointerout',e=>{if(tipTarget&&tipTarget.contains(e.target)&&!tipTarget.contains(e.relatedTarget))this.hideTip();});
  document.addEventListener('pointerdown',()=>this.hideTip());
  document.querySelectorAll('dialog').forEach(d=>d.addEventListener('close',()=>this.hideTip()));
  window.addEventListener('blur',()=>{this.hideTip();this.knobs.forEach(k=>k.cancelGesture());this.end();});window.addEventListener('pagehide',()=>this.end());onNativeFrame(packet=>{if(packet.meter)this.visual.feed(packet.meter);if(this.pending===0)this.sync(packet);});
 }
}
const app=new App();window.__sanmuComp=app;app.init().catch(e=>app.notify(e.message));
