export type ReleaseLang = 'zh' | 'en';

export const internalBeta = {
  id: 'internal-beta-20261006-catalina-ui-fix3',
  date: '2026-10-31',
  betaEndsAt: 'AGPLv3 开源正式版 · 永久无期限 / Open Source (Permanent)',
  products: [
    { slug: 'eq6', name: 'SanMu EQ6', version: '0.1.5', packageName: 'SanMu_EQ6_v0.1.5_macOS_Signed.pkg' },
    { slug: 'comp', name: 'SanMu Comp', version: '0.1.17', packageName: 'SanMu_Comp_v0.1.17_macOS_Signed.pkg' },
    { slug: 'delay', name: 'SanMu Delay', version: '0.1.5', packageName: 'SanMu_Delay_v0.1.5_macOS_Signed.pkg' },
    { slug: 'reverb', name: 'SanMu Reverb', version: '1.0.4', packageName: 'SanMu_Reverb_v1.0.4_macOS_Signed.pkg' },
  ],
  documents: [
    { zhPath: 'product-overview-zh.md', enPath: 'product-overview-en.md', zh: '产品说明', en: 'Product overview' },
    { zhPath: 'user-manual-zh.md', enPath: 'user-manual-en.md', zh: '用户手册', en: 'User manual' },
    { zhPath: 'eula-zh.md', enPath: 'eula-en.md', zh: '开源与授权条款', en: 'License & EULA' },
    { zhPath: 'release-manifest.json', enPath: 'release-manifest.json', zh: '版本清单 JSON', en: 'Release manifest JSON' },
    { zhPath: 'SHA256SUMS.txt', enPath: 'SHA256SUMS.txt', zh: 'macOS 校验值', en: 'macOS checksums' },
    { zhPath: 'signing-notarization-record-zh.md', enPath: 'signing-notarization-record-en.md', zh: '签名与公证记录', en: 'Signing and notarization record' },
    { zhPath: 'windows-build-status-zh.md', enPath: 'windows-build-status-en.md', zh: 'Windows 构建状态', en: 'Windows build status' },
  ],
} as const;
