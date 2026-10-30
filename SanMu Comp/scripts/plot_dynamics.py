"""绘制独立 DSP 测量结果；不读取或修改插件参数。"""
import argparse
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument("directories", nargs="+")
parser.add_argument("--output", required=True)
args = parser.parse_args()
plt.rcParams["font.family"] = "Arial Unicode MS"
fig, axes = plt.subplots(2, 2, figsize=(13, 8), constrained_layout=True)
for index, directory in enumerate(args.directories):
    root = Path(directory)
    label = "修改前" if index == 0 else "修改后"
    def trace_path(name):
        path = root / name
        return path if path.exists() else path.with_suffix(path.suffix + ".gz")

    trace = np.genfromtxt(trace_path("fet_100hz.csv"), delimiter=",", names=True)
    visible = trace["seconds"] > -0.03
    axes[0, 0].plot(trace["seconds"][visible] * 1000, trace["gr_db"][visible], label=label)
    axes[0, 1].plot(trace["seconds"][visible] * 1000, trace["output"][visible], label=label)
    step = np.genfromtxt(trace_path("fet_step_48000.csv"), delimiter=",", names=True)
    axes[1, 0].plot(step["seconds"], step["gr_db"], label=label)
    with (root / "metrics.csv").open() as f:
        rows = list(csv.DictReader(f))
    for comp in [-12, -24, -36]:
        selected = [r for r in rows if r["engine"] == "LEV" and float(r["control"]) == comp]
        axes[1, 1].plot([float(r["input_db"]) for r in selected],
                       [float(r["output_rms_db"]) for r in selected],
                       linestyle="--" if index == 0 else "-", label=f"{label} COMP {comp}")
axes[0, 0].set(title="FET 100 Hz、20:1、最快释放：增益纹波", xlabel="时间（毫秒）", ylabel="增益衰减（dB）")
axes[0, 1].set(title="相同条件的输出波形", xlabel="时间（毫秒）", ylabel="采样幅度")
axes[1, 0].set(title="FET 强音转弱音：包络恢复", xlabel="时间（秒）", ylabel="增益衰减（dB）")
axes[1, 1].set(title="LEV 稳态传递曲线，GAIN = -12 dB", xlabel="输入峰值（dBFS）", ylabel="输出有效值（dBFS）")
for ax in axes.flat:
    ax.grid(alpha=0.2)
    ax.legend(fontsize=8)
fig.savefig(args.output, dpi=140)
