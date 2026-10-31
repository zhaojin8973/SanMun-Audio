# SanMu Audio 内测包签名与公证记录

版本：`internal-beta-20261002`
内测截止：`2026-10-31 23:59 (Asia/Shanghai)`

## macOS

四个 Universal 2 `.pkg` 均已完成：

- `Developer ID Application: JIN ZHAO (H577C4U5MX)` 插件签名
- `Developer ID Installer: JIN ZHAO (H577C4U5MX)` 安装器签名
- PACE 对应 Wrap Config 的 Signing Only 数字签名与 `wraptool verify`
- Apple Notary Accepted、公证票据装订、`stapler validate`
- `spctl -a -v --type install` 与 `pkgutil --check-signature`

| 产品 | Apple 公证提交 ID | PACE Wrap Config GUID |
|---|---|---|
| SanMu EQ6 | `8d390165-6235-4553-98ac-5b4000b76782` | `2A6E58F0-A062-11F1-8E61-00505692C25A` |
| SanMu Comp | `5b5cdf98-ea2a-47b8-94b4-504b9f15465d` | `1C46A820-BB26-11F1-8E61-00505692C25A` |
| SanMu Delay | `abc0a6b5-f312-4087-87f9-5d4a75ed31b2` | `53742CF0-BB26-11F1-BA61-005056928F3B` |
| SanMu Reverb | `fb783773-8c13-4448-9f56-bff01075845a` | `F92A1BB0-BB20-11F1-BA61-005056928F3B` |

PACE 当前配置是 Signing Only：它验证 AAX 的 PACE 数字签名，但没有启用运行时许可包装。是否要求用户插入 iLok，需要另行选择带许可体验的 PACE 配置。

## Windows

当前 Mac 工作区没有 Windows PE/COFF 工具链或 Windows 实机，因此没有伪造 Windows 二进制。请在 Windows 10/11 + Visual Studio 2022 Developer PowerShell 中运行：

```powershell
.\packaging\windows\build_sanmu_windows.ps1
```

脚本默认生成 VST3 ZIP；AAX 需要 Windows AAX SDK 与 PACE 工具链。生成 MSI/EXE 还需要 WiX 或 Inno Setup，并完成 Windows 代码签名与宿主扫描。
