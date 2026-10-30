import {icon as sharedIcon} from './shared/icons.js';
const paths = {
  gainMatch: '<path d="M4 8h16M4 16h16m-8-14-3 3h6l-3-3Zm0 20-3-3h6l-3 3Z"/>',
  polarity: '<circle cx="12" cy="12" r="7"/><path d="M5 21 19 3"/>',
  scan: '<path d="M8 3H3v5m13-5h5v5M3 16v5h5m13-5v5h-5M7 14l3-4 3 5 4-7"/>',
  stereo: '<circle cx="9" cy="12" r="6"/><circle cx="15" cy="12" r="6"/>',
  left: '<circle cx="9" cy="12" r="6"/><path opacity=".3" d="M13 6a6 6 0 1 1 0 12"/>',
  right: '<circle cx="15" cy="12" r="6"/><path opacity=".3" d="M11 6a6 6 0 1 0 0 12"/>',
  mid: '<path opacity=".4" d="M12 7a6 6 0 1 0 0 10 6 6 0 1 0 0-10"/><path d="M12 7c-4 2-4 8 0 10 4-2 4-8 0-10Z"/>',
  side: '<path d="M12 7a6 6 0 1 0 0 10M12 7a6 6 0 1 1 0 10"/><path opacity=".3" d="M12 7c-4 2-4 8 0 10 4-2 4-8 0-10Z"/>',
  check: '<path d="m5 12 4 4 10-10"/>',
  slope: '<path d="m3 19 12-14h6M9 19 9-10h3"/>',
  plus: '<path d="M12 5v14M5 12h14"/>',
  list: '<path d="M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01"/>',
  power: '<path d="M12 2v10M18.4 5.6a9 9 0 1 1-12.8 0"/>',
  headphones: '<path d="M3 14h3a2 2 0 0 1 2 2v3a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-7a9 9 0 0 1 18 0v7a2 2 0 0 1-2 2h-1a2 2 0 0 1-2-2v-3a2 2 0 0 1 2-2h3"/>',
  trash: '<path d="M3 6h18M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2M10 11v6M14 11v6"/>'
  ,filterBell: '<path d="M2 17h3c4 0 3-10 7-10s3 10 7 10h3"/>'
  ,filterLowShelf: '<path d="M2 7h4c6 0 5 10 11 10h5"/>'
  ,filterHighShelf: '<path d="M2 17h4c6 0 5-10 11-10h5"/>'
  ,filterLowCut: '<path d="M3 18c4 0 5-8 9-10 3-2 5-2 9-2"/>'
  ,filterHighCut: '<path d="M3 6c4 0 6 0 9 2 4 2 5 10 9 10"/>'
  ,activity: '<path d="M3 12h4l2-6 4 12 3-8 2 2h3"/>'
  ,resetGain: '<path d="M3 12h18M12 3v5m-3-3 3 3 3-3M12 21v-5m-3 3 3-3 3 3"/>'
};
export const icon = name => paths[name] ? `<svg viewBox="0 0 24 24" aria-hidden="true">${paths[name]}</svg>` : sharedIcon(name);
export function applyIcons(root = document) { root.querySelectorAll('[data-icon]').forEach(el => { el.innerHTML = icon(el.dataset.icon); }); }
