#!/usr/bin/env python3
"""固定音高与启动相位，记录中低频持续响应和停音后的尾音；不修改输出电平。"""
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
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plugin',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--reference',action='store_true')
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    sr=48000
    report={'说明':'相同输入、100%湿声、用户确认参考参数；每次新实例，分别前置1秒与2.7秒静音。',
            'sample_rate':sr,'measurements':{},'parameters':{}}
    for family in ['SanMu']+(['VVV'] if args.reference else []):
        for hz in [80,100,125,140,150,160,170,180,190,200,210,225,250,300,400,600]:
            for lead in [1,2.7]:
                x=np.zeros((2,sr*8),np.float32);n=int(sr*.8);t=np.arange(n)/sr
                envelope=np.minimum(t/.02,1)*np.minimum((.8-t)/.02,1)
                x[:,int(lead*sr):int(lead*sr)+n]=.05*np.sin(2*np.pi*hz*t)*envelope
                if family=='SanMu':p=configured(args.plugin)
                else:
                    p=pedalboard.load_plugin('/Library/Audio/Plug-Ins/VST3/ValhallaVintageVerb.vst3')
                    for k,v in dict(PARAMETERS,mix=100).items():setattr(p,k,v)
                report['parameters'][family]={k:str(v) for k,v in p.parameters.items()}
                y=p.process(x,sr,buffer_size=128,reset=True)
                assert y.shape==x.shape and np.isfinite(y).all()
                z=y[:,int(lead*sr):]
                key=f'{family}_{hz}Hz_lead{lead}'
                report['measurements'][key]={
                    'frequency':hz,'lead':lead,'energy':float(np.sum(z.astype(np.float64)**2)),
                    'sustain_rms':float(np.sqrt(np.mean(z[:,int(.4*sr):int(.8*sr)]**2))),
                    'tail_100_800_rms':float(np.sqrt(np.mean(z[:,int(.9*sr):int(1.6*sr)]**2))),
                    'tail_800_1800_rms':float(np.sqrt(np.mean(z[:,int(1.6*sr):int(2.6*sr)]**2))),
                    'peak':float(np.max(abs(z))),'input_pcm_sha256':hashlib.sha256(x.tobytes()).hexdigest()}
                # 原始波形保留第一组，第二组保留参数与数值用于检查调制相位敏感性。
                if lead==1:sf.write(args.output/f'{key}.wav',y.T,sr,subtype='FLOAT')
            print(f'{family}：{hz} Hz 已记录',flush=True)
    report['sanmu_binary_sha256']=hashlib.sha256((args.plugin/'Contents/MacOS/SanMu Reverb').read_bytes()).hexdigest()
    (args.output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')

if __name__=='__main__':main()
