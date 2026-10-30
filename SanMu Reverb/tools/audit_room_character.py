#!/usr/bin/env python3
"""保存房间原始脉冲及控制端点，所有输入为合成诊断信号。"""
import argparse,json,hashlib
from pathlib import Path
import numpy as np
import soundfile as sf
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from audit_plate_character import DEFAULT,measure
from check_plate_vst3 import render
p=argparse.ArgumentParser();p.add_argument('--plugin',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False);sr=48000
x=np.zeros((2,sr*7),np.float32);x[:,sr]=.1
base=dict(DEFAULT,category=1,decay=.8,predelay=5,bass_mult=1,mod_depth=0)
cases={'共同默认':dict(DEFAULT,category=1),'紧凑基准':base,'开阔基准':dict(base,mode=1)}
for key in ['early_diff','late_diff','attack']:
 for value in [0,100]:cases[key+str(value)]=dict(base,**{key:value})
rows={};waves={}
for name,v in cases.items():
 y,_,_=render(a.plugin,x,sr,v);waves[name]=y;rows[name]={'参数':v,'测量':measure(y,sr)}
 sf.write(a.output/(name+'.wav'),y.T,sr,subtype='FLOAT')
for key in ['early_diff','late_diff','attack']:
 lo,hi=waves[key+'0'],waves[key+'100'];rows[key+'差异']={'最大样本差':float(abs(lo-hi).max()),'归一化形状差':float(np.linalg.norm(lo/np.linalg.norm(lo)-hi/np.linalg.norm(hi)))}
plt.rcParams['font.sans-serif']=['Arial Unicode MS'];plt.rcParams['axes.unicode_minus']=False
fig,axs=plt.subplots(3,1,figsize=(12,9))
for name in ['紧凑基准','开阔基准']:
 y=waves[name][:,sr:];axs[0].plot(np.arange(7200)/48,y[0,:7200],label=name,alpha=.7,linewidth=.5)
 w=240;energy=np.mean(y[:,:y.shape[1]//w*w].reshape(2,-1,w)**2,axis=(0,2))
 axs[1].plot(np.arange(len(energy))*.005,10*np.log10(np.maximum(energy,1e-30)),label=name)
for key in ['early_diff','late_diff','attack']:
 lo,hi=waves[key+'0'],waves[key+'100'];diff=(hi-lo)[0,sr:sr+7200];axs[2].plot(np.arange(7200)/48,diff,label=key,linewidth=.6)
axs[0].set_xlabel('脉冲后时间 / 毫秒');axs[0].set_ylabel('左声道原始振幅')
axs[1].set_xlim(0,1.2);axs[1].set_ylim(-130,-50);axs[1].set_xlabel('脉冲后时间 / 秒');axs[1].set_ylabel('5毫秒均方电平 / dBFS')
axs[2].set_xlabel('脉冲后时间 / 毫秒');axs[2].set_ylabel('参数端点的样本差')
for ax in axs:ax.grid(alpha=.2);ax.legend()
fig.tight_layout();fig.savefig(a.output/'原始脉冲与控制.png',dpi=140)
(a.output/'report.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:v for k,v in rows.items() if '差异' in k},ensure_ascii=False))
