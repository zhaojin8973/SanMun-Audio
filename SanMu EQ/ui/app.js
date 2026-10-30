import {setBandColour,translucentColour} from './shared/band-colour.js';
import {mountDialogLayer,positionDialogLayer} from './shared/dialog-layer.js';
import {callNative,isNative,onFrame} from './bridge.js';
import {applyIcons,icon} from './icons.js';
import {Knob,clamp} from './controls.js';
import {Graph,frequencyColour} from './graph.js';
import {createBandTools,bindAudition} from './band-tools.js';
import {createAnalyserSettings,spectrumDefaults} from './analyser-settings.js';
import {tooltipText} from './tooltips.js';
import {renderManual} from './manual.js';
const $=s=>document.querySelector(s);
const translations={'频率':'Frequency','增益':'Gain','带宽 Q':'Bandwidth Q','输入':'Input','输出 L':'Output L','输出 R':'Output R','频段':'Band','频谱':'Spectrum','设置':'Settings','浅色':'Light','深色':'Dark','外观':'Appearance','界面语言':'Interface language','窗口 · 界面缩放':'Window · Interface scale','悬停提示':'Tooltips','开启':'On','关闭':'Off','清除峰值':'Clear peaks','自动增益':'Auto gain','极性反转':'Invert polarity','联动':'Link','未选择频段':'No band selected','保存预设':'Save preset','保存':'Save','预设':'Presets','频段列表':'Bands','滤波类型':'Filter','声道':'Channel','状态':'Status','斜率':'Slope','已开启':'Enabled','已旁通':'Bypassed','独听中':'Solo','删除频段':'Delete band','频段开关':'Band bypass','频段独听':'Solo band','添加频段':'Add band','撤销':'Undo','重做':'Redo','上一个预设':'Previous preset','下一个预设':'Next preset','整体旁通':'Bypass','复制到另一快照':'Copy to other slot'};
Object.assign(translations,{'向下':'Down','向上':'Up','触发':'Trigger','频谱选峰':'Spectrum peaks','输出联动':'Link outputs','自动增益补偿':'Auto gain compensation'});
Object.assign(translations,{'频谱分析':'Spectrum analyser','悬停选峰':'Peak grab','峰值保持':'Peak hold','显示范围':'Range','分辨率':'Resolution','显示速度':'Speed','显示倾斜':'Tilt','低 · 2048':'Low · 2048','中 · 4096':'Medium · 4096','高 · 8192':'High · 8192','最高 · 16384':'Maximum · 16384','快':'Fast','中':'Medium','慢':'Slow','Pre 为处理前，Post 为处理后；倾斜只改变显示。外部侧链与跨实例冲突检测暂未接入。':'Pre: before EQ. Post: after EQ. Tilt affects display only. External sidechain and cross-instance collision detection are unavailable.'});
Object.assign(translations,{'更多频段操作':'More band actions','旁通此频段':'Bypass band','启用此频段':'Enable band','增益反向':'Invert gain','关闭动态':'Disable dynamics','开启动态':'Enable dynamics','复制频段':'Copy band','粘贴到此频段':'Paste into band','复制为新频段':'Duplicate band','剪切频段':'Cut band'});
Object.assign(translations,{'跟随界面':'Follow interface','帮助手册':'Help manual','关于 SanMu Audio':'About SanMu Audio','返回设置':'Back to settings','最多 24 段的动态均衡器':'Dynamic EQ with up to 24 bands'});
Object.assign(translations,{'线性相位处理':'Linear Phase Processing','启用':'Enable','精度':'Quality','日常':'Daily','高精度':'High precision','极限':'Extreme','当前延迟':'Current latency','动态均衡':'Dynamic EQ','动态开关':'Dynamic EQ','增益归零':'Reset gain','范围':'Range','阈值':'Threshold','自动阈值由系统跟随；切换到 MANUAL 后可调':'Auto threshold follows the signal; switch to MANUAL to edit','线性相位暂停音色处理':'Character paused in Linear Phase','声道位置':'Placement','按住独听，左右拖动扫频':'Hold to audition; drag to sweep','精确均衡，保持信号干净':'Precise EQ; clean signal','非对称软饱和，增加偶次谐波':'Asymmetric soft saturation; even harmonics','对称软饱和，轻微柔化峰值':'Symmetric soft saturation; subtle peak rounding','线性相位均衡，支持动态处理':'Linear phase EQ with dynamic processing','自动起音 / 释放':'Auto attack / release','切除滤波器不支持动态':'Dynamics unavailable for cuts','旧工程动态暂停，点击开关启用':'Legacy dynamics paused; click power to enable','已关闭':'Off','点击电平表清除峰值':'Click meter to clear peaks','斜率适用于高切和低切':'Slope applies to low and high cuts'});
class App {
  constructor(){this.selected=1;this.knobs=[];this.scale=1;this.queue=Promise.resolve();this.pendingEdits=0;this.gesture=false;this.held=[-100,-100];this.auditionBand=-1;this.pendingSetValues=new Map();this.setFlushScheduled=false;this.setFlushInFlight=false;this.setDrainPromise=Promise.resolve();}
  t(s){return this.ui?.language==='en'?(translations[s]||s):s;}
  linearActive(){return !!this.values?.eq_mode;}
  linearLegacy(n){return !!(this.linearActive()&&((this.linearLegacyMask||0)&(1<<(n-1))));}
  bands(){return Array.from({length:24},(_,i)=>i+1).filter(n=>this.values[`band${n}_present`]);}
  beginBand(frequency,settings={}){
    const n=Array.from({length:24},(_,i)=>i+1).find(i=>!this.values[`band${i}_present`]);if(!n)return -1;
    this.cancelAudition?.();this.begin();
    const defaults={enabled:1,solo:0,type:0,slope:0,placement:0,freq:frequency,gain:0,q:1,dynamic:0,dyn_range:6,dyn_threshold:-24,dyn_auto:1,dyn_upward:0,...settings};
    for(const [key,value]of Object.entries(defaults))this.set(`band${n}_${key}`,value);
    this.set(`band${n}_present`,1);this.select(n);return n;
  }
  request(command,data={},merge=true){this.pendingEdits++;this.queue=this.queue.then(()=>callNative(command,data)).then(result=>{if(merge&&result?.values&&!this.gesture&&!this.pendingSetValues.size)this.accept(result);return result;}).catch(e=>{this.notify(e.message);throw e;}).finally(()=>this.pendingEdits--);const result=this.queue;this.queue=this.queue.catch(()=>{});return result;}
  command(command,data={}){this.cancelAudition?.();return this.request(command,data).catch(()=>{});}
  scheduleSetDrain(){if(this.setFlushScheduled||this.setFlushInFlight||!this.pendingSetValues.size)return;this.setFlushScheduled=true;const run=()=>{this.setFlushScheduled=false;this.flushPendingSets();};if(!document.hidden&&typeof requestAnimationFrame==='function')requestAnimationFrame(run);else setTimeout(run,16);}
  queueSet(id,value){this.pendingSetValues.set(id,value);this.scheduleSetDrain();}
  flushPendingSets(){if(this.setFlushInFlight||!this.pendingSetValues.size)return;const entries=[...this.pendingSetValues];this.pendingSetValues.clear();this.setFlushInFlight=true;const work=this.request('setMany',{values:Object.fromEntries(entries)},false).catch(()=>{});this.setDrainPromise=work.finally(()=>{this.setFlushInFlight=false;if(this.pendingSetValues.size)this.scheduleSetDrain();});}
  drainPendingSets(){if(!this.setFlushInFlight&&this.pendingSetValues.size)this.flushPendingSets();return this.setDrainPromise.then(()=>this.pendingSetValues.size?this.drainPendingSets():undefined);}
  begin(){this.gesture=true;this.request('begin',{},false).catch(()=>{});}
  end(){this.gesture=false;return this.drainPendingSets().then(()=>this.request('end')).catch(()=>{});}
  set(id,value){const d=this.schema[id];if(!d)return;value=clamp(value,d.min,d.max);this.values[id]=value;const send=(key,val)=>this.gesture?this.queueSet(key,val):this.request('set',{id:key,value:val},false).catch(()=>{});send(id,value);
    if(id==='input_gain_l'){this.values.input_gain_r=value;send('input_gain_r',value);}
    if(this.ui.outputLink&&(id==='output_gain_l'||id==='output_gain_r')){const other=id==='output_gain_l'?'output_gain_r':'output_gain_l';this.values[other]=value;send(other,value);}
    this.scheduleParameterRefresh(id);
  }
  scheduleParameterRefresh(id){
    this.changedViewIds??=new Set();this.changedViewIds.add(id);this.graph?.draw();
    if(this.viewRequest)return;this.viewRequest=(document.hidden?setTimeout:requestAnimationFrame)(()=>{this.viewRequest=0;const changed=this.changedViewIds;this.changedViewIds=new Set();
      if([...changed].every(key=>/_freq$|_gain$|_q$|_dyn_range$|_dyn_threshold$/.test(key))){this.knobs.filter(k=>changed.has(k.id)).forEach(k=>k.update());this.tools?.refresh();this.dynamicMeter();}
      else this.refresh();
    });
  }
  accept(state){Object.assign(this,{linearLegacyMask:state.linearLegacyMask||0,linearQuality:state.linearQuality??this.linearQuality??0,values:state.values,ui:state.ui,preset:state.preset,ab:state.ab,dirty:state.dirty,canUndo:state.canUndo,canRedo:state.canRedo});if(state.schema)this.schema=state.schema;if(state.presets)this.presets=state.presets;if(state.frame)this.frame=state.frame;if(state.selected!=null)this.selected=state.selected;if(this.selected>0&&!this.values[`band${this.selected}_present`])this.selected=-1;this.refresh();}
  select(n){if(this.selected!==n){this.cancelAudition?.();this.knobs.filter(k=>k.id.startsWith('band')).forEach(k=>k.cancel());this.selected=n;}this.refresh();}
  notify(message){$('#notification').textContent=message;$('#notification').hidden=false;clearTimeout(this.noticeTimer);this.noticeTimer=setTimeout(()=>$('#notification').hidden=true,4000);}
  resize(){this.scale=Math.min(innerWidth/1100,innerHeight/750);$('#app').style.transform=`scale(${this.scale})`;this.knobs.forEach(k=>k.draw());this.graph?.draw();this.positionDialogs();}
  positionDialogs(){positionDialogLayer(this.scale);}
  updateUi(patch){if(['spectrumPre','spectrumPost','spectrumResolution'].some(key=>key in patch)){this.graph.heldSpectrum=null;this.graph.heldPre=null;}this.ui={...this.ui,...patch};$('#tooltip').hidden=true;this.refresh();this.command('ui',patch);}
  refresh(){if(!this.values||!this.graph)return;this.ui={...spectrumDefaults,...this.ui};document.documentElement.dataset.theme=this.ui.theme;document.documentElement.lang=this.ui.language;
    document.querySelectorAll('[data-i18n]').forEach(el=>el.textContent=this.t(el.dataset.i18n));this.analyserSettings?.refresh();
    for(const name of ['theme','language','scale','tips'])document.querySelectorAll(`input[name="${name}"]`).forEach(el=>el.checked=String(this.ui[name])===el.value);
    document.querySelectorAll('input[name="mode"]').forEach(el=>el.checked=Number(el.value)===Number(this.values.character_mode));document.querySelectorAll('input[name="ab"]').forEach(el=>el.checked=el.value===this.ab);
    $('#bypass').setAttribute('aria-pressed',!!this.values.global_bypass);$('#undo').disabled=!this.canUndo;$('#redo').disabled=!this.canRedo;$('#preset-name').textContent=this.preset;$('#preset-dirty').hidden=!this.dirty;
    const mode=Number(this.values.character_mode||0);$('.instrument').dataset.mode=mode;
    const linearEnabled=!!this.values.eq_mode, quality=Number(this.linearQuality??0);
    $('#linear-phase-toggle')?.setAttribute('aria-pressed',linearEnabled);
    if($('#linear-quality')) $('#linear-quality').value=String(quality);
    if($('#linear-latency-note')){const latency=this.frame?.linearLatencies?.[quality]??this.frame?.latency??0;$('#linear-latency-note').textContent=linearEnabled?`${this.t('当前延迟')} ${latency} samples · ${(latency/(this.frame?.sampleRate||48000)*1000).toFixed(1)} ms`:this.t('关闭');}
    for(const [id,value] of [['spectrum',this.ui.spectrum],['auto-gain',this.values.auto_gain],['phase-invert',this.values.phase_invert],['output-link',this.ui.outputLink]])$('#'+id).setAttribute('aria-pressed',!!value);
    $('#range>span').textContent=this.ui.range==='auto'?'Auto':`±${this.ui.range} dB`;
    const bands=this.bands(),selected=bands.includes(this.selected);$('#band-count').textContent=`${bands.length} / 24`;$('#add-band').disabled=bands.length===24;
    $('.dynamic-controls').style.visibility=selected?'visible':'hidden';
    const tabs=$('#band-tabs');for(const el of [...tabs.children])if(!bands.includes(Number(el.dataset.band)))el.remove();
    for(const n of bands){let b=tabs.querySelector(`[data-band="${n}"]`);if(!b){b=document.createElement('button');b.dataset.band=n;b.setAttribute('role','tab');b.textContent=String(n).padStart(2,'0');b.onclick=()=>this.select(n);tabs.append(b);}setBandColour(b,frequencyColour(this.values[`band${n}_freq`],this.ui.theme==='dark'));b.setAttribute('aria-selected',n===this.selected);b.classList.toggle('bypassed',!this.values[`band${n}_enabled`]);}
    for(const child of $('.selected-controls').children)child.hidden=!selected;$('#empty-state').hidden=selected;
    if(selected){const prefix=`band${this.selected}_`,colour=frequencyColour(this.values[`band${this.selected}_freq`],this.ui.theme==='dark');setBandColour($('.selected-controls'),colour);for(const key of ['--knob-accent','--knob-pointer','--knob-tick'])$('.selected-controls').style.setProperty(key,colour);$('.selected-controls').style.setProperty('--knob-track',translucentColour(colour,.21));
      this.knobs[0].rebind(prefix+'freq');this.knobs[1].rebind(prefix+'gain');this.knobs[2].rebind(prefix+'q');const cut=this.values[prefix+'type']>=3;
      this.knobs[1].setDisabled(cut);this.knobs[8].rebind(prefix+'slope');this.knobs[8].setDisabled(!cut);
      for(const key of ['--knob-accent','--knob-pointer','--knob-tick'])$('#slope-knob').style.setProperty(key,colour);$('#slope-knob').style.setProperty('--knob-track',translucentColour(colour,.21));
      const legacy=this.linearLegacy(this.selected),dyn=!!this.values[prefix+'dynamic']&&!legacy,paused=cut;
      const automatic=!!this.values[prefix+'dyn_auto'];$('#band-dynamic').setAttribute('aria-pressed',dyn);$('#band-dynamic').disabled=paused;
      $('#dyn-auto').setAttribute('aria-pressed',automatic);$('#dyn-manual').setAttribute('aria-pressed',!automatic);$('#dyn-auto').disabled=$('#dyn-manual').disabled=paused||!dyn;
      this.knobs[6].rebind(prefix+'dyn_threshold');this.knobs[7].rebind(prefix+'dyn_range');
      this.knobs[6].displayValue=automatic?-24:null;this.knobs[6].setDisabled(paused||!dyn||automatic);this.knobs[7].setDisabled(paused||!dyn);
      for(const [id,value]of [['dyn-down',0],['dyn-up',1]]){$('#'+id).setAttribute('aria-pressed',Number(this.values[prefix+'dyn_upward']||0)===value);$('#'+id).disabled=paused||!dyn;}
      const dynamic=$('.dynamic-controls');dynamic.classList.toggle('inactive',paused||!dyn);setBandColour(dynamic,colour);
      for(const key of ['--knob-accent','--knob-pointer','--knob-tick'])dynamic.style.setProperty(key,colour);dynamic.style.setProperty('--knob-track',translucentColour(colour,.21));
      $('#dynamic-status').textContent=this.t(cut?'切除滤波器不支持动态':legacy&&this.values[prefix+'dynamic']?'旧工程动态暂停，点击开关启用':dyn?'':'已关闭');

    }
    if(!selected)this.knobs[8].setDisabled(true);
    this.knobs.forEach(k=>k.update());this.graph.draw();this.tools?.refresh();this.meters();this.dynamicMeter();if($('#bands-dialog').open&&this.auditionBand<0){const key=JSON.stringify([this.selected,this.ui.language,this.values]);if(key!==this.tableKey){this.tableKey=key;this.renderTable();}}
  }
  meters(){const frame=this.frame||{},now=performance.now(),dt=Math.max(0,(now-(this.meterTime??now))/1000);this.meterTime=now;this.meterValues??=[-100,-100];this.peakTimes??=[0,0];
    for(let i=0;i<2;i++){const raw=frame.meters?.[i]??-100,v=Math.max(raw,this.meterValues[i]-36*dt);this.meterValues[i]=v;
      if(raw>=this.held[i]){this.held[i]=raw;this.peakTimes[i]=now;}else if(now-this.peakTimes[i]>800)this.held[i]=Math.max(raw,this.held[i]-24*dt);
      $('#meter-'+(i?'r':'l')).style.transform=`scaleY(${clamp((v+60)/60)})`;$('#peak-'+(i?'r':'l')).style.top=`${(1-clamp((this.held[i]+60)/60))*100}%`;$('#level-'+(i?'r':'l')).textContent=v<-90?'−∞':v.toFixed(1);
    }
    const track=$('#track-name');track.textContent=frame.trackName||'';track.hidden=!frame.trackName;track.title=frame.trackName||'';
  }

  renderTable(){const body=$('#band-rows');body.replaceChildren();for(const n of this.bands()){const row=document.createElement('tr');row.setAttribute('aria-selected',n===this.selected);const p=`band${n}_`;for(const text of [String(n).padStart(2,'0'),this.schema[p+'type'].choices[this.values[p+'type']],`${Math.round(this.values[p+'freq'])} Hz`,`${this.values[p+'gain'].toFixed(1)} dB`,this.values[p+'q'].toFixed(2),this.schema[p+'placement'].choices[this.values[p+'placement']],this.t(this.values[p+'enabled']?'已开启':'已旁通')]){const cell=document.createElement('td');cell.textContent=text;cell.onclick=()=>this.select(n);row.append(cell);}const cell=document.createElement('td');for(const [glyph,fn]of [['power',()=>this.toggle(p+'enabled')],['headphones',()=>this.toggle(p+'solo')],['trash',()=>this.command('deleteBand',{band:n})]]){const b=document.createElement('button');b.className='icon-button';b.innerHTML=icon(glyph);b.setAttribute('aria-label',this.t(glyph==='trash'?'删除频段':glyph==='power'?'频段开关':'频段独听'));if(glyph==='headphones')bindAudition(b,this,()=>n);else b.onclick=fn;cell.append(b);}row.append(cell);body.append(row);}}
  toggle(id){const match=id.match(/^band(\d+)_dynamic$/),upgrade=match&&this.linearLegacy(Number(match[1]));this.begin();if(upgrade)this.linearLegacyMask&=~(1<<(Number(match[1])-1));this.set(id,upgrade?1:this.values[id]?0:1);this.end();}
  dynamicMeter(){const n=this.selected,p=`band${n}_`,active=n>0&&!this.linearLegacy(n)&&this.values[p+'enabled']&&this.values[p+'dynamic']&&this.values[p+'type']<3&&!this.values.global_bypass&&this.auditionBand<0;
    const reduction=active?Number(this.graph.dynamicDisplay?.[n]??this.frame?.dynamicReduction?.[n]??0):0,range=Number(this.values[p+'dyn_range']||0);
    $('#band-dyn-meter').textContent=`${Math.abs(reduction)<.05?'':reduction<0?'+':'−'}${Math.abs(reduction).toFixed(1)} dB`;$('#dynamic-fill').style.transform=`scaleX(${clamp(Math.abs(reduction)/24)})`;$('#dynamic-limit').style.left=`${clamp(range/24)*100}%`;$('.gr-meter').setAttribute('aria-valuenow',-reduction);$('.gr-meter').setAttribute('aria-valuetext',`${reduction.toFixed(1)} dB / ${range.toFixed(1)} dB`);
    const level=active?Number(this.frame?.dynamicDetector?.[n]??-120):-120,automatic=!!this.values[p+'dyn_auto'],ready=active&&!!this.frame?.dynamicReady?.[n],threshold=automatic?Number(this.frame?.dynamicThreshold?.[n]??-24):this.values[p+'dyn_threshold'];
    $('#detector-fill').style.transform=`scaleX(${clamp((level+90)/96)})`;$('#detector-threshold').style.left=`${clamp((threshold+90)/96)*100}%`;$('#detector-threshold').hidden=automatic;$('#detector-level').textContent=level<=-90?'−∞':level.toFixed(1);$('.detector-meter').setAttribute('aria-valuenow',Math.max(-90,level));
    if(automatic){this.knobs[6].displayValue=-24;this.knobs[6].input.value='AUTO';this.knobs[6].container.querySelector('.knob-readout span').textContent='';this.knobs[6].container.classList.remove('auto-reading');}
    else this.knobs[6].container.classList.remove('auto-reading');
  }
  startAudition(n,cancel){if(this.auditionBand>0)this.cancelAudition?.();
    // 保留旧工程持续独听的可见状态；用户再次试听时解除旧开关，避免松手后仍被历史状态锁住。
    const legacy=this.bands().filter(b=>this.values[`band${b}_solo`]);if(legacy.length){this.begin();for(const b of legacy)this.set(`band${b}_solo`,0);this.end();}
    this.auditionBand=n;this.cancelAudition=cancel;
    const send=()=>callNative('audition',{band:n,active:true}).catch(()=>this.cancelAudition?.());send();this.auditionTimer=setInterval(send,250);this.refresh();
  }
  stopAudition(){clearInterval(this.auditionTimer);this.auditionBand=-1;this.cancelAudition=null;callNative('audition',{active:false}).catch(()=>{});this.refresh();}
  chooseMode(mode){this.cancelAudition?.();this.begin();this.set('character_mode',mode);this.end();}
  chooseLinearQuality(quality){const next=Number(quality);this.linearQualityPending=next;this.linearQuality=next;this.refresh();return this.request('linearQuality',{quality:next}).then(state=>{this.accept(state);this.linearQuality=next;this.linearQualityPending=null;this.refresh();return state;}).catch(()=>{});}
  toggleLinearPhase(){this.begin();this.set('eq_mode',this.values.eq_mode?0:1);this.end();}
  renderPresets(){const search=$('#preset-search').value.toLowerCase();$('#preset-list').replaceChildren();for(const p of this.presets.filter(p=>p.name.toLowerCase().includes(search))){const b=document.createElement('button');b.textContent=p.name;b.onclick=()=>{this.command('loadPreset',{id:p.id});$('#presets-dialog').close();};$('#preset-list').append(b);}}
  async start(){mountDialogLayer();const initial=await callNative('initialize');Object.assign(this,initial);this.ui={...spectrumDefaults,...this.ui};this.graph=new Graph(this);this.tools=createBandTools(this);this.analyserSettings=createAnalyserSettings(this);
    for(const [container,id,label,unit] of [['frequency-knob','band1_freq','频率','Hz'],['gain-knob','band1_gain','增益','dB'],['q-knob','band1_q','带宽 Q',''],['input-knob','input_gain_l','输入','dB'],['output-l-knob','output_gain_l','输出 L','dB'],['output-r-knob','output_gain_r','输出 R','dB'],['threshold-knob','band1_dyn_threshold','阈值','dB'],['range-knob','band1_dyn_range','范围','dB'],['slope-knob','band1_slope','斜率','dB/oct']])this.knobs.push(new Knob($('#'+container),this,id,label,unit));
    applyIcons();
    for(const [button,dialog]of [['settings','settings-dialog'],['preset-open','presets-dialog'],['preset-save','save-dialog'],['band-list','bands-dialog']])$('#'+button).onclick=()=>{if(dialog==='bands-dialog')this.renderTable();if(dialog==='presets-dialog')this.renderPresets();$('#'+dialog).showModal();this.positionDialogs();};
    document.querySelectorAll('dialog .close').forEach(b=>b.onclick=()=>b.closest('dialog').close());document.querySelectorAll('dialog').forEach(d=>d.addEventListener('click',e=>{if(e.target===d){const r=d.getBoundingClientRect();if(e.clientX<r.left||e.clientX>r.right||e.clientY<r.top||e.clientY>r.bottom)d.close();}}));
    for(const name of ['theme','language','scale','tips'])document.querySelectorAll(`input[name="${name}"]`).forEach(el=>el.onchange=()=>this.updateUi({[name]:name==='scale'?Number(el.value):el.value}));
    for(const [id,param] of [['bypass','global_bypass'],['auto-gain','auto_gain'],['phase-invert','phase_invert']])$('#'+id).onclick=()=>this.toggle(param);
    $('#linear-phase-toggle')?.addEventListener('click',()=>this.toggleLinearPhase());
    $('#linear-quality')?.addEventListener('change',e=>this.chooseLinearQuality(e.target.value));
    document.querySelectorAll('input[name="mode"]').forEach(el=>el.onchange=()=>this.chooseMode(Number(el.value)));
    document.querySelectorAll('input[name="ab"]').forEach(el=>el.onchange=()=>this.command('ab',{slot:el.value}));
    for(const [id,command]of [['undo','undo'],['redo','redo'],['copy-ab','copyAB']])$('#'+id).onclick=()=>this.command(command);
    $('#add-band').onclick=()=>this.command('addBand');
    $('#band-dynamic').onclick=()=>this.toggle(`band${this.selected}_dynamic`);
    for(const [id,value]of [['dyn-auto',1],['dyn-manual',0]])$('#'+id).onclick=()=>{this.begin();this.set(`band${this.selected}_dyn_auto`,value);this.end();};
    for(const [id,value]of [['dyn-down',0],['dyn-up',1]])$('#'+id).onclick=()=>{this.begin();this.set(`band${this.selected}_dyn_upward`,value);this.end();};
    $('#spectrum').onclick=()=>{this.graph.focus.exit();this.updateUi({spectrum:!this.ui.spectrum});};$('#output-link').onclick=()=>this.updateUi({outputLink:!this.ui.outputLink});$('.eq-meters').onclick=()=>{this.held=[-100,-100];this.meters();};$('.eq-meters').onkeydown=e=>{if(['Enter',' '].includes(e.key)){e.preventDefault();$('.eq-meters').click();}};
    $('#range').onclick=()=>{const menu=$('#range-menu');menu.hidden=!menu.hidden;menu.querySelectorAll('button').forEach(b=>b.setAttribute('aria-checked',String(this.ui.range)===b.dataset.range));};document.querySelectorAll('[data-range]').forEach(b=>b.onclick=()=>{this.updateUi({range:b.dataset.range==='auto'?'auto':Number(b.dataset.range)});$('#range-menu').hidden=true;});
    document.addEventListener('pointerdown',e=>{if(!e.target.closest('#range,#range-menu'))$('#range-menu').hidden=true;});
    $('#preset-search').oninput=()=>this.renderPresets();for(const [id,step]of [['preset-prev',-1],['preset-next',1]])$('#'+id).onclick=()=>{const index=this.presets.findIndex(p=>p.name===this.preset);this.command('loadPreset',{id:this.presets[(index+step+this.presets.length)%this.presets.length].id});};
    $('#save-form').onsubmit=async e=>{e.preventDefault();try{await this.request('savePreset',{name:$('#save-name').value.trim()});$('#save-dialog').close();}catch(error){$('#save-error').textContent=error.message;}};
    $('#help').onclick=()=>{renderManual($('#manual-content'),this.ui.language);$('#settings-dialog').close();$('#help-dialog').showModal();this.positionDialogs();};
    $('#about').onclick=()=>{$('#settings-dialog').close();$('#about-dialog').showModal();this.positionDialogs();};
    document.querySelectorAll('.back-settings').forEach(button=>button.onclick=()=>{button.closest('dialog').close();$('#settings-dialog').showModal();this.positionDialogs();});
    document.addEventListener('pointerover',e=>{const el=e.target.closest('[data-tip]');if(!el||this.ui.tips==='off'||document.querySelector('dialog[open]'))return;const tip=$('#tooltip'),r=el.getBoundingClientRect();const language=this.ui.tips==='follow'?this.ui.language:this.ui.tips;tip.textContent=tooltipText(el.dataset.tip,language)||(language==='en'?(translations[el.dataset.tip]||el.dataset.tip):el.dataset.tip);tip.hidden=false;tip.style.left=`${clamp((r.left+r.width/2)/this.scale-tip.offsetWidth/2,10,1090-tip.offsetWidth)}px`;tip.style.top=`${Math.min(700,r.bottom/this.scale+8)}px`;});document.addEventListener('pointerout',e=>{if(!e.relatedTarget?.closest?.('[data-tip]'))$('#tooltip').hidden=true;});document.addEventListener('click',()=>$('#tooltip').hidden=true);
    window.addEventListener('resize',()=>this.resize());onFrame(frame=>{this.frame=frame;if(frame.state&&!this.pendingEdits&&!this.gesture&&!this.setFlushInFlight&&!this.pendingSetValues.size){this.accept(frame.state);if(this.linearQualityPending!=null){if(Number(frame.state.linearQuality)===this.linearQualityPending)this.linearQualityPending=null;else{this.linearQuality=this.linearQualityPending;this.refresh();}}}else{this.graph.draw();this.meters();}this.dynamicMeter();});
    document.addEventListener('visibilitychange',()=>{if(document.hidden)this.cancelAudition?.();});window.addEventListener('pagehide',()=>this.cancelAudition?.());
    window.addEventListener('blur',()=>{this.cancelAudition?.();this.knobs.forEach(k=>k.cancel());if(this.graph.drag){this.graph.drag=null;this.end();}});
    this.accept(initial);this.resize();window.eqApp=this;
  }
}
new App().start().catch(error=>{$('#notification').textContent=error.message;$('#notification').hidden=false;});
