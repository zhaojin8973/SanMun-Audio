"""比较同一输入的 RVox 与 LEV 输出；仅用于离线测量，不参与插件算法。"""
import argparse
import csv
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser()
parser.add_argument('reference', type=Path)
parser.add_argument('candidate', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
plt.rcParams['font.family'] = 'Arial Unicode MS'
sr = 48000
t = np.arange(4*sr)/sr

def source(scenario):
    amplitude = np.where((t >= .25) & (t < .25 + [0.03,.3,2,2,2][scenario]), 10**(-6/20), 10**(-48/20))
    if scenario == 3:
        age = t % .55
        amplitude = (1-np.exp(-age/.012))*np.exp(-age/.16)*np.where(t<2,.12,.42)+.0001
        return amplitude*(.7*np.sin(2*np.pi*180*t)+.2*np.sin(2*np.pi*540*t)+.1*np.sin(2*np.pi*2300*t))
    if scenario == 4:
        return 10**(-12/20)*np.sin(2*np.pi*70*t)
    return amplitude*np.sin(2*np.pi*1000*t)

def output(engine, scenario, comp, gain):
    folder, latency = (args.reference, 64) if engine == 'rvox' else (args.candidate,128)
    y = np.fromfile(folder/f'{engine}_{scenario}_{comp}_{gain}.f32',dtype=np.float32)
    return y[latency:]

def db(x): return 20*np.log10(np.maximum(x,1e-12))
def rms(x): return np.sqrt(np.mean(np.square(x,dtype=np.float64)))
def trace(x,y,comp,gain):
    size = min(len(x),len(y))//96*96
    xp = np.sqrt(np.mean(x[:size].reshape(-1,96)**2,axis=1))
    yp = np.sqrt(np.mean(y[:size].reshape(-1,96)**2,axis=1))
    return (np.arange(size//96)+.5)*.002, db(xp)+abs(comp)+gain-db(yp)

rows=[]
for scenario in range(5):
    x=source(scenario)
    for comp in [0,-20,-30]:
        for gain in [0,-12,-30]:
            for engine in ['rvox','lev']:
                y=output(engine,scenario,comp,gain)
                times,gr=trace(x,y,comp,gain)
                row=dict(engine=engine,scenario=scenario,comp=comp,gain=gain,
                    rms_db=db(rms(y)),peak_db=db(np.max(np.abs(y))))
                if scenario < 3:
                    release=.25+[.03,.3,2][scenario]
                    for age in [.01,.05,.1,.3,.6,1.]:
                        mask=(times>release+age)&(times<release+age+.01)
                        row[f'release_{age}s_db']=np.mean(gr[mask])
                if scenario==4:
                    # 最后一秒整周期的相干投影，统计基波之外的失真，不把它伪称为噪声。
                    yy=y[-sr:].astype(float);phase=2*np.pi*70*np.arange(sr)/sr
                    fundamental=2*(np.mean(yy*np.sin(phase))**2+np.mean(yy*np.cos(phase))**2)
                    row['residual_pct']=100*np.sqrt(max(0,np.mean(yy**2)-fundamental)/max(fundamental,1e-24))
                rows.append(row)
keys=list(dict.fromkeys(k for row in rows for k in row))
with (args.output/'metrics.csv').open('w') as f:
    w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(rows)
fig,axes=plt.subplots(2,2,figsize=(12,8))
for ax,scenario in zip(axes.flat,range(4)):
    for engine,label in [('rvox','本机 RVox'),('lev','LEV')]:
        times,gr=trace(source(scenario),output(engine,scenario,-20,-30),-20,-30)
        ax.plot(times,gr,label=label,lw=1.3)
    ax.set(title=['30 毫秒短音','300 毫秒短音','2 秒长音','模拟音节强弱变化'][scenario],xlabel='时间（秒）',ylabel='由输入输出推算总衰减（dB）',ylim=(-1,25))
    ax.grid(alpha=.2);ax.legend()
fig.suptitle('同输入动态行为对照：COMP −20，GAIN −30，门关闭；包含峰值保护')
fig.tight_layout();fig.savefig(args.output/'dynamic-comparison.png',dpi=150);plt.close(fig)
for engine in ['rvox','lev']:
    a=output(engine,0,-30,0);b=output(engine,0,-30,-30)*10**(30/20)
    print(engine,'短音改变GAIN后的归一波形差',float(np.max(np.abs(a-b))))
for row in rows:
    if (row['scenario']==2 and row['comp']==-20 and row['gain']==-30) or (row['scenario']==4 and row['comp']==-30 and row['gain']==-30): print(row)
