import { getProduct, type Lang } from './products';

// 从原始音乐播放实录中选取视野；网页只裁切显示区域，不重绘或拉伸 UI。
export const detailRegions = {
  'eq-spectrum': { slug:'eq6', theme:'dark', x:95, y:226, width:1500, height:620, zh:'频谱与动态频段', en:'Spectrum and dynamic bands' },
  'eq-dynamics': { slug:'eq6', theme:'light', x:510, y:1065, width:825, height:185, zh:'动态阈值与增益衰减', en:'Dynamic threshold and gain reduction' },
  'eq-character': { slug:'eq6', theme:'light', x:35, y:121, width:530, height:90, zh:'Clean、Tube、Discrete 音色', en:'Clean, Tube and Discrete character' },
  'comp-history': { slug:'comp', theme:'light', x:76, y:230, width:1450, height:365, zh:'输入、输出与增益衰减曲线', en:'Input, output and gain-reduction history' },
  'comp-modes': { slug:'comp', theme:'light', x:28, y:127, width:492, height:86, zh:'LEV、VCA、FET、Opto 模式', en:'LEV, VCA, FET and Opto modes' },
  'comp-controls': { slug:'comp', theme:'light', x:472, y:680, width:904, height:350, zh:'压缩比、启动与释放', en:'Ratio, attack and release' },
  'delay-time': { slug:'delay', theme:'light', x:35, y:184, width:1315, height:297, zh:'左右声道时间编辑器', en:'Left and right time editor' },
  'delay-routing': { slug:'delay', theme:'light', x:1230, y:96, width:475, height:144, zh:'延迟路由与时间同步', en:'Delay routing and time sync' },
  'delay-ducking': { slug:'delay', theme:'light', x:802, y:923, width:590, height:225, zh:'Ducking 触发与释放', en:'Ducking trigger and release' },
  'reverb-field': { slug:'reverb', theme:'dark', x:185, y:280, width:1595, height:510, zh:'混响声场响应', en:'Reverb sound-field response' },
  'reverb-time': { slug:'reverb', theme:'light', x:132, y:808, width:1750, height:305, zh:'Pre-delay、Decay 与空间大小', en:'Pre-delay, decay and space size' },
  'reverb-controls': { slug:'reverb', theme:'dark', x:44, y:1105, width:2042, height:250, zh:'阻尼、扩散、调制与输出均衡', en:'Damping, diffusion, modulation and output EQ' },
} as const;

export type DetailKey = keyof typeof detailRegions;
export function getDetail(key:DetailKey, lang:Lang) {
  const region=detailRegions[key];
  const product=getProduct(region.slug);
  const dark=region.theme==='dark';
  return { ...region, name:product.name, label:region[lang], src:dark?product.darkImage:product.image, sourceWidth:dark?product.darkImageWidth:product.imageWidth, sourceHeight:dark?product.darkImageHeight:product.imageHeight };
}
