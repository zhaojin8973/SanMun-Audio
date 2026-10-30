import { spawnSync } from 'node:child_process';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const root = fileURLToPath(new URL('..', import.meta.url));
mkdirSync(path.join(root, 'public/audio'), { recursive: true });
const compiler = process.env.SANMU_EMXX || 'em++';
const result = spawnSync(compiler, [
  'dsp/bridge.cpp', '-std=c++20', '-O3', '--no-entry',
  '-sMODULARIZE=1', '-sEXPORT_ES6=1', '-sENVIRONMENT=web,worker',
  '-sWASM_ASYNC_COMPILATION=1', '-sFILESYSTEM=0', '-sALLOW_MEMORY_GROWTH=0',
  '-sINITIAL_MEMORY=33554432', '-sSTACK_SIZE=262144', '-sMALLOC=emmalloc',
  '-sEXPORTED_FUNCTIONS=["_hall_init","_hall_reset","_hall_buffer","_hall_parameters","_hall_commit","_hall_process"]',
  '-sEXPORTED_RUNTIME_METHODS=["HEAPF32"]', '-o', 'public/audio/hall-dsp.js',
], { cwd: root, stdio: 'inherit' });
if (result.error) console.error('未找到编译器，请设置 SANMU_EMXX 为 em++ 的绝对路径。', result.error.message);
if (result.status === 0) {
  const files = ['../../source/dsp/engines/hall/HallField.h', 'dsp/bridge.cpp', 'public/audio/hall-dsp.js', 'public/audio/hall-dsp.wasm'];
  const hashes = Object.fromEntries(files.map(file => [file, createHash('sha256').update(readFileSync(path.join(root, file))).digest('hex')]));
  writeFileSync(path.join(root, 'public/audio/hall-build.json'), JSON.stringify({ builtAt: new Date().toISOString(), hashes }, null, 2));
}
process.exit(result.status ?? 1);
