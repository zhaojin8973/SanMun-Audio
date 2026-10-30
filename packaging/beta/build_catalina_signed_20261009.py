#!/usr/bin/env python3
"""生成 Catalina 内测用的 PACE AAX、Installer 签名和可公证 pkg。"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'internal-test-20261009-catalina-pace-installer-signed'
APP_SIGN = os.environ.get('SANMU_APP_SIGN_ID', '')
INSTALLER_SIGN = os.environ.get('SANMU_INSTALLER_SIGN_ID', '')
PACE_ACCOUNT = os.environ.get('SANMU_PACE_ACCOUNT', '')
ENTITLEMENTS = ROOT / 'packaging/entitlements.plist'
WRAPTOOL = Path(os.environ.get('SANMU_WRAPTOOL', '/Applications/PACEAntiPiracy/Eden/Fusion/Versions/6/bin/wraptool'))

PRODUCTS = [
    ('EQ6', 'SanMu EQ', 'build-release-20260928', 'SanMuQ6', 'SanMu EQ6', '0.1.5',
     os.environ.get('SANMU_EQ_WCGUID', ''), 'com.SanMuAudio.SanMuQ6'),
    ('Comp', 'SanMu Comp', 'build-release-20260928', 'SanMuComp', 'SanMu Comp', '0.1.17',
     os.environ.get('SANMU_COMP_WCGUID', ''), 'com.SanMuAudio.SanMuComp'),
    ('Delay', 'SanMu Delay', 'build-release-20260928', 'SanMuDelay', 'SanMu Delay', '0.1.5',
     os.environ.get('SANMU_DELAY_WCGUID', ''), 'com.SanMuAudio.SanMuDelay'),
    ('Reverb', 'SanMu Reverb', 'build-release-20260928-make', 'SanMuReverb', 'SanMu Reverb', '1.0.4',
     os.environ.get('SANMU_REVERB_WCGUID', ''), 'com.SanMuAudio.SanMuReverb'),
]


def run(args: list[str], log: Path | None = None) -> str:
    if log is None:
        result = subprocess.run(args, cwd=ROOT, check=True, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        return result.stdout
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open('w', encoding='utf-8') as handle:
        subprocess.run(args, cwd=ROOT, check=True, text=True,
                       stdout=handle, stderr=subprocess.STDOUT)
    return log.read_text(encoding='utf-8')


def sign_application(bundle: Path) -> None:
    run(['codesign', '--force', '--timestamp', '--options', 'runtime',
         '--entitlements', str(ENTITLEMENTS), '--sign', APP_SIGN, str(bundle)])


def make_distribution(short: str, title: str, version: str, bundle_id: str,
                      component_dir: Path, resources: Path, package_path: Path) -> None:
    refs: list[str] = []
    choices: list[str] = []
    formats = [
        ('vst3', 'VST3', 'vst3'),
        ('au', 'Audio Unit (AU)', 'component'),
        ('aax', 'AAX (Pro Tools)', 'aaxplugin'),
    ]
    for key, label, suffix in formats:
        identifier = f'{bundle_id}.{suffix}'
        filename = f'{short}-{key}.pkg'
        refs.append(f'<pkg-ref id="{identifier}" version="{version}" onConclusion="none">#{filename}</pkg-ref>')
        choices.append(f'<choice id="{identifier}" title="{label}" visible="true" selected="true"><pkg-ref id="{identifier}"/></choice>')
    outline = ''.join(f'<line choice="{bundle_id}.{suffix}"/>' for _, _, suffix in formats)
    xml = (
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<installer-gui-script minSpecVersion="1">\n'
        '<welcome file="welcome.html"/>\n'
        + ''.join(refs)
        + f'<options customize="always" require-scripts="false" hostArchitectures="x86_64,arm64"/>\n'
          f'<choices-outline><line choice="default">{outline}</line></choices-outline>\n'
          f'<choice id="default" title="{title} v{version}"/>\n'
        + ''.join(choices)
        + '\n</installer-gui-script>\n'
    )
    distribution = OUT / f'{short}-distribution.xml'
    distribution.write_text(xml, encoding='utf-8')
    run(['productbuild', '--distribution', str(distribution), '--resources', str(resources),
         '--package-path', str(component_dir), '--sign', INSTALLER_SIGN, str(package_path)],
        OUT / 'logs' / f'{short}-productbuild.log')


def main() -> None:
    if not WRAPTOOL.exists():
        raise SystemExit(f'找不到 PACE wraptool：{WRAPTOOL}')
    if not ENTITLEMENTS.exists():
        raise SystemExit(f'找不到 entitlements：{ENTITLEMENTS}')
    shutil.rmtree(OUT, ignore_errors=True)
    (OUT / 'bundles').mkdir(parents=True)
    (OUT / 'components').mkdir()
    (OUT / 'pkg').mkdir()
    (OUT / 'stage').mkdir()
    (OUT / 'logs').mkdir()
    (OUT / 'welcome.html').write_text(
        '<!doctype html><html lang="zh-CN"><meta charset="utf-8">'
        '<body style="font:14px -apple-system,sans-serif;padding:12px">'
        '<h2>SanMu Audio 限时内测版</h2>'
        '<p>本轮内测有效至北京时间 2026 年 10 月 31 日 23:59。</p>'
        '<p>到期后插件停止效果处理并输出原声。安装时可选择 VST3、AU、AAX 格式。</p>'
        '</body></html>', encoding='utf-8')

    manifest: dict[str, object] = {
        'release': 'internal-beta-20261009-catalina-pace-installer-signed',
        'betaEndsAt': '2026-10-31T23:59:00+08:00',
        'pace': 'signing-only',
        'products': [],
    }
    for short, project, build, target, name, version, wrap_guid, bundle_id in PRODUCTS:
        source = ROOT / project / build / 'source' / f'{target}_artefacts/Release'
        destination = OUT / 'bundles' / short
        destination.mkdir(parents=True)
        for fmt, suffix in [('VST3', '.vst3'), ('AU', '.component'), ('AAX', '.aaxplugin')]:
            shutil.copytree(source / fmt / f'{name}{suffix}', destination / f'{name}{suffix}')
        sign_application(destination / f'{name}.vst3')
        sign_application(destination / f'{name}.component')

        aax = destination / f'{name}.aaxplugin'
        wrapped = destination / f'{name}.aaxplugin.pace-signed'
        run([str(WRAPTOOL), 'sign', '--verbose', '--account', PACE_ACCOUNT,
             '--wcguid', wrap_guid, '--signid', APP_SIGN, '--dsig1-compat', 'off',
             '--in', str(aax), '--out', str(wrapped), '--autoinstall', 'on'],
            OUT / 'logs' / f'{short}-pace-sign.log')
        shutil.rmtree(aax)
        wrapped.rename(aax)
        run([str(WRAPTOOL), 'verify', '--in', str(aax)], OUT / 'logs' / f'{short}-pace-verify.log')
        run(['codesign', '--verify', '--deep', '--strict', '--verbose=2', str(destination / f'{name}.vst3')],
            OUT / 'logs' / f'{short}-vst3-verify.log')
        run(['codesign', '--verify', '--deep', '--strict', '--verbose=2', str(destination / f'{name}.component')],
            OUT / 'logs' / f'{short}-au-verify.log')
        run(['codesign', '--verify', '--deep', '--strict', '--verbose=2', str(aax)],
            OUT / 'logs' / f'{short}-aax-verify.log')

        stage = OUT / 'stage' / short
        (stage / 'Library/Audio/Plug-Ins/VST3').mkdir(parents=True)
        (stage / 'Library/Audio/Plug-Ins/Components').mkdir(parents=True)
        (stage / 'Library/Application Support/Avid/Audio/Plug-Ins').mkdir(parents=True)
        shutil.copytree(destination / f'{name}.vst3', stage / 'Library/Audio/Plug-Ins/VST3' / f'{name}.vst3')
        shutil.copytree(destination / f'{name}.component', stage / 'Library/Audio/Plug-Ins/Components' / f'{name}.component')
        shutil.copytree(aax, stage / 'Library/Application Support/Avid/Audio/Plug-Ins' / f'{name}.aaxplugin')

        components = OUT / 'components'
        for key, relative, suffix in [
            ('vst3', 'Library/Audio/Plug-Ins/VST3', '.vst3'),
            ('au', 'Library/Audio/Plug-Ins/Components', '.component'),
            ('aax', 'Library/Application Support/Avid/Audio/Plug-Ins', '.aaxplugin'),
        ]:
            component = components / f'{short}-{key}.pkg'
            run(['pkgbuild', '--root', str(stage), '--identifier', f'{bundle_id}.{suffix[1:] or key}',
                 '--version', version, '--install-location', '/', '--ownership', 'recommended', str(component)],
                OUT / 'logs' / f'{short}-{key}-pkgbuild.log')

        package = OUT / 'pkg' / f'SanMu_{short}_v{version}_CatalinaSigned.pkg'
        make_distribution(short, name, version, bundle_id, components, OUT, package)
        run(['pkgutil', '--check-signature', str(package)], OUT / 'logs' / f'{short}-installer-signature.log')
        sha = hashlib.sha256(package.read_bytes()).hexdigest()
        cast = manifest['products']
        assert isinstance(cast, list)
        cast.append({'name': name, 'version': version, 'package': package.name,
                     'sha256': sha, 'paceVerified': True, 'installerSigned': True,
                     'notarized': False})
        print(f'完成 {name}: {package}')

    (OUT / 'SHA256SUMS.txt').write_text('\n'.join(
        f"{product['sha256']}  pkg/{product['package']}" for product in manifest['products']
    ) + '\n', encoding='utf-8')
    (OUT / 'release-manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'输出目录：{OUT}')


if __name__ == '__main__':
    main()
