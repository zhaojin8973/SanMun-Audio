; SanMu Audio Windows VST3 Inno Setup Script
; Generates standard Windows 64-bit VST3 installer wizard

#ifndef MyAppName
  #define MyAppName "SanMu EQ6"
#endif
#ifndef MyAppVersion
  #define MyAppVersion "0.1.5"
#endif
#ifndef MyAppPublisher
  #define MyAppPublisher "SanMu Audio"
#endif
#ifndef MyAppURL
  #define MyAppURL "https://github.com/zhaojin8973/SanMun-Audio"
#endif
#ifndef MyOutputDir
  #define MyOutputDir "."
#endif
#ifndef MyOutputBaseFilename
  #define MyOutputBaseFilename "SanMu_EQ6_Windows_Installer"
#endif
#ifndef MySourceVst3Dir
  #define MySourceVst3Dir "VST3\SanMu EQ6.vst3"
#endif
#ifndef MyLicenseFile
  #define MyLicenseFile "..\..\LICENSE"
#endif

[Setup]
AppId={{D37E58C1-825B-4B7E-9721-SanMuAudio-{#MyAppName}}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={commoncf64}\VST3
DisableDirPage=no
AppendDefaultDirName=no
DefaultGroupName={#MyAppPublisher}
DisableProgramGroupPage=yes
LicenseFile={#MyLicenseFile}
OutputDir={#MyOutputDir}
OutputBaseFilename={#MyOutputBaseFilename}
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
WizardStyle=modern

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#MySourceVst3Dir}\*"; DestDir: "{app}\{#MyAppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
chinesesimp.WelcomeLabel1=欢迎使用 [name] 安装向导
chinesesimp.WelcomeLabel2=此向导将在您的计算机上安装 [name] 64位 VST3 音频效果器插件。%n%n建议在安装前关闭所有正在运行的音频宿主（如 Cubase、FL Studio、Studio One、Reaper 等）。%n%n点击“下一步”继续。
english.WelcomeLabel1=Welcome to the [name] Setup Wizard
english.WelcomeLabel2=This wizard will install [name] 64-bit VST3 audio plug-in on your computer.%n%nIt is recommended that you close your DAW applications before continuing.%n%nClick Next to continue.
