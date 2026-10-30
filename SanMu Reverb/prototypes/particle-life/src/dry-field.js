import { frequencyColor } from './life.js';

// 固定在中央的短光丝直接显示输入；没有预延迟、扩散或混响生命周期。
export class DryField {
  constructor(capacity = 1000, seed = 1908) {
    this.capacity = capacity;
    this.seed = seed;
    this.time = null;
    this.motion = true;
    this.position = new Float32Array(capacity * 6);
    this.color = new Float32Array(capacity * 6);
    this.opacity = new Float32Array(capacity * 2);
    this.base = new Float32Array(capacity * 3);
    this.tangent = new Float32Array(capacity * 3);
    this.band = new Uint8Array(capacity);
    this.weight = new Float32Array(capacity);
    this.levels = new Float64Array(4);
    this.latest = new Float64Array(4);
    this.pending = new Float64Array(4);
    this.rms = 0;
    for (let i = 0; i < capacity; i += 2) {
      const angle = this.random() * Math.PI * 2;
      const elevation = this.random() * 2 - 1;
      const horizontal = Math.sqrt(1 - elevation * elevation);
      const radius = .12 + .88 * this.random() ** .65;
      const band = (i / 2) % 4;
      const color = frequencyColor(band, this.random() * .2);
      const length = .012 + this.random() * .035;
      const weight = .5 + this.random() * .5;
      for (let side = 0; side < 2 && i + side < capacity; side++) {
        const index = i + side, sign = side ? -1 : 1;
        this.band[index] = band;
        this.weight[index] = weight;
        this.base[index * 3] = sign * Math.cos(angle) * horizontal * radius * 1.5;
        this.base[index * 3 + 1] = sign * elevation * radius * .85;
        this.base[index * 3 + 2] = sign * Math.sin(angle) * horizontal * radius * .95;
        this.tangent[index * 3] = -sign * Math.sin(angle) * length;
        this.tangent[index * 3 + 1] = sign * horizontal * length * .4;
        this.tangent[index * 3 + 2] = sign * Math.cos(angle) * length;
        for (let endpoint = 0; endpoint < 2; endpoint++) for (let c = 0; c < 3; c++)
          this.color[index * 6 + endpoint * 3 + c] = color[c] * .38 + .62;
      }
    }
  }

  random() { this.seed = (Math.imul(1664525, this.seed) + 1013904223) >>> 0; return this.seed / 4294967296; }

  ingest(metrics, retainTransient = true) {
    for (let b = 0; b < 4; b++) {
      const value = metrics.dry?.[b];
      this.latest[b] = Number.isFinite(value) ? Math.max(0, value) : 0;
      // 保留两次绘制间出现的短瞬态，防止被后到的静音帧覆盖。
      this.pending[b] = retainTransient ? Math.max(this.pending[b], this.latest[b]) : this.latest[b];
    }
    this.rms = Number.isFinite(metrics.dryRms) ? Math.max(0, metrics.dryRms) : 0;
  }

  clear() {
    this.levels.fill(0); this.latest.fill(0); this.pending.fill(0); this.opacity.fill(0);
    this.time = null; this.rms = 0;
  }

  update(time) {
    if (!Number.isFinite(time)) return;
    const dt = this.time === null ? 1 / 60 : Math.max(0, time - this.time);
    this.time = time;
    for (let b = 0; b < 4; b++) {
      const target = Math.max(this.pending[b], this.latest[b]);
      const follow = target > this.levels[b] ? .008 : .025;
      this.levels[b] += (target - this.levels[b]) * (1 - Math.exp(-dt / follow));
      this.pending[b] = 0;
    }
    for (let i = 0; i < this.capacity; i++) {
      const level = this.levels[this.band[i]];
      const strength = 1 - Math.exp(-Math.sqrt(level / .055));
      const scale = this.motion ? .92 + .08 * strength : 1;
      const length = this.motion ? .55 + 1.15 * strength : 1;
      const opacity = level < .00003 ? 0 : strength * this.weight[i] * .8;
      for (let endpoint = 0; endpoint < 2; endpoint++) {
        this.opacity[i * 2 + endpoint] = opacity;
        for (let c = 0; c < 3; c++)
          this.position[i * 6 + endpoint * 3 + c] = this.base[i * 3 + c] * scale
            + this.tangent[i * 3 + c] * length * (endpoint ? 1 : -1);
      }
    }
  }
}
