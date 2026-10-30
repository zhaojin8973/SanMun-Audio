// 运行时频段颜色同时提供通道，兼容样式可以直接计算透明色而不依赖 color-mix。
export function setBandColour(element, colour) {
    const channels = colour.match(/[\d.]+/g)?.slice(0, 3).map(Number) || [0, 0, 0];
    element.style.setProperty('--band', colour);
    ['r', 'g', 'b'].forEach((channel, index) => element.style.setProperty(`--band-${channel}`, String(channels[index])));
    element.style.setProperty('--band-a', '1');
}

export function translucentColour(colour, opacity) {
    const channels = colour.match(/[\d.]+/g)?.slice(0, 3).map(Number) || [0, 0, 0];
    return `rgba(${channels.join(',')},${opacity})`;
}
