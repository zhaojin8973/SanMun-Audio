#!/usr/bin/env python3
"""生成用于中低频尾音检查的合成谐波短句，分别保存原始和匹配电平输出。"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import pedalboard
import soundfile as sf
from check_hall_vst3 import configured
from render_reference import PARAMETERS

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline',type=Path,required=True)
    p.add_argument('--candidate',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--baseline-label',default='0906A')
    p.add_argument('--candidate-label',default='0906B候选')
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    sr=48000;dry=np.zeros((2,14*sr),np.float32)
    # 合成谐波列覆盖150～200Hz，带轻微滑音与元音谱包络；不是用户的人声或真实录音。
    for i,(f0,formant) in enumerate([(150,550),(175,650),(195,450),(125,550),(170,700),(185,550)]):
        t=np.arange(int(.6*sr))/sr
        pitch=f0*(1+.012*np.sin(2*np.pi*4.7*t))
        phase=2*np.pi*np.cumsum(pitch)/sr
        wave=np.zeros_like(t)
        for h in range(1,32):
            hz=h*f0
            colour=.22+1.8*np.exp(-.5*((hz-formant)/170)**2)+.65*np.exp(-.5*((hz-1350)/280)**2)
            wave+=np.sin(h*phase+.17*h)*colour/(h**1.15)
        envelope=np.minimum(t/.03,1)*np.minimum((.6-t)/.08,1)
        wave*=envelope*.07
        at=int((.25+i*1.1)*sr);dry[:,at:at+len(t)]=wave
    padded=np.pad(dry,((0,0),(sr,0)))
    ref=pedalboard.load_plugin('/Library/Audio/Plug-Ins/VST3/ValhallaVintageVerb.vst3')
    for k,v in dict(PARAMETERS,mix=100).items():setattr(ref,k,v)
    plugins={args.baseline_label:configured(args.baseline),args.candidate_label:configured(args.candidate),'VintageVerb':ref}
    signals={name:p.process(padded,sr,buffer_size=128,reset=True)[:,sr:] for name,p in plugins.items()}
    sf.write(args.output/'干声_合成谐波短句.wav',dry.T,sr,subtype='FLOAT')
    base_rms=float(np.sqrt(np.mean(signals[args.baseline_label][:,:8*sr]**2)))
    report={'说明':f'合成诊断信号，不是真实人声。100%湿声、同输入、前置1秒静音；匹配文件对齐前8秒RMS到{args.baseline_label}。',
            'sample_rate':sr,'input_pcm_sha256':hashlib.sha256(dry.tobytes()).hexdigest(),'signals':{}}
    for label,y in signals.items():
        assert np.isfinite(y).all()
        rms=float(np.sqrt(np.mean(y[:,:8*sr]**2)));gain=base_rms/rms
        sf.write(args.output/f'{label}_原始湿声.wav',y.T,sr,subtype='FLOAT')
        assert abs(y*gain).max()<.95
        sf.write(args.output/f'{label}_匹配电平.wav',(y*gain).T,sr,subtype='PCM_24')
        report['signals'][label]={'raw_rms':rms,'raw_peak':float(abs(y).max()),'matched_gain_db':float(20*np.log10(gain))}
    (args.output/'说明.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__':main()
