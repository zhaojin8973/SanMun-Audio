export const spectrumDefaults={spectrumPre:false,spectrumPost:true,spectrumGrab:true,spectrumRange:90,spectrumResolution:2,spectrumSpeed:1,spectrumTilt:0};

export function createAnalyserSettings(app){
  const group=document.createElement('fieldset');group.className='setting-group analyser-settings';group.id='analyser-settings';
  group.innerHTML='<legend data-i18n="频谱分析">频谱分析</legend><div class="analyser-switches"><button id="spectrum-pre" aria-pressed="false">Pre</button><button id="spectrum-post" aria-pressed="true">Post</button><button id="spectrum-hover" aria-pressed="true" data-i18n="悬停选峰">悬停选峰</button><button id="spectrum-hold" aria-pressed="false" data-i18n="峰值保持">峰值保持</button></div>';
  const options=[['spectrumRange','显示范围',[[60,'60 dB'],[90,'90 dB'],[120,'120 dB']]],['spectrumResolution','分辨率',[[0,'低 · 2048'],[1,'中 · 4096'],[2,'高 · 8192'],[3,'最高 · 16384']]],['spectrumSpeed','显示速度',[[0,'快'],[1,'中'],[2,'慢']]],['spectrumTilt','显示倾斜',[[0,'0 dB/oct'],[1.5,'1.5 dB/oct'],[3,'3 dB/oct'],[4.5,'4.5 dB/oct'],[6,'6 dB/oct']]]];
  for(const [key,name,values]of options){const label=document.createElement('label');label.className='analyser-option';label.innerHTML=`<span data-i18n="${name}">${name}</span><select data-spectrum-option="${key}" aria-label="${name}"></select>`;for(const [value,text]of values){const option=document.createElement('option');option.value=value;option.dataset.label=text;option.textContent=text;label.querySelector('select').append(option);}label.querySelector('select').onchange=e=>{app.graph.focus.exit();app.updateUi({[key]:Number(e.target.value)});};group.append(label);}
  const note=document.createElement('p');note.className='analyser-note';note.dataset.i18n='Pre 为处理前，Post 为处理后；倾斜只改变显示。外部侧链与跨实例冲突检测暂未接入。';group.append(note);
  document.querySelector('#settings-dialog .setting-links').before(group);
  for(const [id,key]of [['spectrum-pre','spectrumPre'],['spectrum-post','spectrumPost'],['spectrum-hover','spectrumGrab']])group.querySelector('#'+id).onclick=()=>{app.graph.focus.exit();app.updateUi({[key]:!app.ui[key]});};
  const hold=group.querySelector('#spectrum-hold');let down=0,previous=false,pointer=null;
  const set=value=>{if(app.spectrumHold!==value){app.spectrumHold=value;app.graph.heldSpectrum=value?[...(app.ui.spectrumPost?app.graph.motion.values:app.graph.preMotion.values)]:null;app.graph.heldPre=value?[...app.graph.preMotion.values]:null;}hold.setAttribute('aria-pressed',value);app.graph.draw();};
  hold.onpointerdown=e=>{if(e.button)return;e.preventDefault();down=performance.now();previous=!!app.spectrumHold;pointer=e.pointerId;hold.setPointerCapture(pointer);set(true);};
  hold.onpointerup=e=>{if(e.pointerId!==pointer)return;pointer=null;set(performance.now()-down<250?!previous:previous);};
  const cancel=()=>{if(pointer!=null){pointer=null;set(previous);}};hold.onpointercancel=cancel;hold.onlostpointercapture=cancel;window.addEventListener('blur',cancel);
  hold.onclick=e=>{if(!e.detail)set(!app.spectrumHold);};
  return {refresh(){
    for(const [id,key]of [['spectrum-pre','spectrumPre'],['spectrum-post','spectrumPost'],['spectrum-hover','spectrumGrab']])group.querySelector('#'+id).setAttribute('aria-pressed',!!app.ui[key]);
    for(const select of group.querySelectorAll('select')){select.value=app.ui[select.dataset.spectrumOption];select.setAttribute('aria-label',app.t(select.previousElementSibling.dataset.i18n));for(const option of select.options)option.textContent=app.t(option.dataset.label);}
  }};
}
