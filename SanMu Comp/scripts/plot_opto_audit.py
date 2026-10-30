"""绘制生产引擎的 Opto 前后测量；时间从强音结束算起。"""
import argparse
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument("before", type=Path)
parser.add_argument("after", type=Path)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
plt.rcParams["font.family"] = "Arial Unicode MS"
fig, axes = plt.subplots(2, 2, figsize=(13, 8), constrained_layout=True)
summary = []
for root, label, style in [(args.before, "修改前", "--"), (args.after, "修改后", "-")]:
    for duration, color in [(0.01, "tab:blue"), (0.1, "tab:orange"), (2.0, "tab:green")]:
        path = root / f"opto_release_{duration:.6f}.csv"
        if not path.exists():
            path = path.with_suffix(".csv.gz")
        data = np.genfromtxt(path, delimiter=",", names=True)
        t = data["seconds"] - duration - 0.1
        for ax, bound in [(axes[0, 0], 0.15), (axes[0, 1], 2.8)]:
            visible = (t >= 0) & (t <= bound)
            ax.plot(t[visible], data["gr_db"][visible], color=color, linestyle=style,
                    label=f"{label}：强音 {duration * 1000:g} 毫秒")
        for elapsed in [0.06, 0.5, 1.0, 2.0]:
            summary.append([label, duration, elapsed, np.interp(elapsed, t, data["gr_db"])])
    with (root / "metrics.csv").open() as f:
        rows = [r for r in csv.DictReader(f) if r["engine"] == "OPTO_OUTPUT"]
    axes[1, 0].semilogy([float(r["output_control"]) for r in rows],
                        [float(r["residual_percent"]) for r in rows],
                        marker="o", linestyle=style, label=label)

with (args.after / "opto_frequency.csv").open() as f:
    rows = list(csv.DictReader(f))
for sr in [44100, 48000, 96000, 192000]:
    selected = [r for r in rows if float(r["sample_rate"]) == sr]
    axes[1, 1].semilogx([float(r["hz"]) for r in selected],
                        [float(r["output_rms_db"]) + 33.0103 for r in selected],
                        marker="o", label=f"{sr / 1000:g} kHz")
axes[0, 0].set(title="强音结束后的早期恢复", xlabel="结束后时间（秒）", ylabel="增益衰减（dB）")
axes[0, 1].set(title="持续时间与恢复慢尾", xlabel="结束后时间（秒）", ylabel="增益衰减（dB）")
axes[1, 0].set(title="GAIN 与基波外残差，PR 60、输入峰值 -6 dBFS", xlabel="GAIN 刻度", ylabel="残差（%）")
axes[1, 1].set(title="修改后 PR 0 小信号响应，四条曲线重合", xlabel="频率（Hz）", ylabel="相对输入有效值（dB）")
for ax in axes.flat:
    ax.grid(alpha=0.2)
    ax.legend(fontsize=8)
fig.savefig(args.output, dpi=140)
with (args.after / "release_summary.csv").open("w") as f:
    writer = csv.writer(f)
    writer.writerow(["版本", "强音秒数", "结束后秒数", "衰减分贝"])
    writer.writerows(summary)
