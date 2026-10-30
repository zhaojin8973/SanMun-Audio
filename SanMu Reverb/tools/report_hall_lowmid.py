#!/usr/bin/env python3
"""对照冻结初稿，验证晚期频带变化，并检查密度、时序、宽度与电平的保留程度。"""
import argparse
import json
from pathlib import Path
import numpy as np
import scipy.signal as signal
import soundfile as sf
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def band_data(audio,sr,lo,hi):
    y=signal.sosfilt(signal.butter(4,[lo,hi],btype='bandpass',fs=sr,output='sos'),audio,axis=0)
    energy=np.mean(y*y,axis=1);tail=np.cumsum(energy[::-1])[::-1]
    db=10*np.log10(np.maximum(tail,1e-30)/tail[0]);t=np.arange(len(y))/sr
    use=(db<-5)&(db>-25);fit=np.polyfit(t[use],db[use],1)
    return float(energy[int(.3*sr):int(3*sr)].sum()),float(-60/fit[0]),db

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--audit',type=Path,required=True)
    p.add_argument('--baseline',type=Path,required=True)
    p.add_argument('--baseline-label',default='0906A')
    p.add_argument('--candidate-label',default='0906B候选')
    p.add_argument('--profile',choices=['175','225'],default='175',help='按预先设计的175Hz加深或225Hz延展目标验证')
    args=p.parse_args();out=args.audit/'summary';out.mkdir(exist_ok=False)
    a=json.loads((args.baseline/'report.json').read_text())['measurements']
    b=json.loads((args.audit/'controls/report.json').read_text())['measurements']
    for k,v in b.items():assert a[k]['input_pcm_sha256']==v['input_pcm_sha256']
    base,sr=sf.read(args.baseline/'SanMu_base_impulse_lead1.wav')
    candidate,sr2=sf.read(args.audit/'controls/SanMu_base_impulse_lead1.wav');assert sr==sr2
    base=base[sr:];candidate=candidate[sr:]
    report={'状态':f'{args.candidate_label}的客观验证，真实人声听感待用户复听','对照版本':args.baseline_label,'晚期频带':{},'结构保留':{},'固定音高':{}}
    plt.rcParams['font.family']='Arial Unicode MS';fig,ax=plt.subplots(2,2,figsize=(12,8))
    extend=args.profile=='225'
    bands=([(70,110),(150,200),(210,300),(300,450),(600,1200),(2000,6000)] if extend
           else [(70,110),(150,200),(250,400),(500,1000)])
    decay_band=210 if extend else 150
    for lo,hi in bands:
        e0,t0,d0=band_data(base,sr,lo,hi);e1,t1,d1=band_data(candidate,sr,lo,hi)
        change=float(10*np.log10(e1/e0))
        report['晚期频带'][f'{lo}～{hi}Hz']={'300～3000ms能量变化_dB':change,f'{args.baseline_label}_T20外推秒':t0,'候选_T20外推秒':t1}
        assert abs(t1/t0-1)<.05
        if extend:
            bounds={70:(-.15,.15),150:(-.4,.4),210:(-1.6,-.65),300:(-.95,-.35),600:(-.3,.05),2000:(-.04,.04)}
            low,high=bounds[lo];assert low<change<high,(lo,change)
        elif lo==150:assert -1.5<change<-.65
        else:assert abs(change)<.5
        if lo==decay_band:
            for label,d in [(args.baseline_label,d0),(args.candidate_label,d1)]:ax[0,1].plot(np.arange(len(d))/sr,d,label=label)
    ax[0,0].bar(list(report['晚期频带']),[v['300～3000ms能量变化_dB'] for v in report['晚期频带'].values()])
    ax[0,0].set(title='晚期声场频带变化',ylabel=f'候选相对{args.baseline_label}／dB',ylim=(-1.6,.4))
    ax[0,0].tick_params(axis='x',labelrotation=25,labelsize=9)
    ax[0,1].set(title=('210～300Hz' if extend else '150～200Hz')+' 归一化衰减',xlim=(0,4),ylim=(-45,0),xlabel='输入后秒',ylabel='剩余能量／dB');ax[0,1].legend()
    for key in ['SanMu_base_impulse_lead1','SanMu_base_noise_lead1','SanMu_base_chord_lead1']:
        old,new=a[key],b[key]
        values={'总电平变化_dB':float(10*np.log10(new['energy']/old['energy'])),
                '中位时刻变化_ms':new['median_ms']-old['median_ms'],
                '晚期密度变化':new['density_200_500']-old['density_200_500'],
                '左右相关变化':new['lr_correlation']-old['lr_correlation']}
        report['结构保留'][key]=values
        # 延展目标包含这组和弦的220/262/330Hz，电平应相应降低；宽带仍用原有约束。
        if extend and 'chord' in key:assert -1.5<values['总电平变化_dB']<.05
        else:assert abs(values['总电平变化_dB'])<.3
        # 和弦的能量中位时刻会随不同音高的权重变化，不能单独作为声场时序不变的契约。
        # 用脉冲与宽带噪声验证原有建立过程；和弦仍完整报告数值，并检查电平及声像。
        if 'chord' not in key:assert abs(values['中位时刻变化_ms'])<5
        assert abs(values['晚期密度变化'])<.03 and abs(values['左右相关变化'])<.04
    first=int(.04*sr)
    report['前40ms最大样本差']=float(np.max(abs(base[:first]-candidate[:first])))
    assert report['前40ms最大样本差']==0
    old=json.loads((args.audit/'before/report.json').read_text())['measurements']
    new=json.loads((args.audit/'after/report.json').read_text())['measurements']
    for hz in sorted(set(v['frequency'] for v in new.values())):
        keys=[k for k,v in new.items() if v['frequency']==hz]
        for k in keys:assert new[k]['input_pcm_sha256']==old[k]['input_pcm_sha256']
        before=np.mean([old[k]['tail_100_800_rms']**2 for k in keys])
        after=np.mean([new[k]['tail_100_800_rms']**2 for k in keys])
        report['固定音高'][str(hz)]=float(10*np.log10(after/before))
    ax[1,0].semilogx([float(k) for k in report['固定音高']],list(report['固定音高'].values()),'o-')
    ax[1,0].axvspan(210 if extend else 150,300 if extend else 200,alpha=.1,color='brown')
    ax[1,0].set(title='停音后100～800ms，两个启动相位的能量均值',xlabel='音高／Hz',ylabel=f'候选相对{args.baseline_label}／dB',ylim=(-1.6,.45))
    for label,directory in [(args.baseline_label,args.baseline),(args.candidate_label,args.audit/'controls')]:
        y,rate=sf.read(directory/'SanMu_base_impulse_lead1.wav');e=np.mean(y[rate:rate*2]**2,axis=1).reshape(-1,480).mean(axis=1)
        ax[1,1].plot(np.arange(100)*10+5,10*np.log10(e+1e-30),label=label)
    ax[1,1].set(title='原始湿声建立包络',xlabel='输入后毫秒',ylabel='能量／dB',xlim=(20,800),ylim=(-90,-45));ax[1,1].legend()
    for axis in ax.flat:axis.grid(alpha=.15)
    fig.tight_layout();fig.savefig(out/'微调核查.png',dpi=150)
    (out/'结论.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__':main()
