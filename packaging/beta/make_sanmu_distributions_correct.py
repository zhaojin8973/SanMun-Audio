from pathlib import Path
import os, subprocess, shutil, tempfile
root=Path(__file__).resolve().parents[2]
installer_sign=os.environ.get('SANMU_INSTALLER_SIGN_ID','')
out=root/'internal-test-20260930-catalina-ui-fix2'
comp=out/'components'; final=out/'pkg'
if comp.exists(): shutil.rmtree(comp)
if final.exists(): shutil.rmtree(final)
comp.mkdir(parents=True);final.mkdir(parents=True)
(out/'welcome.html').write_text('<!doctype html><html lang="zh-CN"><meta charset="utf-8"><body style="font:14px -apple-system,sans-serif;padding:12px"><h2>SanMu Audio 限时内测版</h2><p>本轮内测有效至北京时间 <b>2026 年 10 月 31 日 23:59</b>，11 月 1 日 00:00 到期。</p><p>到期后插件保留工程参数，停止效果处理并输出原声。请提前冻结轨道或导出音频，并安装后续内测版本。</p><p>安装时可选择 VST3、AU、AAX 格式。</p></body></html>')
items=[('EQ','SanMu EQ6','0.1.5','SanMu EQ6','com.SanMuAudio.SanMuQ6'),('Comp','SanMu Comp','0.1.17','SanMu Comp','com.SanMuAudio.SanMuComp'),('Delay','SanMu Delay','0.1.5','SanMu Delay','com.SanMuAudio.SanMuDelay'),('Reverb','SanMu Reverb','1.0.4','SanMu Reverb','com.SanMuAudio.SanMuReverb')]
for short,title,version,name,bundle in items:
  source=out/'stage'/short
  paths={'vst3':Path('Library/Audio/Plug-Ins/VST3'),'au':Path('Library/Audio/Plug-Ins/Components'),'aax':Path('Library/Application Support/Avid/Audio/Plug-Ins')}
  labels={'vst3':'VST3','au':'Audio Unit (AU)','aax':'AAX (Pro Tools)'}
  pkgrefs=[];choices=[]
  for key,relative in paths.items():
    rootdir=Path(tempfile.mkdtemp(prefix=f'sanmu-{short}-{key}-'))
    payload=rootdir/relative;payload.mkdir(parents=True)
    src=source/relative/(name+{'vst3':'.vst3','au':'.component','aax':'.aaxplugin'}[key])
    shutil.copytree(src,payload/src.name)
    cid=bundle+'.'+({'vst3':'vst3','au':'component','aax':'aaxplugin'}[key])
    package=comp/f'{short}-{key}.pkg'
    subprocess.run(['pkgbuild','--root',str(rootdir),'--identifier',cid,'--version',version,'--install-location','/','--ownership','recommended',str(package)],check=True)
    pkgrefs.append(f'<pkg-ref id="{cid}" version="{version}" onConclusion="none">#{package.name}</pkg-ref>')
    path=str((relative/src.name)).replace('\\','/')
    choices.append(f'<pkg-ref id="{cid}"><bundle-version><bundle CFBundleShortVersionString="{version}" CFBundleVersion="{version}" id="{bundle}" path="{path}"/></bundle-version></pkg-ref>')
    choices.append(f'<choice id="{cid}" title="{labels[key]}" visible="true" selected="true"><pkg-ref id="{cid}"/></choice>')
  xml='<?xml version="1.0" encoding="utf-8"?>\n<installer-gui-script minSpecVersion="1"><welcome file="welcome.html"/>\n'+''.join(pkgrefs)+f'\n<options customize="always" require-scripts="false" hostArchitectures="x86_64,arm64"/>\n<choices-outline><line choice="default"><line choice="{bundle}.vst3"/><line choice="{bundle}.component"/><line choice="{bundle}.aaxplugin"/></line></choices-outline>\n<choice id="default" title="{title} v{version}"/>\n'+''.join(choices)+'\n</installer-gui-script>\n'
  dist=out/f'{short}-distribution.xml';dist.write_text(xml)
  dest=final/f'{name.replace(" ","_")}_v{version}_Beta_Until20261031.pkg'
  subprocess.run(['productbuild','--distribution',str(dist),'--resources',str(out),'--package-path',str(comp),'--sign',installer_sign,str(dest)],check=True)
  print(dest)
