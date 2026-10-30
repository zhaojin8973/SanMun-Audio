#!/usr/bin/env python3
"""使用本机已安装的参考 VST3，保存固定参数、真实输入与离线输出。"""

import argparse
import hashlib
import json
import plistlib
from pathlib import Path

import numpy as np
import pedalboard
from pedalboard.io import AudioFile


PARAMETERS = {
    "mix": 24.8, "predelay": 20.0, "decay": "3.99 s", "size": 100.0,
    "attack": 50.0, "bassmult": 1.5, "bassxover": 300.0,
    "highshelf": -24.0, "highfreq": 6000.0,
    "earlydiffusion": 100.0, "latediffusion": 100.0,
    "modrate": 2.53, "moddepth": 38.0, "highcut": 8010.0, "lowcut": 10.0,
    "colormode": "seventies", "reverbmode": "Concert Hall", "bypass": False,
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plugin", type=Path,
                        default=Path("/Library/Audio/Plug-Ins/VST3/ValhallaVintageVerb.vst3"))
    parser.add_argument("--input", type=Path, required=True, help="包含完整静音尾段的输入 WAV")
    parser.add_argument("--output", type=Path, required=True, help="新建输出目录，拒绝覆盖证据")
    parser.add_argument("--mix", type=float, default=24.8)
    parser.add_argument("--early", type=float, default=100.0)
    parser.add_argument("--late", type=float, default=100.0)
    parser.add_argument("--attack", type=float, default=50.0)
    parser.add_argument("--warmup", type=float, default=1.0, help="静音预热秒数，避开插件启动时参数及增益过渡")
    parser.add_argument("--block", type=int, default=128)
    args = parser.parse_args()
    if any(not 0 <= v <= 100 for v in (args.mix, args.early, args.late, args.attack)) or args.block < 1:
        parser.error("比例必须在 0 到 100，音频块必须为正整数")
    if not np.isfinite(args.warmup) or not 0 <= args.warmup <= 10:
        parser.error("静音预热必须在 0 到 10 秒")
    with AudioFile(str(args.input)) as handle:
        audio = handle.read(handle.frames)
        sample_rate = handle.samplerate
    if audio.shape[0] == 1:
        audio = np.repeat(audio, 2, axis=0)
    if audio.shape[0] != 2 or not np.isfinite(audio).all():
        parser.error("只接受有限值的单声道或立体声 WAV")
    requested = dict(PARAMETERS, mix=args.mix, earlydiffusion=args.early,
                     latediffusion=args.late, attack=args.attack)
    plugin = pedalboard.load_plugin(str(args.plugin))
    for key, value in requested.items():
        setattr(plugin, key, value)
    actual = {key: str(getattr(plugin, key)) for key in requested}
    for key, value in requested.items():
        result = getattr(plugin, key)
        if isinstance(value, (int, float)) and not isinstance(value, bool):
            if abs(float(result) - value) > 0.011:
                raise RuntimeError(f"参考参数未精确写入：{key} 请求 {value}，实际 {result}")
        elif result != value:
            raise RuntimeError(f"参考参数不一致：{key} 请求 {value}，实际 {result}")
    # 新实例先跑静音，使参数与内部增益完成启动过渡；预热后不重置插件。
    # 仅移除人为追加的前导静音，不做峰值对齐，不改变输入文件自身的时间原点。
    warmup_frames = round(args.warmup * sample_rate)
    padded = np.pad(audio, ((0, 0), (warmup_frames, 0)))
    output = plugin.process(padded, sample_rate, buffer_size=args.block, reset=True)[:, warmup_frames:]
    if output.shape != audio.shape or not np.isfinite(output).all():
        raise RuntimeError("参考输出长度、声道或数值异常")
    args.output.mkdir(parents=True, exist_ok=False)
    with AudioFile(str(args.output / "output.wav"), "w", sample_rate,
                   num_channels=2, bit_depth=32) as handle:
        handle.write(output)
    info = plistlib.loads((args.plugin / "Contents/Info.plist").read_bytes())
    manifest = {
        "status": "已渲染参考，未经人工听审",
        "plugin": str(args.plugin.resolve()),
        "plugin_version": info.get("CFBundleShortVersionString"),
        "renderer": f"pedalboard {pedalboard.__version__}",
        "input": str(args.input.resolve()),
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "output_sha256": hashlib.sha256((args.output / "output.wav").read_bytes()).hexdigest(),
        "sample_rate": sample_rate, "block": args.block, "frames": audio.shape[1],
        "warmup_frames": warmup_frames, "warmup_seconds": args.warmup,
        "requested": requested, "actual": actual,
        "peak": float(np.max(np.abs(output))),
        "note": "参考渲染使用独立离线宿主；REAPER 路由仍需原宿主核验。未自动补偿响度或时间。",
    }
    (args.output / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
