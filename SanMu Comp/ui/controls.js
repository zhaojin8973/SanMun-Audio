import {drawKnob} from './shared/draw-knob.js';
export const clamp=(x,min=0,max=1)=>Math.max(min,Math.min(max,x));
export const normal=(d,value)=>Math.pow(clamp((value-d.min)/(d.max-d.min)),d.skew||1);
export function denormal(d,n){const v=d.min+(d.max-d.min)*Math.pow(clamp(n),1/(d.skew||1));return clamp(d.step?d.min+Math.round((v-d.min)/d.step)*d.step:v,d.min,d.max);}
const labels={vox_comp:['Compression','压缩量','dB'],vox_gain:['Gain','输出增益','dB'],vox_gate:['Gate','门限','dB'],vox_mix:['Mix','混合','%'],output_trim:['Output trim','输出微调','dB'],stereo_link:['Stereo link','立体声联动','%'],vca_threshold:['Threshold','阈值','dB'],vca_ratio:['Ratio','比例',':1'],vca_attack:['Attack','启动','ms'],vca_release:['Release','释放','ms'],vca_makeup:['Makeup','补偿增益','dB'],headroom:['Headroom','余量','dB'],fet_input:['Input','输入',''],fet_output:['Output','输出',''],fet_attack:['Attack','启动',''],fet_release:['Release','释放',''],opto_gain:['Gain','增益',''],opto_peak_reduction:['Peak reduction','压缩量','']};
export function info(id){if(labels[id])return labels[id];if(id.endsWith('sidechain_hp'))return ['Sidechain HP','侧链高通','Hz'];if(id.endsWith('_mix'))return ['Mix','混合','%'];if(id.endsWith('_stereo_link'))return labels.stereo_link;if(id.endsWith('_output_trim'))return labels.output_trim;return [id,id,''];}
export class Knob{
 constructor(container,id,app,{size='',ends=null}={}){
  this.id=id;this.app=app;this.container=container;this.d=app.schema[id];this.reverse=id==='vox_comp';this.draw=drawKnob;
  container.className='knob '+size;container.dataset.parameter=id;container.dataset.tip='tip:'+id;const [name,,unit]=info(id);
  container.innerHTML=`<label class="knob-label" for="edit-${id}">${name}</label><div class="dial" id="knob-${id}" role="slider" tabindex="0"><canvas aria-hidden="true"></canvas></div><div class="scale-ends"><span></span><span></span></div><div class="knob-readout"><input id="edit-${id}" type="text" inputmode="decimal" autocomplete="off" spellcheck="false"><span>${unit}</span></div>`;
  this.dial=container.querySelector('.dial');this.canvas=container.querySelector('canvas');this.input=container.querySelector('input');this.getValue=()=>app.values[id];this.normalized=()=>this.reverse?1-normal(this.d,this.getValue()):normal(this.d,this.getValue());
  this.dial.setAttribute('aria-valuemin',this.reverse?-this.d.max:this.d.min);this.dial.setAttribute('aria-valuemax',this.reverse?-this.d.min:this.d.max);
  const range=ends||(id.startsWith('fet_')&&['fet_input','fet_output'].includes(id)?['48','0']:id==='vox_comp'?['0','36']:id.startsWith('fet_')?['1','7']:['','']);
  container.querySelectorAll('.scale-ends span').forEach((s,i)=>s.textContent=range[i]);
  const reset=()=>{app.begin();this.set(this.d.initial);app.end();};
  this.dial.addEventListener('dblclick',reset);
  this.dial.addEventListener('pointerdown',e=>{if(e.button!==0)return;e.preventDefault();this.dial.focus();if(e.metaKey||e.ctrlKey){reset();return;}app.begin();this.pointer=e.pointerId;this.y=e.clientY;this.drag=this.normalized();this.dial.setPointerCapture(e.pointerId);container.classList.add('dragging');});
  this.dial.addEventListener('pointermove',e=>{if(this.pointer!==e.pointerId)return;const delta=(this.y-e.clientY)/(240*app.scale);this.y=e.clientY;this.drag=clamp(this.drag+delta);this.set(denormal(this.d,this.reverse?1-this.drag:this.drag));});
  const finish=e=>{if(this.pointer!==e.pointerId)return;this.pointer=null;container.classList.remove('dragging');app.end();};
  for(const event of ['pointerup','pointercancel','lostpointercapture'])this.dial.addEventListener(event,finish);
  this.dial.addEventListener('wheel',e=>{e.preventDefault();app.begin();const delta=-Math.sign(e.deltaY||e.deltaX)*.01;const n=clamp(this.normalized()+delta);this.set(denormal(this.d,this.reverse?1-n:n));app.end();},{passive:false});
  this.dial.addEventListener('keydown',e=>{if(!['ArrowUp','ArrowDown','ArrowLeft','ArrowRight','Home','End','Enter'].includes(e.key))return;e.preventDefault();if(e.key==='Enter'){this.input.focus();this.input.select();return;}app.begin();let n=this.normalized();if(e.key==='Home')n=0;else if(e.key==='End')n=1;else n+=['ArrowUp','ArrowRight'].includes(e.key)?.01:-.01;this.set(denormal(this.d,this.reverse?1-clamp(n):clamp(n)));app.end();});
  this.input.addEventListener('focus',()=>{this.editing=true;container.classList.remove('invalid');this.input.value=this.numeric().toFixed(this.digits());this.input.select();});
  this.input.addEventListener('blur',()=>{this.editing=false;this.update();});
  this.input.addEventListener('change',()=>this.commit());
  this.input.addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();this.commit();}if(e.key==='Escape'){e.preventDefault();this.editing=false;this.input.blur();container.classList.remove('invalid');this.update();}});
  this.observer=new ResizeObserver(()=>this.draw());this.observer.observe(this.dial);this.update();
 }
 numeric(){const v=this.getValue();return this.reverse?-v:['fet_input','fet_output'].includes(this.id)?48-v:v;}
 digits(){return this.d.step<.1?2:this.d.step<1?1:0;}
 set(v){this.app.set(this.id,v);this.update();}
 commit(){
  if(!this.editing)return;const text=this.input.value.trim();const match=text.match(/^([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*(kHz|Hz|ms|s|dB|%|:1|千赫兹|赫兹|毫秒|秒|分贝)?$/i);const unit=info(this.id)[2];let v=match?Number(match[1]):NaN;const suffix=match?.[2]?.toLowerCase();
  const valid={Hz:['hz','khz','赫兹','千赫兹'],ms:['ms','s','毫秒','秒'],dB:['db','分贝'],'%':['%'],':1':[':1'],'':[]};
  if(suffix&&!valid[unit]?.includes(suffix))v=NaN;if(suffix==='khz'||suffix==='千赫兹')v*=1000;if(unit==='ms'&&(suffix==='s'||suffix==='秒'))v*=1000;
  if(this.reverse)v=-v;else if(['fet_input','fet_output'].includes(this.id))v=48-v;
  if(!Number.isFinite(v)||v<this.d.min||v>this.d.max){this.container.classList.add('invalid');this.app.notify(this.app.t('Enter a value within the parameter range.'));return;}
  this.editing=false;this.app.begin();this.set(denormal(this.d,normal(this.d,v)));this.app.end();this.input.blur();this.update();
 }
 update(){
  const key=JSON.stringify([this.getValue(),this.app.ui.language,this.app.ui.theme,this.editing]);if(this.renderKey===key)return;this.renderKey=key;
  const [en,zh,unit]=info(this.id),name=this.app.ui.language==='zh'?zh:en;this.container.querySelector('.knob-label').textContent=name;this.dial.setAttribute('aria-label',name);this.input.setAttribute('aria-label',name);
  const value=this.numeric();let text=value.toFixed(this.digits());if(this.id.endsWith('sidechain_hp')&&value===0||this.id==='vox_gate'&&value===-80)text='OFF';if(this.id==='vca_release'&&value>=1200)text='AUTO';
  this.container.querySelector('.knob-readout span').textContent=['AUTO','OFF'].includes(text)?'':unit;this.container.querySelector('.knob-readout').classList.toggle('unitless',['AUTO','OFF'].includes(text)||!unit);if(!this.editing)this.input.value=text;this.dial.setAttribute('aria-valuenow',String(value));this.dial.setAttribute('aria-valuetext',`${text} ${unit}`);this.draw();
 }
 cancelGesture(){if(this.pointer!=null){const pointer=this.pointer;this.pointer=null;this.container.classList.remove('dragging');if(this.dial.hasPointerCapture(pointer))this.dial.releasePointerCapture(pointer);this.app.end();}}
 dispose(){this.cancelGesture();this.observer.disconnect();}
}
