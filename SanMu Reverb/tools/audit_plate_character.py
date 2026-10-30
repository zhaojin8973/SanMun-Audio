#!/usr/bin/env python3
"""检查板式在共同默认参数下的频谱、建立时间和低频共振；渲染真实插件并保留原始输出。"""
import argparse,hashlib,json,subprocess
from pathlib import Path
import numpy as np
import scipy.signal as sig
import soundfile as sf
import pedalboard
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from check_plate_vst3 import render
from render_reference import PARAMETERS

DEFAULT=dict(category=0,mode=0,decay=3.99,predelay=20,mix=100,high_freq=6000,high_shelf=-24,bass_freq=300,bass_mult=1.5,
             size=100,attack=50,early_diff=100,late_diff=100,mod_rate=2.53,mod_depth=38,low_cut=10,high_cut=8000)
P2=dict(DEFAULT,decay=2.39,predelay=15,high_shelf=-18,bass_mult=1,attack=30,mod_rate=.8,mod_depth=20,low_cut=20,high_cut=9000)

def measure(x,sr,onset=1):
 z=x[:,int(sr*onset):].astype(float);power=np.mean(z*z,axis=0);total=power.sum();end=int(sr*.5)
 result={'能量':float(total),'峰值':float(abs(z).max()),'前500ms质心ms':float(np.dot(power[:end],np.arange(end))/power[:end].sum()/sr*1000),
         '前80ms能量占比':float(power[:int(sr*.08)].sum()/total),'左右相关':float(np.corrcoef(z)[0,1]),'频带':{}}
 for low,high in [(60,100),(100,200),(200,350),(350,700),(700,1400),(1400,2800),(2800,5600),(5600,10000)]:
  y=sig.sosfilt(sig.butter(3,[low,high],btype='bandpass',fs=sr,output='sos'),z,axis=1)
  e=np.mean(y*y,axis=0);edc=np.cumsum(e[::-1])[::-1];db=10*np.log10(np.maximum(edc,1e-30)/max(edc[0],1e-30));mask=(db<-5)&(db>-25)
  rt=-60/np.polyfit(np.arange(len(db))[mask]/sr,db[mask],1)[0]
  result['频带'][f'{low}-{high}']={'能量':float(e.sum()),'尾声RT60':float(rt)}
 # 白脉冲输出的带内谱起伏，包含频谱斜率；仅作统一条件下的诊断，不能等同听感嗡鸣。
 f,p=sig.welch(z,fs=sr,nperseg=32768,axis=1);db=10*np.log10(np.maximum(p.mean(axis=0),1e-30))
 band=(f>=80)&(f<400);result['低中频谱起伏P95-P10_dB']=float(np.percentile(db[band],95)-np.percentile(db[band],10))
 return result

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--plugin',type=Path);p.add_argument('--renderer',type=Path);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--references',action='store_true');args=p.parse_args();args.output.mkdir(parents=True,exist_ok=False);sr=48000
 counter=0
 def candidate(x,values):
  nonlocal counter
  if args.plugin:return render(args.plugin,x,sr,values)[:2]
  if not args.renderer:raise ValueError('请指定plugin或renderer')
  input_path=args.output/f'offline_input{counter}.wav';directory=args.output/f'offline{counter}';counter+=1
  sf.write(input_path,x.T,sr,subtype='FLOAT')
  command=[str(args.renderer),'--source','wav','--input',str(input_path),'--tail','.1','--output',str(directory)]
  for k,v in values.items():command.extend(['--'+k.replace('_','-'),str(v)])
  subprocess.run(command,check=True,stdout=subprocess.DEVNULL)
  y,_=sf.read(directory/'output.wav',dtype='float32');return y[:x.shape[1]].T,values
 signals={};impulse=np.zeros((2,sr*9),np.float32);impulse[:,sr]=.1;signals['impulse']=impulse
 for hz in [100,160,220,250,315,400]:
  x=np.zeros_like(impulse);t=np.arange(int(sr*.25))/sr
  x[:,sr:sr+len(t)]=(.06*np.sin(2*np.pi*hz*t)*np.sin(np.pi*np.arange(len(t))/(len(t)-1))**2).astype(np.float32);signals[f'tone{hz}']=x
 rows={};callbacks={
  'plate_default':lambda x:candidate(x,DEFAULT),
  'hall_default':lambda x:candidate(x,dict(DEFAULT,category=2)),
  'plate_p2':lambda x:candidate(x,P2)}
 if args.references:
  def reference(x,mode,color):
   plugin=pedalboard.load_plugin('/Library/Audio/Plug-Ins/VST3/ValhallaVintageVerb.vst3')
   values=dict(PARAMETERS,mix=100,reverbmode=mode,colormode=color,highcut=8000)
   for k,v in values.items():setattr(plugin,k,v)
   y=plugin.process(x,sr,buffer_size=128,reset=True)
   assert np.isfinite(y).all()
   return y,{k:str(getattr(plugin,k)) for k in values}
  for mode in ['Plate','Concert Hall']:
   for color in ['now','seventies']:
    callbacks['vvv_'+mode.replace(' ','_')+'_'+color]=lambda x,m=mode,c=color:reference(x,m,c)
 for name,callback in callbacks.items():
  rows[name]={}
  for kind,x in signals.items():
   if name=='plate_p2' and kind!='impulse':continue
   y,actual=callback(x);sf.write(args.output/f'{name}_{kind}.wav',y.T,sr,subtype='FLOAT')
   rows[name][kind]=measure(y,sr,1 if kind=='impulse' else 1.25)
   if kind=='impulse':rows[name]['参数回读']=actual
  print('已渲染 '+name,flush=True)
 for kind,x in signals.items():sf.write(args.output/f'input_{kind}.wav',x.T,sr,subtype='FLOAT')
 plt.rcParams['font.sans-serif']=['Arial Unicode MS'];plt.rcParams['axes.unicode_minus']=False
 fig,axes=plt.subplots(2,1,figsize=(12,8))
 for name in callbacks:
  z,_=sf.read(args.output/f'{name}_impulse.wav');z=z[sr:];w=480
  power=np.mean(z[:len(z)//w*w].reshape(-1,w,2)**2,axis=(1,2));axes[0].plot(np.arange(len(power))*.01,10*np.log10(np.maximum(power,1e-30)),label=name)
  f,s=sig.welch(z,fs=sr,nperseg=32768,axis=0);axes[1].plot(f,10*np.log10(np.maximum(s.mean(axis=1),1e-30)),label=name,linewidth=.7)
 axes[0].set_xlim(0,.6);axes[0].set_ylim(-105,-55);axes[0].set_xlabel('脉冲后时间 / 秒');axes[0].set_ylabel('10ms均方电平 / dBFS');axes[0].legend(fontsize=8,ncol=2)
 axes[1].set_xlim(60,450);axes[1].set_ylim(-135,-90);axes[1].set_xlabel('频率 / Hz');axes[1].set_ylabel('功率谱 / dB');axes[1].legend(fontsize=8,ncol=2)
 for ax in axes:ax.grid(alpha=.25)
 fig.tight_layout();fig.savefig(args.output/'原始数据.png',dpi=150)
 report={'二进制SHA256':hashlib.sha256((args.plugin/'Contents/MacOS/SanMu Reverb' if args.plugin else args.renderer).read_bytes()).hexdigest(),'默认参数':DEFAULT,'结果':rows}
 (args.output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':main()
