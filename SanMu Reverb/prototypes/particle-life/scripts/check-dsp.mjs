import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const root = fileURLToPath(new URL('..', import.meta.url));
try {
  for (const manifest of ['hall-build.json', 'suite-build.json']) {
    const build = JSON.parse(readFileSync(path.join(root, 'public/audio', manifest), 'utf8'));
    for (const [file, expected] of Object.entries(build.hashes)) {
      const actual = createHash('sha256').update(readFileSync(path.join(root, file))).digest('hex');
      if (actual !== expected) throw new Error(`声音模块与源码不一致：${file}`);
    }
  }
} catch (error) {
  console.error(`混响模块需要重新构建。${error.message}\n请先设置 SANMU_EMXX，再运行 npm run build:dsp 与 npm run build:suite。`);
  process.exit(1);
}
