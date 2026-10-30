#!/usr/bin/env python3
"""测量板式低频：原始频带包络及短音激励后的能量起伏，不用静态电平代替稳定性。"""
import argparse,json,sys,subprocess
from pathlib import Path
import numpy as np
import scipy.signal as sig
import soundfile as sf
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from check_plate_vst3 import render

VALUES=dict(category=0,mode=0,decay=2.39,predelay=15,mix=100,high_freq=6000,high_shelf=-18,
 bass_freq=300,bass_mult=1,size=100,attack=30,early_diff=100,late_diff=100,mod_rate=.8,mod_depth=20,low_cut=20,high_cut=9000)

def measure(x,sr,start):
 z=x[:,int(start*sr):].astype(float)
 power=np.mean(z*z,axis=0);edc=np.cumsum(power[::-1])[::-1]
 db=10*np.log10(np.maximum(edc,1e-30)/max(edc[0],1e-30));mask=(db<-5)&(db>-25)
 rt=None
 if mask.sum()>100:
  slope=np.polyfit(np.arange(len(db))[mask]/sr,db[mask],1)[0];rt=-60/slope
 w=int(sr*.03);rms=np.mean(power[:len(power)//w*w].reshape(-1,w),axis=1)
 # 去掉最初100ms，测建立后的30ms能量反弹，保留绝对时间与原始增益。
 t=np.arange(len(rms))*.03
 logs=10*np.log10(np.maximum(rms,1e-30));chosen=(t>=.1)&(t<.9)
 rise=float(np.max(np.diff(logs[chosen])))
 return dict(energy=float(power.sum()),rt60=rt,max_30ms_rebound_db=rise),t,logs

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--plugin',type=Path);a.add_argument('--renderer',type=Path);a.add_argument('--output',type=Path,required=True)
 args=a.parse_args();args.output.mkdir(parents=True,exist_ok=False);sr=48000
 if bool(args.plugin)==bool(args.renderer):raise ValueError('需要且只能指定plugin或renderer之一')
 counter=0
 def run(signal):
  nonlocal counter
  if args.plugin:return render(args.plugin,signal,sr,VALUES)
  directory=args.output/f'offline_{counter}';input_path=args.output/f'input_{counter}.wav';counter+=1
  sf.write(input_path,signal.T,sr,subtype='FLOAT')
  command=[str(args.renderer),'--source','wav','--input',str(input_path),'--tail','.1','--output',str(directory)]
  for k,v in VALUES.items():command.extend(['--'+k.replace('_','-'),str(v)])
  subprocess.run(command,check=True,stdout=subprocess.DEVNULL)
  data,_=sf.read(directory/'output.wav',dtype='float32')
  return data[:signal.shape[1]].T,VALUES,0
 impulse=np.zeros((2,sr*7),np.float32);impulse[:,sr]=.1
 y,readback,_=run(impulse);sf.write(args.output/'input_impulse.wav',impulse.T,sr,subtype='FLOAT');sf.write(args.output/'impulse.wav',y.T,sr,subtype='FLOAT')
 plt.rcParams['font.sans-serif']=['Arial Unicode MS'];plt.rcParams['axes.unicode_minus']=False
 fig,axes=plt.subplots(2,1,figsize=(11,8));report={'参数回读':readback,'频带':{},'短音':{}}
 for low,high in [(40,100),(100,200),(200,350),(350,700),(700,1400),(1400,2800)]:
  z=sig.sosfilt(sig.butter(3,[low,high],btype='bandpass',fs=sr,output='sos'),y,axis=1)
  m,t,db=measure(z,sr,1);key=f'{low}-{high}';report['频带'][key]=m;axes[0].plot(t,db,label=key+' Hz')
 for hz in [60,80,100,130,160,200,250,315,400,630]:
  x=np.zeros_like(impulse);t=np.arange(int(sr*.25))/sr
  wave=.06*np.sin(2*np.pi*hz*t)*np.sin(np.pi*np.arange(len(t))/(len(t)-1))**2;x[:,sr:sr+len(t)]=wave
  z,_,_=run(x);sf.write(args.output/f'tone_{hz}.wav',z.T,sr,subtype='FLOAT')
  m,t,db=measure(z,sr,1.25);report['短音'][str(hz)]=m
  if hz in [80,130,200,250,400]:axes[1].plot(t,db,label=f'{hz} Hz')
 for ax in axes:ax.set_xlim(0,1.5);ax.grid(alpha=.25);ax.legend(ncol=3);ax.set_ylabel('30ms均方电平 / dBFS')
 axes[0].set_ylim(-125,-70);axes[1].set_ylim(-85,-25)
 axes[0].set_title('同一脉冲的各频带尾声，原始电平');axes[1].set_title('250ms平滑短音停止后的尾声，原始电平');axes[1].set_xlabel('激励后时间 / 秒')
 fig.tight_layout();fig.savefig(args.output/'低频原始数据.png',dpi=140)
 (args.output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='参数回读'},ensure_ascii=False,indent=2))
if __name__=='__main__':main()
