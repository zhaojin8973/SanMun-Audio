// 浏览器设计预览仅模拟参数状态；插件内始终通过原生桥接读取音频数据。
const values={},schema={},frequencies=[40,150,500,2000,5000,10000];
const add=(id,min,max,step,initial,skew=1,choices)=>{schema[id]={min,max,step,initial,skew,choices};values[id]=initial;};
for(let n=1;n<=24;n++){
  add(`band${n}_present`,0,1,1,n<=6?1:0);add(`band${n}_enabled`,0,1,1,n<=6?1:0);add(`band${n}_solo`,0,1,1,0);
  add(`band${n}_freq`,20,20000,1,frequencies[n-1]||1000,.25);add(`band${n}_gain`,-24,24,.1,0);add(`band${n}_q`,.1,40,.01,1,.5);
  add(`band${n}_type`,0,4,1,n===1?1:n===6?2:0,1,['Bell','Low Shelf','High Shelf','Low Cut','High Cut']);
  add(`band${n}_dynamic`,0,1,1,0);add(`band${n}_dyn_range`,0,24,.1,6);add(`band${n}_dyn_threshold`,-60,0,.1,-24);
  add(`band${n}_dyn_auto`,0,1,1,1);add(`band${n}_dyn_upward`,0,1,1,0);
  add(`band${n}_slope`,0,3,1,0,1,['12 dB/oct','24 dB/oct','36 dB/oct','48 dB/oct']);add(`band${n}_placement`,0,4,1,0,1,['Stereo','Left','Right','Mid','Side']);
}
for(const id of ['input_gain_l','input_gain_r','output_gain_l','output_gain_r'])add(id,-24,24,.1,0);
for(const id of ['phase_invert','eq_mode','global_bypass','auto_gain'])add(id,0,1,1,0);
add('character_mode',0,2,1,0,1,['Clean','Tube','Discrete']);
add('linear_quality',0,2,1,0,1,['Daily','High precision','Extreme']);
let ui={theme:'light',language:'zh',scale:1,tips:'follow',spectrum:true,range:12,outputLink:true},ab='A',preset='Default Settings';
try{ui={...ui,...JSON.parse(localStorage.getItem('sanmu-eq-preview-ui')||'{}')};}catch{}
let baseline=JSON.stringify(values),transaction=null;const history=[],redo=[],slots={A:structuredClone(values),B:structuredClone(values)};let context;
const presets=[{id:'factory:0',name:'Default Settings'},{id:'factory:1',name:'Vocal Focus'},{id:'factory:2',name:'Warm Mix'}];const userPresets=new Map();
export function previewFrame(){
  context ||= new OfflineAudioContext(2,512,48000);
  const freqs=Float32Array.from({length:320},(_,i)=>20*Math.pow(1000,i/319)),curves={};
  for(let n=1;n<=24;n++){if(!values[`band${n}_present`])continue;
    const filter=context.createBiquadFilter(),type=values[`band${n}_type`];filter.type=['peaking','lowshelf','highshelf','highpass','lowpass'][type];filter.frequency.value=values[`band${n}_freq`];filter.gain.value=values[`band${n}_gain`];filter.Q.value=type>=3?.707*values[`band${n}_q`]:values[`band${n}_q`];
    const mag=new Float32Array(320),phase=new Float32Array(320);filter.getFrequencyResponse(freqs,mag,phase);curves[n]=Array.from(mag,v=>20*Math.log10(Math.max(1e-8,v))*(type>=3?values[`band${n}_slope`]+1:1));
  }
  const demo=new URLSearchParams(location.search).has('demo');
  const spectrum=demo?Array.from({length:320},(_,i)=>-45-i*.085+6*Math.sin(i*.037)+2*Math.sin(i*.6)):[];
  const dynamicReduction={},dynamicCurves={},rangeCurves={},dynamicDetector={},dynamicThreshold={},dynamicReady={};
  for(let n=1;n<=24;n++)if(values[`band${n}_dynamic`]&&values[`band${n}_enabled`]&&values[`band${n}_type`]<3){
    const range=values[`band${n}_dyn_range`];dynamicReduction[n]=demo?Math.min(3,range*.35)*(values[`band${n}_dyn_upward`]?-1:1):0;
    dynamicDetector[n]=demo?-21:-120;dynamicThreshold[n]=values[`band${n}_dyn_auto`]?-25:values[`band${n}_dyn_threshold`];dynamicReady[n]=demo;
    for(const [target,amount] of [[dynamicCurves,dynamicReduction[n]],[rangeCurves,range*(values[`band${n}_dyn_upward`]?-1:1)]]){const filter=context.createBiquadFilter();filter.type=['peaking','lowshelf','highshelf'][values[`band${n}_type`]];filter.frequency.value=values[`band${n}_freq`];filter.Q.value=values[`band${n}_q`];filter.gain.value=-amount;const mag=new Float32Array(320);filter.getFrequencyResponse(freqs,mag,new Float32Array(320));target[n]=Array.from(mag,v=>20*Math.log10(Math.max(1e-8,v)));}
  }
  return {curves,spectrum,inputSpectrum:demo?spectrum.map(v=>v+1.5):[],spectrumPeak:demo?spectrum.map((v,i)=>v+3+1.2*Math.sin(i*.11)):[],dynamicReduction,dynamicCurves,rangeCurves,dynamicDetector,dynamicThreshold,dynamicReady,meters:demo?[-19.3,-20.6]:[-100,-100],sampleRate:48000,latency:values.eq_mode?[10626,272162,544418][values.linear_quality||0]:4,linearQuality:values.linear_quality||0,linearLatencies:[10626,272162,544418]};
}
function state(){return {values:structuredClone(values),ui:{...ui},ab,preset,dirty:baseline!==JSON.stringify(values),canUndo:!!history.length,canRedo:!!redo.length,frame:previewFrame()};}
function begin(){transaction ||= structuredClone(values);}
function end(){if(transaction&&JSON.stringify(transaction)!==JSON.stringify(values)){history.push(transaction);redo.length=0;}transaction=null;}
export async function preview(command,data={}){
  if(command==='initialize')return {...state(),schema,presets};
  if(command==='state')return state();
  if(command==='audition')return true;
  if(command==='begin'){begin();return true;}
  if(command==='end'){end();return state();}
  if(command==='setMany'){for(const [id,value]of Object.entries(data.values||{}))await preview('set',{id,value});return true;}
  if(command==='set'){
    const d=schema[data.id];if(!d||!Number.isFinite(data.value)||data.value<d.min||data.value>d.max)throw Error('参数超出范围');begin();values[data.id]=data.value;return true;
  }
  if(command==='ui'){ui={...ui,...data};localStorage.setItem('sanmu-eq-preview-ui',JSON.stringify(ui));return state();}
  if(command==='undo'||command==='redo'){end();const from=command==='undo'?history:redo,to=command==='undo'?redo:history;if(from.length){to.push(structuredClone(values));Object.assign(values,from.pop());}return state();}
  if(command==='ab'){end();slots[ab]=structuredClone(values);ab=data.slot;Object.assign(values,slots[ab]);return state();}
  if(command==='copyAB'){slots[ab==='A'?'B':'A']=structuredClone(values);return state();}
  if(command==='addBand'){
    const n=Array.from({length:24},(_,i)=>i+1).find(i=>!values[`band${i}_present`]);if(!n)throw Error('已达到 24 段');begin();
    for(const [key,value] of Object.entries({present:1,enabled:1,solo:0,type:0,slope:0,placement:0,freq:data.freq||1000,gain:data.gain||0,q:1,dynamic:0,dyn_range:6,dyn_threshold:-24,dyn_auto:1,dyn_upward:0}))values[`band${n}_${key}`]=value;end();return {...state(),selected:n};
  }
  if(command==='deleteBand'){begin();for(const key of ['present','enabled','solo'])values[`band${data.band}_${key}`]=0;end();return {...state(),selected:-1};}
  if(command==='loadPreset'){
    begin();if(userPresets.has(data.id))Object.assign(values,userPresets.get(data.id));else{
      for(const [id,d]of Object.entries(schema))values[id]=d.initial;
      if(data.id==='factory:1'){values.band1_type=3;values.band1_freq=80;values.band3_gain=-3;values.band4_gain=2.5;values.band6_gain=2;}
      if(data.id==='factory:2'){values.band2_gain=2;values.band3_gain=-2;values.band6_gain=-1.5;}
    }preset=presets.find(p=>p.id===data.id)?.name||'Default Settings';baseline=JSON.stringify(values);end();return state();
  }
  if(command==='savePreset'){if(!data.name?.trim()||/[\\/:]/.test(data.name))throw Error('预设名称无效');if(presets.some(p=>p.name===data.name))throw Error('同名预设已存在');const id=`user:${data.name}`;presets.push({id,name:data.name});userPresets.set(id,structuredClone(values));preset=data.name;baseline=JSON.stringify(values);return {...state(),presets};}
  throw Error('未知请求');
}
if(new URLSearchParams(location.search).has('demo')){values.band1_type=3;values.band1_freq=55;values.band2_gain=2.5;values.band3_gain=-3.8;values.band3_q=2.2;values.band4_gain=3;values.band5_gain=-2.4;values.band6_gain=1.8;}
