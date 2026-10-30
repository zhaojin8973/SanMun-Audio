import postcss from 'postcss';
import values from 'postcss-value-parser';

// 发布资源使用同一套数值配色，旧、新引擎都不依赖 color-mix 的实现差异。
const channelNames = ['r', 'g', 'b', 'a'];
const channel = (name, index) => `var(${name}-${channelNames[index]})`;
const splitArguments = nodes => {
  const result = [[]];
  for (const node of nodes) {
    if (node.type === 'div' && node.value === ',') result.push([]);
    else if (node.type !== 'space' && node.type !== 'comment') result[result.length - 1].push(node);
  }
  return result;
};
const variable = node => node?.type === 'function' && node.value === 'var'
  ? values.stringify(node.nodes).trim().split(',')[0] : null;
const number = value => String(Number(Number(value).toFixed(8)));

function literalChannels(value) {
  const text = value.trim();
  if (text === 'transparent') return ['0', '0', '0', '0'];
  if (/^#[\da-f]{3,8}$/i.test(text)) {
    let hex = text.slice(1);
    if (hex.length === 3 || hex.length === 4) hex = [...hex].map(c => c + c).join('');
    if (hex.length === 6 || hex.length === 8) return [0, 2, 4].map(i => String(parseInt(hex.slice(i, i + 2), 16)))
      .concat(hex.length === 8 ? number(parseInt(hex.slice(6), 16) / 255) : '1');
  }
  const triplet = text.match(/^([\d.]+)\s+([\d.]+)\s+([\d.]+)$/);
  if (triplet) return [...triplet.slice(1), '1'];
  const rgb = text.match(/^rgba?\(\s*([\d.]+)[,\s]+([\d.]+)[,\s]+([\d.]+)(?:\s*[,/]\s*([\d.]+))?\s*\)$/);
  if (rgb) return [rgb[1], rgb[2], rgb[3], rgb[4] || '1'];
  const alias = text.match(/^var\((--[\w-]+)\)$/);
  if (alias) return channelNames.map((_, i) => channel(alias[1], i));
  const rgbAlias = text.match(/^rgba?\(var\((--[\w-]+)\)(?:\s*\/\s*(.+))?\)$/);
  if (rgbAlias) return [0, 1, 2].map(i => channel(rgbAlias[1], i)).concat(rgbAlias[2] || '1');
  return null;
}

function colourPart(nodes) {
  const text = values.stringify(nodes).trim();
  const name = variable(nodes[0]);
  const transparent = nodes[0]?.type === 'word' && nodes[0].value === 'transparent';
  const channels = !name && !transparent ? literalChannels(text) : null;
  if (!name && !transparent && !channels) throw new Error(`兼容配色不支持此颜色：${text}`);
  const percentage = nodes[1] ? Number(nodes[1].value.replace('%', '')) / 100 : null;
  if (nodes[1] && !nodes[1].value.endsWith('%') || nodes.length > 2) throw new Error('兼容配色权重格式不正确');
  return { name, transparent, channels, percentage };
}

const partChannel = (part, index) => part.name ? channel(part.name, index) : part.channels[index];

// CSS Color 5 的 sRGB 混合使用预乘 alpha。这里保留归一化权重和总权重
// 两个步骤，避免“70% + 70%”等权重在旧 WebKit 中变成过亮或过暗的颜色。
function mixedChannels(first, second, firstWeight, secondWeight) {
  const total = firstWeight + secondWeight;
  if (!(total > 0)) throw new Error('兼容配色权重不能全部为零');
  const w1 = number(firstWeight / total), w2 = number(secondWeight / total);
  const scale = number(Math.min(1, total));
  if (first.transparent && second.transparent) return ['0', '0', '0', '0'];
  if (second.transparent || first.transparent) {
    const part = second.transparent ? first : second;
    const weight = second.transparent ? w1 : w2;
    return [0, 1, 2].map(i => partChannel(part, i))
      .concat(`calc(${partChannel(part, 3)} * ${weight} * ${scale})`);
  }
  const a1 = partChannel(first, 3), a2 = partChannel(second, 3);
  const alpha = `(${a1} * ${w1} + ${a2} * ${w2})`;
  // max() is supported by Safari 11 and prevents 0/0 when both source
  // colours are fully transparent. The branch has no visible colour then,
  // so the tiny epsilon has no effect on rendered pixels.
  const denominator = `max(${alpha}, 0.000001)`;
  return [0, 1, 2]
    .map(i => `calc((${partChannel(first, i)} * ${a1} * ${w1} + ${partChannel(second, i)} * ${a2} * ${w2}) / ${denominator})`)
    .concat(`calc(${alpha} * ${scale})`);
}

function lowerColours(text, used) {
  const ast = values(text);
  ast.walk(node => {
    if (node.type !== 'function') return;
    if (node.value === 'color-mix') {
      const args = splitArguments(node.nodes);
      if (args.length !== 3 || values.stringify(args[0]).replace(/\s/g, '') !== 'insrgb')
        throw new Error('发布配色只允许明确的 srgb 混合');
      const first = colourPart(args[1]), second = colourPart(args[2]);
      for (const part of [first, second]) if (part.name) used.add(part.name);
      const firstWeight = first.percentage ?? (second.percentage === null ? .5 : 1 - second.percentage);
      const secondWeight = second.percentage ?? (1 - firstWeight);
      const parts = mixedChannels(first, second, firstWeight, secondWeight);
      node.value = 'rgba'; node.nodes = values(parts.join(', ')).nodes;
      return false;
    }
    if (node.value === 'rgb' || node.value === 'rgba') {
      const args = splitArguments(node.nodes);
      if (args.length !== 1) return;
      const name = variable(args[0][0]);
      if (name) {
        used.add(name);
        const slash = args[0].findIndex(n => n.type === 'div' && n.value === '/');
        const alpha = slash >= 0 ? values.stringify(args[0].slice(slash + 1)).trim() : '1';
        node.value = 'rgba'; node.nodes = values([0, 1, 2].map(i => channel(name, i)).concat(alpha).join(', ')).nodes;
        return false;
      }
    }
  });
  return ast.toString();
}

export function lowerLegacyCss(source, filename) {
  const ast = postcss.parse(source, { from: filename });
  const used = new Set();
  const definitions = [];
  ast.walkRules(rule => {
    if (rule.selector.includes(':where(')) {
      throw new Error(`${filename}: 不自动展开 :where()（会改变 CSS 特异性）`);
    }
  });
  ast.walkDecls(declaration => {
    if (declaration.prop.startsWith('--')) definitions.push({ declaration, original: declaration.value });
    declaration.value = lowerColours(declaration.value, used);
    if (declaration.prop === 'gap') {
      const gaps = values(declaration.value).nodes.filter(node => node.type !== 'space');
      if (gaps.length <= 2) {
        const row = values.stringify([gaps[0]]), column = values.stringify([gaps[1] || gaps[0]]);
        declaration.cloneBefore({ prop: '--sanmu-flex-gap-x', value: column });
        declaration.cloneBefore({ prop: '--sanmu-flex-gap-y', value: row });
      }
    } else if (declaration.prop === 'row-gap' || declaration.prop === 'column-gap') {
      declaration.cloneBefore({ prop: declaration.prop === 'row-gap' ? '--sanmu-flex-gap-y' : '--sanmu-flex-gap-x', value: declaration.value });
    }
  });
  // 通道必须跟随原变量所在规则，主题、类型与悬停状态继承才保持一致。
  let previous = -1;
  while (previous !== used.size) {
    previous = used.size;
    for (const entry of definitions) if (used.has(entry.declaration.prop)) {
      for (const match of entry.original.matchAll(/var\((--[\w-]+)\)/g)) used.add(match[1]);
    }
  }
  for (const entry of definitions) {
    const components = literalChannels(entry.original);
    if (!components && !used.has(entry.declaration.prop)) continue;
    if (!components) throw new Error(`${filename}: 无法转换配色变量 ${entry.declaration.prop}=${entry.original}`);
    components.forEach((value, i) => entry.declaration.cloneBefore({ prop: `${entry.declaration.prop}-${channelNames[i]}`, value }));
  }
  return ast.toString();
}
