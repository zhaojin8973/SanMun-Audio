import { dspOrder } from './parameters.js';

export class AudioEngine {
  constructor(parameters, onMetrics, onState, onError, configuration = {}) {
    this.parameters = parameters;
    this.onMetrics = onMetrics;
    this.onState = onState;
    this.onError = onError;
    this.loop = true;
    this.sourceId = 'chord';
    this.volume = .55;
    this.muted = false;
    this.buffers = new Map();
    this.revision = 0;
    this.configuration = configuration;
  }

  async prepare() {
    if (this.ready) { await this.context.resume(); return; }
    if (this.preparing) return this.preparing;
    this.preparing = this.initialize();
    try { await this.preparing; } finally { this.preparing = null; }
  }

  async initialize() {
    try {
      this.context = new AudioContext({ latencyHint: 'interactive' });
      await this.context.resume();
      const response = await fetch(this.configuration.module || '/audio/hall-dsp.wasm');
      if (!response.ok) throw new Error('混响模块未能载入');
      const module = await WebAssembly.compile(await response.arrayBuffer());
      await this.context.audioWorklet.addModule(this.configuration.worklet || '/audio/hall-worklet.js');
      this.worklet = new AudioWorkletNode(this.context, this.configuration.processor || 'sanmu-hall', {
        numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2],
        channelCount: 2, channelCountMode: 'explicit',
        processorOptions: { module, parameters: dspOrder.map(key => this.parameters[key]), mix: this.parameters.mix,
          ...this.configuration.state?.() },
      });
      this.output = this.context.createGain();
      this.output.gain.value = this.muted ? 0 : this.volume;
      this.worklet.connect(this.output).connect(this.context.destination);
      await new Promise((resolve, reject) => {
        const timeout = setTimeout(() => reject(new Error('混响模块启动超时，请重新播放。')), 8000);
        const fail = error => {
          clearTimeout(timeout);
          if (this.ready) {
            this.stop();
            void this.disposeGraph();
            this.onError(error.message);
          } else reject(error);
        };
        this.worklet.port.onmessage = ({ data }) => {
          if (data.type === 'ready') { clearTimeout(timeout); resolve(); }
          else if (data.type === 'failure') fail(new Error(`混响模块启动失败：${data.message}`));
          else if (data.type === 'meters') { this.latest = data; this.onMetrics(data); }
        };
        this.worklet.onprocessorerror = () => {
          fail(new Error('声音处理已停止，请重新播放。'));
        };
      });
      this.ready = true;
    } catch (error) {
      await this.disposeGraph();
      throw error;
    }
  }

  async disposeGraph() {
    const { context, source, sourceGain, worklet, output } = this;
    // 先清空公开状态，关闭上下文期间的控件操作也不能访问旧节点。
    this.ready = false;
    this.context = this.source = this.sourceGain = this.worklet = this.output = this.latest = null;
    this.buffers.clear();
    if (source) { source.onended = null; try { source.stop(); } catch { /* 音源可能已经停止。 */ } }
    if (worklet) {
      worklet.onprocessorerror = null;
      worklet.port.onmessage = null;
      worklet.port.close();
    }
    for (const node of [source, sourceGain, worklet, output]) {
      try { node?.disconnect(); } catch { /* 部分初始化的节点也需要继续清理。 */ }
    }
    try { if (context && context.state !== 'closed') await context.close(); }
    catch { /* 关闭失败不能覆盖最初的启动错误，也不能阻止重试。 */ }
  }

  setParameters() {
    this.worklet?.port.postMessage({ type: 'parameters', values: dspOrder.map(key => this.parameters[key]), mix: this.parameters.mix,
      ...this.configuration.state?.() });
  }

  setVolume(value = this.volume) {
    this.volume = value;
    if (this.output && this.context && this.context.state !== 'closed')
      this.output.gain.setTargetAtTime(this.muted ? 0 : value, this.context.currentTime, .015);
  }

  async play() {
    const revision = ++this.revision;
    await this.prepare();
    if (revision !== this.revision) return;
    this.releaseSource();
    const buffer = this.sourceId === 'file' ? this.fileBuffer : this.builtin(this.sourceId);
    if (!buffer) return;
    this.source = this.context.createBufferSource();
    this.source.buffer = buffer;
    this.source.loop = this.loop;
    this.sourceGain = this.context.createGain();
    this.sourceGain.gain.setValueAtTime(0, this.context.currentTime);
    this.sourceGain.gain.linearRampToValueAtTime(1, this.context.currentTime + .01);
    this.source.connect(this.sourceGain).connect(this.worklet);
    const source = this.source, gain = this.sourceGain;
    source.onended = () => {
      source.disconnect(); gain.disconnect();
      if (this.source === source) { this.source = null; this.onState(false); }
    };
    source.start();
    this.onState(true);
  }

  releaseSource() {
    if (!this.source) return;
    const source = this.source, gain = this.sourceGain;
    const time = this.context.currentTime;
    gain.gain.cancelScheduledValues(time);
    gain.gain.setValueAtTime(gain.gain.value, time);
    gain.gain.linearRampToValueAtTime(0, time + .015);
    source.onended = () => { source.disconnect(); gain.disconnect(); };
    source.stop(time + .02);
    this.source = null;
  }

  stop() { ++this.revision; this.releaseSource(); this.onState(false); }
  setLoop(value) { this.loop = value; if (this.source) this.source.loop = value; }
  clear() { this.worklet?.port.postMessage({ type: 'reset' }); }

  async loadFile(file) {
    if (file.size > 100 * 1024 * 1024) throw new Error('请选择小于 100 MB 的音频文件。');
    await this.prepare();
    let buffer;
    try { buffer = await this.context.decodeAudioData(await file.arrayBuffer()); }
    catch { throw new Error('无法读取这段音频，请使用 WAV、MP3、AAC 或 FLAC 文件。'); }
    this.stop();
    this.clear();
    this.fileBuffer = buffer;
    this.sourceId = 'file';
  }

  builtin(id) {
    if (this.buffers.has(id)) return this.buffers.get(id);
    const rate = this.context.sampleRate;
    const isTone = id.startsWith('tone');
    const buffer = this.context.createBuffer(2, Math.ceil(rate * 7), rate);
    let seed = 20260908;
    const noise = () => { seed = (1664525 * seed + 1013904223) >>> 0; return seed / 2147483648 - 1; };
    const starts = id === 'snare' ? [.1, 1.7, 3.3] : [.1, 2.9];
    for (let channel = 0; channel < 2; channel++) {
      const data = buffer.getChannelData(channel);
      for (const start of starts) {
        let lowNoise = 0;
        const duration = id === 'snare' ? .45 : 1.7;
        for (let i = 0; i < rate * duration; i++) {
          const t = i / rate;
          let value = 0;
          if (id === 'snare') {
            const white = noise();
            lowNoise += .15 * (white - lowNoise);
            value = .26 * (white - lowNoise) * Math.exp(-t * 25)
              + .2 * Math.sin(2 * Math.PI * 185 * t) * Math.exp(-t * 35);
          } else {
            const frequencies = isTone ? [id === 'tone-high' ? 6000 : id === 'tone-mid' ? 880 : 220] : [130.81, 196, 261.63, 329.63, 493.88];
            for (const frequency of frequencies) {
              value += Math.sin(2 * Math.PI * frequency * t);
              if (!isTone) value += .24 * Math.sin(4 * Math.PI * frequency * t) + .07 * Math.sin(8 * Math.PI * frequency * t);
            }
            value *= (isTone ? .16 : .065) * Math.min(1, t * 200) * Math.exp(-t * 4);
          }
          const index = Math.floor(start * rate) + i;
          if (index < data.length) data[index] += value;
        }
      }
    }
    this.buffers.set(id, buffer);
    return buffer;
  }
}
