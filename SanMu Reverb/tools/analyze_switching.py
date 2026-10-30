#!/usr/bin/env python3
"""检查切换原始样本和逐块耗时，时限比较不等同宿主实时丢帧证明。"""
import argparse
import json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

p = argparse.ArgumentParser()
p.add_argument('directory', type=Path)
a = p.parse_args()
report = {}
plt.rcParams['font.sans-serif'] = ['Arial Unicode MS']
plt.rcParams['axes.unicode_minus'] = False
fig, axes = plt.subplots(2, 1, figsize=(12, 7))
for path in sorted(a.directory.glob('[0-9]*_[0-9]*.csv')):
    rate, block = map(int, path.stem.split('_'))
    rows = np.genfromtxt(path, delimiter=',', names=True)
    audio = np.fromfile(path.with_suffix('.f32'), np.float32).reshape(2, -1)
    us = rows['microseconds']
    report[path.stem] = {'耗时微秒P50': float(np.median(us)), '耗时微秒P99': float(np.percentile(us, 99)),
                         '最大耗时微秒': float(us.max()), '块时限微秒': block / rate * 1e6,
                         '超时块数': int(np.sum(us > block / rate * 1e6)),
                         '峰值': float(np.max(abs(audio))), '最大相邻样本差': float(np.max(abs(np.diff(audio, axis=1))))}
    if rate == 96000 and block == 32:
        axes[0].plot(rows['block'] * block / rate, us, linewidth=.5)
        axes[0].axhline(block / rate * 1e6, color='red', linestyle='--', label='单块时间预算')
        axes[1].plot(np.arange(audio.shape[1]) / rate, audio[0], linewidth=.4)
resets = np.genfromtxt(a.directory / 'reset.csv', delimiter=',', names=True)
for category in [0, 1, 2, 3]:
    us = resets['microseconds'][resets['category'] == category]
    report['清空' + str(category)] = {'中位数微秒': float(np.median(us)), '最大微秒': float(us.max())}
for name in ['transition_events', 'ui_intermediate_events']:
    rows = np.genfromtxt(a.directory / (name + '.csv'), delimiter=',', names=True,
                         dtype=None, encoding='utf-8')
    rows = np.atleast_1d(rows)
    report[name] = {'事件数': len(rows), '最大边界样本差': float(np.max(rows['boundary'])),
                    '最大窗口峰值': float(np.max(rows['max_window']))}
axes[0].set_ylabel('每块处理耗时 / 微秒'); axes[0].legend()
axes[1].set_ylabel('输出样本'); axes[1].set_xlabel('时间 / 秒')
for ax in axes: ax.grid(alpha=.2)
fig.tight_layout(); fig.savefig(a.directory / '切换原始数据.png', dpi=140)
(a.directory / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(report, ensure_ascii=False, indent=2))
