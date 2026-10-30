// 弹窗放在未缩放的文档层，旧浏览器的兼容弹窗和原生弹窗共用同一套坐标。
export function mountDialogLayer() {
    document.querySelectorAll('#app dialog').forEach(dialog => document.body.appendChild(dialog));
}

export function positionDialogLayer(scale = 1) {
    const validScale = Number.isFinite(scale) && scale > 0 ? scale : 1;
    for (const dialog of document.querySelectorAll('dialog[open]')) {
        dialog.style.position = 'fixed';
        dialog.style.transform = `scale(${validScale})`;
        dialog.style.transformOrigin = 'top left';
        dialog.style.margin = '0';
        dialog.style.right = 'auto';
        dialog.style.bottom = 'auto';
        dialog.style.maxHeight = `${Math.max(64, innerHeight / validScale - 24)}px`;
        dialog.style.maxWidth = `${Math.max(64, innerWidth / validScale - 32)}px`;
        const settings = dialog.id === 'settings-dialog';
        const width = dialog.offsetWidth * validScale;
        const height = dialog.offsetHeight * validScale;
        dialog.style.left = `${Math.max(8, settings ? innerWidth - width - 16 * validScale : (innerWidth - width) / 2)}px`;
        dialog.style.top = `${Math.max(8, settings ? 12 * validScale : (innerHeight - height) / 2)}px`;
    }
}
