# SanMu Audio beta signing and notarization record

Release: `internal-beta-20261002`
Beta expiry: `2026-10-31 23:59 (Asia/Shanghai)`

## macOS

All four Universal 2 `.pkg` files completed:

- `Developer ID Application: JIN ZHAO (H577C4U5MX)` plug-in signing
- `Developer ID Installer: JIN ZHAO (H577C4U5MX)` installer signing
- PACE Signing Only signatures and `wraptool verify` for the four product Wrap Configs
- Apple Notary Accepted, ticket stapled, and `stapler validate`
- `spctl -a -v --type install` and `pkgutil --check-signature`

| Product | Apple notarization submission ID | PACE Wrap Config GUID |
|---|---|---|
| SanMu EQ6 | `8d390165-6235-4553-98ac-5b4000b76782` | `2A6E58F0-A062-11F1-8E61-00505692C25A` |
| SanMu Comp | `5b5cdf98-ea2a-47b8-94b4-504b9f15465d` | `1C46A820-BB26-11F1-8E61-00505692C25A` |
| SanMu Delay | `abc0a6b5-f312-4087-87f9-5d4a75ed31b2` | `53742CF0-BB26-11F1-BA61-005056928F3B` |
| SanMu Reverb | `fb783773-8c13-4448-9f56-bff01075845a` | `F92A1BB0-BB20-11F1-BA61-005056928F3B` |

The current PACE configuration is Signing Only: it verifies the PACE digital signature but does not enable a runtime license wrapper. Requiring an iLok for users requires a separate PACE customer-experience configuration.

## Windows

The current Mac workspace has no Windows PE/COFF toolchain or Windows host, so no Windows binary is being represented as ready. On Windows 10/11 with Visual Studio 2022 Developer PowerShell, run:

```powershell
.\\packaging\\windows\\build_sanmu_windows.ps1
```

The script defaults to a VST3 ZIP. AAX requires the Windows AAX SDK and PACE toolchain. MSI/EXE output additionally requires WiX or Inno Setup, Windows code signing and host scans.
