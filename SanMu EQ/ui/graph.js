import {setBandColour} from './shared/band-colour.js';
import {clamp} from './controls.js';
import {SpectrumFocus} from './spectrum-focus.js';
import {SpectrumMotion} from './spectrum-motion.js';
import {responseCurve} from './response.js';
export const bandColour = (n, dark = false) => (dark ? ['#80a9f2','#72cfba','#e4bd84','#e691ae','#b5a0e6','#d4a1d8'] : ['#3e6dc4','#328e78','#ae7d36','#b75578','#8361b7','#ac64b0'])[(n - 1) % 6];
// 频率决定颜色；采用连续的多级色相过渡，声道位置另外使用形状标记。
const spectrumStops=['#ec5265','#f17449','#f89b42','#eec94c','#add755','#53c891','#3ac5ba','#46b9e3','#648eec','#7881ed'];
export function frequencyColour(frequency,dark=false){const p=clamp(Math.log(frequency/20)/Math.log(1000))*(spectrumStops.length-1),i=Math.min(spectrumStops.length-2,Math.floor(p)),t=p-i;
  const rgb=hex=>[1,3,5].map(k=>parseInt(hex.slice(k,k+2),16)),a=rgb(spectrumStops[i]),b=rgb(spectrumStops[i+1]);return `rgb(${a.map((v,k)=>Math.round((v+(b[k]-v)*t)*(dark?1:.78))).join(',')})`;
}
export class Graph {
  constructor(app) {
    this.app = app; this.canvas = document.querySelector('#response'); this.nodes = document.querySelector('#nodes'); this.buttons = new Map(); this.range = 12;
    this.motion=new SpectrumMotion();this.preMotion=new SpectrumMotion();this.focus=new SpectrumFocus(this);
    document.addEventListener('pointermove',e=>this.moveDrag(e));
    for(const event of ['pointerup','pointercancel','lostpointercapture'])document.addEventListener(event,e=>this.finishDrag(e));
    this.canvas.addEventListener('dblclick', e => {
      const p = this.point(e); if (!this.inside(p)) return;
      app.command('addBand', {freq: this.frequency(p.x), gain: clamp((this.area.h / 2 - (p.y - this.area.y)) / this.area.h * 2 * this.range, -24, 24)});
    });
    this.canvas.addEventListener('pointerdown', e => {if(e.button)return;e.preventDefault();document.activeElement?.blur();app.select(-1);});
    this.canvas.addEventListener('pointermove', e => {
      const p = this.point(e), readout = document.querySelector('#cursor-value');
      readout.hidden = !this.inside(p)||this.focus.amount>0; if (readout.hidden) return;
      readout.textContent = `${Math.round(this.frequency(p.x))} Hz`;
      readout.style.left = `${clamp(p.x + 12, 0, this.canvas.clientWidth - 95)}px`; readout.style.top = `${p.y + 16}px`;
    });
    this.canvas.addEventListener('pointerleave', () => document.querySelector('#cursor-value').hidden = true);
    this.observer = new ResizeObserver(() => this.draw()); this.observer.observe(this.canvas);
  }
  point(e) { const b = this.canvas.getBoundingClientRect(); return {x:(e.clientX-b.left)/this.app.scale,y:(e.clientY-b.top)/this.app.scale}; }
  inside(p) { const a=this.area; return a && p.x>=a.x&&p.x<=a.x+a.w&&p.y>=a.y&&p.y<=a.y+a.h; }
  frequency(x) { return 20 * Math.pow(1000,(x-this.area.x)/this.area.w); }
  spectrumY(db){return this.area.y+this.area.h*(1-clamp((db+this.app.ui.spectrumRange)/(this.app.ui.spectrumRange+12)));}
  position(n) { const v=this.app.values,a=this.area,type=v[`band${n}_type`];return {x:a.x+Math.log(v[`band${n}_freq`]/20)/Math.log(1000)*a.w,y:a.y+a.h*(.5-clamp(type>=3?Math.max(0,20*Math.log10(v[`band${n}_q`])):v[`band${n}_gain`],-this.range,this.range)/(2*this.range))}; }
  startDrag(n,e,begin=true,capture=this.buttons.get(n)){
    this.app.select(n);if(begin)this.app.begin();
    this.drag={n,pointer:e.pointerId,x:e.clientX,y:e.clientY,freq:this.app.values[`band${n}_freq`],gain:this.app.values[`band${n}_gain`],q:this.app.values[`band${n}_q`]};
    capture?.setPointerCapture(e.pointerId);this.app.tools?.show(n);this.draw();
  }
  moveDrag(e){
    const d=this.drag;if(!d||d.pointer!==e.pointerId)return;
    this.app.set(`band${d.n}_freq`,clamp(d.freq*Math.pow(1000,(e.clientX-d.x)/(this.area.w*this.app.scale)),20,20000));
    if(this.app.values[`band${d.n}_type`]>=3)this.app.set(`band${d.n}_q`,clamp(d.q*Math.pow(10,(d.y-e.clientY)/(this.area.h*this.app.scale)*2),.1,40));
    else this.app.set(`band${d.n}_gain`,clamp(d.gain+(d.y-e.clientY)/(this.area.h*this.app.scale)*this.range*2,-24,24));
  }
  finishDrag(e){if(this.drag?.pointer!==e.pointerId)return;const n=this.drag.n;this.drag=null;this.app.end();this.app.tools?.show(n);this.draw();}
  bindNode(n) {
    const button=document.createElement('button');button.className='eq-node';button.dataset.band=n;button.innerHTML=`<span>${String(n).padStart(2,'0')}</span><small class=placement-badge hidden></small>`;button.setAttribute('role','tab');button.setAttribute('aria-label',`${this.app.t('频段')} ${n}`);this.nodes.append(button);this.buttons.set(n,button);
    button.addEventListener('focus',()=>{if(button.matches(':focus-visible'))this.app.tools?.show(n);});
    button.addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();this.app.tools?.show(n);document.querySelector('#node-type-open')?.focus();}});
    button.addEventListener('pointerdown',e=>{
      if(e.button)return;e.preventDefault();this.app.select(n);this.app.tools?.show(n);if(e.altKey||e.metaKey||e.ctrlKey){this.app.begin();this.app.set(`band${n}_${this.app.values[`band${n}_type`]>=3?'q':'gain'}`,this.app.values[`band${n}_type`]>=3?1:0);this.app.end();return;}document.activeElement?.blur();this.startDrag(n,e);
    });
    button.addEventListener('contextmenu',e=>{e.preventDefault();this.app.tools?.show(n);this.app.tools?.expand();});
    button.addEventListener('wheel',e=>{e.preventDefault();this.app.begin();this.app.set(`band${n}_q`,clamp(this.app.values[`band${n}_q`]*Math.pow(1.08,-Math.sign(e.deltaY)),.1,40));this.app.end();},{passive:false});
    button.addEventListener('keydown',e=>{
      if(e.key==='Delete'||e.key==='Backspace'){e.preventDefault();this.app.command('deleteBand',{band:n});return;}
      if(!['ArrowLeft','ArrowRight','ArrowUp','ArrowDown'].includes(e.key))return;e.preventDefault();this.app.begin();
      if(['ArrowLeft','ArrowRight'].includes(e.key))this.app.set(`band${n}_freq`,clamp(this.app.values[`band${n}_freq`]*Math.pow(1.02,e.key==='ArrowRight'?1:-1),20,20000));
      else if(this.app.values[`band${n}_type`]<3)this.app.set(`band${n}_gain`,clamp(this.app.values[`band${n}_gain`]+(e.key==='ArrowUp'?.1:-.1),-24,24));this.app.end();
    });return button;
  }
  draw() {
    if(this.paintRequest)return;
    const render=now=>{this.paintRequest=0;this.paint(now);if(!document.hidden)this.draw();};
    this.paintRequest=document.hidden?setTimeout(()=>render(performance.now()),16):requestAnimationFrame(render);
  }
  paint(now = performance.now()) {
    if(!this.app.values)return;
    const {canvas,app}=this,w=canvas.clientWidth,h=canvas.clientHeight,dpr=Math.min(3,(devicePixelRatio||1)*app.scale);if(!w||!h)return;
    if(canvas.width!==Math.round(w*dpr))canvas.width=Math.round(w*dpr);if(canvas.height!==Math.round(h*dpr))canvas.height=Math.round(h*dpr);const c=canvas.getContext('2d');c.setTransform(dpr,0,0,dpr,0,0);c.clearRect(0,0,w,h);const css=getComputedStyle(canvas),dark=app.ui.theme==='dark',muted=css.getPropertyValue('--muted').trim(),line=css.getPropertyValue('--line').trim();
    this.area={x:36,y:22,w:w-76,h:h-50};const a=this.area,bands=app.bands();
    if(!this.drag){const peak=Math.max(12,...bands.filter(n=>app.values[`band${n}_type`]<3).map(n=>Math.abs(app.values[`band${n}_gain`])));this.range=app.ui.range==='auto'?(peak>24?30:peak>12?24:12):Number(app.ui.range)||12;}
    const y=db=>a.y+a.h*(.5-db/(this.range*2)),x=f=>a.x+Math.log(f/20)/Math.log(1000)*a.w;
    c.font='10px -apple-system, BlinkMacSystemFont, sans-serif';c.fillStyle=muted;c.lineWidth=.6;c.strokeStyle=line;
    c.save();c.globalAlpha=.45;c.lineWidth=.5;
    for(const decade of [10,100,1000,10000])for(let multiplier=2;multiplier<10;multiplier++){
      const f=decade*multiplier;if(f<=20||f>=20000||[20,50,100,200,500,1000,2000,5000,10000].includes(f))continue;
      c.beginPath();c.moveTo(x(f),a.y);c.lineTo(x(f),a.y+a.h);c.stroke();
    }c.restore();
    for(let db=-this.range;db<=this.range;db+=this.range/4){c.beginPath();c.moveTo(a.x,y(db));c.lineTo(a.x+a.w,y(db));c.stroke();c.textAlign='right';c.fillText(db>0?`+${db}`:String(db),a.x-9,y(db)+3);}
    for(const f of [20,50,100,200,500,1000,2000,5000,10000,20000]){c.beginPath();c.moveTo(x(f),a.y);c.lineTo(x(f),a.y+a.h);c.stroke();c.textAlign=f===20?'left':f===20000?'right':'center';c.fillText(f>=1000?`${f/1000}k`:String(f),x(f),h-9);}
    if(app.ui.spectrum){c.textAlign='left';c.fillText(app.ui.spectrumTilt?'dB':'dBFS',a.x+a.w+5,12);for(let db=0;db>=-app.ui.spectrumRange;db-=app.ui.spectrumRange/3)c.fillText(String(db),a.x+a.w+5,this.spectrumY(db)+3);}
    const gradient=c.createLinearGradient(a.x,0,a.x+a.w,0);
    spectrumStops.forEach((colour,i)=>gradient.addColorStop(i/(spectrumStops.length-1),colour));
    c.save();c.beginPath();c.rect(a.x,a.y,a.w,a.h);c.clip();
    const pre=this.preMotion.update(app.frame?.inputSpectrum,now,app.ui.spectrumSpeed),post=this.motion.update(app.frame?.spectrum,now,app.ui.spectrumSpeed);
    const accumulate=(data,key)=>{if(!app.spectrumHold)return data;this[key]=data.map((v,i)=>Math.max(v,this[key]?.[i]??-150));return this[key];};
    const visual=data=>data.map((v,i)=>v+(app.ui.spectrumTilt||0)*Math.log2(20*Math.pow(1000,i/(data.length-1))/1000));
    const spectrum=visual(accumulate(app.ui.spectrumPost?post:app.ui.spectrumPre?pre:[],'heldSpectrum')),focus=this.focus.amount,curveOpacity=1-focus;this.spectrumValues=spectrum;
    if(app.ui.spectrum&&app.ui.spectrumPre&&app.ui.spectrumPost&&pre.length){const input=visual(accumulate(pre,'heldPre'));c.beginPath();input.forEach((db,i)=>{const px=a.x+i/(input.length-1)*a.w,py=this.spectrumY(db);i?c.lineTo(px,py):c.moveTo(px,py);});c.strokeStyle=muted;c.lineWidth=.8;c.globalAlpha=.45*(1-focus);c.stroke();}
    if(app.ui.spectrum&&spectrum?.length){
      c.beginPath();c.moveTo(a.x,a.y+a.h);spectrum.forEach((db,i)=>c.lineTo(a.x+i/(spectrum.length-1)*a.w,this.spectrumY(db)));c.lineTo(a.x+a.w,a.y+a.h);c.closePath();c.globalAlpha=(dark?.34:.27)+focus*.3;c.fillStyle=gradient;c.fill();
    }
    c.globalAlpha=1;
    const curves={},dynamicCurves={},count=320,total=new Float32Array(count);
    const dt=Math.min(100,now-(this.lastPaint??now));this.lastPaint=now;this.curveCache??=new Map();this.dynamicDisplay??={};
    for(const n of bands){
      const p=`band${n}_`,v=app.values,key=[v[p+'type'],v[p+'freq'],v[p+'gain'],v[p+'q'],v[p+'slope']].join(':');
      let cached=this.curveCache.get(n);if(cached?.key!==key){cached={key,curve:responseCurve(v[p+'type'],v[p+'freq'],v[p+'gain'],v[p+'q'],v[p+'slope'])};this.curveCache.set(n,cached);}curves[n]=cached.curve;
      const active=v[p+'dynamic']&&v[p+'type']<3&&!app.linearLegacy(n)&&!v.global_bypass;
      const target=active?Number(app.frame?.dynamicReduction?.[n]||0):0,current=this.dynamicDisplay[n]??target;
      const reduction=current+(target-current)*(1-Math.exp(-dt/35));this.dynamicDisplay[n]=reduction;
      if(active)dynamicCurves[n]=responseCurve(v[p+'type'],v[p+'freq'],-reduction,v[p+'q']);
    }const solo=bands.some(n=>app.values[`band${n}_solo`]);
    for(const n of bands){if(!app.values[`band${n}_enabled`]||(solo&&!app.values[`band${n}_solo`]))continue;const curve=curves[n];if(!curve)continue;
      c.beginPath();for(let i=0;i<count;i++){const db=curve[Math.round(i/(count-1)*(curve.length-1))]||0;total[i]+=db;i?c.lineTo(a.x+i/(count-1)*a.w,y(db)):c.moveTo(a.x,y(db));}
      // 只有正在编辑的频段显示淡色区域，避免多条单段响应线与总响应重叠。
      if(n===app.selected&&(this.drag||!document.querySelector('#node-tools')?.hidden)){
        c.lineTo(a.x+a.w,y(0));c.lineTo(a.x,y(0));c.closePath();c.fillStyle=frequencyColour(app.values[`band${n}_freq`],dark);c.globalAlpha=.07*curveOpacity;c.fill();
      }
    }
    for(const n of bands)if(app.values[`band${n}_enabled`]&&(!solo||app.values[`band${n}_solo`]))for(let i=0;i<count;i++)total[i]+=dynamicCurves[n]?.[i]||0;
    c.beginPath();for(let i=0;i<count;i++)i?c.lineTo(a.x+i/(count-1)*a.w,y(total[i])):c.moveTo(a.x,y(total[i]));c.strokeStyle=gradient;c.lineWidth=2;c.globalAlpha=(app.values.global_bypass?.3:1)*curveOpacity;c.stroke();c.restore();
    for(const [n,button] of this.buttons)if(!bands.includes(n)){button.remove();this.buttons.delete(n);}
    const placed=[];
    for(const n of [...bands].sort((a,b)=>(b===this.drag?.n)-(a===this.drag?.n))){const button=this.buttons.get(n)||this.bindNode(n),anchor=this.position(n);let pos={...anchor};
      const free=p=>p.x>=a.x&&p.x<=a.x+a.w&&p.y>=a.y&&p.y<=a.y+a.h&&placed.every(q=>Math.hypot(q.x-p.x,q.y-p.y)>=28);
      if(n!==this.drag?.n&&!free(pos)){let found=false;for(let r=1;r<12&&!found;r++)for(let dx=-r;dx<=r&&!found;dx++)for(let dy=-r;dy<=r&&!found;dy++){if(Math.abs(dx)!==r&&Math.abs(dy)!==r)continue;const p={x:anchor.x+dx*29,y:anchor.y+dy*29};if(free(p)){pos=p;found=true;}}}
      placed.push(pos);const col=frequencyColour(app.values[`band${n}_freq`],dark);setBandColour(button,col);button.style.left=`${pos.x}px`;button.style.top=`${pos.y}px`;button.setAttribute('aria-selected',n===app.selected);button.classList.toggle('bypassed',!app.values[`band${n}_enabled`]);button.classList.toggle('dynamic',!app.linearLegacy(n)&&!!app.values[`band${n}_dynamic`]&&app.values[`band${n}_type`]<3);button.title='';
      if(Math.hypot(pos.x-anchor.x,pos.y-anchor.y)>1){c.strokeStyle=col;c.globalAlpha=.4*curveOpacity;c.beginPath();c.moveTo(anchor.x,anchor.y);c.lineTo(pos.x,pos.y);c.stroke();c.globalAlpha=1;}
      const placement=app.values[`band${n}_placement`],badge=button.querySelector('.placement-badge');badge.hidden=!placement;badge.textContent=['','L','R','M','S'][placement];button.dataset.placement=placement;button.setAttribute('aria-label',`${app.t('频段')} ${n}, ${app.schema.band1_placement.choices[placement]}`);
      button.style.setProperty('--dynamic-opacity',String(.25+.75*clamp(Math.abs(this.dynamicDisplay[n]||0)/Math.max(.1,app.values[`band${n}_dyn_range`]))));
    }
    this.focus.render();app.tools?.refresh();
  }
}
