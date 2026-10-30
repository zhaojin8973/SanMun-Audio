#!/usr/bin/env python3
"""检查已编译VST3的常用声音保留、早期端点、模式差异及新预设渲染。"""
import argparse,hashlib,json
from pathlib import Path
import numpy as np
import soundfile as sf
from check_hall_vst3 import configured,PARAMETERS

MAP={'decay':'decay_s','predelay':'predelay_ms','high_freq':'high_freq_hz','high_shelf':'high_shelf_db',
     'bass_freq':'bass_freq_hz','bass_mult':'bass_mult_x','mod_rate':'mod_rate_hz','low_cut':'low_cut_hz','high_cut':'high_cut_hz'}

def set_values(plugin,values):
    for source,value in values.items():
        key=MAP.get(source,source)
        _,low,high,skew=PARAMETERS[key]
        raw=((value-low)/(high-low))**skew
        plugin.parameters[key].raw_value=raw
        assert abs(plugin.parameters[key].raw_value-raw)<1e-5,(source,value)

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--plugin',type=Path,required=True)
    a.add_argument('--baseline',type=Path,required=True)
    a.add_argument('--presets',type=Path,required=True)
    a.add_argument('--output',type=Path,required=True)
    args=a.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    sr=48000
    report={'状态':'客观验证与合成诊断渲染，Cathedral及新预设待真实素材复听',
            '二进制SHA256':hashlib.sha256((args.plugin/'Contents/MacOS/SanMu Reverb').read_bytes()).hexdigest(),
            '基线SHA256':hashlib.sha256((args.baseline/'Contents/MacOS/SanMu Reverb').read_bytes()).hexdigest(),
            '参数回读':{},'保留检查':{},'零值检查':{},'模式检查':{},'预设':{}}
    def render(path,x,values):
        plugin=configured(path);set_values(plugin,values)
        y=plugin.process(x,sr,buffer_size=128,reset=True)
        assert np.isfinite(y).all()
        return y,{k:str(v) for k,v in plugin.parameters.items()}
    impulse=np.zeros((2,sr*12),np.float32);impulse[:,sr]=.1
    noise=impulse.copy();noise.fill(0);noise[:,sr:sr*2]=np.random.default_rng(906).normal(0,.04,(2,sr))
    for kind,x in [('impulse',impulse),('noise',noise)]:
        for early in [5,50,100]:
            old,_=render(args.baseline,x,{'early_diff':early})
            new,_=render(args.plugin,x,{'early_diff':early})
            difference=float(abs(old-new).max())
            report['保留检查'][f'{kind}_EARLY{early}']=difference
            assert difference<2e-7,difference
        sf.write(args.output/f'input_{kind}.wav',x.T,sr,subtype='FLOAT')
    for early in [0,.1,1,4.9,5,100]:
        y,actual=render(args.plugin,impulse,{'early_diff':early})
        w=y[:,sr:sr+1920].astype(float);e=np.sum(w*w,axis=1)
        report['零值检查'][str(early)]={'前40ms左比右dB':float(10*np.log10(e[0]/e[1]))}
        sf.write(args.output/f'concert_early{early:g}.wav',y.T,sr,subtype='FLOAT')
        if early==0: assert abs(report['零值检查'][str(early)]['前40ms左比右dB'])<.001
    for mode in [0,1,2]:
        y,actual=render(args.plugin,impulse,{'mode':mode})
        report['参数回读'][str(mode)]=actual
        z=y[:,sr:].astype(float);power=np.mean(z*z,axis=0)
        metrics={'总能量':float(power.sum()),'前500ms质心ms':float(np.dot(power[:24000],np.arange(24000))/power[:24000].sum()/sr*1000),
                 '中位时刻ms':float(np.searchsorted(np.cumsum(power),power.sum()/2)/sr*1000)}
        report['模式检查'][str(mode)]=metrics
        sf.write(args.output/f'mode{mode}.wav',y.T,sr,subtype='FLOAT')
    phrase=np.zeros((2,sr*18),np.float32)
    for onset,freq,duration in [(1,130.81,1.3),(1.7,196,1.4),(2.5,261.63,1.3),(3.4,164.81,1.6),(4.1,220,1.4),(5,329.63,1.5)]:
        t=np.arange(int(sr*duration))/sr
        envelope=(1-np.exp(-t/.015))*np.exp(-t/.45)
        wave=sum(np.sin(2*np.pi*freq*k*t)/k**1.6 for k in range(1,9))*.06*envelope
        start=round(onset*sr);phrase[:,start:start+len(t)]+=wave.astype(np.float32)
    sf.write(args.output/'input_synthetic_phrase.wav',phrase.T,sr,subtype='FLOAT')
    presets=json.loads(args.presets.read_text())[:9]
    for i,preset in enumerate(presets):
        values=preset['values'];wet,actual=render(args.plugin,phrase,dict(values,mix=100))
        mixed,_=render(args.plugin,phrase,values)
        angle=values['mix']/100*np.pi/2
        error=float(abs(mixed-(phrase*np.cos(angle)+wet*np.sin(angle))).max())
        assert error<2e-6,error
        peak=float(abs(wet).max());assert peak<1,peak
        report['预设'][preset['name']]={'设置':values,'回读':actual,'纯湿峰值':peak,'纯湿RMS':float(np.sqrt(np.mean(wet*wet))),
                                            '插入混合误差':error,'输入说明':'同一合成谐波短句，不代表真实钢琴、弦乐或人声的听审'}
        sf.write(args.output/f'preset{i+1:02d}_wet.wav',wet.T,sr,subtype='FLOAT')
        sf.write(args.output/f'preset{i+1:02d}_insert.wav',mixed.T,sr,subtype='FLOAT')
        print('预设已验证：'+preset['name'],flush=True)
    (args.output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ['参数回读','预设']},ensure_ascii=False,indent=2))
if __name__=='__main__':main()
