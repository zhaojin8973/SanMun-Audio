from pathlib import Path
import subprocess,json,hashlib
root=Path(__file__).resolve().parents[2]
audit=root/'internal-audit-20260929-beta'
out=root/'internal-test-20260929-expiring-beta'
keychain=Path.home()/'Library/Keychains/login.keychain-db'
credentials=['--keychain-profile','sanmu-notary','--keychain',str(keychain)]
results=[]
for pkg in sorted((out/'pkg').glob('*.pkg')):
    print('提交公证：'+pkg.name,flush=True)
    p=subprocess.run(['xcrun','notarytool','submit',str(pkg),*credentials,'--wait','--output-format','json'],capture_output=True,text=True)
    (audit/(pkg.stem+'-notary.json')).write_text(p.stdout)
    (audit/(pkg.stem+'-notary.stderr.log')).write_text(p.stderr)
    if p.returncode:
        print(p.stderr,flush=True)
        raise SystemExit(p.returncode)
    data=json.loads(p.stdout)
    if data.get('status')!='Accepted':
        detail=subprocess.run(['xcrun','notarytool','log',data['id'],*credentials],capture_output=True,text=True)
        (audit/(pkg.stem+'-rejection.json')).write_text(detail.stdout+detail.stderr)
        raise SystemExit('公证未通过，请检查日志')
    checks=[]
    for command in [['xcrun','stapler','staple',str(pkg)],['xcrun','stapler','validate',str(pkg)],['spctl','-a','-v','--type','install',str(pkg)],['pkgutil','--check-signature',str(pkg)]]:
        r=subprocess.run(command,capture_output=True,text=True)
        checks.append(r.stdout+r.stderr)
        (audit/(pkg.stem+'-verification.log')).write_text('\n'.join(checks))
        if r.returncode: raise SystemExit(r.stdout+r.stderr)
    results.append({'package':pkg.name,'submission':data,'verified':True,'sha256':hashlib.sha256(pkg.read_bytes()).hexdigest()})
    (audit/'notarization-results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2))
    print('公证与装订验证通过：'+pkg.name,flush=True)
if len(results)!=4: raise SystemExit('安装包数量异常')
(out/'SHA256SUMS.txt').write_text('\n'.join(r['sha256']+'  pkg/'+r['package'] for r in results)+'\n')
readme=out/'内测说明.md'
readme.write_text(readme.read_text().replace('**状态：已签名，尚未完成 Apple 公证，请勿分发。**','**状态：四个安装包已通过 Apple 公证、票据装订与安装安全验证。**'))
print('四个限时内测包全部完成')
