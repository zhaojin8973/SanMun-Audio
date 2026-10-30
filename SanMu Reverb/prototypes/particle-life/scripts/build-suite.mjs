import { spawnSync } from 'node:child_process';
import { readFileSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const root = fileURLToPath(new URL('..', import.meta.url));
const result = spawnSync(process.env.SANMU_EMXX || 'em++', [
  'dsp/suite-bridge.cpp', '-std=c++20', '-O3', '--no-entry',
  '-sMODULARIZE=1', '-sEXPORT_ES6=1', '-sENVIRONMENT=web,worker',
  '-sWASM_ASYNC_COMPILATION=1', '-sFILESYSTEM=0', '-sALLOW_MEMORY_GROWTH=0',
  '-sINITIAL_MEMORY=67108864', '-sSTACK_SIZE=262144', '-sMALLOC=emmalloc',
  '-sEXPORTED_FUNCTIONS=["_suite_init","_suite_reset","_suite_buffer","_suite_parameters","_suite_commit","_suite_process"]',
  '-sEXPORTED_RUNTIME_METHODS=["HEAPF32"]', '-o', 'public/audio/suite-dsp.js',
], { cwd: root, stdio: 'inherit' });
if (result.error) console.error('四类试听模块编译失败：', result.error.message);
if (result.status === 0) {
  const files = ['hall/HallField.h', 'hall/CathedralField.h', 'plate/PlateField.h', 'plate/SteelField.h',
    'room/RoomField.h', 'chamber/ChamberField.h'].map(file => `../../source/dsp/engines/${file}`);
  files.push('dsp/suite-bridge.cpp', 'public/audio/suite-dsp.js', 'public/audio/suite-dsp.wasm');
  const hashes = Object.fromEntries(files.map(file => [file, createHash('sha256').update(readFileSync(path.join(root, file))).digest('hex')]));
  writeFileSync(path.join(root, 'public/audio/suite-build.json'), JSON.stringify({ builtAt: new Date().toISOString(), hashes }, null, 2));
}
process.exit(result.status ?? 1);
