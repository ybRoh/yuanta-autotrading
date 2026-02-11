; Yuanta AutoTrading NSIS Installer Script
; NSIS (Nullsoft Scriptable Install System)
;
; 사용법:
; 1. NSIS 다운로드 및 설치: https://nsis.sourceforge.io/Download
; 2. 이 파일을 마우스 오른쪽 클릭 -> Compile NSIS Script
; 3. 같은 폴더에 설치파일 생성됨

;--------------------------------
; 기본 설정

!define PRODUCT_NAME "유안타 자동매매"
!define PRODUCT_NAME_ENG "YuantaAutoTrading"
!define PRODUCT_VERSION "1.0.0"
!define PRODUCT_PUBLISHER "Your Company"
!define PRODUCT_WEB_SITE "https://github.com/your-repo/yuanta-autotrading"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\yuanta_autotrading.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME_ENG}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

;--------------------------------
; 압축 설정

SetCompressor /SOLID lzma
SetCompressorDictSize 64

;--------------------------------
; Modern UI

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

; MUI 설정
!define MUI_ABORTWARNING
!define MUI_ICON "..\assets\icon.ico"
!define MUI_UNICON "..\assets\icon.ico"

; 시작 페이지
!define MUI_WELCOMEPAGE_TITLE "유안타 자동매매 시스템 설치"
!define MUI_WELCOMEPAGE_TEXT "이 프로그램은 유안타증권 Open API를 사용한 주식 자동매매 시스템입니다.$\r$\n$\r$\n설치를 시작하려면 [다음]을 클릭하세요."

; 완료 페이지
!define MUI_FINISHPAGE_RUN "$INSTDIR\yuanta_autotrading.exe"
!define MUI_FINISHPAGE_RUN_TEXT "프로그램 실행"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\docs\README.md"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "README 열기"

;--------------------------------
; 페이지

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

;--------------------------------
; 언어

!insertmacro MUI_LANGUAGE "Korean"
!insertmacro MUI_LANGUAGE "English"

;--------------------------------
; 설치 정보

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "..\dist\${PRODUCT_NAME_ENG}_Setup_${PRODUCT_VERSION}.exe"
InstallDir "$PROGRAMFILES64\${PRODUCT_NAME_ENG}"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show
RequestExecutionLevel admin

;--------------------------------
; 설치 섹션

Section "메인 프로그램" SEC01
    SetOutPath "$INSTDIR"
    SetOverwrite on

    ; 64비트 확인
    ${If} ${RunningX64}
        SetRegView 64
    ${Else}
        MessageBox MB_OK|MB_ICONSTOP "이 프로그램은 64비트 Windows에서만 실행됩니다."
        Abort
    ${EndIf}

    ; 실행 파일
    File "..\build\bin\Release\yuanta_autotrading.exe"
    File "..\build\bin\Release\backtest.exe"

    ; DLL 파일 (있는 경우)
    File /nonfatal "..\lib\*.dll"

    ; 설정 폴더
    SetOutPath "$INSTDIR\config"
    File "..\config\settings.json"
    File "..\config\settings.ini"

    ; 문서 폴더
    SetOutPath "$INSTDIR\docs"
    File "..\README.md"
    File /nonfatal "..\LICENSE"

    ; 로그 및 데이터 폴더 생성
    CreateDirectory "$INSTDIR\logs"
    CreateDirectory "$INSTDIR\data"

    ; 시작 메뉴 바로가기
    CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\yuanta_autotrading.exe"
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\백테스트.lnk" "$INSTDIR\backtest.exe"
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\설정 폴더.lnk" "$INSTDIR\config"
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\제거.lnk" "$INSTDIR\uninst.exe"

    ; 바탕화면 바로가기
    CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\yuanta_autotrading.exe"
SectionEnd

Section "Visual C++ 런타임" SEC02
    ; VC++ 런타임 설치 (필요시)
    SetOutPath "$TEMP"
    File /nonfatal "..\redist\vc_redist.x64.exe"
    IfFileExists "$TEMP\vc_redist.x64.exe" 0 +3
        ExecWait '"$TEMP\vc_redist.x64.exe" /quiet /norestart'
        Delete "$TEMP\vc_redist.x64.exe"
SectionEnd

Section -Post
    WriteUninstaller "$INSTDIR\uninst.exe"
    WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\yuanta_autotrading.exe"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\yuanta_autotrading.exe"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"

    ; 설치 크기 계산
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

;--------------------------------
; 제거 섹션

Section Uninstall
    ; 프로세스 종료
    nsExec::ExecToLog 'taskkill /F /IM yuanta_autotrading.exe'
    nsExec::ExecToLog 'taskkill /F /IM backtest.exe'

    ; 파일 삭제
    Delete "$INSTDIR\yuanta_autotrading.exe"
    Delete "$INSTDIR\backtest.exe"
    Delete "$INSTDIR\*.dll"
    Delete "$INSTDIR\uninst.exe"

    ; 폴더 삭제
    RMDir /r "$INSTDIR\config"
    RMDir /r "$INSTDIR\docs"
    RMDir /r "$INSTDIR\logs"
    RMDir /r "$INSTDIR\data"
    RMDir "$INSTDIR"

    ; 시작 메뉴 삭제
    Delete "$SMPROGRAMS\${PRODUCT_NAME}\*.lnk"
    RMDir "$SMPROGRAMS\${PRODUCT_NAME}"

    ; 바탕화면 바로가기 삭제
    Delete "$DESKTOP\${PRODUCT_NAME}.lnk"

    ; 레지스트리 삭제
    DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
    DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
    SetAutoClose true
SectionEnd

;--------------------------------
; 함수

Function .onInit
    ; 이전 설치 확인
    ReadRegStr $R0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString"
    StrCmp $R0 "" done

    MessageBox MB_YESNO|MB_ICONQUESTION \
        "이전 버전이 설치되어 있습니다. 제거하고 새 버전을 설치하시겠습니까?" \
        IDYES uninst
    Abort

uninst:
    ExecWait '$R0 /S'

done:
FunctionEnd
