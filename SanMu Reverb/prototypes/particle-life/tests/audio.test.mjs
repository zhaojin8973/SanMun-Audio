import test from 'node:test';
import assert from 'node:assert/strict';
import { AudioEngine } from '../src/audio.js';
import { defaults } from '../src/parameters.js';

function setup(t, failure = '') {
  const contexts = [], nodes = [], states = [], errors = [];
  let mode = failure;
  t.mock.method(globalThis, 'fetch', async () => ({ ok: mode !== 'fetch', arrayBuffer: async () => new ArrayBuffer(0) }));
  t.mock.method(WebAssembly, 'compile', async () => ({}));
  class Node {
    constructor() {
      this.disconnected = false;
      this.gain = { value: 1, setTargetAtTime() {}, setValueAtTime() {}, linearRampToValueAtTime() {}, cancelScheduledValues() {} };
    }
    connect(target) { return target; }
    disconnect() { this.disconnected = true; }
    start() {}
    stop() { this.onended?.(); }
  }
  class Context {
    constructor() {
      this.state = 'suspended'; this.currentTime = 0; this.sampleRate = 48000;
      this.destination = new Node();
      this.audioWorklet = { addModule: async () => {} };
      contexts.push(this);
    }
    async resume() { this.state = 'running'; }
    async close() { this.state = 'closed'; if (mode === 'close') throw new Error('关闭失败'); }
    createGain() { const node = new Node(); nodes.push(node); return node; }
    createBufferSource() { return new Node(); }
  }
  class Worklet extends Node {
    constructor() {
      super(); nodes.push(this);
      this.port = { postMessage() {}, close() { this.closed = true; } };
      setImmediate(() => {
        if (mode === 'processor') this.onprocessorerror?.();
        else this.port.onmessage?.({ data: { type: ['startup', 'close'].includes(mode) ? 'failure' : 'ready', message: '测试启动失败' } });
      });
    }
  }
  for (const [name, value] of Object.entries({ AudioContext: Context, AudioWorkletNode: Worklet })) {
    const previous = Object.getOwnPropertyDescriptor(globalThis, name);
    Object.defineProperty(globalThis, name, { configurable: true, value });
    t.after(() => previous ? Object.defineProperty(globalThis, name, previous) : delete globalThis[name]);
  }
  const engine = new AudioEngine({ ...defaults }, () => {}, value => states.push(value), value => errors.push(value));
  engine.builtin = () => ({});
  return { engine, contexts, nodes, states, errors, recover() { mode = ''; } };
}

for (const failure of ['fetch', 'startup', 'processor', 'close']) {
  test(`音频失败后清理、静音、调音量与重新播放：${failure}`, async t => {
    const { engine, contexts, nodes, states, recover } = setup(t, failure);
    await assert.rejects(engine.play(), failure === 'close' ? /测试启动失败/ : /混响|声音/);
    assert(!engine.ready);
    assert.equal(engine.context, null);
    assert.equal(engine.output, null);
    assert.equal(engine.worklet, null);
    assert.equal(contexts[0].state, 'closed');
    assert(nodes.every(node => node.disconnected));
    engine.muted = true;
    assert.doesNotThrow(() => { engine.setVolume(.25); engine.setParameters(); engine.clear(); engine.stop(); });
    assert(!states.includes(true));
    recover(); await engine.play();
    assert.equal(contexts.length, 2);
    assert.equal(engine.output.gain.value, 0);
    assert.equal(states.at(-1), true);
    engine.stop();
  });
}

test('运行中处理器失效后可以重建并播放', async t => {
  const { engine, contexts, states, errors } = setup(t);
  await engine.play();
  engine.worklet.onprocessorerror();
  assert.equal(states.at(-1), false);
  assert(!engine.ready);
  assert.equal(engine.output, null);
  assert.equal(contexts[0].state, 'closed');
  assert.equal(errors.length, 1);
  assert.doesNotThrow(() => engine.setVolume(.4));
  await engine.play();
  assert.equal(contexts.length, 2);
  assert.equal(states.at(-1), true);
  engine.stop();
});

test('并发播放只初始化一次，准备中停止不会迟到播放', async t => {
  const { engine, contexts, states } = setup(t);
  const first = engine.play(), second = engine.play();
  engine.stop(); await Promise.all([first, second]);
  assert.equal(contexts.length, 1);
  assert(!states.includes(true));
  await engine.play();
  assert.equal(states.at(-1), true);
  assert.equal(contexts.length, 1);
  engine.stop();
});
