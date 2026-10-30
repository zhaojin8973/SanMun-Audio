# Windows 自动构建与内测下载

四款插件只维护这个 Git 仓库中的一套源码。Mac 上修改并推送后，GitHub Actions（GitHub 的自动构建服务）从同一个源码提交生成 Windows x64 VST3 候选包。Win11 测试电脑只需下载、安装和测试，无需复制工程，也不需要另改一份代码。

当前工作流是 `.github/workflows/windows-build.yml`，构建入口是 `packaging/windows/build_sanmu_windows.ps1`。配置已写入仓库不代表 Windows 构建、代码签名或宿主验证已经完成；是否成功，以对应的 Actions 运行记录和产物为准。

## 测试电脑如何拿到插件

1. 登录 GitHub，打开这个源码仓库的 **Actions**。
2. 选择 **Windows 内测构建**，打开最近一次四款任务均为绿色的运行记录。
3. 在页面底部 **Artifacts**（构建产物）下载所需的 `SanMu-产品名-Windows-x64-VST3-运行编号`。不要下载名称包含 `build-logs` 的日志包。
4. 解压下载文件，再解压其中的 `SanMu_..._Windows_x64_VST3.zip`。压缩包旁保留了 `SHA256SUMS.txt`、`build-results.json` 和 `source-version.json`，便于确认文件对应的源码和测试版本。
5. 完全退出音频宿主，把 `VST3` 目录下整个 `.vst3` 文件夹复制到 `C:\Program Files\Common Files\VST3`。不要只复制它里面的 `.vst3` 二进制文件；写入这个系统目录时可能需要管理员权限。
6. 重新启动 REAPER、Cubase、Studio One 等支持 VST3 的 64 位宿主，并重新扫描插件。

本流程生成 ZIP 候选包，尚未生成双击安装的 EXE/MSI，也不包含 Windows AAX。Pro Tools 无法加载 VST3，因此不能用这一批候选包在 Pro Tools 内测试。

测试电脑不需要 Visual Studio、Node.js 或 WebView2 SDK。插件界面需要 Microsoft WebView2 Runtime（运行网页界面的组件）；Windows 11 通常已带有它，若运行时缺失，请安装 [Microsoft 官方 Evergreen WebView2 Runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/#download-section)。SDK 用于开发，Runtime 用于运行，不要把二者混淆。[Microsoft 的分发说明](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution)介绍了运行时检查与安装方法。

## 修改一次，两个平台同步

开发者在 Mac 上修改仓库里的源码、UI 或共享打包工具，提交并推送到 `main`。工作流会对 EQ6、Comp、Delay、Reverb 各运行一个 Windows 构建任务；它们都使用此次提交及该提交锁定的依赖。

如果只改了网站或说明文件，自动构建不会触发。需要手动重建时，在 **Actions → Windows 内测构建 → Run workflow** 选择分支即可；手动运行仍使用所选分支的同一份源码。

发生问题时，反馈产品版本、宿主及版本、Windows 版本、操作步骤、截图和 `source-version.json` 中的 `commit`。开发者修正同一仓库，再推送生成下一批候选包；测试电脑直接替换插件，不维护第二套工程。

## 构建环境与输出边界

工作流使用 GitHub `windows-2022` 构建机器和 Visual Studio 2022，目标为 x64 Release。构建机是 Windows Server 2022；它负责编译，不能代替 Windows 11 真机的宿主验证。[GitHub 构建机的软件清单](https://github.com/actions/runner-images/blob/main/images/windows/Windows2022-Readme.md)可核对 CMake、Visual Studio 和 NuGet 的供应情况。

工具和依赖配置：

- Node.js 24；网页依赖由各自的 `package-lock.json` 配合 `npm ci` 安装。
- Microsoft.Web.WebView2 SDK 固定 `1.0.3485.44`，通过官方 NuGet 下载，并将 SDK 根目录传给 CMake。SDK 固定版本不限制测试电脑的 Evergreen Runtime 自动更新。
- Checkout 使用递归子模块，并关闭凭据保留；Actions 权限只有 `contents: read`。
- 第三方 Actions 固定完整提交 SHA：checkout `v6.1.0`、setup-node `v6.5.0`、upload-artifact `v6.0.0`。这是 [GitHub 官方建议的固定方式](https://docs.github.com/en/actions/reference/security/secure-use)。升级时需重新核对官方仓库的版本和 SHA。
- 每款插件分别保存压缩包、SHA256、源码版本、构建结果和日志；保留 30 天。失败的产品不会阻止其他产品完成，但失败任务只保存诊断日志，不应当作可安装包。
- 本工作流不需要签名密码，不创建 GitHub Release，不公开发布，不上传 SanMu 网站。若仓库是私有仓库，下载者需要相应访问权限；[GitHub 产物下载说明](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/download-workflow-artifacts)解释了访问与下载方式。

## Windows AAX 与签名

目前自动任务只构建 VST3。Windows AAX 需要受许可的 Windows AAX SDK；发布给正常 Pro Tools 使用前，还必须完成 Windows PACE 数字签名及相应的 Windows 代码签名验证。不能把未签名 AAX 安装到 Pro Tools 后就认定兼容性正常。

Mac 钥匙串里的 Developer ID Application、Developer ID Installer，以及 `sanmu-notary` 都服务于 Apple 平台，不能用于 Windows Authenticode（Windows 的文件签名）或替代 Windows PACE 工具配置。开发者的 iLok/PACE 授权可按 PACE 官方流程在 Windows 签名环境配置；不要把凭据、私钥、SDK 或 iLok 信息提交到仓库。

本地 Windows 开发环境如确实需要自行构建，可使用以下命令；普通内测用户直接下载 Actions 产物即可：

```powershell
nuget install Microsoft.Web.WebView2 -Version 1.0.3485.44 -OutputDirectory C:\SanMuDependencies -Source https://api.nuget.org/v3/index.json -NonInteractive
.\packaging\windows\build_sanmu_windows.ps1 -Products All -WebView2PackageLocation C:\SanMuDependencies
```

这些候选包仍沿用源码设置的内测截止时间。网站上提供下载前，应确认截止时间、Windows 签名、真实宿主验证和下载记录一致；不要把“自动编译成功”描述为“已经正式发行”。
