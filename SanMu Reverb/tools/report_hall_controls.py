#!/usr/bin/env python3
"""汇总同一输入的原始电平、Attack 时序与 Late 密度，生成可核查图表。"""
import argparse
import json
from pathlib import Path
import numpy as np
import soundfile as sf
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before',type=Path,required=True)
    p.add_argument('--after',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    args.output.mkdir(parents=True,exist_ok=False)
    before=json.loads((args.before/'report.json').read_text())['measurements']
    after=json.loads((args.after/'report.json').read_text())['measurements']
    db=lambda ratio:float(10*np.log10(ratio))
    report={'状态':'固定输入的客观检查，不能替代听审','增益差_dB':{},'Attack':{},'Late':{}}
    for kind in ['impulse','burst','noise','chord']:
        key=f'base_{kind}_lead1'
        f1=before[f'SanMu_{key}'];f2=after[f'SanMu_{key}'];ref=before[f'VVV_{key}']
        assert f1['input_pcm_sha256']==f2['input_pcm_sha256']==ref['input_pcm_sha256']
        values={'F1相对VVV':db(f1['energy']/ref['energy']),
                'F2相对VVV':db(f2['energy']/ref['energy'])}
        if f'Supernova_{key}' in before:
            values['F2相对Supernova']=db(f2['energy']/before[f'Supernova_{key}']['energy'])
        report['增益差_dB'][kind]=values
        assert abs(values['F2相对VVV'])<3, (kind,values)
    for label,source,family in [('F1',before,'SanMu'),('F2',after,'SanMu'),('VVV',before,'VVV')]:
        report['Attack'][label]={v:source[f'{family}_{n}_impulse_lead1']['median_ms']
            for v,n in [('0','attack0'),('50','base'),('100','attack100')]}
        report['Late'][label]={v:source[f'{family}_{n}_impulse_lead1']['density_200_500']
            for v,n in [('0','late0'),('50','late50'),('100','base')]}
    a=report['Attack']['F2'];d=report['Late']['F2']
    assert a['0']<a['50']<a['100'] and a['100']-a['0']>150
    assert d['0']<.55 and d['50']>d['0']+.15 and d['50']<d['100']-.05 and d['100']>.8
    report['静音预热检查_dB']={}
    for label,source,family in [('F1',before,'SanMu'),('F2',after,'SanMu'),('VVV',before,'VVV')]:
        e1=source[f'{family}_base_impulse_lead1']['energy']
        e3=source[f'{family}_base_impulse_lead3']['energy']
        report['静音预热检查_dB'][label]=db(e1/e3)
        assert abs(db(e1/e3))<.5
    report['参考冷启动相对预热_dB']=db(before['VVV_base_impulse_lead0']['energy']/before['VVV_base_impulse_lead1']['energy'])
    (args.output/'结论.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    plt.rcParams['font.family']='Arial Unicode MS'
    fig,ax=plt.subplots(2,3,figsize=(15,8))
    for i,(label,directory,family) in enumerate([('F1',args.before,'SanMu'),('F2',args.after,'SanMu'),('VVV',args.before,'VVV')]):
        for name,caption in [('attack0','Attack 0'),('base','Attack 50'),('attack100','Attack 100')]:
            y,sr=sf.read(directory/f'{family}_{name}_impulse_lead1.wav')
            power=np.mean(y[sr:sr*2]**2,axis=1).reshape(-1,480).mean(axis=1)
            ax[0,i].plot(np.arange(100)*10+5,10*np.log10(power/power.sum()+1e-14),label=caption)
        ax[0,i].set(title=f'{label}：建立过程',xlim=(0,800),ylim=(-50,-5),xlabel='输入后毫秒',ylabel='归一化能量／dB')
        ax[0,i].legend()
    names=['脉冲','短噪声','持续噪声','和弦'];xx=np.arange(4)
    for i,label in enumerate(['F1相对VVV','F2相对VVV']):
        ax[1,0].bar(xx+i*.35,[v[label] for v in report['增益差_dB'].values()],.35,label=label)
    ax[1,0].set(xticks=xx+.175,xticklabels=names,ylabel='原始湿声电平差／dB',title='与预热后参考的电平差')
    ax[1,0].axhline(0,color='black',lw=.5);ax[1,0].legend()
    for label,values in report['Late'].items():ax[1,1].plot([0,50,100],list(values.values()),'o-',label=label)
    ax[1,1].set(xlabel='Late／%',ylabel='200～500 ms 回声密度',title='高斯参考值为1，不是相似度分数');ax[1,1].legend()
    for label,values in report['Attack'].items():ax[1,2].plot([0,50,100],list(values.values()),'o-',label=label)
    ax[1,2].set(xlabel='Attack／%',ylabel='累计一半能量到达时刻／ms',title='前后距离的时序代理量');ax[1,2].legend()
    fig.tight_layout();fig.savefig(args.output/'参数核查.png',dpi=150)
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__':main()
