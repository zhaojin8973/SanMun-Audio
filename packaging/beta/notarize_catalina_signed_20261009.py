#!/usr/bin/env python3
"""提交 2026-10-09 Catalina 签名包、公证、装订并更新校验值。"""

from __future__ import annotations

import hashlib
import json
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'internal-test-20261009-catalina-pace-installer-signed'
AUDIT = OUT / 'notarization'


def run(args: list[str], log: Path) -> subprocess.CompletedProcess[str]:
    log.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True)
    log.write_text(result.stdout + result.stderr, encoding='utf-8')
    return result


def main() -> None:
    packages = sorted((OUT / 'pkg').glob('*.pkg'))
    if len(packages) != 4:
        raise SystemExit(f'需要 4 个 pkg，实际找到 {len(packages)} 个')
    AUDIT.mkdir(parents=True, exist_ok=True)
    records: list[dict[str, object]] = []
    for package in packages:
        stem = package.stem
        print(f'提交 Apple 公证：{package.name}', flush=True)
        result = run([
            'xcrun', 'notarytool', 'submit', str(package),
            '--keychain-profile', 'sanmu-notary', '--wait', '--output-format', 'json',
        ], AUDIT / f'{stem}-submit.log')
        if result.returncode:
            raise SystemExit(f'公证提交失败：{package.name}')
        try:
            submission = json.loads(result.stdout)
        except json.JSONDecodeError as exc:
            raise SystemExit(f'公证响应不是 JSON：{package.name}') from exc
        if submission.get('status') != 'Accepted':
            submission_id = submission.get('id')
            if submission_id:
                run(['xcrun', 'notarytool', 'log', str(submission_id),
                     '--keychain-profile', 'sanmu-notary'], AUDIT / f'{stem}-rejection.log')
            raise SystemExit(f'公证未通过：{package.name}，状态={submission.get("status")}')
        staple = run(['xcrun', 'stapler', 'staple', str(package)], AUDIT / f'{stem}-staple.log')
        if staple.returncode:
            raise SystemExit(f'票据装订失败：{package.name}')
        validate = run(['xcrun', 'stapler', 'validate', str(package)], AUDIT / f'{stem}-stapler-validate.log')
        if validate.returncode:
            raise SystemExit(f'票据验证失败：{package.name}')
        gatekeeper = run(['spctl', '-a', '-v', '--type', 'install', str(package)], AUDIT / f'{stem}-spctl.log')
        if gatekeeper.returncode:
            raise SystemExit(f'Gatekeeper 验证失败：{package.name}')
        signature = run(['pkgutil', '--check-signature', str(package)], AUDIT / f'{stem}-pkg-signature.log')
        if signature.returncode:
            raise SystemExit(f'Installer 签名验证失败：{package.name}')
        records.append({
            'package': package.name,
            'submissionId': submission.get('id'),
            'status': submission.get('status'),
            'sha256': hashlib.sha256(package.read_bytes()).hexdigest(),
            'stapled': True,
            'gatekeeper': 'passed',
        })
        print(f'公证、装订与 Gatekeeper 验证通过：{package.name}', flush=True)

    (AUDIT / 'notarization-results.json').write_text(json.dumps(records, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    (OUT / 'SHA256SUMS.txt').write_text('\n'.join(
        f"{record['sha256']}  pkg/{record['package']}" for record in records
    ) + '\n', encoding='utf-8')
    manifest_path = OUT / 'release-manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    for product in manifest['products']:
        record = next(item for item in records if item['package'] == product['package'])
        product['sha256'] = record['sha256']
        product['notarized'] = True
        product['submissionId'] = record['submissionId']
    manifest['status'] = 'signed-notarized-pending-catalina-host-validation'
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'四个包全部完成：{OUT}', flush=True)


if __name__ == '__main__':
    main()
