#!/usr/bin/env python3
"""比较输入滤波与传播阻尼的作用，保留音量及尾声寿命两种证据。"""
import argparse,json
from pathlib import Path
import numpy as np
import soundfile as sf
from scipy.signal import butter,sosfilt
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);args=p.parse_args()
 names=['before','input_only','after'];reports={name:json.loads((args.root/name/'report.json').read_text()) for name in names}
 result={}
 for section in ['频带','短音']:
  result[section]={}
  for key,before in reports['before'][section].items():
   entry={name:reports[name][section][key] for name in names}
   after=entry['after'];entry['总能量变化dB']=float(10*np.log10(after['energy']/before['energy']))
   entry['尾声寿命变化百分比']=float((after['rt60']/before['rt60']-1)*100)
   result[section][key]=entry
 plt.rcParams['font.sans-serif']=['Arial Unicode MS'];plt.rcParams['axes.unicode_minus']=False
 fig,axes=plt.subplots(2,2,figsize=(12,8))
 for key,ax in zip(['80','160','250','400'],axes.flat):
  for name,label in zip(names,['P1','只改输入高通','P2：输入高通与传播阻尼']):
   x,sr=sf.read(args.root/name/f'tone_{key}.wav');x=x[int(sr*1.25):]
   step=int(sr*.03);power=np.mean(x[:len(x)//step*step].reshape(-1,step,2)**2,axis=(1,2))
   ax.plot(np.arange(len(power))*.03,10*np.log10(np.maximum(power,1e-30)),label=label)
  ax.set_title(key+' Hz短音停止后的纯湿尾声');ax.set_xlim(0,1.2);ax.set_ylim(-80,-25);ax.grid(alpha=.25);ax.legend(fontsize=8)
  ax.set_xlabel('停止后时间 / 秒');ax.set_ylabel('30ms均方电平 / dBFS')
 fig.tight_layout();fig.savefig(args.root/'前后对照.png',dpi=150)
 # 仅用高通后的衰减必须基本相同，证明该比较不是把音量下降误当成寿命变化。
 for key in ['100','130','160','200','250','315']:
  e=result['短音'][key]
  assert abs(e['input_only']['rt60']/e['before']['rt60']-1)<.01
  assert e['after']['rt60']<e['before']['rt60']*.96
  assert -3<e['总能量变化dB']<0
 for key in ['700-1400','1400-2800']:
  assert abs(result['频带'][key]['总能量变化dB'])<.2
 (args.root/'comparison.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
 print(json.dumps({section:{key:{k:v for k,v in x.items() if k not in names} for key,x in rows.items()} for section,rows in result.items()},ensure_ascii=False,indent=2))
if __name__=='__main__':main()
