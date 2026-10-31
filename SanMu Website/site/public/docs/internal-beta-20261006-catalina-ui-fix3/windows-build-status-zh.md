# Windows 内测构建状态

版本日期：2026-10-02

四个 Windows 目标版本与 macOS 内测版本保持一致：

| 产品 | 目标版本 | 目标系统 | 目标格式 | 当前状态 |
|---|---:|---|---|---|
| SanMu EQ6 | 0.1.5 | Windows 10 22H2 / Windows 11 x64 | VST3 / AAX | 尚未构建 |
| SanMu Comp | 0.1.17 | Windows 10 22H2 / Windows 11 x64 | VST3 / AAX | 尚未构建 |
| SanMu Delay | 0.1.5 | Windows 10 22H2 / Windows 11 x64 | VST3 / AAX | 尚未构建 |
| SanMu Reverb | 1.0.4 | Windows 10 22H2 / Windows 11 x64 | VST3 / AAX | 尚未构建 |

开放 Windows 下载前必须完成：

1. 在 Windows 工具链中生成 VST3 和 AAX 二进制，并确认插件元数据版本与上表一致。
2. 对安装器和插件执行 Windows 代码签名。
3. 在 Windows 10 22H2 与 Windows 11 上分别扫描 Cubase、REAPER 或 Studio One，以及 Pro Tools 的 AAX。
4. 覆盖单声道、立体声、采样率切换、工程保存恢复、主题/语言偏好、内测到期旁通和卸载回滚。
5. 生成 Windows 安装包 SHA256，并把 `版本清单.json` 中相应状态改为 `ready` 后再打开网站按钮。

本工作区已提供 [`packaging/windows/build_sanmu_windows.ps1`](../../packaging/windows/build_sanmu_windows.ps1)。在 Windows 10/11 + Visual Studio 2022 开发者 PowerShell 中，从工作区根目录执行：

```powershell
.\packaging\windows\build_sanmu_windows.ps1
```

默认生成 VST3 ZIP。只有在 Windows AAX SDK、PACE 工具链和对应凭据均已配置后，才使用 `-BuildAax`；AAX 不能由 macOS 的 PACE 或 Apple 签名替代。

当前脚本生成的是带安装说明的 ZIP 内测包，不是 MSI/EXE 安装器；生成 MSI/EXE 还需要在 Windows 上配置 WiX 或 Inno Setup，并完成 Windows 代码签名。没有 Windows 实机产物、签名和宿主扫描证据前，不能把 Windows 状态改为 `ready`，也不能在网站打开下载按钮。


本轮 Windows 前置还要求 Node/npm 依赖和 Microsoft.Web.WebView2 SDK；缺一项脚本都会停止，不会回退到旧 IE 内核。
