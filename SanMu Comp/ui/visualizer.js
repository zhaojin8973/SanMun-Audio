export class Visualizer{
 constructor(app){this.app=app;this.canvas=document.querySelector('#history');this.latest={time:0,input:-100,output:-100,gr:0};this.frames=[];this.paused=false;this.holds={input:-100,output:-100,gr:0};this.clips={input:false,output:false};this.dirty=true;
  this.canvas.addEventListener('pointermove',e=>{const b=this.canvas.getBoundingClientRect();this.hover=(e.clientX-b.left)/b.width;this.draw();});this.canvas.addEventListener('pointerleave',()=>{this.hover=null;document.querySelector('#history-point').hidden=true;this.draw();});
  this.observer=new ResizeObserver(()=>this.draw());this.observer.observe(this.canvas);
 }
 spanMs(){return Number(this.app.ui?.historySpan)||6000;}
 clear(){this.holds={input:-100,output:-100,gr:0};this.clips={input:false,output:false};this.updateMeters();}
 history(enabled){if(this.enabled===enabled)return;if(this.enabled!==enabled){this.frames=[];this.enabled=enabled;this.paused=false;}this.canvas.hidden=!enabled;document.querySelector('#history-off').hidden=enabled;document.querySelector('#pause').disabled=!enabled;this.draw();}
 pause(){this.paused=!this.paused;if(!this.paused)this.frames=[];this.draw();}
 feed(frame){
  if(!frame||!['time','input','output','gr'].every(k=>Number.isFinite(frame[k])))return;
  const span=this.spanMs();
  if(!this.paused&&(frame.time-this.latest.time>1500||frame.time<this.latest.time))this.frames=[];this.latest=frame;
  if(this.enabled&&!this.paused){
   // 保留原始读数；仅曲线采用 45 毫秒平滑，电平表与悬停读数仍读原始值。
   if(this.frames.at(-1)?.time===frame.time)this.frames.pop();
   const previous=this.frames.at(-1),alpha=previous?1-Math.exp(-(frame.time-previous.time)/45):1;
   const display={};for(const key of ['input','output','gr']){
    const target=key==='gr'?Math.max(0,frame[key]):Math.max(-60,Math.min(0,frame[key]));
    const value=previous?.display?.[key]??target;
    display[key]=Math.abs(target-value)<.015?target:value+(target-value)*alpha;
   }
   this.frames.push({...frame,display});
   // 边界之前留一个样本，才能插值到固定的左边界，避免逐帧裁掉端点。
   while(this.frames.length>2&&this.frames[1].time<=frame.time-span)this.frames.shift();
  }
  for(const k of ['input','output','gr']){this.holds[k]=Math.max(this.holds[k],frame[k]);if(k!=='gr')this.clips[k]||=frame[k]>=0;}
  this.updateMeters();if(!this.paused)this.draw();
 }
 visibleFrames(){
  const frames=this.frames,span=this.spanMs();if(frames.length<2)return frames;
  const start=frames.at(-1).time-span;
  const first=frames[0],next=frames[1];if(first.time>=start||next.time<=first.time)return frames;
  const ratio=(start-first.time)/(next.time-first.time),edge={time:start,display:{}};
  for(const key of ['input','output','gr']){
   edge[key]=first[key]+(next[key]-first[key])*ratio;
   const a=first.display?.[key]??first[key],b=next.display?.[key]??next[key];
   edge.display[key]=a+(b-a)*ratio;
  }
  return [edge,...frames.slice(1)];
 }
 updateMeters(){for(const key of ['input','gr','output']){const value=this.latest[key];const h=key==='gr'?value/24:(value+60)/60;const bar=document.querySelector(`[data-meter=${key}]`);const fill=bar.querySelector('.meter-fill');fill.style.transform='none';const empty=100-Math.min(1,Math.max(0,h))*100;fill.style.clipPath=key==='gr'?`inset(0 0 ${empty}% 0)`:`inset(${empty}% 0 0 0)`;bar.setAttribute('role','meter');bar.setAttribute('aria-label',key);bar.setAttribute('aria-valuenow',value);bar.setAttribute('aria-valuemin',key==='gr'?0:-60);bar.setAttribute('aria-valuemax',key==='gr'?24:0);const hold=bar.querySelector('.peak-line');hold.style.opacity='1';hold.style.top=`${Math.min(100,Math.max(0,key==='gr'?this.holds[key]/24*100:-this.holds[key]/60*100))}%`;const out=document.querySelector('#meter-'+key);out.textContent=key!=='gr'&&value<=-90?'−∞':(key==='gr'?-value:value).toFixed(1);out.classList.toggle('clipped',!!this.clips[key]);out.title=this.clips[key]?'CLIP':`${this.holds[key].toFixed(1)} dB`;}}
 draw(){if(this.scheduled)return;this.scheduled=requestAnimationFrame(()=>{this.scheduled=null;this.paint();});}
 paint(){
  if(!this.enabled)return;const c=this.canvas,w=c.clientWidth,h=c.clientHeight;if(!w||!h)return;const dpr=Math.min(3,(devicePixelRatio||1)*c.getBoundingClientRect().width/w);if(c.width!==Math.round(w*dpr))c.width=Math.round(w*dpr);if(c.height!==Math.round(h*dpr))c.height=Math.round(h*dpr);const g=c.getContext('2d');g.setTransform(dpr,0,0,dpr,0,0);g.clearRect(0,0,w,h);const style=getComputedStyle(document.querySelector('.instrument'));const colour=k=>style.getPropertyValue(k).trim();const x0=40,x1=w-16;const gr={top:16,height:82};const lv={top:122,height:126};const levelRange=Number(this.app.ui.levelRange)===30?30:60;const range=Number(this.app.ui.grRange)||12;const span=this.spanMs(),spanSec=span/1000;g.font='9px '+style.fontFamily;g.lineWidth=.6;
  const grid=(plot,max,step,unit)=>{g.fillStyle=colour('--muted');g.textAlign='right';g.fillText(unit,x0-8,plot.top-7);for(let v=0;v<=max;v+=step){const y=plot.top+v/max*plot.height;g.strokeStyle=colour('--line');g.beginPath();g.moveTo(x0,y);g.lineTo(x1,y);g.stroke();g.fillStyle=colour('--muted');g.fillText(v===0?'0':'−'+v,x0-8,y+3);}};grid(gr,range,range/3,'GR dB');grid(lv,levelRange,levelRange/3,'dBFS');
  // GR 与 IN/OUT 双表头分区，数值按当前量程重映射。
  g.textAlign='left';g.fillStyle=colour('--muted');g.font='8px '+style.fontFamily;g.fillText('IN / OUT',x0+3,lv.top-7);
  const frames=this.visibleFrames();const now=this.frames.at(-1)?.time??this.latest.time;const x=f=>x1-(now-f.time)/span*(x1-x0);const y=(f,key)=>key==='gr'?gr.top+Math.max(0,Math.min(range,f.display?.gr??f.gr))/range*gr.height:lv.top+Math.min(levelRange,Math.max(0,-(f.display?.[key]??f[key])))/levelRange*lv.height;
  if(this.frames.length&&this.app.ui.layers?.input!==false&&this.app.ui.layers?.output!==false){g.fillStyle=colour('--trace-out');g.globalAlpha=.055;g.beginPath();frames.forEach((f,i)=>i?g.lineTo(x(f),y(f,'input')):g.moveTo(x(f),y(f,'input')));[...frames].reverse().forEach(f=>g.lineTo(x(f),y(f,'output')));g.closePath();g.fill();g.globalAlpha=1;}
  if(this.frames.length&&this.app.ui.layers?.gr!==false){g.fillStyle=colour('--trace-gr');g.globalAlpha=.08;g.beginPath();g.moveTo(x(frames[0]),gr.top);frames.forEach(f=>g.lineTo(x(f),y(f,'gr')));g.lineTo(x(frames.at(-1)),gr.top);g.closePath();g.fill();g.globalAlpha=1;}
  for(const key of ['input','output','gr']){if(this.app.ui.layers?.[key]===false||!this.frames.length)continue;g.strokeStyle=colour(key==='input'?'--trace-in':key==='output'?'--trace-out':'--trace-gr');g.lineWidth=key==='output'?1.5:1.25;g.lineJoin='round';g.lineCap='butt';g.setLineDash([]);g.globalAlpha=key==='input'?.55:key==='gr'?.88:.92;g.beginPath();frames.forEach((f,i)=>i?g.lineTo(x(f),y(f,key)):g.moveTo(x(f),y(f,key)));g.stroke();g.globalAlpha=1;}g.setLineDash([]);g.fillStyle=colour('--muted');g.font='9px '+style.fontFamily;for(let i=0;i<=4;i++){g.textAlign=i===0?'left':i===4?'right':'center';const sec=spanSec*(1-i/4);g.fillText(i===4?this.app.t('Now'):'−'+(Number.isInteger(sec)?sec:sec.toFixed(1))+' s',x0+(x1-x0)*i/4,h-6);}
  if(!this.frames.length){g.textAlign='center';g.fillStyle=colour('--muted');g.fillText(this.app.t(this.app.nativeReady?'Waiting for curve data':'Enable the curve demo above to preview GR zoom.'),(x0+x1)/2,lv.top+lv.height/2);}
  if(this.paused){g.textAlign='right';g.fillStyle=colour('--muted');g.fillText(this.app.t('Frozen'),x1,gr.top-7);}
  if(this.frames.some(f=>['input','output'].some(key=>this.app.ui.layers?.[key]!==false&&(f[key]>0||(f[key]>-90&&f[key]<-levelRange))))){g.fillStyle=colour('--muted');g.textAlign='right';g.fillText(this.app.ui.language==='zh'?'IN/OUT 超出显示范围':'IN/OUT outside view',x1,lv.top-7);}
  if(this.frames.some(f=>f.gr>range)){g.textAlign='right';g.fillStyle=colour('--trace-gr');g.fillText('> '+range+' dB',x1,gr.top+12);}
  if(this.hover!=null&&this.frames.length){const time=now-(x1-this.hover*w)/(x1-x0)*span;const f=this.frames.reduce((a,b)=>Math.abs(a.time-time)<Math.abs(b.time-time)?a:b);g.strokeStyle=colour('--muted');g.lineWidth=.6;g.beginPath();g.moveTo(x(f),gr.top);g.lineTo(x(f),lv.top+lv.height);g.stroke();const o=document.querySelector('#history-point');o.hidden=false;o.textContent=`${((f.time-now)/1000).toFixed(2)} s   IN ${f.input.toFixed(1)}   OUT ${f.output.toFixed(1)} dBFS   GR −${f.gr.toFixed(1)} dB`;}
 }
}
