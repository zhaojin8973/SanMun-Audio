#!/usr/bin/env python3
"""
SanMu Audio 官方 macOS 正式版完整打包与公证脚本
- 4 款插件：EQ6, Comp, Delay, Reverb
- 3 种格式：VST3 + AU + AAX (Pro Tools)
- 签名体系：PACE Eden wraptool + Apple Developer ID Application + Developer ID Installer
- 官方公证：xcrun notarytool submit --keychain-profile sanmu-notary + stapler staple
"""

import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path("/Users/zhaojin/Documents/SanMun Audio")
BUILD_DIR = Path("/tmp/sanmu_macos_build")
FINAL_OUT = Path("/Users/zhaojin/Downloads/SanMu插件最终审核/OpenSource_Signed_Installers")
BACKUP_DIR = Path("/Users/zhaojin/Downloads/SanMu插件最终审核/OpenSource_Signed_Installers_backup_unnotarized")

WRAPTOOL = "/Applications/PACEAntiPiracy/Eden/Fusion/Versions/6/bin/wraptool"
APP_SIGN = "Developer ID Application: JIN ZHAO (H577C4U5MX)"
INSTALLER_SIGN = "Developer ID Installer: JIN ZHAO (H577C4U5MX)"
KEYCHAIN_PROFILE = "sanmu-notary"
PACE_ACCOUNT = "zhaojinoo"
ENTITLEMENTS = ROOT / "packaging/entitlements.plist"

PRODUCTS = [
    {
        "short": "EQ6",
        "name": "SanMu EQ6",
        "version": "0.1.5",
        "bundle_id": "com.SanMuAudio.SanMuQ6",
        "guid": "23AB2060-A063-11F1-8437-00505692AD3E",
        "category": "EQ",
    },
    {
        "short": "Comp",
        "name": "SanMu Comp",
        "version": "0.1.17",
        "bundle_id": "com.SanMuAudio.SanMuComp",
        "guid": "1C46A820-BB26-11F1-8E61-00505692C25A",
        "category": "Dynamics",
    },
    {
        "short": "Delay",
        "name": "SanMu Delay",
        "version": "0.1.5",
        "bundle_id": "com.SanMuAudio.SanMuDelay",
        "guid": "53742CF0-BB26-11F1-BA61-005056928F3B",
        "category": "Delay",
    },
    {
        "short": "Reverb",
        "name": "SanMu Reverb",
        "version": "1.0.4",
        "bundle_id": "com.SanMuAudio.SanMuReverb",
        "guid": "F92A1BB0-BB20-11F1-BA61-005056928F3B",
        "category": "Reverb",
    },
]

def run(cmd: list[str], check: bool = True) -> subprocess.CompletedProcess:
    print(f"-> {' '.join(str(c) for c in cmd)}", flush=True)
    res = subprocess.run(cmd, text=True, capture_output=True)
    if check and res.returncode != 0:
        print(f"STDERR: {res.stderr}", flush=True)
        print(f"STDOUT: {res.stdout}", flush=True)
        raise RuntimeError(f"Command failed with code {res.returncode}: {' '.join(str(c) for c in cmd)}")
    return res

def patch_vst3_moduleinfo(vst3_path: Path, category: str):
    info_path = vst3_path / "Contents/Resources/moduleinfo.json"
    if not info_path.exists():
        return
    try:
        content = info_path.read_text(encoding="utf-8")
        data = json.loads(content)
        classes = data.get("Compatibility", {}).get("Classes", [])
        for c in classes:
            subs = c.get("Sub Categories", [])
            if "Fx" not in subs:
                subs.insert(0, "Fx")
            if category not in subs:
                subs.append(category)
            c["Sub Categories"] = subs
        info_path.write_text(json.dumps(data, indent=2), encoding="utf-8")
        print(f"已更新 {vst3_path.name} 的 moduleinfo.json 类别: ['Fx', '{category}']")
    except Exception as e:
        print(f"警告：更新 {info_path} 失败: {e}")

def sign_bundle(bundle_path: Path):
    run([
        "codesign", "--force", "--timestamp", "--deep", "--options", "runtime",
        "--entitlements", str(ENTITLEMENTS),
        "--sign", APP_SIGN,
        str(bundle_path)
    ])
    run(["codesign", "--verify", "--deep", "--strict", "--verbose=2", str(bundle_path)])

def make_distribution_xml(p: dict, pkg_dir: Path) -> Path:
    xml = f"""<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>{p['name']} v{p['version']}</title>
    <options customize="always" require-scripts="false" hostArchitectures="x86_64,arm64"/>
    <pkg-ref id="{p['bundle_id']}.vst3" version="{p['version']}" onConclusion="none">#component-vst3.pkg</pkg-ref>
    <pkg-ref id="{p['bundle_id']}.component" version="{p['version']}" onConclusion="none">#component-au.pkg</pkg-ref>
    <pkg-ref id="{p['bundle_id']}.aaxplugin" version="{p['version']}" onConclusion="none">#component-aax.pkg</pkg-ref>
    <choices-outline>
        <line choice="default">
            <line choice="{p['bundle_id']}.vst3"/>
            <line choice="{p['bundle_id']}.component"/>
            <line choice="{p['bundle_id']}.aaxplugin"/>
        </line>
    </choices-outline>
    <choice id="default" title="{p['name']} v{p['version']} 混音插件"/>
    <choice id="{p['bundle_id']}.vst3" title="VST3 格式 (Cubase / FL Studio / Ableton Live)" visible="true" selected="true">
        <pkg-ref id="{p['bundle_id']}.vst3"/>
    </choice>
    <choice id="{p['bundle_id']}.component" title="Audio Unit (AU) 格式 (Logic Pro / GarageBand)" visible="true" selected="true">
        <pkg-ref id="{p['bundle_id']}.component"/>
    </choice>
    <choice id="{p['bundle_id']}.aaxplugin" title="AAX 格式 (Pro Tools · 已签名数字证书)" visible="true" selected="true">
        <pkg-ref id="{p['bundle_id']}.aaxplugin"/>
    </choice>
</installer-gui-script>
"""
    xml_path = pkg_dir / "distribution.xml"
    xml_path.write_text(xml, encoding="utf-8")
    return xml_path

def main():
    print("=== 开始构建 SanMu Audio 官方 macOS 正式版安装包 ===", flush=True)
    if not Path(WRAPTOOL).exists():
        sys.exit(f"错误: 找不到 wraptool: {WRAPTOOL}")

    # 1. 备份旧安装包
    if FINAL_OUT.exists() and not BACKUP_DIR.exists():
        print(f"备份旧未公证安装包至: {BACKUP_DIR}")
        shutil.copytree(FINAL_OUT, BACKUP_DIR)

    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    BUILD_DIR.mkdir(parents=True)

    extracted_dir = Path("/tmp/sanmu_extracted")
    if not extracted_dir.exists():
        sys.exit(f"错误: 找不到解包源: {extracted_dir}")

    avid_system_dir = Path("/Library/Application Support/Avid/Audio/Plug-Ins")

    final_pkgs = []

    for p in PRODUCTS:
        short = p["short"]
        name = p["name"]
        ver = p["version"]
        guid = p["guid"]
        category = p["category"]

        print(f"\n----------------------------------------")
        print(f"处理插件: {name} (v{ver})")
        print(f"----------------------------------------")

        p_work = BUILD_DIR / short
        p_work.mkdir(parents=True)

        # 找源文件
        vst3_src = list(extracted_dir.glob(f"**/{name}.vst3"))[0]
        au_src = list(extracted_dir.glob(f"**/{name}.component"))[0]
        aax_src = Path(f"/Users/zhaojin/Downloads/SanMu插件最终审核/OpenSource_Signed_Installers/AAX_Ready_For_iLok/{name}.aaxplugin")

        # 复制到工作区
        vst3_dst = p_work / f"{name}.vst3"
        au_dst = p_work / f"{name}.component"
        aax_dst = p_work / f"{name}.aaxplugin"
        aax_wrapped = p_work / f"{name}.aaxplugin.wrapped"

        shutil.copytree(vst3_src, vst3_dst)
        shutil.copytree(au_src, au_dst)
        shutil.copytree(aax_src, aax_dst)

        # 更新 VST3 moduleinfo
        patch_vst3_moduleinfo(vst3_dst, category)

        # 签名 VST3 与 AU
        print(f"签名 VST3: {vst3_dst.name}")
        sign_bundle(vst3_dst)

        print(f"签名 AU: {au_dst.name}")
        sign_bundle(au_dst)

        # 使用 PACE wraptool 签名 AAX
        print(f"使用 PACE wraptool 为 AAX 签名: {aax_dst.name}")
        run([
            WRAPTOOL, "sign", "--verbose",
            "--account", PACE_ACCOUNT,
            "--wcguid", guid,
            "--signid", APP_SIGN,
            "--dsig1-compat", "off",
            "--in", str(aax_dst),
            "--out", str(aax_wrapped),
            "--autoinstall", "on"
        ])
        shutil.rmtree(aax_dst)
        aax_wrapped.rename(aax_dst)

        # 校验 AAX PACE 签名
        print(f"校验 AAX PACE 签名: {aax_dst.name}")
        v_res = run([WRAPTOOL, "verify", "--in", str(aax_dst)])
        print(f"PACE 校验成功: {v_res.stdout.splitlines()[-2] if v_res.stdout.splitlines() else 'OK'}")

        # 补充 Apple 开发者签名
        sign_bundle(aax_dst)

        # 同步复制一份到当前系统的 Pro Tools 目录中，供本机立即使用
        if avid_system_dir.exists():
            dest_in_avid = avid_system_dir / f"{name}.aaxplugin"
            try:
                if dest_in_avid.exists():
                    shutil.rmtree(dest_in_avid)
                shutil.copytree(aax_dst, dest_in_avid)
                print(f"已同步安装至当前系统 Pro Tools: {dest_in_avid}")
            except Exception as e:
                print(f"系统安装提示: {e}")

        # 制作 Stage 目录用于 pkgbuild
        stage_vst3 = p_work / "stage_vst3/Library/Audio/Plug-Ins/VST3"
        stage_au = p_work / "stage_au/Library/Audio/Plug-Ins/Components"
        stage_aax = p_work / "stage_aax/Library/Application Support/Avid/Audio/Plug-Ins"

        stage_vst3.mkdir(parents=True)
        stage_au.mkdir(parents=True)
        stage_aax.mkdir(parents=True)

        shutil.copytree(vst3_dst, stage_vst3 / f"{name}.vst3")
        shutil.copytree(au_dst, stage_au / f"{name}.component")
        shutil.copytree(aax_dst, stage_aax / f"{name}.aaxplugin")

        # 分别生成 3 个子组件 pkg
        comp_dir = p_work / "components"
        comp_dir.mkdir()

        pkg_vst3 = comp_dir / "component-vst3.pkg"
        pkg_au = comp_dir / "component-au.pkg"
        pkg_aax = comp_dir / "component-aax.pkg"

        print("生成子组件 component-vst3.pkg...")
        run(["pkgbuild", "--root", str(p_work / "stage_vst3"), "--identifier", f"{p['bundle_id']}.vst3",
             "--version", ver, "--install-location", "/", "--ownership", "recommended", str(pkg_vst3)])

        print("生成子组件 component-au.pkg...")
        run(["pkgbuild", "--root", str(p_work / "stage_au"), "--identifier", f"{p['bundle_id']}.component",
             "--version", ver, "--install-location", "/", "--ownership", "recommended", str(pkg_au)])

        print("生成子组件 component-aax.pkg...")
        run(["pkgbuild", "--root", str(p_work / "stage_aax"), "--identifier", f"{p['bundle_id']}.aaxplugin",
             "--version", ver, "--install-location", "/", "--ownership", "recommended", str(pkg_aax)])

        # 生成 productbuild 安装包
        dist_xml = make_distribution_xml(p, p_work)
        final_pkg_name = f"SanMu_{short}_v{ver}_macOS_Signed.pkg"
        final_pkg_path = p_work / final_pkg_name

        print(f"使用 productbuild 打包并签名最终安装包: {final_pkg_name}")
        run([
            "productbuild",
            "--distribution", str(dist_xml),
            "--package-path", str(comp_dir),
            "--sign", INSTALLER_SIGN,
            str(final_pkg_path)
        ])

        # 验证 pkg 签名
        run(["pkgutil", "--check-signature", str(final_pkg_path)])
        final_pkgs.append(final_pkg_path)

    # 2. 批量提交 Apple 官方公证并装订
    print("\n========================================")
    print("开始向 Apple 官方服务器提交公证 (Notarization)...")
    print("========================================")

    for pkg in final_pkgs:
        print(f"\n正在提交 Apple 公证: {pkg.name} ...", flush=True)
        res = run([
            "xcrun", "notarytool", "submit", str(pkg),
            "--keychain-profile", KEYCHAIN_PROFILE,
            "--wait",
            "--output-format", "json"
        ])
        sub_info = json.loads(res.stdout)
        status = sub_info.get("status")
        sub_id = sub_info.get("id")
        print(f"Apple 公证结果: ID={sub_id}, Status={status}")

        if status != "Accepted":
            log_res = run(["xcrun", "notarytool", "log", sub_id, "--keychain-profile", KEYCHAIN_PROFILE], check=False)
            print(f"公证失败日志: {log_res.stdout}")
            sys.exit(f"公证未通过: {pkg.name}")

        # 装订安全票据
        print(f"正在向安装包装订 Apple 官方公证票据 (Staple): {pkg.name} ...")
        run(["xcrun", "stapler", "staple", str(pkg)])
        run(["xcrun", "stapler", "validate", str(pkg)])

        # Gatekeeper 终验
        print(f"正在进行 Gatekeeper 安全终验: {pkg.name} ...")
        sp_res = run(["spctl", "-a", "-vvv", "-t", "install", str(pkg)])
        print(f"Gatekeeper 校验输出:\n{sp_res.stderr}\n{sp_res.stdout}")
        if "accepted" not in (sp_res.stderr + sp_res.stdout):
            sys.exit(f"Gatekeeper 终验未通过: {pkg.name}")
        print(f"✅ {pkg.name} 完美通过 Apple 官方公证！双击可直接静默安装！")

    # 3. 复制到 FINAL_OUT 并生成压缩包与校验码
    print("\n========================================")
    print(f"正在输出最终公证安装包至: {FINAL_OUT}")
    print("========================================")
    FINAL_OUT.mkdir(parents=True, exist_ok=True)

    sha_lines = []
    for pkg in final_pkgs:
        dest_file = FINAL_OUT / pkg.name
        if dest_file.exists():
            dest_file.unlink()
        shutil.copy2(pkg, dest_file)
        sha = hashlib.sha256(dest_file.read_bytes()).hexdigest()
        sha_lines.append(f"{sha}  {pkg.name}")
        print(f"产物就绪: {dest_file.name} (SHA256: {sha[:16]}...)")

    (FINAL_OUT / "SHA256SUMS.txt").write_text("\n".join(sha_lines) + "\n", encoding="utf-8")

    # 重新打包 4合1 压缩包
    suite_zip = FINAL_OUT / "SanMu_Audio_macOS_Signed_Installers_Suite.zip"
    if suite_zip.exists():
        suite_zip.unlink()
    print("生成 4 合 1 压缩包...")
    run([
        "zip", "-q", "-j", str(suite_zip),
        str(FINAL_OUT / "SanMu_EQ6_v0.1.5_macOS_Signed.pkg"),
        str(FINAL_OUT / "SanMu_Comp_v0.1.17_macOS_Signed.pkg"),
        str(FINAL_OUT / "SanMu_Delay_v0.1.5_macOS_Signed.pkg"),
        str(FINAL_OUT / "SanMu_Reverb_v1.0.4_macOS_Signed.pkg"),
        str(FINAL_OUT / "SHA256SUMS.txt"),
    ])

    print("\n🎉 全部 4 款插件 macOS 官方正式版安装包构建、签名、Apple 公证、装订 100% 完成！")

if __name__ == "__main__":
    main()
