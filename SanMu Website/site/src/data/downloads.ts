/**
 * 商业链路与下载地址单一状态源。
 * 现已完成 Apple Developer ID 签名与官方公证、Avid PACE AAX 签名。
 */
export const releaseStatus = {
  zh: '已正式开放下载',
  en: 'Downloads Available',
} as const;

export const downloads = {
  macos: {
    tag: 'v1.0.0-macos',
    suiteZip: 'https://github.com/zhaojin8973/SanMun-Audio/releases/download/v1.0.0-macos/SanMu_Audio_macOS_Signed_Installers_Suite.zip',
    eq6: '/downloads/macos/SanMu_EQ6_v0.1.5_macOS_Signed.pkg',
    comp: '/downloads/macos/SanMu_Comp_v0.1.17_macOS_Signed.pkg',
    delay: '/downloads/macos/SanMu_Delay_v0.1.5_macOS_Signed.pkg',
    reverb: '/downloads/macos/SanMu_Reverb_v1.0.4_macOS_Signed.pkg',
    checksums: '/downloads/macos/SHA256SUMS.txt',
  },
  windows: {
    status: 'source_ready',
    repo: 'https://github.com/zhaojin8973/SanMun-Audio',
  },
  repo: 'https://github.com/zhaojin8973/SanMun-Audio',
} as const;

