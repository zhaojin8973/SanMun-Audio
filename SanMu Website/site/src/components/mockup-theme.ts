// 插件示意 SVG 的双主题调色板（Light/Dark），供 EQ6 / Comp 共用
// 依据用户实拍截图 + JuCE 实现 token；新增主题在此扩展
export const mockupPalettes = {
  light: {
    bg1: '#FBFDFC',
    bg2: '#EEF3F1',
    edge: 'rgba(148,163,184,0.35)',
    grid: 'rgba(148,163,184,0.28)',
    ink: '#1F2937',
    ink2: '#64748B',
    ink3: '#94A3B8',
    track: '#E8EEEC',
    pill: '#fff',
    pillEdge: 'rgba(148,163,184,0.35)',
    toolBg: '#EEF3F1',
    curve: '#1F2937',
    glow: 'rgba(52,211,153,0.25)',
    logoBox: '#EFF6F3',
    logoEdge: 'rgba(52,211,153,0.4)',
    logoText: '#0f766e',
    fft: 1,
  },
  dark: {
    bg1: '#23262B',
    bg2: '#121418',
    edge: 'rgba(255,255,255,0.14)',
    grid: 'rgba(255,255,255,0.10)',
    ink: '#F1F5F9',
    ink2: '#9DA6B3',
    ink3: '#6B7683',
    track: '#2E3237',
    pill: '#1B1E23',
    pillEdge: 'rgba(255,255,255,0.16)',
    toolBg: '#1B1E23',
    curve: '#E5EAF0',
    glow: 'rgba(52,211,153,0.30)',
    logoBox: '#101418',
    logoEdge: 'rgba(52,211,153,0.45)',
    logoText: '#34D399',
    fft: 1,
  },
};

export type MockupTheme = 'light' | 'dark';
