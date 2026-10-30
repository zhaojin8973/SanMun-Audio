// 小卡片精确输入：聚焦时固定目标频段，实时回显不打断编辑，非法输入不写入参数。
export function bindCardInput(input,field,app){
  let editing=false,band=-1;
  const format=value=>field==='freq'?`${value.toFixed(value<1000?1:0)} Hz`:field==='gain'?`${value>0?'+':''}${value.toFixed(2)} dB`:`Q ${value.toFixed(3)}`;
  const update=()=>{
    const n=app.selected,p=`band${n}_`,valid=app.values[p+'present'];
    if(editing&&band!==n){editing=false;input.blur();}
    input.disabled=!valid||(field==='gain'&&app.values[p+'type']>=3);
    input.setAttribute('aria-label',app.t({freq:'频率',gain:'增益',q:'带宽 Q'}[field]));
    if(!editing)input.value=input.disabled&&field==='gain'?'— dB':valid?format(app.values[p+field]):'';
  };
  const commit=()=>{
    if(!editing)return true;
    const match=input.value.trim().match(/^(?:Q\s*)?([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*(Hz|kHz|dB)?$/i);
    let value=match?Number(match[1]):NaN;const suffix=match?.[2]?.toLowerCase();
    if(suffix&&!(field==='freq'?['hz','khz']:field==='gain'?['db']:[]).includes(suffix))value=NaN;
    if(suffix==='khz')value*=1000;
    const id=`band${band}_${field}`,schema=app.schema[id];
    if(!Number.isFinite(value)||!schema||value<schema.min||value>schema.max){input.setAttribute('aria-invalid','true');return false;}
    editing=false;input.removeAttribute('aria-invalid');if(app.values[`band${band}_present`]){app.begin();app.set(id,value);app.end();}return true;
  };
  input.onfocus=()=>{editing=true;band=app.selected;input.value=String(app.values[`band${band}_${field}`]);input.select();};
  input.onkeydown=e=>{e.stopPropagation();if(e.key==='Enter'){e.preventDefault();if(commit())input.blur();}if(e.key==='Escape'){e.preventDefault();editing=false;input.removeAttribute('aria-invalid');input.blur();update();}};
  input.onblur=()=>{commit();editing=false;input.removeAttribute('aria-invalid');update();};
  return {update,isEditing:()=>editing};
}
