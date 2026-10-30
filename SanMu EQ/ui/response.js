// 与原生 TptSvf::magnitudeDb 相同的解析响应；拖动时直接使用当前指针参数。
// 仅显示计算，音频和自动化仍由原生处理器负责。
export function magnitudeDb(type, cutoff, q, gain, frequency) {
  const linear = Math.pow(10, gain / 20), A = Math.sqrt(linear);
  let ratio = frequency / cutoff, damping = 1 / Math.max(.01, q);
  if (type === 0) damping /= A;
  if (type === 1) ratio *= Math.sqrt(A);
  if (type === 2) ratio /= Math.sqrt(A);
  const real = 1 - ratio * ratio, imaginary = damping * ratio, square = real * real + imaginary * imaginary;
  const lpR = real / square, lpI = -imaginary / square;
  const hpR = -ratio * ratio * lpR, hpI = -ratio * ratio * lpI;
  const bpR = ratio * imaginary / square, bpI = ratio * real / square;
  let re, im;
  if (type === 3) { re = hpR; im = hpI; }
  else if (type === 4) { re = lpR; im = lpI; }
  else if (type === 0) { re = 1 + (linear - 1) * damping * bpR; im = (linear - 1) * damping * bpI; }
  else { const a = damping * (A - 1), b = linear - 1; re = 1 + a * bpR + b * (type === 1 ? lpR : hpR); im = a * bpI + b * (type === 1 ? lpI : hpI); }
  return 20 * Math.log10(Math.max(1e-10, Math.hypot(re, im)));
}
const cutQ = [[.70710678], [1.306563, .541196], [1.931852, .707107, .517638], [2.562915, .899976, .601345, .509796]];
export function responseCurve(type, frequency, gain, q, slope = 0, count = 320) {
  const stages = type < 3 ? [q * (type === 0 ? 1 : .70710678)] : cutQ[slope].map((value, index) => value * (index ? 1 : q));
  return Float32Array.from({length: count}, (_, i) => stages.reduce((sum, stage) => sum + magnitudeDb(type, frequency, stage, type >= 3 ? 0 : gain, 20 * Math.pow(1000, i / (count - 1))), 0));
}
