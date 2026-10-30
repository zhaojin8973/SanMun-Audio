import { build, transform as transformJs } from 'esbuild';
import { transform as transformCss } from 'lightningcss';
import { readFile, writeFile, mkdir, readdir, copyFile, rm } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { parse } from 'acorn';
import { lowerLegacyCss } from './legacy-css.mjs';
const tools = path.dirname(fileURLToPath(import.meta.url));

async function walk(directory) {
  const items = [];
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    const name = path.join(directory, entry.name);
    if (entry.isDirectory()) items.push(...await walk(name)); else items.push(name);
  }
  return items;
}

// 原生发布统一以 Safari 13 和 ES2019 为上限，编译失败不得回退到原始脚本。
export async function prepareCompatibility(directory) {
  await build({ entryPoints: [path.join(tools, 'runtime.js')], bundle: true, format: 'iife',
    target: ['safari13', 'es2019'], outfile: path.join(directory, 'sanmu-compat.js'), minify: false });
  await copyFile(path.join(tools, 'runtime.css'), path.join(directory, 'sanmu-compat.css'));
  await copyFile(path.join(tools, 'node_modules/dialog-polyfill/dialog-polyfill.css'), path.join(directory, 'sanmu-dialog-base.css'));
  const files = await walk(directory);
  for (const filename of files) {
    if (filename.endsWith('.js')) {
      const output = await transformJs(await readFile(filename, 'utf8'), { target: ['safari13', 'es2019'],
        sourcefile: path.basename(filename), legalComments: 'inline' });
      parse(output.code, { ecmaVersion: 2019, sourceType: 'module' });
      await writeFile(filename, output.code);
    } else if (filename.endsWith('.css')) {
      const output = transformCss({ filename, code: Buffer.from(lowerLegacyCss(await readFile(filename, 'utf8'), filename)), targets: { safari: 13 << 16 },
        minify: false, errorRecovery: false });
      await writeFile(filename, output.code);
    } else if (filename.endsWith('.html')) {
      let html = await readFile(filename, 'utf8');
      html = html.replace('</head>', '<link rel="stylesheet" href="./sanmu-compat.css"></head>');
      // 放在 head 内同步执行，始终先于模块入口；body 还未生成时由观察器补注册。
      html = html.replace('<head>', '<head><script src="./sanmu-compat.js"></script><link rel="stylesheet" href="./sanmu-dialog-base.css">');
      await writeFile(filename, html);
    }
  }
}
export function archiveUi(directory, output) {
  if (process.platform === 'win32') {
    const source = directory.replace(/'/g, "''");
    const destination = output.replace(/'/g, "''");
    execFileSync('powershell.exe', [
      '-NoProfile', '-NonInteractive', '-Command',
      `Compress-Archive -Path '${source}\\*' -DestinationPath '${destination}' -Force`,
    ], { stdio: 'inherit' });
    return;
  }
  execFileSync('/usr/bin/ditto', ['-c', '-k', directory, output], { stdio: 'inherit' });
}
async function main() {
  const [source, archive] = process.argv.slice(2).map(p => path.resolve(p));
  if (!source || !archive) throw new Error('必须提供界面目录与输出归档');
  const directory = path.join(path.dirname(archive), path.basename(archive, '.zip') + '-compat');
  await rm(directory, { recursive: true, force: true }); await mkdir(directory, { recursive: true });
  for (const filename of await walk(source)) {
    if (!['.html', '.css', '.json', '.svg', '.png', '.woff', '.woff2'].includes(path.extname(filename))) continue;
    if (path.basename(filename) === 'preview.html') continue;
    const output = path.join(directory, path.relative(source, filename));
    await mkdir(path.dirname(output), { recursive: true }); await copyFile(filename, output);
  }
  await build({ entryPoints: [path.join(source, 'app.js')], bundle: true, platform: 'browser', format: 'iife',
    target: ['safari13', 'es2019'], outfile: path.join(directory, 'app.js') });
  const index = path.join(directory, 'index.html');
  await writeFile(index, (await readFile(index, 'utf8')).replace('type="module" src="app.js"', 'defer src="app.js"'));
  await prepareCompatibility(directory); await rm(archive, { force: true }); archiveUi(directory, archive);
  console.log('已生成旧版 WebKit 兼容界面：' + archive);
}
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) await main();
