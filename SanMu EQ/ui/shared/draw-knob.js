// 取自 Reverb-2026-09-10 的旋钮绘制；仅替换参数归一化入口。
export function drawKnob() {
    const size = this.dial.clientWidth;
    if (!size) return;
    // 画布按最终显示尺寸分配像素，窗口放大时不拉伸已有位图。
    const ratio = Math.min(3, (devicePixelRatio || 1) * (this.app?.scale || 1));
    const backingSize = Math.round(size * ratio);
    if (this.canvas.width !== backingSize || this.canvas.height !== backingSize) {
      this.canvas.width = backingSize;
      this.canvas.height = backingSize;
    }
    const context = this.canvas.getContext('2d');
    context.setTransform(ratio, 0, 0, ratio, 0, 0);
    context.clearRect(0, 0, size, size);
    const style = getComputedStyle(this.container);
    const colour = name => style.getPropertyValue(name).trim();
    const centre = size / 2, radius = size * .335, ring = size * .43;
    const start = Math.PI * .75, end = Math.PI * 2.25;
    const angle = start + this.normalized() * (end - start);
    context.lineCap = 'round';
    const arc = (from, to, stroke, width) => {
      context.beginPath(); context.arc(centre, centre, ring, from, to);
      context.strokeStyle = stroke; context.lineWidth = width; context.stroke();
    };
    // 盘面保持中性材质；外圈与指针使用当前类型的旋钮色，不直接沿用胶囊颜色。
    arc(start, end, colour('--knob-track') || colour('--track'), 1.5);
    if (angle > start + .001) arc(start, angle, colour('--knob-accent') || colour('--accent'), 1.6);
    context.strokeStyle = colour('--knob-tick') || colour('--tick'); context.lineWidth = 1;
    const divisions = this.id.endsWith('_slope') ? 3 : 8;
    for (let i = 0; i <= divisions; i++) {
      const a = start + i / divisions * (end - start);
      context.beginPath();
      context.moveTo(centre + Math.cos(a) * (ring + 4), centre + Math.sin(a) * (ring + 4));
      context.lineTo(centre + Math.cos(a) * (ring + (i % 4 === 0 ? 7 : 5.5)), centre + Math.sin(a) * (ring + (i % 4 === 0 ? 7 : 5.5)));
      context.stroke();
    }
    context.shadowColor = colour('--knob-shadow'); context.shadowBlur = 5; context.shadowOffsetY = 2;
    const gradient = context.createLinearGradient(0, centre - radius, size, centre + radius);
    gradient.addColorStop(0, colour('--knob-top')); gradient.addColorStop(1, colour('--knob-bottom'));
    context.beginPath(); context.arc(centre, centre, radius, 0, Math.PI * 2);
    context.fillStyle = gradient; context.fill();
    context.shadowColor = 'transparent'; context.shadowBlur = 0; context.shadowOffsetY = 0;
    context.strokeStyle = colour('--knob-edge'); context.lineWidth = .8; context.stroke();
    // 固定的材质短弧，没有最佳参数范围含义；浅、深主题分别定义可见的弧线颜色。
    context.beginPath(); context.arc(centre, centre, radius - 2.5, Math.PI * 1.1, Math.PI * 1.8);
    context.strokeStyle = colour('--knob-shine'); context.lineWidth = .7; context.stroke();
    context.beginPath();
    context.moveTo(centre + Math.cos(angle) * radius * .52, centre + Math.sin(angle) * radius * .52);
    context.lineTo(centre + Math.cos(angle) * radius * .86, centre + Math.sin(angle) * radius * .86);
    context.strokeStyle = colour('--knob-pointer') || colour('--pointer'); context.lineWidth = size > 90 ? 2.5 : 2; context.stroke();
}
