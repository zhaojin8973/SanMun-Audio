[CmdletBinding()]
param(
  [string]$Root = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
  [ValidateSet('All', 'EQ6', 'Comp', 'Delay', 'Reverb')]
  [string[]]$Products = @('All'),
  [string]$Output = (Join-Path $Root 'windows-packages'),
  [string]$BuildRoot = (Join-Path $Root 'windows-build'),
  [string]$WebView2PackageLocation = $env:JUCE_WEBVIEW2_PACKAGE_LOCATION,
  [string]$AaxSdkPath = (Join-Path $Root 'aax-sdk-2-9-0'),
  [switch]$BuildAax
)

$ErrorActionPreference = 'Stop'
$Configuration = 'Release'
$Generator = 'Visual Studio 17 2022'
$License = 'GNU AGPLv3 (Permanent / Open Source)'

function Invoke-Checked {
  param([string]$File, [string[]]$Arguments)
  & $File @Arguments
  if ($LASTEXITCODE -ne 0) { throw "$File 失败，退出码 $LASTEXITCODE" }
}

function Install-LockedNodeDependencies {
  param([string]$Directory)
  if (-not (Test-Path (Join-Path $Directory 'package-lock.json') -PathType Leaf)) {
    throw "找不到网页构建锁文件：$(Join-Path $Directory 'package-lock.json')"
  }
  # 必须在当前 Windows 上重装原生依赖，不能复用从 Mac 拷来的 node_modules。
  Write-Host "安装锁定的网页依赖：$Directory"
  Invoke-Checked -File $script:NpmCommand -Arguments @('--prefix', $Directory, 'ci', '--no-audit', '--no-fund')
}

if ($env:OS -ne 'Windows_NT') { throw '此脚本需要在 Windows 上运行；Mac 请继续使用现有 macOS 构建流程。' }
$Root = (Resolve-Path -LiteralPath $Root).Path
$Output = [IO.Path]::GetFullPath($Output)
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot)
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { throw '未找到 CMake，请安装 Visual Studio 2022 与 CMake 工具。' }
if (-not (Get-Command node -ErrorAction SilentlyContinue)) { throw '未找到 Node.js；插件网页界面构建需要 Node.js。' }
# 优先使用 npm.cmd，避免 PowerShell 执行策略拦截 npm.ps1。
$npm = Get-Command npm.cmd -ErrorAction SilentlyContinue
if (-not $npm) { throw '未找到 npm.cmd；请安装 Node.js LTS。' }
$script:NpmCommand = $npm.Source

# 检测 Inno Setup 编译器 (iscc)
$iscc = Get-Command iscc.exe -ErrorAction SilentlyContinue
if (-not $iscc) {
  $defaultIscc = Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'
  if (Test-Path $defaultIscc) {
    $iscc = [PSCustomObject]@{ Source = $defaultIscc }
  }
}
if ($iscc) {
  Write-Host "找到 Inno Setup 编译器：$($iscc.Source)"
} else {
  Write-Host "未检测到 Inno Setup；将仅生成绿色便携 ZIP 包（如需 .exe 安装器请安装 Inno Setup 6）。"
}

$allProducts = @(
  @{ Slug='EQ6'; Project='SanMu EQ'; Target='SanMuQ6'; Name='SanMu EQ6' },
  @{ Slug='Comp'; Project='SanMu Comp'; Target='SanMuComp'; Name='SanMu Comp' },
  @{ Slug='Delay'; Project='SanMu Delay'; Target='SanMuDelay'; Name='SanMu Delay' },
  @{ Slug='Reverb'; Project='SanMu Reverb'; Target='SanMuReverb'; Name='SanMu Reverb' }
)
$selectedProducts = @($allProducts | Where-Object { $Products -contains 'All' -or $Products -contains $_.Slug })
if ($selectedProducts.Count -eq 0) { throw '至少选择一款产品。' }

# JUCE 搜索的是包的父目录；既接受 NuGet root，也接受直接指定的包目录。
# SDK 提供编译头文件和加载器；测试电脑仍需安装 Microsoft Edge WebView2 Runtime。
if (-not $WebView2PackageLocation) {
  $WebView2PackageLocation = Join-Path $env:USERPROFILE 'AppData\Local\PackageManagement\NuGet\Packages'
}
$nugetRoot = [IO.Path]::GetFullPath($WebView2PackageLocation)
if (Test-Path (Join-Path $nugetRoot 'build\native\include\WebView2.h') -PathType Leaf) {
  $nugetRoot = Split-Path -Parent $nugetRoot
}
$webView2Packages = @(Get-ChildItem -LiteralPath $nugetRoot -Directory -Filter '*Microsoft.Web.WebView2*' -ErrorAction SilentlyContinue)
if ($webView2Packages.Count -ne 1) {
  throw "WebView2 SDK 目录须恰好包含一份 Microsoft.Web.WebView2 包：$nugetRoot。请为该构建指定独立的 -WebView2PackageLocation 目录。"
}
$webView2Package = $webView2Packages[0].FullName
if (-not (Test-Path (Join-Path $webView2Package 'build\native\include\WebView2.h') -PathType Leaf) -or
    -not (Test-Path (Join-Path $webView2Package 'build\native\x64\WebView2LoaderStatic.lib') -PathType Leaf)) {
  throw "WebView2 SDK 缺少 x64 编译文件：$webView2Package"
}
Write-Host "使用 WebView2 SDK：$webView2Package"
if ($BuildAax) {
  $AaxSdkPath = [IO.Path]::GetFullPath($AaxSdkPath)
  if (-not (Test-Path (Join-Path $AaxSdkPath 'Interfaces\AAX.h') -PathType Leaf)) {
    throw "未找到 AAX SDK：$AaxSdkPath。请使用 -AaxSdkPath 指定已获许可的 SDK。"
  }
}

Install-LockedNodeDependencies -Directory (Join-Path $Root 'packaging\web')
if ($selectedProducts.Slug -contains 'Reverb') {
  Install-LockedNodeDependencies -Directory (Join-Path $Root 'SanMu Reverb\prototypes\particle-life')
}

New-Item -ItemType Directory -Force -Path $Output | Out-Null
$checksums = [System.Collections.Generic.List[string]]::new()
$results = [System.Collections.Generic.List[object]]::new()
$sourceCommit = $null
if ((Get-Command git -ErrorAction SilentlyContinue) -and (Test-Path (Join-Path $Root '.git'))) {
  $sourceCommit = & git -C $Root rev-parse HEAD 2>$null
  if ($LASTEXITCODE -ne 0) { $sourceCommit = $null }
}

foreach ($product in $selectedProducts) {
  $projectPath = Join-Path $Root $product.Project
  $buildPath = Join-Path $BuildRoot $product.Slug
  $stagePath = Join-Path $buildPath 'stage'
  $projectCmake = Join-Path $projectPath 'CMakeLists.txt'
  if (-not (Test-Path $projectCmake -PathType Leaf)) { throw "找不到项目：$projectPath" }
  if (-not (Test-Path (Join-Path $Root 'external\JUCE\CMakeLists.txt') -PathType Leaf) -and
      -not (Test-Path (Join-Path $projectPath 'external\JUCE\CMakeLists.txt') -PathType Leaf)) {
    throw "JUCE 子模块未初始化：$projectPath。请先执行 git submodule update --init --recursive。"
  }
  $versionMatch = [regex]::Match((Get-Content -LiteralPath $projectCmake -Raw), '(?is)project\s*\([^)]*?\bVERSION\s+([0-9]+(?:\.[0-9]+){2,3})')
  if (-not $versionMatch.Success) { throw "无法从 CMake 项目读取版本号：$projectCmake" }
  $version = $versionMatch.Groups[1].Value

  $configure = @('-S', $projectPath, '-B', $buildPath, '-G', $Generator, '-A', 'x64',
    '-DCMAKE_BUILD_TYPE=Release', '-DSANMU_COPY_PLUGIN_AFTER_BUILD=OFF',
    "-DSANMU_BUILD_AAX=$($BuildAax.IsPresent.ToString().ToUpperInvariant())",
    "-DJUCE_WEBVIEW2_PACKAGE_LOCATION=$nugetRoot")
  if ($BuildAax) { $configure += "-DSANMU_AAX_SDK_PATH=$AaxSdkPath" }
  Invoke-Checked -File 'cmake' -Arguments $configure

  $targets = @("$($product.Target)_VST3")
  if ($BuildAax) { $targets += "$($product.Target)_AAX" }
  Invoke-Checked -File 'cmake' -Arguments (@('--build', $buildPath, '--config', $Configuration, '--target') + $targets)

  if (Test-Path $stagePath) { Remove-Item -LiteralPath $stagePath -Recurse -Force }
  $vst3 = Join-Path $buildPath "source\$($product.Target)_artefacts\$Configuration\VST3\$($product.Name).vst3"
  if (-not (Test-Path $vst3)) { throw "未找到 $($product.Name) VST3 产物：$vst3" }
  New-Item -ItemType Directory -Force -Path (Join-Path $stagePath 'VST3') | Out-Null
  Copy-Item -LiteralPath $vst3 -Recurse -Destination (Join-Path $stagePath 'VST3')

  if ($BuildAax) {
    $aax = Join-Path $buildPath "source\$($product.Target)_artefacts\$Configuration\AAX\$($product.Name).aaxplugin"
    if (-not (Test-Path $aax)) { throw "未找到 $($product.Name) AAX 产物：$aax" }
    New-Item -ItemType Directory -Force -Path (Join-Path $stagePath 'AAX') | Out-Null
    Copy-Item -LiteralPath $aax -Recurse -Destination (Join-Path $stagePath 'AAX')
  }

  @"
SanMu Audio $($product.Name) Windows 社区版 (VST3)
版本：$version
授权：GNU AGPLv3 开源协议，永久无期限限制。

VST3 安装：将 VST3\$($product.Name).vst3 复制到 C:\Program Files\Common Files\VST3 目录。
$(if ($BuildAax) { 'AAX 仅为未签名的构建产物，须先完成 Windows PACE 发布签名再供普通 Pro Tools 使用。' } else { '本包为 VST3 格式；AAX 需要 Windows AAX SDK 和 PACE 发布签名。' })
界面运行依赖 Microsoft Edge WebView2 Runtime；Windows 10/11 通常已自带，若缺失请前往微软官网安装 Evergreen WebView2 Runtime。
首次使用前请完全退出音频宿主，安装后重新扫描插件。
"@ | Set-Content -Encoding UTF8 (Join-Path $stagePath 'Windows安装说明.txt')

  $suffix = if ($BuildAax) { 'VST3_AAX' } else { 'VST3' }
  $archive = Join-Path $Output "SanMu_$($product.Slug)_v${version}_Windows_x64_$suffix.zip"
  if (Test-Path $archive) { Remove-Item -LiteralPath $archive -Force }
  Compress-Archive -Path (Join-Path $stagePath '*') -DestinationPath $archive -CompressionLevel Optimal
  $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash.ToLowerInvariant()
  $checksums.Add("$hash  $([IO.Path]::GetFileName($archive))")
  $results.Add([ordered]@{ product=$product.Slug; version=$version; sourceCommit=$sourceCommit;
    file=[IO.Path]::GetFileName($archive); sha256=$hash; architecture='x64'; includesAax=$BuildAax.IsPresent;
    paceSigned=$false; authenticodeSigned=$false; hostVerified=$false })
  Write-Host "已生成绿色便携包：$archive"

  # 如果检测到 Inno Setup，生成双击安装向导 (.exe)
  $installerScript = Join-Path $Root 'packaging\windows\installer.iss'
  $licenseFile = Join-Path $Root 'LICENSE'
  $installerBaseName = "SanMu_$($product.Slug)_v${version}_Windows_x64_Installer"
  if ($iscc -and (Test-Path $installerScript -PathType Leaf)) {
    Write-Host "正在使用 Inno Setup 生成 Windows 安装向导 ($installerBaseName.exe)..."
    $isccArgs = @(
      "/DMyAppName=$($product.Name)",
      "/DMyAppVersion=$version",
      "/DMyProductSlug=$($product.Slug)",
      "/DMySourceVst3Dir=$vst3",
      "/DMyOutputDir=$Output",
      "/DMyOutputBaseFilename=$installerBaseName",
      "/DMyLicenseFile=$licenseFile",
      $installerScript
    )
    Invoke-Checked -File $iscc.Source -Arguments $isccArgs
    $installerExe = Join-Path $Output "$installerBaseName.exe"
    if (Test-Path $installerExe) {
      $exeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installerExe).Hash.ToLowerInvariant()
      $checksums.Add("$exeHash  $installerBaseName.exe")
      $results.Add([ordered]@{ product=$product.Slug; version=$version; sourceCommit=$sourceCommit;
        file="$installerBaseName.exe"; sha256=$exeHash; type='installer_exe'; architecture='x64';
        paceSigned=$false; authenticodeSigned=$false; hostVerified=$false })
      Write-Host "已生成双击安装程序：$installerExe"
    }
  }
}

$checksums | Set-Content -Encoding ASCII (Join-Path $Output 'SHA256SUMS.txt')
ConvertTo-Json -InputObject @($results.ToArray()) -Depth 5 | Set-Content -Encoding UTF8 (Join-Path $Output 'build-results.json')
Write-Host "Windows 内测候选包完成。校验文件：$(Join-Path $Output 'SHA256SUMS.txt')"
