import {clamp} from './controls.js';

// 按局部突出度选择真实谱峰，限制间距，不把宽带细碎起伏都变成候选点。
export function findSpectrumPeaks(values){
  if(!values?.length||values.length<16)return [];
  const size=values.length,smooth=values.map((v,i)=>(values[Math.max(0,i-1)]+2*v+values[Math.min(size-1,i+1)])/4);
  const radius=Math.max(3,Math.round(size/40)),candidates=[];
  for(let i=2;i<size-2;i++){
    if(!Number.isFinite(smooth[i])||smooth[i]<-78||smooth[i]<=smooth[i-1]||smooth[i]<smooth[i+1])continue;
    let left=smooth[i],right=smooth[i];
    for(let d=1;d<=radius;d++){left=Math.min(left,smooth[Math.max(0,i-d)]);right=Math.min(right,smooth[Math.min(size-1,i+d)]);}
    const prominence=smooth[i]-Math.max(left,right);if(prominence<1.2)continue;
    candidates.push({index:i,frequency:20*Math.pow(1000,i/(size-1)),db:values[i],score:smooth[i]+Math.min(8,prominence)});
  }
  const chosen=[];
  for(const peak of candidates.sort((a,b)=>b.score-a.score)){
    if(chosen.every(p=>Math.abs(Math.log2(p.frequency/peak.frequency))>.65))chosen.push(peak);
    if(chosen.length===5)break;
  }
  return chosen.sort((a,b)=>a.frequency-b.frequency);
}

// 候选目标有自己的寿命与缓慢跟踪，不等同于每帧重新排名的瞬时峰。
export class StablePeakTracker{
  constructor(){this.tracks=[];this.pending=[];this.nextId=1;}
  update(values,now,locked=new Set()){
    const candidates=findSpectrumPeaks(values),remaining=[...candidates],initial=this.nextId===1;
    for(const track of this.tracks){
      const best=remaining.reduce((a,b)=>!a||Math.abs(Math.log2(b.frequency/track.frequency))<Math.abs(Math.log2(a.frequency/track.frequency))?b:a,null);
      if(best&&Math.abs(Math.log2(best.frequency/track.frequency))<.38){
        remaining.splice(remaining.indexOf(best),1);track.seen=now;
        if(!locked.has(track.id)){const shift=clamp(Math.log2(best.frequency/track.frequency)*.1,-.018,.018);track.frequency*=Math.pow(2,shift);track.db+=(best.db-track.db)*.09;}
      }
    }
    this.tracks=this.tracks.filter(t=>locked.has(t.id)||now-t.seen<4000);
    const pending=[];
    for(const peak of remaining){
      if(this.tracks.some(t=>Math.abs(Math.log2(t.frequency/peak.frequency))<.65))continue;
      const old=this.pending.find(p=>Math.abs(Math.log2(p.frequency/peak.frequency))<.2),candidate={...peak,since:old?.since??now};pending.push(candidate);
      if(this.tracks.length<5&&(initial||now-candidate.since>=700)){this.tracks.push({...peak,id:this.nextId++,seen:now});}
    }
    // 首次进入一次建立候选集，之后新峰需要持续显著才补入。
    this.pending=pending;return this.tracks;
  }
}

export class SpectrumFocus {
  constructor(graph) {
    this.graph=graph;this.amount=0;this.target=0;this.peaks=[];this.dwell=0;this.anchor=null;this.tracker=new StablePeakTracker();
    this.layer=document.createElement('div');this.layer.id='spectrum-peaks';graph.canvas.parentElement.append(this.layer);
    document.addEventListener('pointermove',e=>this.hover(e));
    document.addEventListener('pointerdown',e=>{if(!e.target.closest('.spectrum-peak'))this.exit();});
    document.addEventListener('keydown',e=>{if(e.key==='Escape')this.exit();});
    window.addEventListener('blur',()=>this.exit());
    document.addEventListener('visibilitychange',()=>{if(document.hidden)this.exit();});
  }
  hover(e) {
    const g=this.graph,p=g.point(e),a=g.area,data=g.spectrumValues;
    if(this.target){
      const w=g.canvas.clientWidth,h=g.canvas.clientHeight;
      if(!g.drag&&(p.x<-8||p.x>w+8||p.y<-8||p.y>h+8))this.exit();
      return;
    }
    const nearNode=[...g.buttons.values()].some(b=>Math.hypot(parseFloat(b.style.left)-p.x,parseFloat(b.style.top)-p.y)<30);
    const valid=g.app.ui.spectrum&&g.app.ui.spectrumGrab&&!g.drag&&(e.target===g.canvas||e.target?.closest?.('#spectrum-peaks'))&&g.inside(p)&&data?.length&&!nearNode;
    if(!valid){this.cancelDwell();return;}
    const center=Math.round(clamp((p.x-a.x)/a.w)*(data.length-1)),span=Math.max(4,Math.round(18/Math.max(1,a.w)*(data.length-1)));
    let localPeakDb=-150;
    for(let i=Math.max(0,center-span);i<=Math.min(data.length-1,center+span);i++)if(Number.isFinite(data[i]))localPeakDb=Math.max(localPeakDb,data[i]);
    if(p.y<g.spectrumY(localPeakDb)-36||!data.some(v=>v>-78)){this.cancelDwell();return;}
    if(this.anchor&&Math.hypot(p.x-this.anchor.x,p.y-this.anchor.y)>16){
      clearTimeout(this.dwell);this.dwell=0;this.anchor=p;
    }
    this.anchor??=p;
    if(!this.dwell)this.dwell=setTimeout(()=>{this.dwell=0;this.enter();},320);
  }
  cancelDwell(){clearTimeout(this.dwell);this.dwell=0;this.anchor=null;}
  enter() {
    this.cancelDwell();
    const raw=findSpectrumPeaks(this.graph.spectrumValues);
    const tracked=this.tracker.update(this.graph.spectrumValues,performance.now());
    const candidates=raw.length?raw:tracked;
    if(!this.graph.app.ui.spectrum||!candidates.length)return;
    this.target=1;this.amount=.2;this.graph.nodes.classList.add('spectrum-focused');
    this.peaks=candidates.map(peak=>{
      const button=document.createElement('button');button.className='spectrum-peak';button.innerHTML='<span></span><i class="candidate-node"></i>';this.layer.append(button);
      button.onpointerdown=e=>this.activate(e,peak);
      button.onclick=async e=>{if(e.detail)return;this.exit();await this.graph.app.command('addBand',{freq:peak.frequency,gain:0});this.graph.app.tools?.show(this.graph.app.selected);};
      return {...peak,button};
    });this.graph.draw();
  }
  exit(){this.cancelDwell();this.target=this.amount=0;this.peaks=[];this.graph.nodes?.classList.remove('spectrum-focused');this.layer.replaceChildren();this.graph.draw();}
  activate(e,peak) {
    if(e.button||this.graph.app.bands().length>=24)return;
    e.preventDefault();document.activeElement?.blur();this.graph.canvas.setPointerCapture(e.pointerId);this.exit();
    const n=this.graph.app.beginBand(peak.frequency);if(n>0)this.graph.startDrag(n,e,false,this.graph.canvas);
  }
  render() {
    const g=this.graph;if(!g.app.ui.spectrum||!g.app.ui.spectrumGrab){if(this.target)this.exit();return;}
    if(!this.target){
      if(g.spectrumValues?.length)this.tracker.update(g.spectrumValues,performance.now());
      return;
    }
    const a=g.area,used=[];
    for(const peak of this.peaks){
      const b=peak.button,text=peak.frequency>=1000?`${(peak.frequency/1000).toFixed(2)} kHz`:`${Math.round(peak.frequency)} Hz`;
      b.querySelector('span').textContent=text;b.dataset.frequency=peak.frequency;b.setAttribute('aria-label',g.app.ui.language==='en'?`Add and drag ${text}`:`点击或拖动添加 ${text} 频段`);
      const x=a.x+Math.log(peak.frequency/20)/Math.log(1000)*a.w,left=clamp(x-b.offsetWidth/2,a.x,a.x+a.w-b.offsetWidth);
      let top=clamp(g.spectrumY(peak.db)-36,a.y,a.y+a.h-44);
      const original=top;for(let row=0;row<8;row++){const candidate=clamp(original+(row%2?1:-1)*Math.ceil(row/2)*44,a.y,a.y+a.h-44);if(used.every(other=>Math.abs(left-other.left)>=80||Math.abs(candidate-other.top)>=44)){top=candidate;break;}}
      used.push({left,top});b.style.left=`${left}px`;b.style.top=`${top}px`;b.style.setProperty('--point-x',`${x-left}px`);b.disabled=g.app.bands().length>=24;
    }
  }
}
