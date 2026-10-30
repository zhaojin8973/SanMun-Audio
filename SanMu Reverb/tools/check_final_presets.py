#!/usr/bin/env python3
"""逐个核对正式预设的参数来源与纯湿输出，避免清单整理误改已认可声音。"""
import argparse
import json
from pathlib import Path
import numpy as np
from check_plate_vst3 import render
from check_hall_vst3 import configured
from measure_hall_e1 import set_values


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ['plugin', 'baseline', 'presets', 'old-presets', 'output']:
        p.add_argument('--' + key, type=Path, required=True)
    a = p.parse_args()
    current = json.loads(a.presets.read_text())
    old = json.loads(a.old_presets.read_text())
    assert len(current) == 23 and len({x['name'] for x in current}) == 23
    assert all(not any(tag in x['name'] for tag in ['历史', '090', 'P1', 'P2', 'P3']) for x in current)
    sr = 48000
    x = np.zeros((2, sr * 4), np.float32)
    x[:, sr] = .1
    x[:, sr + 24000:sr + 28800] = np.random.default_rng(908).normal(0, .04, (2, 4800))
    report = {'预设': [], '说明': '四秒脉冲与立体声噪声诊断；不等同完整长尾或真实乐器听审。'}
    for item in current:
        matches = [v for v in old if v['values'] == item['values']]
        assert len(matches) == 1, item['name']
        values = dict(item['values'], mix=100)
        before, _, _ = render(a.baseline, x, sr, values)
        after, _, _ = render(a.plugin, x, sr, values)
        error = float(np.max(np.abs(before - after)))
        assert error == 0, (item['name'], error)
        # 上一版宿主状态在当前真实插件恢复后仍发出同样声音。
        origin = configured(a.baseline)
        set_values(origin, item['values'])
        restored = configured(a.plugin)
        restored.raw_state = origin.raw_state
        original_output = origin.process(x, sr, reset=True)
        restored_output = restored.process(x, sr, reset=True)
        assert np.array_equal(original_output, restored_output), item['name']
        report['预设'].append({'名称': item['name'], '来源': matches[0]['name'], '纯湿最大样本差': error,
                               '旧宿主状态输出一致': True, 'Mix': item['values']['mix']})
        print(item['name'] + '：参数、纯湿声音和旧工程状态一致', flush=True)
    a.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')


if __name__ == '__main__':
    main()
