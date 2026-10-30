#!/usr/bin/env python3
"""Plate真实VST3验证：实际参数、干湿路由、Hall保留和合成素材诊断。"""
import argparse,hashlib,json,time
from pathlib import Path
import numpy as np
import pedalboard
import soundfile as sf
from check_hall_vst3 import configured
from measure_hall_e1 import set_values

def render(path,x,sr,values):
    plugin=configured(path);set_values(plugin,values)
    start=time.perf_counter();y=plugin.process(x,sr,buffer_size=128,reset=True)
    elapsed=time.perf_counter()-start
    assert np.isfinite(y).all()
    return y,{k:str(v) for k,v in plugin.parameters.items()},elapsed

def repetition(x,sr):
    # 逐20ms均方包络消除衰减趋势，再测40～400ms重复相关；使用双声道功率合并。
    start,end=int(sr*.1),int(sr*1.1);z=x[:,start:end].astype(float)
    block=int(sr*.02);count=z.shape[1]//block
    z=z[:,:count*block].reshape(2,count,block)
    z=z/np.sqrt(np.maximum(np.mean(z*z,axis=2,keepdims=True),1e-30));z=z.reshape(2,-1)
    size=1
    while size<2*z.shape[1]:size*=2
    spectrum=np.fft.rfft(z,n=size,axis=1)
    ac=np.fft.irfft(np.abs(spectrum)**2,n=size,axis=1).sum(axis=0)
    return float(np.max(np.abs(ac[int(sr*.04):int(sr*.4)]))/ac[0])

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['plugin','baseline','render','presets','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--mode',type=int,choices=[0,1],default=0)
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    report={'状态':'自动检查；Plate候选尚待用户真实人声、军鼓复听','宿主':f'pedalboard {pedalboard.__version__}',
      '二进制SHA256':hashlib.sha256((args.plugin/'Contents/MacOS/SanMu Reverb').read_bytes()).hexdigest(),
      '基线SHA256':hashlib.sha256((args.baseline/'Contents/MacOS/SanMu Reverb').read_bytes()).hexdigest(),
      '保留检查':{},'预设':{},'控制':{}}
    manifest=json.loads((args.render/'manifest.json').read_text());sr=int(manifest['sample_rate'])
    values={k.replace('-','_'):v for k,v in manifest['parameters'].items()};values.update(category=0,mode=args.mode)
    x,_=sf.read(args.render/'input.wav',dtype='float32');expected,_=sf.read(args.render/'output.wav',dtype='float32');x=x.T.copy()
    actual,readback,elapsed=render(args.plugin,x,sr,values)
    difference=float(np.max(np.abs(actual.T-expected)));assert difference<2e-6,difference
    dry,_,_=render(args.plugin,x,sr,dict(values,mix=0));dry_error=float(np.max(abs(dry-x)));assert dry_error==0
    mixed,_,_=render(args.plugin,x,sr,dict(values,mix=24.8))
    mix_error=float(abs(mixed-x*np.cos(.248*np.pi/2)-actual*np.sin(.248*np.pi/2)).max());assert mix_error<2e-6
    report['路由']={'离线DSP最大样本差':difference,'干声端点误差':dry_error,'24.8%混合误差':mix_error,
        '参数回读':readback,'处理时间与音频时长比':elapsed/(x.shape[1]/sr)}
    sf.write(args.output/'pure_wet.wav',actual.T,sr,subtype='FLOAT')
    for control in ['early_diff','late_diff','attack']:
        a,_,_=render(args.plugin,x,sr,dict(values,**{control:0}));b,_,_=render(args.plugin,x,sr,dict(values,**{control:100}))
        an=a/np.sqrt(np.sum(a*a));bn=b/np.sqrt(np.sum(b*b));diff=float(np.sqrt(np.sum((an-bn)**2)))
        assert diff>.2
        report['控制'][control]={'匹配电平后结构差':diff,'零值周期相关峰':repetition(a,sr),'满值周期相关峰':repetition(b,sr)}
        sf.write(args.output/f'{control}_0.wav',a.T,sr,subtype='FLOAT');sf.write(args.output/f'{control}_100.wav',b.T,sr,subtype='FLOAT')
    # 相同输入、同一个初始化时间线验证全部保留类型与模式。
    impulse=np.zeros((2,int(sr*9)),np.float32);impulse[:,int(sr)]=.1
    noise=np.zeros_like(impulse);noise[:,int(sr):int(sr*1.3)]=np.random.default_rng(906).normal(0,.04,(2,int(sr*.3)))
    for kind,signal in [('脉冲',impulse),('独立立体声噪声',noise)]:
        for category in [1,2,3]:
            for mode in [0,1,2]:
                v={'category':category,'mode':mode}
                old,_,_=render(args.baseline,signal,sr,v);new,_,_=render(args.plugin,signal,sr,v)
                error=float(abs(old-new).max());assert error==0,(category,mode,error)
                report['保留检查'][f'{kind}_类型{category}_模式{mode}']=error
    presets=json.loads(args.presets.read_text())
    for preset in presets[:9]:
        old,_,_=render(args.baseline,noise,sr,preset['values']);new,_,_=render(args.plugin,noise,sr,preset['values'])
        error=float(abs(old-new).max());assert error==0
        report['保留检查'][preset['name']]=error
    # 合成素材仅用于覆盖连续谐波和宽带瞬态，不能替代真实素材听审。
    phrase=np.zeros((2,int(sr*12)),np.float32);hits=phrase.copy()
    for onset,freq in [(1,130.81),(1.7,196),(2.5,261.63),(3.4,164.81),(4.1,220),(5,329.63)]:
        t=np.arange(int(sr*1.2))/sr
        wave=sum(np.sin(2*np.pi*freq*k*t)/k**1.6 for k in range(1,9))*.05*(1-np.exp(-t/.012))*np.exp(-t/.38)
        pos=int(sr*onset);phrase[:,pos:pos+len(t)]+=wave.astype(np.float32)
    for index,onset in enumerate([1,2.5,4,5.5]):
        t=np.arange(int(sr*.5))/sr
        wave=(np.random.default_rng(index+80).normal(0,1,len(t))*.15*np.exp(-t/.055)+.1*np.sin(2*np.pi*185*t)*np.exp(-t/.09))
        pos=int(sr*onset);hits[:,pos:pos+len(t)]+=wave.astype(np.float32)
    for name,signal in [('合成谐波短句',phrase),('合成噪声鼓击',hits)]:
        sf.write(args.output/f'{name}_干声.wav',signal.T,sr,subtype='FLOAT')
        for preset in presets[0:3]:
            wet,readback,elapsed=render(args.plugin,signal,sr,preset['values']);peak=float(abs(wet).max());assert peak<1
            report['预设'][name+'_'+preset['name']]={'设置':preset['values'],'回读':readback,'峰值':peak,
                    '处理时间与音频时长比':elapsed/(signal.shape[1]/sr)}
            sf.write(args.output/f'{name}_{preset["name"]}.wav',wet.T,sr,subtype='FLOAT')
    # 商业参考由独立的音色审计工具按真实回读参数对照；
    # 不再从相邻目录隐式拿取旧版2.39秒参考，避免默认3.99秒被错误比较。
    (args.output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ['预设']},ensure_ascii=False,indent=2))
if __name__=='__main__':main()
