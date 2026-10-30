import createHall from './hall-dsp.js';

class HallProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super();
    const compiled = options.processorOptions.module;
    const loading = createHall({
      instantiateWasm(imports, receive) {
        const instance = new WebAssembly.Instance(compiled, imports);
        receive(instance, compiled);
        return instance.exports;
      },
    });
    this.lastParameters = options.processorOptions.parameters;
    this.mix = this.mixTarget = options.processorOptions.mix / 100;
    this.mixStep = this.mixRemaining = 0;
    this.poles = new Float64Array(12);
    this.coefficients = [240, 1400, 5000].map(hz => 1 - Math.exp(-2 * Math.PI * hz / sampleRate));
    this.energy = new Float64Array(8);
    this.count = this.dryL = this.dryR = this.wetL = this.wetR = this.cross = this.peak = 0;
    this.port.onmessage = ({ data }) => {
      if (data.type === 'parameters') {
        this.setParameters(data.values);
        this.mixTarget = Math.min(1, Math.max(0, data.mix / 100));
        this.mixRemaining = Math.max(1, Math.floor(sampleRate * .02));
        this.mixStep = (this.mixTarget - this.mix) / this.mixRemaining;
      } else if (data.type === 'reset' && this.dsp) {
        this.dsp._hall_reset();
        this.setParameters(this.lastParameters);
        this.poles.fill(0);
        this.energy.fill(0);
        this.count = this.dryL = this.dryR = this.wetL = this.wetR = this.cross = this.peak = 0;
      }
    };
    loading.then(dsp => {
      this.dsp = dsp;
      dsp._hall_init(sampleRate);
      const offset = dsp._hall_buffer() / 4;
      this.buffers = Array.from({ length: 4 }, (_, i) => dsp.HEAPF32.subarray(offset + i * 128, offset + (i + 1) * 128));
      this.parameters = dsp.HEAPF32.subarray(dsp._hall_parameters() / 4, dsp._hall_parameters() / 4 + 14);
      this.setParameters(this.lastParameters);
      this.port.postMessage({ type: 'ready' });
    }).catch(error => this.port.postMessage({ type: 'failure', message: error.message }));
  }

  setParameters(values) {
    this.lastParameters = values;
    if (!this.dsp) return;
    this.parameters.set(values);
    this.dsp._hall_commit();
  }

  measure(x, channel, wet) {
    const base = (wet * 2 + channel) * 3;
    const offset = wet * 4;
    let previous = 0;
    for (let band = 0; band < 3; band++) {
      const position = base + band;
      this.poles[position] += this.coefficients[band] * (x - this.poles[position]);
      const value = this.poles[position] - previous;
      this.energy[offset + band] += value * value;
      previous = this.poles[position];
    }
    this.energy[offset + 3] += (x - previous) ** 2;
  }

  process(inputs, outputs) {
    const input = inputs[0];
    const output = outputs[0];
    if (!output?.[0] || !this.dsp) return true;
    const n = output[0].length;
    if (n > 128) { output.forEach(channel => channel.fill(0)); return true; }
    if (input?.[0]) {
      this.buffers[0].set(input[0]);
      this.buffers[1].set(input[1] || input[0]);
    } else { this.buffers[0].fill(0); this.buffers[1].fill(0); }
    this.dsp._hall_process(n);
    for (let i = 0; i < n; i++) {
      const left = this.buffers[0][i], right = this.buffers[1][i];
      const wetLeft = this.buffers[2][i], wetRight = this.buffers[3][i];
      if (this.mixRemaining > 0) this.mix = --this.mixRemaining === 0 ? this.mixTarget : this.mix + this.mixStep;
      const wetGain = Math.sin(this.mix * Math.PI / 2), dryGain = Math.cos(this.mix * Math.PI / 2);
      output[0][i] = left * dryGain + wetLeft * wetGain;
      if (output[1]) output[1][i] = right * dryGain + wetRight * wetGain;
      this.dryL += left * left; this.dryR += right * right;
      this.wetL += wetLeft * wetLeft; this.wetR += wetRight * wetRight;
      this.cross += wetLeft * wetRight;
      this.peak = Math.max(this.peak, Math.abs(left), Math.abs(right));
      this.measure(left, 0, 0); this.measure(right, 1, 0);
      this.measure(wetLeft, 0, 1); this.measure(wetRight, 1, 1);
    }
    this.count += n;
    // 缩短观察统计窗，让原声层及时保留起音；混响音频计算不变。
    if (this.count >= 512) {
      this.port.postMessage({
        type: 'meters', time: currentTime + n / sampleRate, duration: this.count / sampleRate,
        dry: Array.from(this.energy.subarray(0, 4), x => Math.sqrt(x / (2 * this.count))),
        wet: Array.from(this.energy.subarray(4, 8), x => Math.sqrt(x / (2 * this.count))),
        dryRms: Math.sqrt((this.dryL + this.dryR) / (2 * this.count)),
        wetRms: Math.sqrt((this.wetL + this.wetR) / (2 * this.count)), peak: this.peak,
        pan: (this.dryR - this.dryL) / (this.dryR + this.dryL + 1e-12),
        wetPan: (this.wetR - this.wetL) / (this.wetR + this.wetL + 1e-12),
        correlation: this.cross / Math.sqrt(this.wetL * this.wetR + 1e-24),
      });
      this.energy.fill(0);
      this.count = this.dryL = this.dryR = this.wetL = this.wetR = this.cross = this.peak = 0;
    }
    return true;
  }
}
registerProcessor('sanmu-hall', HallProcessor);
