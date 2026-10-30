import test from 'node:test';
import assert from 'node:assert/strict';
import { WaterSurface } from '../src/water-surface.js';
import { defaults } from '../src/parameters.js';
const fixture = () => ({ water: new WaterSurface(), life: { parameters: { ...defaults }, wet: [.025, .018, .006, .002] }, dry: { levels: [.055, .034, .011, .003] } });
const state = water => ({ wet: water.uniforms.wetBands.value.toArray(), dry: water.uniforms.dryBands.value.toArray(), phase: water.uniforms.phase.value.toArray(), dryPhase: water.uniforms.dryPhase.value, flow: water.uniforms.flow.value });

test('水膜保持输入参考独立，隐藏湿声不重置声音状态', () => {
  const { water, life, dry } = fixture();
  water.update(0, life, dry, 1, 1, true); water.update(.03, life, dry, 1, 1, true);
  const original = state(water);
  water.update(.03, life, dry, 0, 1, true);
  assert.deepEqual(state(water), original); assert.equal(water.uniforms.wetGain.value, 0); assert.equal(water.uniforms.dryGain.value, 1);
  water.update(.03, life, dry, 1, 0, true);
  assert.deepEqual(state(water), original); assert.equal(water.uniforms.dryGain.value, 0);
});

test('尾音存在时水膜继续，声音归零后回到静止，不自行造浪', () => {
  const { water, life, dry } = fixture(); water.update(0, life, dry, 1, 1, true);
  dry.levels.fill(0); water.update(.03, life, dry, 1, 1, true);
  assert(water.uniforms.wetBands.value.x > 0); assert.deepEqual(water.uniforms.dryBands.value.toArray(), [0,0,0,0]);
  life.wet.fill(0); water.update(.06, life, dry, 1, 1, true); const still = state(water);
  water.update(2, life, dry, 1, 1, true); assert.deepEqual(state(water), still);
  assert.deepEqual(still.wet, [0,0,0,0]);
});

test('暂停与减少动态不推进波纹相位，跳转、清空和异常输入保持有限值', () => {
  const { water, life, dry } = fixture(); water.update(10, life, dry, 1, 1, false); const still = state(water);
  water.update(11, life, dry, 1, 1, false); assert.deepEqual(state(water), still);
  water.update(11, life, dry, 1, 1, true); assert.deepEqual(state(water), still);
  water.update(1, life, dry, 1, 1, true); assert.equal(water.uniforms.flow.value, 0);
  life.wet = [NaN, Infinity, -1, 1e9]; dry.levels = [...life.wet]; life.parameters.size = NaN;
  water.update(1.03, life, dry, NaN, Infinity, true);
  for (const array of [state(water).wet, state(water).dry, state(water).phase]) assert(array.every(Number.isFinite));
  assert.equal(water.uniforms.wetGain.value, 0); assert.equal(water.uniforms.space.value, .52);
  water.clear(); assert.deepEqual(water.uniforms.wetBands.value.toArray(), [0,0,0,0]);
});
