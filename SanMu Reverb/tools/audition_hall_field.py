#!/usr/bin/env python3
"""固定合成测试素材，生成旧版、新版与参考的原始及匹配电平湿声。"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import soundfile as sf
from pedalboard import load_plugin
from check_hall_vst3 import configured
from render_reference import PARAMETERS as REFERENCE_PARAMETERS

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--old',type=Path,required=True)
    p.add_argument('--new',type=Path,required=True)
    p.add_argument('--reference',type=Path,default=Path('/Library/Audio/Plug-Ins/VST3/ValhallaVintageVerb.vst3'))
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    if args.output.exists(): raise RuntimeError('输出目录已存在')
    sr=48000
    dry=np.zeros((2,sr*10),dtype=np.float32)
    rng=np.random.default_rng(20260905)
    # 短促噪声、低频敲击、短和弦：均为合成诊断信号，不伪称真实乐器录音。
    for start in [.25,1.25]:
        t=np.arange(int(.07*sr))/sr
        hit=(rng.uniform(-1,1,len(t))*.55+np.sin(2*np.pi*170*t)*.45)*np.exp(-t/.009)*.4
        offset=int(start*sr); dry[:,offset:offset+len(hit)]+=hit
    t=np.arange(int(.7*sr))/sr
    chord=sum(np.sin(2*np.pi*f*t)*np.exp(-t/.14) for f in [220,261.6256,329.6276])*.085
    dry[:,int(2.25*sr):int(2.25*sr)+len(t)]+=chord
    old=configured(args.old); new=configured(args.new)
    ref=load_plugin(str(args.reference))
    for k,v in dict(REFERENCE_PARAMETERS,mix=100).items(): setattr(ref,k,v)
    # 三个插件均先运行一秒静音，保留原始输入时序，避免参考启动增益影响比较。
    padded=np.pad(dry,((0,0),(sr,0)))
    signals={label:plugin.process(padded,sr,buffer_size=128,reset=True)[:,sr:]
             for label,plugin in [('旧版',old),('新版',new),('参考',ref)]}
    args.output.mkdir(parents=True)
    sf.write(args.output/'干声_合成诊断.wav',dry.T,sr,subtype='FLOAT')
    report={"说明":"100%湿声；相同合成输入。匹配文件只匹配前5秒整体RMS，不改时间或频谱。",
            "sample_rate":sr,"warmup_seconds":1,"seed":20260905,"signals":{}}
    for label,signal in signals.items():
        assert np.isfinite(signal).all()
        sf.write(args.output/f'{label}_原始湿声.wav',signal.T,sr,subtype='FLOAT')
        rms=float(np.sqrt(np.mean(signal[:,:sr*5]**2)))
        gain=.025/rms
        matched=signal*gain
        assert np.max(np.abs(matched))<.95
        sf.write(args.output/f'{label}_匹配电平.wav',matched.T,sr,subtype='PCM_24')
        report['signals'][label]={"raw_rms":rms,"matched_gain_db":float(20*np.log10(gain)),
            "raw_peak":float(np.max(np.abs(signal)))}
    report['input_pcm_sha256']=hashlib.sha256(dry.tobytes()).hexdigest()
    (args.output/'说明.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__': main()
