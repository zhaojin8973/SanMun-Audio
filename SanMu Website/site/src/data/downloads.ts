/**
 * 商业链路与下载状态单一来源。
 * 价格与发售策略筹备中，暂时关闭公网直接下载。
 */
export const releaseStatus = {
  zh: '定价与发售筹备中 · 暂未开放下载',
  en: 'Pricing & Launch In Preparation · Downloads Closed',
} as const;

export const downloads = {
  macos: {
    status: 'pending_pricing',
  },
  windows: {
    status: 'pending_pricing',
  },
  repo: 'https://github.com/zhaojin8973/SanMun-Audio',
} as const;


