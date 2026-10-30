#!/usr/bin/env python3
"""用独立宿主验证构建的 VST3 与同参数离线 DSP 输出一致。"""
import argparse
import hashlib
import json
import time
from pathlib import Path
import numpy as np
import pedalboard
import soundfile as sf

PARAMETERS = {
    "category": (2, 0, 3, 1), "mode": (0, 0, 2, 1),
    "decay_s": (3.99, .2, 20, .45), "predelay_ms": (20, 0, 250, .5),
    "mix": (100, 0, 100, 1), "high_freq_hz": (6000, 1000, 20000, .35),
    "high_shelf_db": (-24, -24, 0, 1), "bass_freq_hz": (300, 50, 1000, .4),
    "bass_mult_x": (1.5, .25, 4, .5), "size": (100, 0, 100, 1),
    "attack": (50, 0, 100, 1), "early_diff": (100, 0, 100, 1),
    "late_diff": (100, 0, 100, 1), "mod_rate_hz": (2.53, .1, 10, .35),
    "mod_depth": (38, 0, 100, 1), "low_cut_hz": (10, 10, 500, .35),
    "high_cut_hz": (8010, 1000, 20000, .35), "bypass": (0, 0, 1, 1)
}

def configured(path):
    plugin = pedalboard.load_plugin(str(path))
    # category 与宿主对象自身属性重名；必须写真实参数，不能写 plugin.category。
    for key, (value, low, high, skew) in PARAMETERS.items():
        raw = ((value-low)/(high-low))**skew
        plugin.parameters[key].raw_value = raw
        if abs(plugin.parameters[key].raw_value-raw) > 1e-5:
            raise RuntimeError(f"参数写入不一致：{key}")
    return plugin

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--plugin', type=Path, required=True)
    p.add_argument('--render', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--mode', type=int, choices=[0,1,2], default=0)
    args = p.parse_args()
    if args.output.exists(): raise RuntimeError('输出目录已存在，拒绝覆盖')
    x, sr = sf.read(args.render/'input.wav', dtype='float32')
    expected, expected_sr = sf.read(args.render/'output.wav', dtype='float32')
    assert expected_sr == sr
    plugin = configured(args.plugin)
    plugin.parameters['mode'].raw_value = args.mode / 2
    start = time.perf_counter()
    actual = plugin.process(x.T.copy(), sr, buffer_size=128, reset=True).T
    elapsed = time.perf_counter()-start
    difference = float(np.max(np.abs(actual-expected)))
    assert np.isfinite(actual).all() and difference < 2e-6, difference
    # 相同插件验证 Mix=0 的干声端点及24.8%的真实干湿加法。
    dry_plugin = configured(args.plugin)
    dry_plugin.parameters['mode'].raw_value = args.mode / 2
    dry_plugin.parameters['mix'].raw_value = 0
    dry = dry_plugin.process(x.T.copy(),sr,buffer_size=128,reset=True).T
    dry_error = float(np.max(np.abs(dry-x)))
    assert dry_error == 0
    mix_plugin = configured(args.plugin)
    mix_plugin.parameters['mode'].raw_value = args.mode / 2
    mix_plugin.parameters['mix'].raw_value = .248
    mixed = mix_plugin.process(x.T.copy(),sr,buffer_size=128,reset=True).T
    expected_mix = x*np.cos(.248*np.pi/2)+actual*np.sin(.248*np.pi/2)
    mix_error = float(np.max(np.abs(mixed-expected_mix)))
    assert mix_error < 2e-6, mix_error
    binary = args.plugin/'Contents/MacOS/SanMu Reverb'
    report = dict(status='VST3算法与干湿路由验证通过，未经人工听审',
        plugin=str(args.plugin.resolve()), binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        parameter_readback={k:str(plugin.parameters[k]) for k in PARAMETERS},
        sample_rate=sr, block=128, frames=len(x), elapsed_seconds=elapsed,
        processing_to_audio_ratio=elapsed/(len(x)/sr), maximum_dsp_difference=difference,
        dry_endpoint_error=dry_error, mix_24_8_error=mix_error,
        host=f'pedalboard {pedalboard.__version__}')
    args.output.mkdir(parents=True)
    sf.write(args.output/'output.wav',actual,sr,subtype='FLOAT')
    (args.output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__ == '__main__': main()
