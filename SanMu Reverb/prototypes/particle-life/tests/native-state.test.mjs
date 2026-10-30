import test from 'node:test';
import assert from 'node:assert/strict';
import { NativeStateQueue } from '../src/studio/native-state.js';

function fixture() {
  const calls = [], states = [], errors = [];
  let host = { values: { decay: 6.25, depth: 38 }, mode: 0, bypass: false };
  const queue = new NativeStateQueue(async (command, data) => {
    calls.push({ command, data: structuredClone(data) });
    if (command === 'patch') host = { ...host, ...data, values: { ...host.values, ...data.values } };
    if (command === 'apply') host = structuredClone(data);
    return command === 'begin' ? true : structuredClone(host);
  }, state => states.push(state), error => errors.push(error));
  return { queue, calls, states, errors, host };
}

test('旋钮增量保留同一时刻到来的其他参数和旁通自动化', async () => {
  const { queue, host, calls, states, errors } = fixture();
  queue.command('begin');
  host.values.decay = 4.75; host.bypass = true;
  for (let value = 39; value <= 48; value++) queue.patch({ values: { depth: value } });
  await queue.command('end');
  assert.deepEqual(calls.map(call => call.command), ['begin', 'patch', 'end']);
  assert.deepEqual(calls[1].data, { values: { depth: 48 } });
  assert.deepEqual(states, [{ values: { decay: 4.75, depth: 48 }, mode: 0, bypass: true }]);
  assert.deepEqual(errors, []); assert.equal(queue.pending, 0);
});

test('连续手势与预设恢复按顺序提交，增量不跨操作合并', async () => {
  const { queue, calls, states } = fixture();
  const preset = { values: { decay: 8, depth: 22 }, mode: 3, bypass: false };
  queue.command('begin'); queue.patch({ values: { depth: 48 } }); queue.command('end');
  queue.command('apply', preset);
  queue.command('begin'); queue.patch({ values: { decay: 2 } }); queue.command('end');
  await queue.tail;
  assert.deepEqual(calls.map(call => call.command), ['begin', 'patch', 'end', 'apply', 'begin', 'patch', 'end']);
  assert.deepEqual(calls[1].data, { values: { depth: 48 } });
  assert.deepEqual(calls[5].data, { values: { decay: 2 } });
  assert.deepEqual(states.at(-1), { values: { decay: 2, depth: 22 }, mode: 3, bypass: false });
});

test('类型与旁通操作只改变指定状态', async () => {
  const { queue, host, states } = fixture();
  host.values.decay = 5;
  queue.patch({ mode: 6 }); queue.patch({ bypass: true });
  await queue.command('end');
  assert.deepEqual(states.at(-1), { values: { decay: 5, depth: 38 }, mode: 6, bypass: true });
});

test('前一请求延迟时新值保留，失败后仍能结束手势并继续操作', async () => {
  const calls = [], errors = [];
  let release;
  const gate = new Promise(resolve => { release = resolve; });
  const queue = new NativeStateQueue(async (command, data) => {
    calls.push({ command, data: structuredClone(data) });
    if (calls.length === 1) { await gate; throw new Error('模拟连接失败'); }
    return { values: data.values || {} };
  }, () => {}, error => errors.push(error.message));
  queue.patch({ values: { depth: 40 } });
  await Promise.resolve();
  queue.patch({ values: { depth: 46 } }); queue.patch({ values: { depth: 48 } });
  queue.command('end');
  assert.equal(calls.length, 1); assert.equal(queue.pending, 3);
  release(); await queue.tail;
  assert.deepEqual(calls.map(call => call.command), ['patch', 'patch', 'end']);
  assert.equal(calls[1].data.values.depth, 48);
  assert.deepEqual(errors, ['模拟连接失败']); assert.equal(queue.pending, 0);
});
