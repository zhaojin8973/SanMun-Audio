#!/usr/bin/env python3
"""固定静音预热、真实参数回读，比较湿声增益、建立包络及扩散端点。"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import pedalboard
import soundfile as sf
from check_hall_vst3 import configured
from render_reference import PARAMETERS

SR = 48000

def probe(kind, lead=1):
    x = np.zeros((2, SR*10), np.float32)
    start = int(lead*SR)
    rng = np.random.default_rng(20260905)
    if kind == 'impulse': x[:,start] = .1
    elif kind == 'noise': x[:,start:start+SR*3] = rng.normal(0,.04,(2,SR*3))
    elif kind == 'burst': x[:,start:start+int(.03*SR)] = rng.normal(0,.04,int(.03*SR))
    elif kind == 'chord':
        t = np.arange(SR)/SR
        wave = sum(np.sin(2*np.pi*f*t) for f in [220,261.6256,329.6276])*.045
        wave *= np.minimum(t/.01,1)*np.minimum((1-t)/.02,1)
        x[:,start:start+SR] = wave
    return x

def measures(y,lead=1):
    z = y[:,int(lead*SR):].astype(np.float64)
    power = np.mean(z*z,axis=0)
    energy = float(power.sum())
    short = power[:int(.5*SR)]
    bins = power[:int(.8*SR)].reshape(-1,480).mean(axis=1)
    density = []
    for at in np.arange(.08,.8,.02):
        window = z[:,int(at*SR):int((at+.02)*SR)]
        rms = np.sqrt(np.mean(window**2,axis=1))[:,None]
        density.append(float(np.mean(abs(window)>rms)/.3173105))
    return dict(energy=energy,peak=float(abs(y).max()),rms=float(np.sqrt(np.mean(y*y))),
        active_rms=float(np.sqrt(np.mean(z[:,:SR*3]**2))),
        centroid_500_ms=float(np.dot(short,np.arange(len(short))/SR)/max(short.sum(),1e-30)*1000),
        median_ms=float(np.searchsorted(np.cumsum(power),energy*.5)/SR*1000),
        c80_db=float(10*np.log10(max(power[:3840].sum(),1e-30)/max(power[3840:].sum(),1e-30))),
        first_120_energy=float(power[:5760].sum()),late_250_600_energy=float(power[12000:28800].sum()),
        peak_10ms_time_ms=float((np.argmax(bins)+.5)*10),density_80_200=float(np.mean(density[:6])),
        density_200_500=float(np.mean(density[6:21])),
        lr_correlation=float(np.corrcoef(z)[0,1]))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--plugin',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--reference',action='store_true')
    p.add_argument('--supernova',action='store_true')
    args=p.parse_args()
    args.output.mkdir(parents=True,exist_ok=False)
    variants=[('base',50,100,100),('attack0',0,100,100),('attack100',100,100,100),
              ('late0',50,100,0),('late50',50,100,50),('early0_attack0',0,0,100),
              ('early0_attack100',100,0,100)]
    report={'说明':'相同原始输入，100%湿声，无电平归一化；主测量前置1秒静音。',
            'sample_rate':SR,'pedalboard':pedalboard.__version__,'measurements':{},'parameters':{}}
    families=['SanMu']+(['VVV'] if args.reference else [])+(['Supernova'] if args.supernova else [])
    for family in families:
        for name,attack,early,late in (variants if family!='Supernova' else variants[:1]):
            cases=[('impulse',1),('burst',1)]
            if name=='base': cases += [('impulse',0),('impulse',3),('noise',1),('chord',1)]
            for kind,lead in cases:
                if family=='SanMu':
                    plugin=configured(args.plugin)
                    for key,value in [('attack',attack),('early_diff',early),('late_diff',late)]:
                        plugin.parameters[key].raw_value=value/100
                    actual={k:str(v) for k,v in plugin.parameters.items()}
                elif family=='VVV':
                    plugin=pedalboard.load_plugin('/Library/Audio/Plug-Ins/VST3/ValhallaVintageVerb.vst3')
                    requested=dict(PARAMETERS,mix=100,attack=attack,earlydiffusion=early,latediffusion=late)
                    for k,v in requested.items(): setattr(plugin,k,v)
                    actual={k:str(getattr(plugin,k)) for k in requested}
                    for k,v in requested.items():
                        got=getattr(plugin,k)
                        assert abs(float(got)-v)<.011 if isinstance(v,(int,float)) and not isinstance(v,bool) else got==v
                else:
                    plugin=pedalboard.load_plugin('/Library/Audio/Plug-Ins/VST3/Supernova Verb.vst3')
                    # 仅作第二个电平尺度参考；参数语义不同，不宣称与 VVV 完全对齐。
                    plugin.global_mix=100
                    plugin.reverb_mode='Vintage Hall'
                    plugin.reverb_decay_time_s=4.0
                    plugin.reverb_pre_delay_ms=20
                    actual={k:str(v) for k,v in plugin.parameters.items()}
                x=probe(kind,lead)
                y=plugin.process(x,SR,buffer_size=128,reset=True)
                assert y.shape==x.shape and np.isfinite(y).all()
                key=f'{family}_{name}_{kind}_lead{lead}'
                sf.write(args.output/f'{key}.wav',y.T,SR,subtype='FLOAT')
                inputfile=args.output/f'input_{kind}_lead{lead}.wav'
                if not inputfile.exists(): sf.write(inputfile,x.T,SR,subtype='FLOAT')
                report['measurements'][key]=dict(measures(y,lead),input_pcm_sha256=hashlib.sha256(x.tobytes()).hexdigest())
                report['parameters'][f'{family}_{name}']=actual
                print(key,json.dumps(report['measurements'][key],ensure_ascii=False),flush=True)
    binary=args.plugin/'Contents/MacOS/SanMu Reverb'
    report['sanmu_binary_sha256']=hashlib.sha256(binary.read_bytes()).hexdigest()
    (args.output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')

if __name__=='__main__': main()
