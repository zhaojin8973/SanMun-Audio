import {clamp} from './controls.js';

// 仅平滑显示，不进入音频、检测器或 GR。明显起音快速响应，小幅起伏缓和，回落稍慢。
export class SpectrumMotion {
  constructor(){this.values=[];this.time=0;}
  update(source,now=performance.now(),speed=1){
    if(!source?.length){this.values=[];this.time=now;return this.values;}
    const clean=source.map(v=>Number.isFinite(v)?clamp(v,-150,20):-150);
    if(this.values.length!==clean.length){this.values=clean;this.time=now;return this.values;}
    const dt=clamp((now-this.time)/1000,0,.1);this.time=now;
    for(let i=0;i<clean.length;i++){
      const difference=clean[i]-this.values[i];
      const seconds=(difference>6?.028:difference>0?.085:.22)*([.55,1,1.8][speed]??1);
      this.values[i]+=difference*(1-Math.exp(-dt/seconds));
    }
    return this.values;
  }
}
