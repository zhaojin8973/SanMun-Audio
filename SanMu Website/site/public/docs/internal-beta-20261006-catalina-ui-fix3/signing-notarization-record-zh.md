# 签名与公证记录（2026-10-06 Catalina 候选包）

## macOS 当前证据

- 四个插件核心均为 Universal 2（x86_64 + arm64），最低系统为 macOS 10.15。
- 插件包通过 Developer ID Application: JIN ZHAO (H577C4U5MX) 校验。
- pkg 当前为 pkgutil --check-signature: Status: no signature，尚未使用 Developer ID Installer。
- 当前候选 AAX 仅有 Apple 代码签名，没有 PACE 数字信封；PACE 配置仍需在 iLok 授权和 Catalina 主机验收后执行。
- 当前没有 Apple notarization RequestUUID、Accepted 状态或 stapler 证据，因此不能称为已公证。

## Windows

当前 macOS 工作区没有 Windows PE/COFF 工具链或 Windows 实机，因此不代表任何 Windows 二进制已 ready。请在 Windows 10/11 + Visual Studio 2022 Developer PowerShell 中运行 packaging/windows/build_sanmu_windows.ps1，再完成 Windows 签名和宿主扫描。
