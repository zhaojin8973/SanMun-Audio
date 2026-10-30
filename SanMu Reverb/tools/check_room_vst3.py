#!/usr/bin/env python3
"""Room真实插件验收：路由、保留类型、状态、预设与合成试听文件。"""
import argparse,hashlib,json,subprocess
from pathlib import Path
import numpy as np
import soundfile as sf
import pedalboard
from audit_plate_character import DEFAULT,measure
from check_plate_vst3 import render,repetition
from check_hall_vst3 import configured
from measure_hall_e1 import set_values
p=argparse.ArgumentParser(description=__doc__)
for key in ['plugin','baseline','renderer','presets','output']:p.add_argument('--'+key,type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False);sr=48000
presets=json.loads(a.presets.read_text());old_presets=json.loads(Path('candidates/0907B/验证/ui_verified/factory_presets.json').read_text())
r={'状态':'Room R1机器验收，待用户真实素材听审；Hall与两个Plate已获上一轮阶段认可',
   '宿主':f'pedalboard {pedalboard.__version__}','保留类型':{},'保留预设':{},'路由':{},'状态往返':{},'新预设':{},'共同默认电平':{},'声源方位':{}}
r['二进制SHA256']=hashlib.sha256((a.plugin/'Contents/MacOS/SanMu Reverb').read_bytes()).hexdigest()
x=np.zeros((2,sr*7),np.float32);x[:,sr]=.1
noise=np.zeros_like(x);noise[:,sr:sr+sr//3]=np.random.default_rng(907).normal(0,.03,(2,sr//3))
sf.write(a.output/'input_impulse.wav',x.T,sr,subtype='FLOAT')
old=pedalboard.load_plugin(str(a.baseline));new=pedalboard.load_plugin(str(a.plugin))
assert set(old.parameters)==set(new.parameters)
assert all(old.parameters[k].raw_value==new.parameters[k].raw_value for k in old.parameters)
assert np.array_equal(old.process(x,sr,reset=True),new.process(x,sr,reset=True))
r['新建默认Plate']='参数及音频逐样本一致'
for kind,signal in [('脉冲',x),('立体声噪声',noise)]:
 for category in [0,2,3]:
  for mode in [0,1,2]:
   v=dict(DEFAULT,category=category,mode=mode)
   before,_,_=render(a.baseline,signal,sr,v);after,_,_=render(a.plugin,signal,sr,v)
   diff=float(abs(before-after).max());assert diff==0,(category,mode,diff)
   r['保留类型'][f'{kind}_{category}_{mode}']=diff
print('三类保留引擎的18项逐样本检查通过',flush=True)
assert len(presets)==len(old_presets)+4
for i,old_preset in enumerate(old_presets):
 assert presets[i]['values']==old_preset['values'],i
 if old_preset['values']['category']==1:continue
 before,_,_=render(a.baseline,noise,sr,old_preset['values']);after,_,_=render(a.plugin,noise,sr,presets[i]['values'])
 diff=float(abs(before-after).max());assert diff==0,(i,diff)
 r['保留预设'][str(i)+'_'+old_preset['name']]=diff
print('37个旧预设参数与编号保留，31个非Room预设声音逐样本一致',flush=True)
for category,mode in [(0,0),(0,1),(2,0),(1,0),(1,1)]:
 y,_,elapsed=render(a.plugin,x,sr,dict(DEFAULT,category=category,mode=mode))
 r['共同默认电平'][f'{category}_{mode}']={'相对输入脉冲能量dB':float(10*np.log10(np.sum(y*y)/np.sum(x*x))),'耗时与音频时长比':elapsed/7}
for mode,index in [(0,37),(1,39)]:
 v=presets[index]['values'];offline=a.output/f'offline_mode{mode}'
 command=[str(a.renderer),'--source','wav','--input',str(a.output/'input_impulse.wav'),'--tail','.1','--output',str(offline)]
 for k,value in v.items():command.extend(['--'+k.replace('_','-'),str(value)])
 subprocess.run(command,check=True,stdout=subprocess.DEVNULL)
 expected,_=sf.read(offline/'output.wav',dtype='float32');inp,_=sf.read(offline/'input.wav',dtype='float32')
 wet,readback,elapsed=render(a.plugin,inp.T.copy(),sr,v)
 diff=float(abs(wet.T-expected).max());assert diff<2e-6,(mode,diff)
 dry,_,_=render(a.plugin,inp.T.copy(),sr,dict(v,mix=0));assert np.array_equal(dry,inp.T)
 mixed,_,_=render(a.plugin,inp.T.copy(),sr,dict(v,mix=24.8))
 mix_error=float(abs(mixed-inp.T*np.cos(.248*np.pi/2)-wet*np.sin(.248*np.pi/2)).max());assert mix_error<2e-6
 r['路由'][str(mode)]={'离线与插件样本差':diff,'干声误差':0,'混合误差':mix_error,'回读':readback,'耗时比':elapsed/(len(inp)/sr)}
 # 中央输入、左右单侧输入以及反相输入只做边界诊断，不要求单侧声源变为中央。
 for kind,signal in [('中央',x),('左',x*np.array([[1],[0]],np.float32)),('右',x*np.array([[0],[1]],np.float32)),('反相',x*np.array([[1],[-1]],np.float32))]:
  for early in [0,100]:
   y,_,_=render(a.plugin,signal,sr,dict(v,early_diff=early));el,er=np.sum(y*y,axis=1);mono=np.sum(((y[0]+y[1])*.5)**2)/(.5*(el+er));balance=10*np.log10(el/er)
   assert np.isfinite(y).all() and el>0 and er>0 and abs(balance)<4 and mono>.2,(mode,kind,early,balance,mono)
   r['声源方位'][f'{mode}_{kind}_Early{early}']={'左右差dB':float(balance),'单声道功率比':float(mono)}
 for control in ['early_diff','late_diff','attack']:
  lo,_,_=render(a.plugin,x,sr,dict(v,**{control:0}));hi,_,_=render(a.plugin,x,sr,dict(v,**{control:100}))
  diff=float(np.linalg.norm(lo/np.linalg.norm(lo)-hi/np.linalg.norm(hi)));assert diff>.2
  sf.write(a.output/f'mode{mode}_{control}0.wav',lo.T,sr,subtype='FLOAT');sf.write(a.output/f'mode{mode}_{control}100.wav',hi.T,sr,subtype='FLOAT')
 for state_mode in ([0] if mode==0 else [1,2]):
  source=configured(a.plugin);set_values(source,dict(v,mode=state_mode));state=source.raw_state
  one=pedalboard.load_plugin(str(a.plugin));one.raw_state=state;two=pedalboard.load_plugin(str(a.plugin));two.raw_state=state
  assert abs(one.parameters['category'].raw_value-1/3)<1e-6 and one.parameters['mode'].raw_value==state_mode/2
  first=one.process(noise,sr,reset=True);second=two.process(noise,sr,reset=True);assert np.array_equal(first,second)
  r['状态往返'][str(state_mode)]=0
print('两种Room路由、参数控制、声源方位和状态往返通过',flush=True)
# 可复现合成素材：短谐波乐句和宽带鼓击，不能替代真实钢琴或鼓组的听审。
phrase=np.zeros((2,sr*8),np.float32);hits=phrase.copy()
for onset,freq in [(1,130.81),(1.6,196),(2.2,261.63),(3,164.81),(3.6,220),(4.2,329.63)]:
 t=np.arange(int(sr*.9))/sr;wave=sum(np.sin(2*np.pi*freq*k*t)/k**1.6 for k in range(1,9))*.06*(1-np.exp(-t/.008))*np.exp(-t/.23);pos=int(sr*onset);phrase[:,pos:pos+len(t)]+=wave.astype(np.float32)
for i,onset in enumerate([1,1.75,2.5,3.25,4,4.75]):
 t=np.arange(sr//3)/sr;wave=np.random.default_rng(70+i).normal(0,1,len(t))*.12*np.exp(-t/.035)+.1*np.sin(2*np.pi*185*t)*np.exp(-t/.065);pos=int(sr*onset);hits[:,pos:pos+len(t)]+=wave.astype(np.float32)
for kind,signal in [('合成谐波短句',phrase),('合成鼓击',hits)]:
 sf.write(a.output/(kind+'_干声.wav'),signal.T,sr,subtype='FLOAT')
 for preset in presets[37:]:
  v=preset['values'];wet,_,elapsed=render(a.plugin,signal,sr,v);mixed,_,_=render(a.plugin,signal,sr,dict(v,mix=20))
  assert abs(wet).max()<1 and abs(mixed).max()<1
  r['新预设'][kind+'_'+preset['name']]={'参数':v,'纯湿峰值':float(abs(wet).max()),'20%混合峰值':float(abs(mixed).max()),'耗时比':elapsed/8}
  sf.write(a.output/(kind+'_'+preset['name']+'_纯湿.wav'),wet.T,sr,subtype='FLOAT');sf.write(a.output/(kind+'_'+preset['name']+'_20%混合.wav'),mixed.T,sr,subtype='FLOAT')
# 冻结的DSP文件必须字节不变；不拿旧Room或UI纳入错误的保留断言。
old_hashes=json.loads(Path('releases/0907B_Plate/源码文件哈希.json').read_text());keep={}
for name,digest in old_hashes.items():
 if name.startswith('source/dsp/') and 'RoomEngine' not in name:
  value=hashlib.sha256(Path(name).read_bytes()).hexdigest();assert value==digest,name;keep[name]=value
r['原DSP保留文件']=keep
(a.output/'report.json').write_text(json.dumps(r,ensure_ascii=False,indent=2)+'\n');print('Room真实VST3全项通过',flush=True)
