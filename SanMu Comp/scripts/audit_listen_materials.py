"""在本地把每条分轨最响的十秒送入生产 LISTEN；只把数值结果写入报告。"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import time

import numpy as np
import soundfile as sf

parser = argparse.ArgumentParser()
parser.add_argument("materials", type=Path)
parser.add_argument("--probe", type=Path, default=Path("build-make/source/listen_material_probe"))
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--gain-db", type=float, default=0)
args = parser.parse_args()
args.output.parent.mkdir(parents=True, exist_ok=True)
rows = []
with tempfile.TemporaryDirectory(prefix="sanmu-listen-material-") as tmp:
    for index, path in enumerate(sorted(args.materials.rglob("*.wav"))):
        with sf.SoundFile(path) as audio:
            rate = audio.samplerate
            power = []
            while True:
                block = audio.read(rate, dtype="float32", always_2d=True)
                if not len(block):
                    break
                power.append(float(np.mean(block.astype("float64") ** 2)))
            start = int(np.argmax(np.convolve(power, np.ones(10), "valid"))) if len(power) >= 10 else 0
            audio.seek(start * rate)
            samples = audio.read(10 * rate, dtype="float32", always_2d=True)
        # 转存浮点样本以统一探针输入，不重采样或有损转换；不修改原分轨、不上传音频。
        clip = Path(tmp) / f"{index:03}.wav"
        sf.write(clip, samples, rate, subtype="FLOAT")
        began = time.monotonic()
        process = subprocess.run([str(args.probe.resolve()), str(clip), "0", str(args.gain_db), "plan"],
                                 check=True, capture_output=True, text=True)
        row = json.loads(process.stdout)
        row.update(original_file=str(path.relative_to(args.materials)), start_second=start,
                   gain_db=args.gain_db, elapsed=round(time.monotonic() - began, 3))
        row.pop("file", None)
        rows.append(row)
        args.output.write_text(json.dumps(rows, ensure_ascii=False, indent=2))
        print(row["original_file"], "模式", row["mode"], "压缩", round(row["gr"], 2),
              "电平差", round(row["level_error"], 2), "可应用", row["ready"], flush=True)
