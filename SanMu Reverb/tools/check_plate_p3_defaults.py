#!/usr/bin/env python3
"""板式P3：核对真实新建默认、无调制共振及短音包络，并保存对照图。"""
import argparse
import hashlib
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import pedalboard
import scipy.signal as sig
import soundfile as sf

from audit_plate_character import DEFAULT, measure
from check_plate_vst3 import render
from measure_hall_e1 import set_values


def envelope(x, sr):
    # 纯音结束120ms以后取780ms；去掉线性衰减趋势，只报告局部起伏，不设听感及格线。
    z = x[:, round(sr * 1.37):round(sr * 2.15)].astype(float)
    width = round(sr * .03)
    power = np.mean(z[:, :z.shape[1] // width * width].reshape(2, -1, width) ** 2, axis=(0, 2))
    db = 10 * np.log10(np.maximum(power, 1e-30))
    time = np.arange(len(db))
    residual = db - np.polyval(np.polyfit(time, db, 1), time)
    return {'去衰减趋势标准差dB': float(residual.std()), '相邻30ms最大回升dB': float(np.diff(db).max())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['plugin', 'baseline', 'before', 'after', 'output']:
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    sr = 48000
    x, _ = sf.read(args.before / 'input_impulse.wav', dtype='float32')
    x = x.T.copy()
    report = {'状态': '机器诊断通过不代表真实素材听审通过', '采样率': sr, '二进制': {},
              '新建默认': {}, '关闭调制': {}, '连续输入': {}, '短音包络': {}}
    for label, path in [('P2', args.baseline), ('P3', args.plugin)]:
        report['二进制'][label] = hashlib.sha256((path / 'Contents/MacOS/SanMu Reverb').read_bytes()).hexdigest()
    # 此处直接加载插件，禁止调用会重写所有参数的configured辅助函数。
    fresh = pedalboard.load_plugin(str(args.plugin))
    old = pedalboard.load_plugin(str(args.baseline))
    for key, parameter in fresh.parameters.items():
        assert abs(parameter.raw_value - old.parameters[key].raw_value) < 1e-7, key
    report['新建默认']['全部参数与P2一致'] = True
    report['新建默认']['回读'] = {k: str(v) for k, v in fresh.parameters.items()}
    for category, name in [(0, 'plate_default'), (2, 'hall_default')]:
        set_values(fresh, {'category': category})
        actual = fresh.process(x, sr, buffer_size=128, reset=True)
        expected, _ = sf.read(args.after / f'{name}_impulse.wav', dtype='float32')
        error = float(abs(actual - expected.T).max())
        assert error < 2e-6, (name, error)
        report['新建默认'][name + '仅切类型误差'] = error
    # 无调制脉冲：判断改善是否只靠调制把窄带峰值涂开。
    for label, path in [('P2', args.baseline), ('P3', args.plugin)]:
        wet, _, _ = render(path, x, sr, dict(DEFAULT, mod_depth=0))
        sf.write(args.output / f'{label}_无调制脉冲.wav', wet.T, sr, subtype='FLOAT')
        report['关闭调制'][label] = measure(wet, sr)
    assert report['关闭调制']['P3']['低中频谱起伏P95-P10_dB'] < 14
    # 独立声道的持续宽带输入，检查持续驱动后的频谱与停止后的衰减。
    noise = np.zeros((2, 14 * sr), np.float32)
    noise[:, sr:5 * sr] = np.random.default_rng(907).normal(0, .025, (2, 4 * sr))
    sf.write(args.output / '连续输入.wav', noise.T, sr, subtype='FLOAT')
    for label, path in [('P2', args.baseline), ('P3', args.plugin)]:
        wet, _, elapsed = render(path, noise, sr, DEFAULT)
        sf.write(args.output / f'{label}_连续输入.wav', wet.T, sr, subtype='FLOAT')
        steady = wet[:, 3 * sr:5 * sr].astype(float)
        bands = {}
        for low, high in [(100, 350), (700, 1400), (2800, 10000)]:
            filtered = sig.sosfilt(sig.butter(3, [low, high], btype='bandpass', fs=sr, output='sos'), steady, axis=1)
            bands[f'{low}-{high}'] = float(10 * np.log10(np.mean(filtered ** 2)))
        report['连续输入'][label] = {'频带均方dBFS': bands, '峰值': float(abs(wet).max()),
                                  '处理时间与音频时长比': elapsed / 14,
                                  '末秒相对停止后首秒能量dB': float(10 * np.log10(np.sum(wet[:, -sr:] ** 2) / np.sum(wet[:, 5 * sr:6 * sr] ** 2)))}
        assert report['连续输入'][label]['峰值'] < 1
        assert report['连续输入'][label]['末秒相对停止后首秒能量dB'] < -60
    sources = [('P2', args.before, 'plate_default'), ('P3', args.after, 'plate_default'),
               ('参考NOW', args.before, 'vvv_Plate_now'), ('参考1970s', args.before, 'vvv_Plate_seventies')]
    for label, directory, prefix in sources:
        report['短音包络'][label] = {}
        for hz in [100, 160, 220, 250, 315, 400]:
            wet, _ = sf.read(directory / f'{prefix}_tone{hz}.wav')
            report['短音包络'][label][str(hz)] = envelope(wet.T, sr)
    before = json.loads((args.before / 'report.json').read_text())['结果']
    after = json.loads((args.after / 'report.json').read_text())['结果']
    new = after['plate_default']['impulse']
    assert new['低中频谱起伏P95-P10_dB'] < 14
    assert abs(10 * np.log10(new['能量'] / before['plate_default']['impulse']['能量'])) < 1.5
    report['默认脉冲对照'] = {'P2': before['plate_default']['impulse'], 'P3': new,
                            'Hall': after['hall_default']['impulse'], '参考NOW': before['vvv_Plate_now']['impulse']}
    plt.rcParams['font.sans-serif'] = ['Arial Unicode MS']
    plt.rcParams['axes.unicode_minus'] = False
    fig, axes = plt.subplots(2, 1, figsize=(11, 8))
    for label, directory, prefix in sources[:3]:
        wet, _ = sf.read(directory / f'{prefix}_impulse.wav')
        f, psd = sig.welch(wet[sr:], fs=sr, nperseg=32768, axis=0)
        axes[0].plot(f, 10 * np.log10(np.maximum(psd.mean(axis=1), 1e-30)), label=label, linewidth=1)
    axes[0].set(xlim=(60, 450), ylim=(-135, -90), xlabel='频率 / Hz', ylabel='脉冲输出功率谱 / dB')
    centers = [80, 150, 275, 525, 1050, 2100, 4200, 7800]
    for label, data in report['默认脉冲对照'].items():
        axes[1].semilogx(centers, [v['尾声RT60'] for v in data['频带'].values()], marker='o', label=label)
    axes[1].set(xlabel='频带代表频率 / Hz', ylabel='估算衰减时间 / 秒', ylim=(0, 6))
    for ax in axes:
        ax.legend()
        ax.grid(alpha=.25)
    fig.suptitle('相同新建默认参数、100%湿声；诊断曲线不等于听感相似度')
    fig.tight_layout()
    fig.savefig(args.output / 'P3对照.png', dpi=160)
    (args.output / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print('新建默认、关闭调制、持续输入和短音诊断完成')


if __name__ == '__main__':
    main()
