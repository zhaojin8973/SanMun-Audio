# Signing and notarization record (2026-10-06 Catalina candidate)

## Current macOS evidence

- All four plug-ins are Universal 2 (x86_64 + arm64) with a macOS 10.15 minimum.
- Plug-in bundles pass Developer ID Application: JIN ZHAO (H577C4U5MX) verification.
- The pkg files report pkgutil --check-signature: Status: no signature; Developer ID Installer signing is still pending.
- The candidate AAX binaries have Apple code signatures only and no PACE digital envelope. PACE wrapping remains a post-validation step.
- There is no Apple notarization RequestUUID, Accepted result or stapler evidence, so the candidate is not notarized.

## Windows

This macOS workspace has no Windows PE/COFF toolchain or Windows host, so no Windows binary is represented as ready. Run packaging/windows/build_sanmu_windows.ps1 on Windows 10/11 with Visual Studio 2022, then complete Windows signing and host scans.
