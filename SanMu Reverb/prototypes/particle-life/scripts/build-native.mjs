import { build } from 'vite';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { mkdirSync, rmSync } from 'node:fs';
import { prepareCompatibility, archiveUi } from '../../../../packaging/web/build-ui.mjs';

const root = fileURLToPath(new URL('..', import.meta.url));
if (!process.argv[2]) throw new Error('必须提供原生界面资源归档的输出路径');
const output = path.resolve(process.argv[2]);
const directory = path.join(path.dirname(output), 'native-ui-dist');
mkdirSync(path.dirname(output), { recursive: true });
await build({ root, configFile: false, base: './', publicDir: false,
  build: { outDir: directory, emptyOutDir: true, target: 'safari13',
    rolldownOptions: { input: path.join(root, 'studio.html') } } });
await prepareCompatibility(directory);
rmSync(output, { force: true });
archiveUi(directory, output);
