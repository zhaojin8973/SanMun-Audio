// 仅用于独立界面预览，不进入插件资源包，也不运行或模拟声音处理。
export async function installPreview(){
 const original=await(await fetch('./parameter-schema.json')).json();const state=structuredClone(original);state.ui={...state.ui,theme:'light',language:'en',scale:1,history:true};
 state.canUndo=false;state.canRedo=false;state.ab='A';let start=null;const undo=[],redo=[],ab={A:structuredClone(state.values),B:structuredClone(state.values)};
 const snapshot=()=>structuredClone(state.values);const response=()=>({...structuredClone(state),canUndo:!!undo.length,canRedo:!!redo.length});
 window.__compPreview={async call(cmd,data={}){
  if(cmd==='initialize'||cmd==='state')return response();
  if(cmd==='begin'){start=snapshot();return true;}
  if(cmd==='set'){const d=state.schema[data.id];if(!d||!Number.isFinite(data.value)||data.value<d.min||data.value>d.max)throw Error('参数值超出范围');const previous=state.values[data.id];state.values[data.id]=data.value;if(['fet_input','fet_output'].includes(data.id)&&state.values.in_out_link>.5){const other=data.id==='fet_input'?'fet_output':'fet_input';state.values[other]=Math.max(0,Math.min(48,state.values[other]-(data.value-previous)));}state.dirty=true;}
  if(cmd==='end'&&start){if(JSON.stringify(start)!==JSON.stringify(state.values)){undo.push(start);redo.length=0;}start=null;}
  if(cmd==='ui')Object.assign(state.ui,data);
  if(cmd==='undo'&&undo.length){redo.push(snapshot());state.values=undo.pop();}
  if(cmd==='redo'&&redo.length){undo.push(snapshot());state.values=redo.pop();}
  if(cmd==='ab'&&['A','B'].includes(data.slot)){ab[state.ab]=snapshot();state.ab=data.slot;state.values=structuredClone(ab[data.slot]);}
  if(cmd==='copyAB')ab[state.ab==='A'?'B':'A']=snapshot();
  if(cmd==='listenStart')throw Error(state.ui.language==='zh'?'这是界面预览。测算需要插件宿主输入音频。':'UI preview: analysis requires audio from the plugin host.');
  if(['loadPreset','savePreset'].includes(cmd))throw Error(state.ui.language==='zh'?'预设文件操作请在插件中使用。':'Preset file operations are available in the plugin.');
  return response();
 }};
 window.addEventListener('message',e=>{if(e.origin!==location.origin)return;if(e.data?.kind==='theme')window.__sanmuComp.preference('theme',e.data.value);if(e.data?.kind==='mode')window.__sanmuComp.change('mode',e.data.value);if(e.data?.kind==='fixture'){clearInterval(window.__fixtureTimer);if(!e.data.value){window.__sanmuComp.visual.frames=[];window.__sanmuComp.visual.feed({time:performance.now(),input:-100,output:-100,gr:0});return;}window.__fixtureTimer=setInterval(()=>{const time=performance.now(),age=(time%510)/1000,level=-21+12*Math.exp(-age/0.14),gr=2+3*Math.exp(-age/0.18);window.__sanmuComp.visual.feed({time,input:level,output:level-gr+3,gr});},33);}});
}
