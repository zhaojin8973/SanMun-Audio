#!/usr/bin/env python3
"""钢板S1与归档P2对照：只接受温和收束，不以P3的平滑度作为目标。"""
import argparse, hashlib, json, subprocess
from pathlib import Path
import numpy as np
import scipy.signal as sig
import soundfile as sf
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from audit_plate_character import DEFAULT, P2, measure
from check_plate_vst3 import render


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    group=parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--renderer',type=Path);group.add_argument('--plugin',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    sr=48000;counter=0;report={'状态':'钢板温和调整候选，仍需听审','基线':'candidates/0906G/SanMu Reverb.vst3','结果':{}}
    def candidate(x,values):
        nonlocal counter
        values=dict(values,mode=1)
        if args.plugin:return render(args.plugin,x,sr,values)[0]
        source=args.output/f'input{counter}.wav';dest=args.output/f'offline{counter}';counter+=1
        sf.write(source,x.T,sr,subtype='FLOAT')
        cmd=[str(args.renderer),'--source','wav','--input',str(source),'--tail','.1','--output',str(dest)]
        for key,value in values.items():cmd.extend(['--'+key.replace('_','-'),str(value)])
        subprocess.run(cmd,check=True,stdout=subprocess.DEVNULL)
        y,_=sf.read(dest/'output.wav',dtype='float32');return y[:x.shape[1]].T
    signals={};impulse=np.zeros((2,9*sr),np.float32);impulse[:,sr]=.1;signals['脉冲']=impulse
    for hz in [100,160,220,250,315,400]:
        x=np.zeros_like(impulse);t=np.arange(sr//4)/sr
        x[:,sr:sr+len(t)]=.06*np.sin(2*np.pi*hz*t)*np.sin(np.pi*np.arange(len(t))/(len(t)-1))**2
        signals[f'短音{hz}']=x
    curves={}
    for setting,values in [('新建默认',DEFAULT),('P2基准',P2),('关闭调制',dict(DEFAULT,mod_depth=0))]:
        report['结果'][setting]={}
        for kind,x in signals.items():
            if setting!='新建默认' and kind!='脉冲':continue
            old,_,_=render(Path(report['基线']),x,sr,values);new=candidate(x,values)
            assert np.isfinite(new).all()
            for name,y in [('P2',old),('S1',new)]:sf.write(args.output/f'{setting}_{kind}_{name}.wav',y.T,sr,subtype='FLOAT')
            onset=1 if kind=='脉冲' else 1.25
            a=measure(old,sr,onset);b=measure(new,sr,onset)
            row={'P2':a,'S1':b,'总能量变化dB':float(10*np.log10(b['能量']/a['能量'])),
                 '波形相关':float(np.corrcoef(old.ravel(),new.ravel())[0,1]),'频带变化':{}}
            for band,metric in a['频带'].items():
                after=b['频带'][band]
                row['频带变化'][band]={'能量变化dB':float(10*np.log10(after['能量']/metric['能量'])),
                                      '尾声缩短百分比':float(100*(1-after['尾声RT60']/metric['尾声RT60']))}
            if kind=='脉冲':
                # 轮廓与高频性格保留；不是要求谱峰谷尽可能小。
                assert row['波形相关']>.97,row
                assert abs(row['总能量变化dB'])<1
                for band in ['1400-2800','2800-5600','5600-10000']:
                    assert abs(row['频带变化'][band]['能量变化dB'])<.15
                assert 3<row['频带变化']['100-200']['尾声缩短百分比']<18
                if setting=='新建默认':curves={'P2':old,'S1':new}
            else:
                start,end=round(sr*1.65),round(sr*2.25)
                row['停止后400至1000ms能量变化dB']=float(10*np.log10(np.sum(new[:,start:end]**2)/np.sum(old[:,start:end]**2)))
            report['结果'][setting][kind]=row
        print('已测量 '+setting,flush=True)
    binary=args.plugin/'Contents/MacOS/SanMu Reverb' if args.plugin else args.renderer
    report['二进制SHA256']=hashlib.sha256(binary.read_bytes()).hexdigest()
    plt.rcParams['font.sans-serif']=['Arial Unicode MS'];plt.rcParams['axes.unicode_minus']=False
    fig,axes=plt.subplots(2,1,figsize=(11,7))
    for name,x in curves.items():
        f,psd=sig.welch(x[:,sr:],fs=sr,nperseg=32768,axis=1)
        axes[0].plot(f,10*np.log10(np.maximum(psd.mean(axis=0),1e-30)),label=name,linewidth=1)
        band=sig.sosfilt(sig.butter(3,[100,350],btype='bandpass',fs=sr,output='sos'),x[:,sr:],axis=1)
        energy=np.mean(band**2,axis=0);edc=np.cumsum(energy[::-1])[::-1]
        axes[1].plot(np.arange(len(edc))/sr,10*np.log10(np.maximum(edc,1e-30)),label=name)
    axes[0].set(xlim=(60,450),ylim=(-145,-100),xlabel='频率 / Hz',ylabel='脉冲输出功率谱 / dB')
    axes[1].set(xlim=(0,4),ylim=(-115,-30),xlabel='脉冲后时间 / 秒',ylabel='100～350Hz剩余能量 / dB')
    for ax in axes:ax.legend();ax.grid(alpha=.25)
    fig.suptitle('P2 → Classic Steel S1：保持共振轮廓，温和缩短低中频尾音')
    fig.tight_layout();fig.savefig(args.output/'温和调整对照.png',dpi=150)
    (args.output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')


if __name__=='__main__':main()
