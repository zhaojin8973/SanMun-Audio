#!/usr/bin/env python3
"""Plate首稿离线诊断：保留原始增益，绘图用于检查时间结构与音色分布。"""
import argparse,json,subprocess
from pathlib import Path
import numpy as np
import soundfile as sf
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

BASE={'category':0,'decay':2.39,'predelay':15,'mix':100,'high-freq':6000,'high-shelf':-18,
      'bass-freq':300,'bass-mult':1,'size':100,'attack':30,'early-diff':100,'late-diff':100,
      'mod-rate':.8,'mod-depth':20,'low-cut':20,'high-cut':9000}

def metrics(x,sr):
    windows=x[:len(x)//960*960].reshape(-1,960,2)
    power=np.mean(windows**2,axis=(1,2))
    spectrum=np.abs(np.fft.rfft(x,axis=0))**2
    hz=np.fft.rfftfreq(len(x),1/sr)
    bands={f'{a}-{b}':float(spectrum[(hz>=a)&(hz<b)].sum()) for a,b in [(20,150),(150,350),(350,1500),(1500,5000),(5000,15000)]}
    total=sum(bands.values())
    return dict(energy=float(np.sum(x*x)),peak=float(np.max(np.abs(x))),correlation=float(np.corrcoef(x.T)[0,1]),
                band_energy_fraction={k:v/total for k,v in bands.items()}),power

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--renderer',type=Path,default=Path('build-hall-f1/source/sanmu_hall_render'))
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    variants={'base':{},'early0':{'early-diff':0},'late0':{'late-diff':0},'attack0':{'attack':0},'attack100':{'attack':100},
              'reference_settings':{'attack':50,'mod-rate':2.53,'mod-depth':38,'low-cut':10,'high-cut':8010}}
    rows={};plt.rcParams['font.sans-serif']=['Arial Unicode MS'];plt.rcParams['axes.unicode_minus']=False
    fig,axes=plt.subplots(2,1,figsize=(11,8))
    for name,values in variants.items():
        command=[str(args.renderer),'--output',str(args.output/name)]
        for k,v in (BASE|values).items():command.extend(['--'+k,str(v)])
        subprocess.run(command,check=True,stdout=subprocess.DEVNULL)
        x,sr=sf.read(args.output/name/'output.wav');rows[name],power=metrics(x,sr)
        axes[0].plot(np.arange(len(power))*.02,10*np.log10(np.maximum(power,1e-30)),label=name)
    for name in ['old_base','reference_plate']:
        x,sr=sf.read(args.output.parent/(name+'.wav'));x=x[sr:]
        rows[name],power=metrics(x,sr)
        axes[1].plot(np.arange(len(power))*.02,10*np.log10(np.maximum(power,1e-30)),label=name)
    x,sr=sf.read(args.output/'reference_settings/output.wav');_,power=metrics(x,sr)
    axes[1].plot(np.arange(len(power))*.02,10*np.log10(np.maximum(power,1e-30)),label='Plate P1 同参数')
    for ax in axes:
        ax.set_xlim(0,1);ax.set_ylim(-130,-50);ax.set_ylabel('20ms窗口均方电平 / dBFS');ax.legend();ax.grid(alpha=.25)
    axes[0].set_title('Plate P1 控制端点：原始输出，未匹配音量');axes[1].set_title('同输入与同参数：旧版、P1、参考；起点按输入脉冲对齐')
    axes[1].set_xlabel('脉冲后时间 / 秒');fig.tight_layout();fig.savefig(args.output/'测量.png',dpi=150)
    rows['same_parameter_energy_difference_db']=10*np.log10(rows['reference_settings']['energy']/rows['reference_plate']['energy'])
    (args.output/'summary.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(rows,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
