import assert from 'node:assert/strict';
import {SpectrumMotion} from '../ui/spectrum-motion.js';

// 有界原始样本：电平阶跃、相邻帧抖动、短促起音、静音与异常值。
for(const step of [1000/60,1000/30]){
  const motion=new SpectrumMotion();motion.update([-60,-60],0);
  let now=0;
  while(now<100){now+=step;motion.update([-30,-60],now);}
  const onset=motion.values[0];assert(onset>-36&&onset<=-30);assert.equal(motion.values[1],-60);
  let min=100,max=-100;
  for(let i=0;i<120;i++){
    now+=step;const value=motion.update([i%2?-32:-28,-60],now)[0];
    if(i>80){min=Math.min(min,value);max=Math.max(max,value);}
  }
  assert(max-min<.6,'4 dB 相邻帧抖动未得到明显缓和');
  for(let i=0;i<Math.ceil(1800/step);i++){now+=step;motion.update([-100,-100],now);}
  assert(motion.values.every(v=>v<-95),'静音后残影过久');
  console.log(`${Math.round(1000/step)} 帧/秒：100 ms 起音 ${onset.toFixed(2)} dB，4 dB 抖动显示摆幅 ${(max-min).toFixed(3)} dB`);
}
const transient=new SpectrumMotion();transient.update([-60],0);
const peak=transient.update([-30],1000/60)[0];assert(peak>-47,'单帧起音被完全抹掉');
transient.update([NaN],33);assert(Number.isFinite(transient.values[0]));
transient.update([],50);assert.equal(transient.values.length,0);
console.log('频谱显示平滑验证通过；未更改原始频谱数据或音频参数');
