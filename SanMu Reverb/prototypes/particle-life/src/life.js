import { defaults } from './parameters.js';

export const bandFrequencies = [140, 640, 2700, 7600];
export const palette = [[1, .62, .8], [.7, .54, 1], [.43, .84, .97], [.69, .77, 1]];
export function frequencyColor(band, variation = 0) {
  const neighbour = palette[Math.min(3, band + 1)];
  return palette[band].map((value, channel) => value + (neighbour[channel] - value) * variation);
}
const clamp = (x, a, b) => Math.min(b, Math.max(a, x));
const smooth = x => x * x * x * (x * (x * 6 - 15) + 10);

// 平滑随机场的相邻区间共享端点与零斜率，不逐帧抽签，也不周期性左右摇摆。
function driftNoise(time, seed) {
  const cell = Math.floor(time), t = smooth(time - cell);
  const hash = n => {
    let x = Math.imul(n ^ seed, 0x45d9f3b);
    x = Math.imul(x ^ (x >>> 16), 0x45d9f3b);
    return ((x ^ (x >>> 16)) >>> 0) / 2147483648 - 1;
  };
  const left = hash(cell);
  return left + (hash(cell + 1) - left) * t;
}

export class ParticleLife {
  constructor(capacity = 12000, seed = 908) {
    this.capacity = capacity;
    this.seed = seed;
    this.cursor = this.count = this.bornTotal = 0;
    this.time = null;
    this.flowTime = 0;
    this.credit = 0;
    this.parameters = { ...defaults };
    for (const name of ['birth', 'loss', 'band', 'character', 'direction', 'elevation', 'radius', 'weight', 'lateGroup'])
      this[name] = new Float64Array(capacity);
    this.alive = new Uint8Array(capacity);
    this.position = new Float32Array(capacity * 3);
    this.color = new Float32Array(capacity * 3);
    this.opacity = new Float32Array(capacity);
    this.pointSize = new Float32Array(capacity);
    this.wet = new Float64Array(4);
    this.spectrum = new Float64Array(4);
    this.stereoWidth = 1;
    this.metrics = { dryRms: 0, wetRms: 0, pan: 0, wetPan: 0, correlation: 0 };
    this.motion = true;
  }

  random() { this.seed = (1664525 * this.seed + 1013904223) >>> 0; return this.seed / 4294967296; }
  clear() {
    this.alive.fill(0); this.opacity.fill(0); this.count = this.credit = this.cursor = 0;
    this.time = null; this.wet.fill(0); this.spectrum.fill(0);
    this.metrics = { dryRms: 0, wetRms: 0, pan: 0, wetPan: 0, correlation: 0 };
  }

  ingest(meters, p) {
    this.metrics = meters;
    if (!Number.isFinite(meters.dryRms) || meters.dryRms < .00008) return;
    const duration = clamp(meters.duration, 0, .05);
    this.credit += Math.min(10000, 21000 * Math.sqrt(meters.dryRms)) * duration * Math.min(1, this.capacity / 9000);
    const count = Math.min(350, 2 * Math.floor(this.credit / 2));
    this.credit -= count;
    const total = meters.dry.reduce((sum, x) => sum + Math.max(0, x), 0);
    if (total < 1e-9) return;
    for (let n = 0; n < count; n += 2) {
      let selection = this.random() * total, band = 0;
      while (band < 3 && selection > meters.dry[band]) selection -= meters.dry[band++];
      const lateGroup = this.random() > .24 ? 1 : 0;
      const attack = p.attack / 100;
      const radius = .035 + .86 * this.random() ** .72;
      const arrival = lateGroup ? .035 + .16 * attack * attack * (.4 + .6 * this.random()) : 0;
      const birth = meters.time - duration * this.random() + p.predelay / 1000 + .002 + arrival + radius * .085;
      const character = this.random(), direction = this.random() * Math.PI * 2;
      const elevation = this.random() * 2 - 1;
      const weight = .65 + this.random() * .35;
      const pointSize = 1.15 + this.random() ** 2 * 1.65;
      const color = frequencyColor(band, character * .23);
      // 从中心向两侧成对唤亮原位尘埃，避免随机抽样或声像统计带动整个声场横移。
      for (let side = 0; side < 2; side++) {
        const i = this.cursor;
        this.cursor = (this.cursor + 1) % this.capacity;
        if (!this.alive[i]) this.count++;
        this.alive[i] = 1; this.opacity[i] = 0; this.bornTotal++;
        this.band[i] = band; this.lateGroup[i] = lateGroup;
        this.birth[i] = birth; this.loss[i] = 0;
        this.character[i] = character;
        this.direction[i] = direction + side * Math.PI;
        this.elevation[i] = elevation * (side ? -1 : 1);
        this.radius[i] = radius; this.weight[i] = weight;
        this.pointSize[i] = pointSize;
        for (let c = 0; c < 3; c++) this.color[i * 3 + c] = color[c];
      }
    }
  }

  update(time, target) {
    if (!Number.isFinite(time)) return;
    const dt = this.time === null ? 0 : Math.max(0, time - this.time);
    this.time = time;
    const smoothing = 1 - Math.exp(-dt / .08);
    for (const key of Object.keys(defaults)) this.parameters[key] += (target[key] - this.parameters[key]) * smoothing;
    const p = this.parameters;
    this.flowTime += dt * (.09 + p.rate * .055);
    const wet = this.metrics.wet || [0, 0, 0, 0];
    for (let b = 0; b < 4; b++) {
      const level = Number.isFinite(wet[b]) ? Math.max(0, wet[b]) : 0;
      const follow = level > this.wet[b] ? .035 : .12;
      this.wet[b] += (level - this.wet[b]) * (1 - Math.exp(-dt / follow));
      const spectrum = clamp((this.wet[b] / .04) ** .55, 0, 1);
      this.spectrum[b] += (spectrum - this.spectrum[b]) * (1 - Math.exp(-dt / .15));
    }
    const size = .45 + .0055 * p.size;
    const attack = p.attack / 100;
    const late = p.late / 100, early = p.early / 100;
    const width = .93 + .07 * Math.sqrt(clamp((1 - this.metrics.correlation) * .5, 0, 1));
    this.stereoWidth += (width - this.stereoWidth) * (1 - Math.exp(-dt / .65));
    const modulation = this.motion ? p.depth * .00012 : 0;
    for (let i = 0; i < this.capacity; i++) {
      if (!this.alive[i]) continue;
      const age = time - this.birth[i];
      if (age < 0) { this.opacity[i] = 0; continue; }
      const band = this.band[i], freq = bandFrequencies[band];
      const bass = 1 / (1 + (freq / p.bassHz) ** 4);
      const high = 1 / (1 + (p.highHz / freq) ** 4);
      const rt = Math.max(.12, p.decay * (1 + (p.bassMult - 1) * bass) / (1 + (-p.highDb / 7) * high));
      // 积分已有能量的损失，改变衰减时不重置年龄或恢复已经消失的粒子。
      this.loss[i] += 60 * Math.min(dt, age) / rt;
      if (this.loss[i] > 68) { this.alive[i] = 0; this.opacity[i] = 0; this.count--; continue; }
      const seed = this.character[i], angle = this.direction[i];
      const isLate = this.lateGroup[i];
      const diffusion = isLate ? late : early;
      const cluster = Math.round(angle / (Math.PI / 4)) * (Math.PI / 4);
      const theta = cluster + (angle - cluster) * diffusion;
      const elevation = this.elevation[i];
      const horizontal = Math.sqrt(Math.max(0, 1 - elevation * elevation));
      const spread = .78 + diffusion * .22;
      // 尘埃已悬浮在三维空间；生命期间只缓慢向外移动很短一段，不从喷口飞向边界。
      const expansion = this.motion ? .11 * (1 - Math.exp(-age * .24)) : 0;
      const radius = this.radius[i] + expansion * (.4 + .6 * seed)
        + (this.motion ? .025 * this.spectrum[band] * (1 - this.radius[i]) : 0);
      const offset = i * 3;
      const sign = i % 2 ? -1 : 1;
      const driftSeed = Math.floor(seed * 2147483647);
      const flow = this.flowTime + seed * 32;
      const depth = isLate ? 1 + attack * .16 : .68;
      this.position[offset] = (Math.cos(theta) * horizontal * radius * spread
        + sign * driftNoise(flow, driftSeed) * modulation) * 4.85 * size * this.stereoWidth;
      this.position[offset + 1] = (elevation * radius
        + sign * driftNoise(flow + 11, driftSeed ^ 713) * modulation) * 2.8 * size;
      this.position[offset + 2] = (Math.sin(theta) * horizontal * radius
        + sign * driftNoise(flow + 23, driftSeed ^ 1987) * modulation) * 3.25 * size * depth;
      const entry = smooth(clamp(age / (.15 + this.radius[i] * .1), 0, 1));
      const groupGain = isLate ? Math.sqrt(.7 + .6 * attack) : .18 + 1.62 * (1 - attack) ** .7;
      const bandGain = (this.wet[band] / .024) ** .5;
      const lossGain = 10 ** (-this.loss[i] / 95);
      const edge = 1 - smooth(clamp((this.radius[i] - .5) / .5, 0, 1)) * .72;
      // 观察的是混合前的纯湿声；声音 MIX 不隐藏供比较的分析信号。
      const energy = entry * lossGain * this.weight[i] * bandGain * groupGain * edge;
      this.opacity[i] = .94 * (1 - Math.exp(-energy * 1.35));
    }
  }
}
