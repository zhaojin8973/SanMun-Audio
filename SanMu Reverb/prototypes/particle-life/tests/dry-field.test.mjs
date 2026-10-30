import test from 'node:test';
import assert from 'node:assert/strict';
import { DryField } from '../src/dry-field.js';
import { ParticleLife } from '../src/life.js';
import { defaults } from '../src/parameters.js';

const input = { time: .01, duration: .01, dryRms: .1, wetRms: .04,
  dry: [.05, .08, .04, .01], wet: [.03, .04, .02, .01], pan: 0, wetPan: 0, correlation: 0 };
const silence = { ...input, dryRms: 0, dry: [0, 0, 0, 0] };

test('原声在首个绘制帧响应，混响预延迟不会推迟原声', () => {
  const dry = new DryField(200), wet = new ParticleLife(12000);
  const p = { ...defaults, predelay: 250, attack: 100 };
  dry.ingest(input); wet.ingest(input, p);
  dry.update(.02); wet.update(.02, p);
  assert(Math.max(...dry.opacity) > .3);
  assert.equal(Math.max(...wet.opacity), 0);
});

test('原声结束后及时归零，混响尾音不会点亮原声层', () => {
  const dry = new DryField(200), wet = new ParticleLife(12000);
  dry.ingest(input); wet.ingest(input, defaults);
  dry.update(.02); wet.update(.02, defaults);
  dry.ingest(silence);
  for (let t = .03; t < .32; t += .01) { dry.update(t); wet.update(t, defaults); }
  assert.equal(Math.max(...dry.opacity), 0);
  assert(Math.max(...wet.opacity) > .1);
});

test('两个绘制帧之间的短瞬态会被保留，隐藏页面不积压旧瞬态', () => {
  const dry = new DryField(200);
  dry.ingest(input); dry.ingest(silence); dry.update(.02);
  assert(Math.max(...dry.opacity) > .2);
  dry.clear();
  dry.ingest(input, false); dry.ingest(silence, false); dry.update(1);
  assert.equal(Math.max(...dry.opacity), 0);
});

test('只有实际输入的频段点亮，纯湿声和异常值不生成原声反馈', () => {
  const dry = new DryField(200);
  dry.ingest({ ...input, dry: [0, .1, 0, 0] }); dry.update(.02);
  for (let i = 0; i < dry.capacity; i++)
    assert.equal(dry.opacity[i * 2] > 0, dry.band[i] === 1);
  dry.clear();
  dry.ingest({ ...input, dry: [NaN, Infinity, -1, 0], dryRms: NaN }); dry.update(.1);
  assert.equal(Math.max(...dry.opacity), 0);
  assert(dry.position.every(Number.isFinite));
});
