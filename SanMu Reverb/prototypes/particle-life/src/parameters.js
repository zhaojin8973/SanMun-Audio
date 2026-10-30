export const parameterGroups = [
  { id: 'space', name: '空间', items: [
    ['decay', '衰减', .2, 20, .01, 3.99, '秒', true],
    ['predelay', '预延迟', 0, 250, 1, 20, '毫秒'],
    ['size', '尺寸', 0, 100, 1, 100, '%'],
    ['attack', '建立', 0, 100, 1, 50, '%'],
    ['early', '早期扩散', 0, 100, 1, 100, '%'],
    ['late', '晚期扩散', 0, 100, 1, 100, '%'],
  ] },
  { id: 'tone', name: '音色', items: [
    ['bassHz', '低频分界', 50, 1000, 1, 300, '赫兹', true],
    ['bassMult', '低频倍数', .25, 4, .01, 1.5, '倍'],
    ['highHz', '高频分界', 1000, 20000, 10, 6000, '赫兹', true],
    ['highDb', '高频阻尼', -24, 0, .1, -24, '分贝'],
    ['lowCut', '低切', 10, 500, 1, 10, '赫兹', true],
    ['highCut', '高切', 1000, 20000, 10, 8010, '赫兹', true],
  ] },
  { id: 'movement', name: '调制', items: [
    ['rate', '速率', .1, 10, .01, 2.53, '赫兹', true],
    ['depth', '深度', 0, 100, 1, 38, '%'],
    ['mix', '混合', 0, 100, 1, 100, '%'],
  ] },
];
export const parameterList = parameterGroups.flatMap(group => group.items);
export const defaults = Object.fromEntries(parameterList.map(([id, , , , , value]) => [id, value]));
export const dspOrder = ['decay', 'predelay', 'highHz', 'highDb', 'bassHz', 'bassMult', 'size', 'attack', 'early', 'late', 'rate', 'depth', 'lowCut', 'highCut'];
export function normalizedValue(item, value) {
  return item[7] ? Math.log(value / item[2]) / Math.log(item[3] / item[2]) : (value - item[2]) / (item[3] - item[2]);
}
export function parameterValue(item, normalized) {
  const value = item[7] ? item[2] * (item[3] / item[2]) ** normalized : item[2] + (item[3] - item[2]) * normalized;
  return Math.min(item[3], Math.max(item[2], Math.round(value / item[4]) * item[4]));
}
export function formatValue(item, value) {
  const digits = item[4] < .1 ? 2 : item[4] < 1 ? 1 : 0;
  return `${value.toFixed(digits)}${item[6] === '%' ? '%' : ` ${item[6]}`}`;
}
