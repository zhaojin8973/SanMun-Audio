; SanMu Audio Windows VST3 4-in-1 Suite Inno Setup Script
; Generates standard Windows 64-bit VST3 Suite installer wizard

#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif
#ifndef MyOutputDir
  #define MyOutputDir "."
#endif
#ifndef MyEq6Dir
  #define MyEq6Dir "VST3\SanMu EQ6.vst3"
#endif
#ifndef MyCompDir
  #define MyCompDir "VST3\SanMu Comp.vst3"
#endif
#ifndef MyDelayDir
  #define MyDelayDir "VST3\SanMu Delay.vst3"
#endif
#ifndef MyReverbDir
  #define MyReverbDir "VST3\SanMu Reverb.vst3"
#endif
#ifndef MyLicenseFile
  #define MyLicenseFile "..\..\LICENSE"
#endif

[Setup]
AppId={{A924FE8B-3651-4191-B72D-SanMuAudioSuite}}
AppName=SanMu Audio Plugin Suite
AppVersion={#MyAppVersion}
AppPublisher=SanMu Audio
AppPublisherURL=https://github.com/zhaojin8973/SanMun-Audio
AppSupportURL=https://github.com/zhaojin8973/SanMun-Audio
AppUpdatesURL=https://github.com/zhaojin8973/SanMun-Audio
DefaultDirName={commoncf64}\VST3
DisableDirPage=no
AppendDefaultDirName=no
DefaultGroupName=SanMu Audio
DisableProgramGroupPage=yes
LicenseFile={#MyLicenseFile}
OutputDir={#MyOutputDir}
OutputBaseFilename=SanMu_Audio_Suite_v{#MyAppVersion}_Windows_x64_Installer
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=admin
WizardStyle=modern

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "完整安装（包含全部四款插件）"
Name: "custom"; Description: "自定义选择安装"

[Components]
Name: "eq6"; Description: "SanMu EQ6 (6 频段动态/静态均衡器)"; Types: full custom; Flags: checkablealone
Name: "comp"; Description: "SanMu Comp (四模式专业母带压缩器)"; Types: full custom; Flags: checkablealone
Name: "delay"; Description: "SanMu Delay (四引擎立体声延迟)"; Types: full custom; Flags: checkablealone
Name: "reverb"; Description: "SanMu Reverb (四空间算法混响)"; Types: full custom; Flags: checkablealone

[Files]
Source: "{#MyEq6Dir}\*"; DestDir: "{app}\SanMu EQ6.vst3"; Components: eq6; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyCompDir}\*"; DestDir: "{app}\SanMu Comp.vst3"; Components: comp; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyDelayDir}\*"; DestDir: "{app}\SanMu Delay.vst3"; Components: delay; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyReverbDir}\*"; DestDir: "{app}\SanMu Reverb.vst3"; Components: reverb; Flags: ignoreversion recursesubdirs createallsubdirs

[Messages]
chinesesimp.WelcomeLabel1=欢迎使用 SanMu Audio 效果器套件安装向导
chinesesimp.WelcomeLabel2=此向导将在您的计算机上安装 SanMu Audio 64位 VST3 音频效果器套件（EQ6、Comp、Delay、Reverb）。%n%n建议在安装前关闭所有正在运行的音频宿主（如 Cubase、FL Studio、Studio One、Reaper 等）。%n%n点击“下一步”继续。
english.WelcomeLabel1=Welcome to the SanMu Audio Plugin Suite Setup Wizard
english.WelcomeLabel2=This wizard will install SanMu Audio 64-bit VST3 plug-in suite (EQ6, Comp, Delay, Reverb) on your computer.%n%nIt is recommended that you close your DAW applications before continuing.%n%nClick Next to continue.
