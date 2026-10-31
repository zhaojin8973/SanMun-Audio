/**
 * 商业链路的单一状态源。
 * 安装包、公证、授权和正式下载地址全部确认后，再在这里接入真实链接。
 */
export const releaseStatus = {
  zh: '安装包尚未开放',
  en: 'Installers are not open yet',
} as const;

export const downloads = {
  macos: '',
  windows: '',
  repo: '',
} as const;
