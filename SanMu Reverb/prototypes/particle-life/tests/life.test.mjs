import test from 'node:test';
import assert from 'node:assert/strict';
import { ParticleLife } from '../src/life.js';
import { defaults, parameterList, parameterValue, normalizedValue } from '../src/parameters.js';

const pulse = (time = 0) => ({ time, duration: .02, dryRms: .12, wetRms: .04,
  dry: [.06, .04, .02, .01], wet: [.025, .02, .015, .01], pan: 0, wetPan: 0, correlation: .25 });
test('无输入不会由尾音重新生成粒子', () => {
  const life = new ParticleLife(1000);
  life.ingest({ ...pulse(), dryRms: 0 }, defaults);
  assert.equal(life.count, 0);
  life.ingest(pulse(), defaults);
  const born = life.bornTotal;
  for (let time = .02; time < 5; time += .02) {
    life.ingest({ ...pulse(time), dryRms: 0 }, defaults);
    life.update(time, defaults);
  }
  assert.equal(life.bornTotal, born);
});
test('预延迟之前湿声粒子不可见，之后逐渐进入', () => {
  const life = new ParticleLife(1000);
  const p = { ...defaults, predelay: 250, attack: 0 };
  life.ingest(pulse(.02), p);
  life.update(.01, p); life.update(.2, p);
  assert.equal(life.opacity.some(x => x > 0), false);
  life.update(.35, p);
  assert(life.opacity.some(x => x > .01));
});
test('延长衰减保留已积累的损失，不恢复年龄和能量', () => {
  const life = new ParticleLife(1000);
  life.ingest(pulse(), defaults);
  for (let time = 0; time < 1; time += .02) life.update(time, defaults);
  const before = [...life.loss], births = [...life.birth];
  life.update(1.02, { ...defaults, decay: 20 });
  assert.deepEqual([...life.birth], births);
  before.forEach((loss, i) => { if (life.alive[i]) assert(life.loss[i] >= loss); });
});
test('种子固定时路径可重复，长衰减和持续输入受容量约束', () => {
  const a = new ParticleLife(400, 1), b = new ParticleLife(400, 1);
  for (let time = 0; time < 4; time += .02) {
    for (const life of [a, b]) { life.ingest(pulse(time), defaults); life.update(time, defaults); }
  }
  assert.deepEqual(a.position, b.position);
  assert(a.count <= 400);
  assert(a.bornTotal > 400);
});
test('分析视图独立于声音混合，零扩散仍保留尾音', () => {
  const life = new ParticleLife(1000);
  life.parameters = { ...defaults, mix: 0, early: 0, late: 0 };
  life.ingest(pulse(), life.parameters);
  life.update(0, life.parameters); life.update(.2, life.parameters);
  assert(life.count > 0);
  assert.equal(life.opacity.some(x => x > 0), true);
});
test('参数端点和默认值往返不丢失', () => {
  for (const item of parameterList)
    for (const value of [item[2], item[3], item[5]])
      assert(Math.abs(parameterValue(item, normalizedValue(item, value)) - value) < 1e-9, item[0]);
});

test('尘埃围绕中心形成三维体积，不集中在前下方喷口', () => {
  const life = new ParticleLife(12000);
  life.ingest(pulse(.02), defaults);
  life.update(.02, defaults); life.update(.4, defaults);
  const limits = [[Infinity, -Infinity], [Infinity, -Infinity], [Infinity, -Infinity]];
  let radialTotal = 0, visible = 0;
  for (let i = 0; i < life.capacity; i++) if (life.opacity[i] > .001) {
    for (let axis = 0; axis < 3; axis++) {
      const value = life.position[i * 3 + axis];
      limits[axis][0] = Math.min(limits[axis][0], value);
      limits[axis][1] = Math.max(limits[axis][1], value);
    }
    radialTotal += Math.hypot(...life.position.subarray(i * 3, i * 3 + 3));
    visible++;
  }
  assert(visible > 30);
  for (const [low, high] of limits) assert(low < -.8 && high > .8);
  assert(radialTotal / visible > 1);
});

test('声像和调制改变时整体中心保持稳定，尘埃逐帧位移连续', () => {
  const life = new ParticleLife(12000);
  const p = { ...defaults, depth: 100, rate: 10, decay: 20 };
  life.parameters = { ...p };
  life.ingest(pulse(.02), p);
  life.update(.02, p);
  let maxStep = 0, maxCentroid = 0;
  for (let frame = 0; frame < 240; frame++) {
    const previous = life.position.slice();
    life.metrics.wetPan = frame % 2 ? 1 : -1;
    life.update(.4 + frame / 60, p);
    const centre = [0, 0, 0];
    let total = 0;
    for (let i = 0; i < life.count; i++) {
      const weight = life.opacity[i];
      total += weight;
      for (let c = 0; c < 3; c++) centre[c] += life.position[i * 3 + c] * weight;
      if (frame > 0) maxStep = Math.max(maxStep, Math.hypot(...centre.map((_, c) => life.position[i * 3 + c] - previous[i * 3 + c])));
    }
    maxCentroid = Math.max(maxCentroid, Math.hypot(...centre) / Math.max(total, 1e-9));
  }
  assert(maxCentroid < .0001, `中心位移：${maxCentroid}`);
  assert(maxStep < .012, `单帧位移：${maxStep}`);
});

test('频带能量实时改变已有尘埃明暗，声音变化不造成大范围甩动', () => {
  const life = new ParticleLife(12000);
  life.ingest(pulse(.02), defaults);
  for (let time = 0; time < 1; time += 1 / 60) life.update(time, defaults);
  const before = life.opacity.slice(), position = life.position.slice();
  const born = life.bornTotal;
  life.metrics.wet = [.08, 0, 0, 0];
  for (let frame = 0; frame < 30; frame++) life.update(1 + frame / 60, defaults);
  let lowBefore = 0, lowAfter = 0, highBefore = 0, highAfter = 0, movement = 0;
  for (let i = 0; i < life.count; i++) {
    if (life.band[i] === 0) { lowBefore += before[i]; lowAfter += life.opacity[i]; }
    else { highBefore += before[i]; highAfter += life.opacity[i]; }
    movement = Math.max(movement, Math.hypot(...[0, 1, 2].map(c => life.position[i * 3 + c] - position[i * 3 + c])));
  }
  assert(lowAfter > lowBefore * 1.15);
  assert(highAfter < highBefore * .3);
  assert.equal(life.bornTotal, born);
  assert(movement < .16);
});
