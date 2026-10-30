#!/usr/bin/env python3
"""真实VST3双风格核验：P3逐样本保留、旧状态迁移、新钢板往返。"""
import argparse,hashlib,json
from pathlib import Path
import numpy as np
import soundfile as sf
import pedalboard
from audit_plate_character import DEFAULT
from check_plate_vst3 import render
from check_hall_vst3 import configured
from measure_hall_e1 import set_values


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ['plugin','presets','output']:parser.add_argument('--'+key,type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    baseline=Path('releases/0907A_PlateP3/SanMu Reverb.vst3');sr=48000
    report={'状态':'P3已获用户定稿认可；Steel S1仅完成机器验证','P3保留':{},'状态往返':{},'预设保留':{}}
    impulse=np.zeros((2,sr*9),np.float32);impulse[:,sr]=.1
    noise=np.zeros_like(impulse);noise[:,sr:sr+sr//3]=np.random.default_rng(907).normal(0,.03,(2,sr//3))
    fresh=pedalboard.load_plugin(str(args.plugin));old=pedalboard.load_plugin(str(baseline))
    assert set(fresh.parameters)==set(old.parameters)
    assert all(fresh.parameters[k].raw_value==old.parameters[k].raw_value for k in fresh.parameters)
    assert fresh.parameters['mode'].raw_value==0
    y=fresh.process(impulse,sr,buffer_size=128,reset=True)
    ref=old.process(impulse,sr,buffer_size=128,reset=True)
    assert np.array_equal(y,ref)
    report['新插入默认P3最大差']=float(abs(y-ref).max())
    settings=[DEFAULT,dict(DEFAULT,early_diff=0),dict(DEFAULT,late_diff=0),dict(DEFAULT,attack=100),
              dict(DEFAULT,mod_depth=0),dict(DEFAULT,decay=1.2,size=50),dict(DEFAULT,bass_mult=.8,high_shelf=-12)]
    for kind,x in [('脉冲',impulse),('立体声噪声',noise)]:
        for i,values in enumerate(settings):
            ref,_,_=render(baseline,x,sr,values);wet,_,_=render(args.plugin,x,sr,values)
            diff=float(abs(wet-ref).max());assert diff==0,(kind,i,diff)
            report['P3保留'][f'{kind}_{i}']=diff
    oldPresets=json.loads(Path('candidates/0907A/验证/ui_final/factory_presets.json').read_text())
    newPresets=json.loads(args.presets.read_text())
    assert len(newPresets)==len(oldPresets)+1
    for i,oldPreset in enumerate(oldPresets):
        newPreset=newPresets[i]
        ref,_,_=render(baseline,noise,sr,oldPreset['values'])
        wet,_,_=render(args.plugin,noise,sr,newPreset['values'])
        diff=float(abs(wet-ref).max());assert diff==0,(i,diff)
        report['预设保留'][f'{i}_{newPreset["name"]}']=diff
    # 将旧插件本身导出的宿主状态交给新插件，不能仅验证手工构造的XML。
    for mode in [0,1,2]:
        old=configured(baseline);set_values(old,dict(DEFAULT,mode=mode))
        state=old.raw_state
        new=pedalboard.load_plugin(str(args.plugin));new.raw_state=state
        assert new.parameters['mode'].raw_value==0,mode
        # 两端都经过同一个宿主状态序列化/恢复过程，避免XML浮点回读精度混入算法对比。
        restoredOld=pedalboard.load_plugin(str(baseline));restoredOld.raw_state=state
        original=old.process(noise,sr,buffer_size=128,reset=True)
        ref=restoredOld.process(noise,sr,buffer_size=128,reset=True)
        wet=new.process(noise,sr,buffer_size=128,reset=True)
        diff=float(abs(ref-wet).max());assert diff==0,(mode,diff)
        report['状态往返'][f'旧模式{mode}迁移到Silk']=diff
        report['状态往返'][f'旧插件自身序列化误差{mode}']=float(abs(original-ref).max())
    steel=configured(args.plugin);set_values(steel,dict(DEFAULT,mode=1))
    saved=steel.raw_state;restored=pedalboard.load_plugin(str(args.plugin));restored.raw_state=saved
    assert restored.parameters['mode'].raw_value==.5
    steel.raw_state=saved
    a=steel.process(noise,sr,buffer_size=128,reset=True);b=restored.process(noise,sr,buffer_size=128,reset=True)
    # 原对象可能保留序列化前的单精度参数缓存；按既有宿主误差门槛检查，另以两个新实例核对确定性。
    roundtrip=float(abs(a-b).max());assert roundtrip<2e-6,roundtrip
    another=pedalboard.load_plugin(str(args.plugin));another.raw_state=saved
    c=another.process(noise,sr,buffer_size=128,reset=True);assert np.array_equal(b,c)
    report['状态往返']['新钢板保存恢复最大差']=roundtrip
    report['状态往返']['两个新实例恢复钢板最大差']=float(abs(b-c).max())
    # 相同输入、相同参数验证两个模式有结构差异，并保留实际试听诊断文件。
    silk,_,_=render(args.plugin,impulse,sr,DEFAULT)
    steel,_,_=render(args.plugin,impulse,sr,dict(DEFAULT,mode=1))
    diff=float(np.linalg.norm(silk/np.linalg.norm(silk)-steel/np.linalg.norm(steel)))
    assert diff>.2
    report['两风格匹配电平后结构差']=diff
    sf.write(args.output/'Modern_Silk_脉冲.wav',silk.T,sr,subtype='FLOAT')
    sf.write(args.output/'Classic_Steel_脉冲.wav',steel.T,sr,subtype='FLOAT')
    preset=newPresets[-1];assert preset['values']['category']==0 and preset['values']['mode']==1
    wet,_,_=render(args.plugin,noise,sr,preset['values']);assert np.isfinite(wet).all() and abs(wet).max()<1
    sf.write(args.output/'经典钢板基准_噪声.wav',wet.T,sr,subtype='FLOAT')
    report['二进制SHA256']=hashlib.sha256((args.plugin/'Contents/MacOS/SanMu Reverb').read_bytes()).hexdigest()
    report['限制']='旧工程若有模式编号的自动化，宿主仍可能写入旧编号1；此时会选择新Steel，需在宿主核对该自动化轨。静态状态与用户预设迁移已验证。'
    (args.output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print('新建默认、14项P3保留、36个既有预设、旧状态迁移与钢板往返通过')


if __name__=='__main__':main()
