#!/usr/bin/env python3
"""回声室C1真实插件验收，保留原始渲染及诊断图；机器通过不等于听审通过。"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
import numpy as np
import soundfile as sf
import pedalboard
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from check_plate_vst3 import render, repetition
from check_hall_vst3 import configured
from measure_hall_e1 import set_values
from audit_plate_character import DEFAULT, measure


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ['plugin', 'baseline', 'renderer', 'presets', 'output']:
        parser.add_argument('--' + key, type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    sr = 48000
    presets = json.loads(args.presets.read_text())
    report = {'状态': '机器验收，待用户真实素材听审', '保留类型': {}, '路由': {}, '默认特性': {},
              '控制': {}, '声源': {}, '预设': {}, '状态往返': {}, '采样块': {},
              '插件哈希': hashlib.sha256((args.plugin/'Contents/MacOS/SanMu Reverb').read_bytes()).hexdigest()}
    impulse = np.zeros((2, sr*8), np.float32)
    impulse[:, sr] = .1
    noise = np.zeros_like(impulse)
    noise[:, sr:sr+sr//3] = np.random.default_rng(908).normal(0, .04, (2, sr//3))
    old, new = pedalboard.load_plugin(str(args.baseline)), pedalboard.load_plugin(str(args.plugin))
    assert set(old.parameters) == set(new.parameters)
    assert all(old.parameters[k].raw_value == new.parameters[k].raw_value for k in old.parameters)
    for name, x in [('脉冲', impulse), ('立体声噪声', noise)]:
        for category in [0, 1, 2]:
            for mode in [0, 1, 2]:
                values = dict(DEFAULT, category=category, mode=mode)
                a, _, _ = render(args.baseline, x, sr, values)
                b, _, _ = render(args.plugin, x, sr, values)
                difference = float(abs(a-b).max())
                assert difference == 0, (category, mode, difference)
                report['保留类型'][f'{name}_{category}_{mode}'] = difference
    print('Hall、Plate、Room全部编号的18项真实输出保留检查通过', flush=True)
    rendered = {}
    for category, mode, label in [(0, 0, '丝滑板式'), (1, 0, '录音室'), (2, 0, '音乐厅'),
                                  (3, 0, '柔暖回声室'), (3, 1, '石质回声室')]:
        values = dict(DEFAULT, category=category, mode=mode)
        y, _, elapsed = render(args.plugin, impulse, sr, values)
        metrics = measure(y, sr)
        metrics['相对输入脉冲能量dB'] = float(10*np.log10(np.sum(y*y)/np.sum(impulse*impulse)))
        metrics['长延迟周期相关峰'] = repetition(y[:, sr:], sr)
        metrics['计算时间比例'] = elapsed/8
        report['默认特性'][label] = metrics
        rendered[label] = y
        sf.write(args.output/(label+'_默认脉冲.wav'), y.T, sr, subtype='FLOAT')
    for mode, index in [(0, 17), (1, 20)]:
        values = presets[index]['values']
        output = args.output/f'offline_{mode}'
        input_path = args.output/f'input_{mode}.wav'
        sf.write(input_path, impulse.T, sr, subtype='FLOAT')
        command = [str(args.renderer), '--source', 'wav', '--input', str(input_path), '--tail', '.1', '--output', str(output)]
        for k, v in values.items():
            command.extend(['--'+k.replace('_', '-'), str(v)])
        subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
        x, _ = sf.read(output/'input.wav', dtype='float32')
        expected, _ = sf.read(output/'output.wav', dtype='float32')
        actual, _, _ = render(args.plugin, x.T.copy(), sr, values)
        error = float(abs(actual.T-expected).max())
        assert error < 2e-6, error
        dry, _, _ = render(args.plugin, x.T.copy(), sr, dict(values, mix=0))
        assert np.array_equal(dry, x.T)
        mixed, _, _ = render(args.plugin, x.T.copy(), sr, dict(values, mix=24.8))
        mix_error = float(abs(mixed-x.T*np.cos(.248*np.pi/2)-actual*np.sin(.248*np.pi/2)).max())
        assert mix_error < 2e-6, mix_error
        report['路由'][mode] = {'离线误差': error, '干声误差': 0, '混合误差': mix_error}
        for control in ['early_diff', 'late_diff', 'attack']:
            a, _, _ = render(args.plugin, impulse, sr, dict(values, **{control: 0}))
            b, _, _ = render(args.plugin, impulse, sr, dict(values, **{control: 100}))
            difference = float(np.linalg.norm(a/np.linalg.norm(a)-b/np.linalg.norm(b)))
            assert difference > .2, (mode, control, difference)
            report['控制'][f'{mode}_{control}'] = difference
        for name, x in [('中央', impulse), ('左', impulse*np.array([[1], [0]], np.float32)),
                        ('右', impulse*np.array([[0], [1]], np.float32)), ('反相', impulse*np.array([[1], [-1]], np.float32))]:
            for early in [0, 100]:
                y, _, _ = render(args.plugin, x, sr, dict(values, early_diff=early))
                el, er = np.sum(y*y, axis=1)
                balance = float(10*np.log10(el/er))
                mono = float(np.sum(((y[0]+y[1])*.5)**2)/(.5*(el+er)))
                assert np.isfinite(y).all() and abs(balance) < 4 and mono > .2, (name, mode, early, balance, mono)
                report['声源'][f'{mode}_{name}_{early}'] = {'左右差dB': balance, '单声道功率比': mono}
        origin = configured(args.plugin)
        set_values(origin, values)
        state = origin.raw_state
        restored = configured(args.plugin)
        restored.raw_state = state
        assert origin.parameters['category'].raw_value == restored.parameters['category'].raw_value
        assert origin.parameters['mode'].raw_value == restored.parameters['mode'].raw_value
        assert np.array_equal(origin.process(noise, sr, reset=True), restored.process(noise, sr, reset=True))
        report['状态往返'][mode] = '参数及输出一致'
        for block in [32, 64, 257, 512, 1024]:
            plugin = configured(args.plugin)
            set_values(plugin, values)
            z = plugin.process(impulse, sr, buffer_size=block, reset=True)
            baseline, _, _ = render(args.plugin, impulse, sr, values)
            error = float(abs(z-baseline).max())
            assert error < 2e-6
            report['采样块'][f'{mode}_{block}'] = error
        print(f'回声室模式 {mode} 的路由、控制、声像、状态与块大小通过', flush=True)
    assert len(presets) == 23
    # 合成信号覆盖低中频谐波和宽带瞬态，不称为真实乐器录音。
    phrase = np.zeros((2, sr*9), np.float32)
    hits = phrase.copy()
    for onset, freq in [(1, 130.81), (1.6, 196), (2.2, 261.63), (3, 164.81), (3.6, 220), (4.2, 329.63)]:
        t = np.arange(int(sr*.9))/sr
        wave = sum(np.sin(2*np.pi*freq*k*t)/k**1.6 for k in range(1, 9))*.06*(1-np.exp(-t/.008))*np.exp(-t/.23)
        pos = int(sr*onset)
        phrase[:, pos:pos+len(t)] += wave.astype(np.float32)
    for i, onset in enumerate([1, 1.75, 2.5, 3.25, 4, 4.75]):
        t = np.arange(sr//3)/sr
        wave = np.random.default_rng(70+i).normal(0, 1, len(t))*.12*np.exp(-t/.035)+.1*np.sin(2*np.pi*185*t)*np.exp(-t/.065)
        pos = int(sr*onset)
        hits[:, pos:pos+len(t)] += wave.astype(np.float32)
    for label, x in [('合成谐波短句', phrase), ('合成鼓击', hits)]:
        sf.write(args.output/(label+'_干声.wav'), x.T, sr, subtype='FLOAT')
        for preset in presets[17:]:
            wet, _, elapsed = render(args.plugin, x, sr, preset['values'])
            mixed, _, _ = render(args.plugin, x, sr, dict(preset['values'], mix=20))
            assert abs(wet).max() < 1 and abs(mixed).max() < 1
            report['预设'][label+'_'+preset['name']] = {'湿声峰值': float(abs(wet).max()), '混合峰值': float(abs(mixed).max()), '计算时间比例': elapsed/9}
            sf.write(args.output/(label+'_'+preset['name']+'_湿声.wav'), wet.T, sr, subtype='FLOAT')
            sf.write(args.output/(label+'_'+preset['name']+'_混合.wav'), mixed.T, sr, subtype='FLOAT')
    plt.rcParams['font.family'] = 'Arial Unicode MS'
    fig, axes = plt.subplots(2, 1, figsize=(11, 7))
    for label, y in rendered.items():
        z = y[:, sr:]
        w = 480
        power = np.mean(z[:, :z.shape[1]//w*w].reshape(2, -1, w)**2, axis=(0, 2))
        axes[0].plot(np.arange(len(power))*.01, 10*np.log10(np.maximum(power, 1e-30)), label=label)
        bands = report['默认特性'][label]['频带']
        frequencies = [np.sqrt(np.prod([int(a) for a in k.split('-')])) for k in bands]
        density = [v['能量']/(int(k.split('-')[1])-int(k.split('-')[0])) for k, v in bands.items()]
        axes[1].semilogx(frequencies, 10*np.log10(density), marker='.', label=label)
    axes[0].set(xlim=(0, .6), xlabel='脉冲后时间（秒）', ylabel='10毫秒窗电平（dBFS）')
    axes[1].set(xlabel='频率（Hz）', ylabel='单位带宽能量（dB，未匹配响度）')
    for ax in axes:
        ax.grid(alpha=.2)
        ax.legend(fontsize=9)
    fig.tight_layout()
    fig.savefig(args.output/'默认音色诊断.png', dpi=130)
    (args.output/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    print('回声室真实插件验收通过，等待真实素材听审', flush=True)


if __name__ == '__main__':
    main()
