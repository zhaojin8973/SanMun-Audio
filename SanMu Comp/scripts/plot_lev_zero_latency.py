"""比较用户试听的前瞻版与零延迟版；以相同公开测试信号对齐输入时间。"""
import argparse
import csv
import json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser()
parser.add_argument('before', type=Path)
parser.add_argument('after', type=Path)
parser.add_argument('report', type=Path)
args = parser.parse_args()
args.report.mkdir(parents=True, exist_ok=True)
plt.rcParams['font.family'] = 'Arial Unicode MS'
sr = 48000
t = np.arange(4 * sr) / sr


def source(scenario):
    if scenario == 3:
        age = t % .55
        amplitude = (1 - np.exp(-age / .012)) * np.exp(-age / .16) * np.where(t < 2, .12, .42) + .0001
        return amplitude * (.7 * np.sin(2*np.pi*180*t) + .2 * np.sin(2*np.pi*540*t) + .1 * np.sin(2*np.pi*2300*t))
    if scenario == 4:
        return 10**(-12/20) * np.sin(2*np.pi*70*t)
    amplitude = np.where((t >= .25) & (t < .25 + [.03, .3, 2][scenario]), 10**(-6/20), 10**(-48/20))
    return amplitude * np.sin(2*np.pi*1000*t)


def read(version, scenario, comp=-20, gain=-30):
    folder, delay = (args.before, 128) if version == 'before' else (args.after, 0)
    raw = np.fromfile(folder / f'lev_{scenario}_{comp}_{gain}.f32', dtype=np.float32)
    return raw[delay:]


def db(value):
    return 20 * np.log10(np.maximum(value, 1e-12))


def rms(value):
    return np.sqrt(np.mean(np.square(value, dtype=np.float64)))


def trace(scenario, y, comp, gain):
    x = source(scenario)
    count = min(len(x), len(y)) // 96 * 96
    xp = np.sqrt(np.mean(x[:count].reshape(-1, 96)**2, axis=1))
    yp = np.sqrt(np.mean(y[:count].reshape(-1, 96)**2, axis=1))
    return (np.arange(count // 96) + .5) * .002, db(xp) + abs(comp) + gain - db(yp)


rows = []
for version in ['before', 'after']:
    for scenario in range(5):
        for comp in [0, -20, -30]:
            for gain in [0, -12, -30]:
                y = read(version, scenario, comp, gain)
                row = dict(version=version, scenario=scenario, comp=comp, gain=gain,
                           peak_db=float(db(np.max(np.abs(y)))), rms_db=float(db(rms(y))))
                assert np.isfinite(y).all()
                assert np.max(np.abs(y)) <= .99901 * 10**(gain/20)
                if scenario == 4:
                    # 整秒 70 Hz 相干投影；仅描述该测试音的基波外能量，不代替音质评判。
                    yy = y[-sr:].astype(float)
                    phase = 2*np.pi*70*np.arange(sr)/sr
                    fundamental = 2 * (np.mean(yy*np.sin(phase))**2 + np.mean(yy*np.cos(phase))**2)
                    row['residual_percent'] = float(100*np.sqrt(max(0, np.mean(yy**2)-fundamental) / max(fundamental, 1e-24)))
                rows.append(row)
keys = list(dict.fromkeys(k for row in rows for k in row))
with (args.report / 'dynamic-metrics.csv').open('w') as file:
    writer = csv.DictWriter(file, fieldnames=keys)
    writer.writeheader()
    writer.writerows(rows)

with (args.report / 'before-static.csv').open() as file:
    old = list(csv.DictReader(file))
with (args.report / 'after-static.csv').open() as file:
    new = list(csv.DictReader(file))
changes = []
for a, b in zip(old, new, strict=True):
    assert (a['input_peak_db'], a['comp']) == (b['input_peak_db'], b['comp'])
    delta = float(b['gr_db']) - float(a['gr_db'])
    assert -.002 <= delta <= 1.01
    if float(a['comp']) == 0:
        assert abs(delta) < 1e-6
    changes.append(dict(input_peak_db=float(a['input_peak_db']), comp=float(a['comp']),
                        before_gr=float(a['gr_db']), after_gr=float(b['gr_db']), extra_gr=delta))
normal = [r['extra_gr'] for r in changes if 3 <= r['before_gr'] <= 15]
summary = dict(static_points=len(changes), normal_extra_gr_range=[min(normal), max(normal)],
               dynamic_renders=len(rows), output_peak_checks='全部通过；仅全湿采样峰值',
               examples=[r for r in changes if r['comp'] == -20 and r['input_peak_db'] in [-36, -24, -18, -12]],
               low_tone=[r for r in rows if r['scenario'] == 4 and r['comp'] == -30 and r['gain'] == -30])
(args.report / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')

fig, axes = plt.subplots(2, 2, figsize=(12, 8))
for scenario, ax in enumerate(axes.flat):
    for version, label in [('before', '0.1.1：前瞻 2.67 毫秒'), ('after', '0.1.2：零延迟、增强压缩')]:
        times, reduction = trace(scenario, read(version, scenario), -20, -30)
        ax.plot(times, reduction, label=label, lw=1.2)
    ax.set(title=['30 毫秒短音', '300 毫秒短音', '2 秒长音', '模拟音节强弱变化'][scenario],
           xlabel='输入时间（秒）', ylabel='由输入输出推算总衰减（dB）', ylim=(-.5, 19))
    ax.grid(alpha=.2)
    ax.legend(fontsize=9)
fig.suptitle('同输入动态比较：COMP −20、GAIN −30、门关闭；旧版已扣除音频延迟')
fig.tight_layout()
fig.savefig(args.report / 'dynamic-comparison.png', dpi=150)
plt.close(fig)

fig, axes = plt.subplots(1, 2, figsize=(12, 4))
ax = axes[0]
mask = (t >= .248) & (t < .254)
ax.plot(t[mask], source(0)[mask], color='grey', alpha=.5, label='输入（幅度参考）')
for version, label in [('before', '0.1.1'), ('after', '0.1.2')]:
    y = read(version, 0, -20, 0)
    ax.plot(t[:len(y)][mask[:len(y)]], y[mask[:len(y)]], label=label, lw=1.3)
ax.set(title='强音最初 4 毫秒：即时控制的取舍', xlabel='输入时间（秒）', ylabel='输出幅度，GAIN 0')
ax.legend()
ax.grid(alpha=.2)
ax = axes[1]
selected = [r for r in changes if r['comp'] == -20]
ax.plot([r['input_peak_db'] for r in selected], [r['extra_gr'] for r in selected], marker='o')
ax.set(title='COMP −20 的实际增强量', xlabel='1 kHz 输入峰值（dBFS）', ylabel='增加的主压缩（dB）', ylim=(0, 1.1))
ax.grid(alpha=.2)
fig.tight_layout()
fig.savefig(args.report / 'onset-and-depth.png', dpi=150)
plt.close(fig)
np.savez_compressed(args.report / 'probe-excerpts.npz',
                    input_short=source(0)[:24000], input_syllables=source(3),
                    before_short=read('before', 0, -20, 0)[:24000],
                    after_short=read('after', 0, -20, 0)[:24000],
                    before_syllables=read('before', 3), after_syllables=read('after', 3))
print(json.dumps(summary, ensure_ascii=False, indent=2))
