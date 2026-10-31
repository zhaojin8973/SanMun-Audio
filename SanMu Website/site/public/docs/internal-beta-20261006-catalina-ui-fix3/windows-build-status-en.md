# Windows internal-beta build status

Release date: 2026-10-02
Beta expiry: 2026-10-31 23:59 (Asia/Shanghai)

The four Windows target versions match the macOS beta versions:

| Product | Version | Target OS | Target formats | Status |
|---|---:|---|---|---|
| SanMu EQ6 | 0.1.5 | Windows 10 22H2 / Windows 11 x64 | VST3 / AAX | Not built |
| SanMu Comp | 0.1.17 | Windows 10 22H2 / Windows 11 x64 | VST3 / AAX | Not built |
| SanMu Delay | 0.1.5 | Windows 10 22H2 / Windows 11 x64 | VST3 / AAX | Not built |
| SanMu Reverb | 1.0.4 | Windows 10 22H2 / Windows 11 x64 | VST3 / AAX | Not built |

Before opening Windows downloads, the release must complete:

1. Build VST3 and AAX binaries with the Windows toolchain and verify the metadata versions.
2. Apply Windows code signing to the binaries and installer.
3. Scan the plug-ins in Cubase, REAPER or Studio One, and Pro Tools for AAX, on Windows 10 22H2 and Windows 11.
4. Cover mono, stereo, sample-rate changes, project save/restore, theme/language preferences, beta-expiry bypass and uninstall rollback.
5. Generate Windows SHA256 checksums and change the matching manifest states to `ready` only after these checks.

The workspace provides `packaging/windows/build_sanmu_windows.ps1`. From the workspace root in a Windows 10/11 Visual Studio 2022 Developer PowerShell, run:

```powershell
.\packaging\windows\build_sanmu_windows.ps1
```

The default output is a VST3 ZIP. Use `-BuildAax` only after the Windows AAX SDK, PACE tools and matching credentials are configured; macOS PACE or Apple signing cannot replace the Windows AAX toolchain.

The script creates ZIP beta packages with installation instructions, not MSI/EXE installers. An MSI/EXE release additionally needs WiX or Inno Setup and Windows code signing. Until Windows artifacts, signatures and host-scan evidence exist, keep the manifest at `not-built` and keep the website download button closed.


This Windows build also requires the Node/npm dependencies and the Microsoft.Web.WebView2 SDK; the script stops instead of falling back to the legacy IE backend.
