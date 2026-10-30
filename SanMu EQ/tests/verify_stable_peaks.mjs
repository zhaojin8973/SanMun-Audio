import assert from 'node:assert/strict';
import {StablePeakTracker} from '../ui/spectrum-focus.js';
const sample=peaks=>Array.from({length:500},(_,i)=>peaks.reduce((v,p)=>v+p[1]*Math.exp(-Math.pow((i/499-p[0])/.025,2)),-80));
const tracker=new StablePeakTracker();
const first=tracker.update(sample([[.2,40],[.6,45]]),0).map(p=>({...p}));
assert.equal(first.length,2);
const id=first[0].id;
for(let t=250;t<=2000;t+=250){const points=tracker.update(sample([[.2+(t%500?-.006:.006),40],[.6,45]]),t);assert.equal(points.length,2);assert.equal(points[0].id,id);assert(Math.abs(Math.log2(points[0].frequency/first[0].frequency))<.03);}
const held={...tracker.tracks[0]};
tracker.update(sample([[.22,55],[.6,45],[.85,40]]),2250,new Set([id]));
assert.equal(tracker.tracks[0].frequency,held.frequency);assert.equal(tracker.tracks[0].db,held.db);
assert.equal(tracker.tracks.length,2,'一帧短暂新峰不应直接加入');
for(let t=2500;t<=3250;t+=250)tracker.update(sample([[.2,40],[.6,45],[.85,40]]),t);
assert.equal(tracker.tracks.length,3,'持续显著的新峰没有加入');
tracker.update(sample([[.6,45],[.85,40]]),5000);assert(tracker.tracks.some(p=>p.id===id),'旧候选过快消失');
tracker.update(sample([[.6,45],[.85,40]]),7500);assert(!tracker.tracks.some(p=>p.id===id),'消失的峰没有退出');
assert.equal(new StablePeakTracker().update(Array(500).fill(-100),0).length,0);
console.log('稳定选峰验证通过：身份保持、微小漂移、鼠标锁定、新峰确认、旧峰退出、静音');
