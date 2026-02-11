; Yuanta AutoTrading Installer Script
; Inno Setup Script (https://jrsoftware.org/isinfo.php)
;
; 사용법:
; 1. Inno Setup 다운로드 및 설치: https://jrsoftware.org/isdl.php
; 2. 이 파일을 Inno Setup Compiler로 열기
; 3. Compile 버튼 클릭
; 4. Output 폴더에 설치파일 생성됨

#define MyAppName "유안타 자동매매"
#define MyAppNameEng "Yuanta AutoTrading"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Your Company"
#define MyAppURL "https://github.com/your-repo/yuanta-autotrading"
#define MyAppExeName "yuanta_autotrading.exe"

[Setup]
; 앱 정보
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

; 설치 경로
DefaultDirName={autopf}\{#MyAppNameEng}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes

; 출력 설정
OutputDir=..\dist
OutputBaseFilename=YuantaAutoTrading_Setup_{#MyAppVersion}
SetupIconFile=..\assets\icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}

; 압축 설정
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes

; 권한 설정
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog

; UI 설정
WizardStyle=modern
WizardResizable=no

; 기타
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 6.1; Check: not IsAdminInstallMode

[Files]
; 실행 파일
Source: "..\build\bin\Release\yuanta_autotrading.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\Release\backtest.exe"; DestDir: "{app}"; Flags: ignoreversion

; 설정 파일
Source: "..\config\settings.json"; DestDir: "{app}\config"; Flags: ignoreversion
Source: "..\config\settings.ini"; DestDir: "{app}\config"; Flags: ignoreversion confirmoverwrite

; 유안타 API DLL (있는 경우)
Source: "..\lib\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; Visual C++ 런타임 (필요시)
Source: "..\redist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist

; 문서
Source: "..\README.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}\docs"; Flags: ignoreversion skipifsourcedoesntexist

[Dirs]
Name: "{app}\logs"; Permissions: users-modify
Name: "{app}\data"; Permissions: users-modify
Name: "{app}\config"; Permissions: users-modify

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\백테스트"; Filename: "{app}\backtest.exe"
Name: "{group}\설정 폴더"; Filename: "{app}\config"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; Visual C++ 런타임 설치 (있는 경우)
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/quiet /norestart"; StatusMsg: "Visual C++ 런타임 설치 중..."; Flags: waituntilterminated skipifdoesntexist

; 설치 완료 후 실행 옵션
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent unchecked

[UninstallDelete]
Type: filesandordirs; Name: "{app}\logs"
Type: filesandordirs; Name: "{app}\data"

[Code]
// 설치 전 확인
function InitializeSetup(): Boolean;
begin
  Result := True;

  // 이전 버전 확인
  if RegKeyExists(HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}_is1') then
  begin
    if MsgBox('이전 버전이 설치되어 있습니다. 제거하고 새 버전을 설치하시겠습니까?',
              mbConfirmation, MB_YESNO) = IDNO then
    begin
      Result := False;
    end;
  end;
end;

// 설치 완료 후 설정 파일 안내
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    MsgBox('설치가 완료되었습니다.' + #13#10 + #13#10 +
           '사용 전 다음 사항을 확인하세요:' + #13#10 +
           '1. config 폴더의 settings.ini 파일에서 계정 정보 설정' + #13#10 +
           '2. 유안타증권 Open API 서비스 신청' + #13#10 +
           '3. 모의투자로 먼저 테스트',
           mbInformation, MB_OK);
  end;
end;
