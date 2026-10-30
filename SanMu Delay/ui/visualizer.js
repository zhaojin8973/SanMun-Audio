// 编辑器表示延迟参数；Duck GR 只使用宿主发送的实际衰减读数。
import { EchoTimeEditor } from './echo-editor.js';
export const EchoVisualizer = EchoTimeEditor;

export class DuckMeter {
    constructor(container) {
        this.track = container.querySelector('.duck-meter-track');
        this.fill = container.querySelector('#duck-meter-fill');
        this.output = container.querySelector('#duck-meter-value');
        this.update(0);
    }
    update(raw) {
        const reduction = Number.isFinite(raw) ? Math.max(0, raw) : 0;
        const shown = reduction.toFixed(1);
        if (this.shown === shown) return;
        this.shown = shown;
        this.fill.style.width = `${Math.min(100, reduction / 24 * 100)}%`;
        this.track.setAttribute('aria-valuenow', String(Math.min(24, reduction)));
        this.track.setAttribute('aria-valuetext', `${shown} dB`);
        this.output.textContent = `${shown} dB`;
    }
}
