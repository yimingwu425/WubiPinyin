; WubiPinyin installation script. Derived from Weasel; see LICENSE.txt.
!include FileFunc.nsh
!include LogicLib.nsh
!include MUI2.nsh
!include x64.nsh
!include winVer.nsh

Unicode true

!ifndef WEASEL_VERSION
!define WEASEL_VERSION 0.1.0
!endif

!ifndef WEASEL_BUILD
!define WEASEL_BUILD 0
!endif

!ifndef PRODUCT_VERSION
!define PRODUCT_VERSION "${WEASEL_VERSION}.${WEASEL_BUILD}"
!endif

!define PRODUCT_REG_KEY "Software\Rime\WubiPinyin"
!define PRODUCT_USER_REG_KEY "Software\Rime\WubiPinyin"
!define WUBIPINYIN_ROOT_VALUE "WubiPinyinRoot"
!define REG_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\WubiPinyin"

Name "五笔·拼音 ${WEASEL_VERSION}"
OutFile "archives\wubipinyin-${PRODUCT_VERSION}-installer.exe"

VIProductVersion "${WEASEL_VERSION}.${WEASEL_BUILD}"
VIAddVersionKey /LANG=2052 "ProductName" "五笔·拼音"
VIAddVersionKey /LANG=2052 "Comments" "Powered by RIME | 中州韻輸入法引擎"
VIAddVersionKey /LANG=2052 "CompanyName" "式恕堂"
VIAddVersionKey /LANG=2052 "LegalCopyright" "Copyleft RIME Developers"
VIAddVersionKey /LANG=2052 "FileDescription" "五笔·拼音输入法"
VIAddVersionKey /LANG=2052 "FileVersion" "${WEASEL_VERSION}"

!define MUI_ICON ..\resource\weasel.ico
SetCompressor /SOLID lzma
RequestExecutionLevel admin

!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

LangString DISPLAYNAME ${LANG_SIMPCHINESE} "五笔·拼音"
LangString LNKFORSETTING ${LANG_SIMPCHINESE} "五笔·拼音设置"
LangString LNKFORDICT ${LANG_SIMPCHINESE} "五笔·拼音词典管理"
LangString LNKFORDEPLOY ${LANG_SIMPCHINESE} "五笔·拼音重新部署"
LangString LNKFORSERVER ${LANG_SIMPCHINESE} "五笔·拼音服务"
LangString LNKFORAPPFOLDER ${LANG_SIMPCHINESE} "五笔·拼音程序文件夹"
LangString LNKFORUNINSTALL ${LANG_SIMPCHINESE} "卸载五笔·拼音"
LangString CONFIRMATION ${LANG_SIMPCHINESE} "将升级已安装的五笔·拼音。继续吗？"
LangString SYSTEMVERSIONNOTOK ${LANG_SIMPCHINESE} "此版本仅支持 Windows 11 x64。"
LangString REMOVE_USER_DATA ${LANG_SIMPCHINESE} "同时删除五笔·拼音的用户词库和设置？"

LangString DISPLAYNAME ${LANG_ENGLISH} "WubiPinyin"
LangString LNKFORSETTING ${LANG_ENGLISH} "WubiPinyin Settings"
LangString LNKFORDICT ${LANG_ENGLISH} "WubiPinyin Dictionary Manager"
LangString LNKFORDEPLOY ${LANG_ENGLISH} "Redeploy WubiPinyin"
LangString LNKFORSERVER ${LANG_ENGLISH} "WubiPinyin Service"
LangString LNKFORAPPFOLDER ${LANG_ENGLISH} "WubiPinyin Program Folder"
LangString LNKFORUNINSTALL ${LANG_ENGLISH} "Uninstall WubiPinyin"
LangString CONFIRMATION ${LANG_ENGLISH} "An existing WubiPinyin installation will be upgraded. Continue?"
LangString SYSTEMVERSIONNOTOK ${LANG_ENGLISH} "This version requires Windows 11 x64."
LangString REMOVE_USER_DATA ${LANG_ENGLISH} "Also delete WubiPinyin user dictionaries and settings?"

Function .onInit
  ${IfNot} ${AtLeastWin11}
    IfSilent unsupported
    MessageBox MB_OK "$(SYSTEMVERSIONNOTOK)"
unsupported:
    Quit
  ${EndIf}
  ${IfNot} ${IsNativeAMD64}
    IfSilent unsupported
    MessageBox MB_OK "$(SYSTEMVERSIONNOTOK)"
    Quit
  ${EndIf}

  ; The installer itself is a 32-bit NSIS executable. Keep application
  ; registration in the native view consumed by the x64 Broker and Settings.
  SetRegView 64
  ReadRegStr $R0 HKLM "${PRODUCT_REG_KEY}" "${WUBIPINYIN_ROOT_VALUE}"
  StrCmp $R0 "" legacy_install_dir
  StrCpy $INSTDIR "$R0"
  Goto install_dir_ready
legacy_install_dir:
  ReadRegStr $R0 HKLM "${PRODUCT_REG_KEY}" "InstallDir"
  StrCmp $R0 "" default_install_dir
  StrCpy $INSTDIR "$R0"
  Goto install_dir_ready
default_install_dir:
  StrCpy $INSTDIR "$PROGRAMFILES64\WubiPinyin"
install_dir_ready:

  ReadRegStr $R0 HKLM "${REG_UNINST_KEY}" "UninstallString"
  StrCmp $R0 "" done
  StrCpy $0 "Upgrade"
  IfSilent upgrade 0
  MessageBox MB_OKCANCEL|MB_ICONINFORMATION "$(CONFIRMATION)" IDOK upgrade
  Abort

upgrade:
  ReadRegStr $R1 HKLM "${PRODUCT_REG_KEY}" "${WUBIPINYIN_ROOT_VALUE}"
  StrCmp $R1 "" remove_previous_registration

  IfFileExists "$R1\WubiPinyinServer.exe" 0 +2
  ExecWait '"$R1\WubiPinyinServer.exe" /quit'
  IfFileExists "$R1\WubiPinyinSetup.exe" 0 +2
  ExecWait '"$R1\WubiPinyinSetup.exe" /u'
  RMDir /r /REBOOTOK "$R1"
remove_previous_registration:
  DeleteRegKey HKLM "${PRODUCT_REG_KEY}"
  DeleteRegKey HKLM "${REG_UNINST_KEY}"
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "WubiPinyinServer"
done:
FunctionEnd

Function un.onInit
  SetRegView 64
FunctionEnd

Section "WubiPinyin"
  SectionIn RO
  WriteRegStr HKLM "${PRODUCT_REG_KEY}" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "${PRODUCT_REG_KEY}" "${WUBIPINYIN_ROOT_VALUE}" "$INSTDIR"
  SetOutPath "$INSTDIR"
  SetOverwrite try

  File "LICENSE.txt"
  File "README.txt"
  File "7-zip-license.txt"
  File "COPYING-curl.txt"
  File "start_service.bat"
  File "stop_service.bat"
  File "WubiPinyin.dll"
  File "WubiPinyinx64.dll"
  File "WubiPinyinServer.exe"
  File "WubiPinyinDeployer.exe"
  ; The bootstrap intentionally stays Win32: it registers both TIP bitnesses.
  ; WubiPinyinSettings.exe is the user-facing x64 settings application.
  File "WubiPinyinSetup.exe"
  File "rime.dll"
  File /nonfatal "sqlite3.dll"
  ; This directory is produced by WubiPinyinSettings\build_release.bat.
  ; Keep every Windows App SDK runtime dependency beside the x64 Settings exe.
  File /r "settings\Release\x64\*.*"

  SetOutPath "$INSTDIR\data"
  File "data\*.yaml"
  File /nonfatal "data\*.txt"
  File /nonfatal "data\*.gram"
  ; stage-locked-sources.ps1 verifies the upstream dictionary hashes before
  ; placing the WubiPinyin schema and both dictionaries in this directory.
  File /r "data\WubiPinyinData\rime\*.*"
  SetOutPath "$INSTDIR\data\opencc"
  File "data\opencc\*.json"
  File "data\opencc\*.ocd*"
  SetOutPath "$INSTDIR\data\preview"
  File /nonfatal "data\preview\*.png"
  SetOutPath "$INSTDIR\WubiPinyinData"
  File "data\WubiPinyinData\sources.lock.json"
  File "data\WubiPinyinData\THIRD_PARTY_NOTICES.md"
  SetOutPath "$INSTDIR\WubiPinyinData\licenses"
  File /r "data\WubiPinyinData\licenses\*.*"

  SetOutPath "$INSTDIR"
  StrCpy $R0 "$APPDATA\WubiPinyin"
  CreateDirectory "$R0"
  ExecWait '"$INSTDIR\WubiPinyinSetup.exe" /s'
  ExecWait '"$INSTDIR\WubiPinyinDeployer.exe" /install'

  WriteRegStr HKLM "${REG_UNINST_KEY}" "DisplayName" "$(DISPLAYNAME)"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "DisplayIcon" '"$INSTDIR\WubiPinyinServer.exe"'
  WriteRegStr HKLM "${REG_UNINST_KEY}" "DisplayVersion" "${WEASEL_VERSION}.${WEASEL_BUILD}"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "${REG_UNINST_KEY}" "Publisher" "RIME Developers and WubiPinyin contributors"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "URLInfoAbout" "https://rime.im/"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "HelpLink" "https://rime.im/docs/"
  WriteRegDWORD HKLM "${REG_UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${REG_UNINST_KEY}" "NoRepair" 1
  WriteUninstaller "$INSTDIR\uninstall.exe"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "WubiPinyinServer" "$INSTDIR\WubiPinyinServer.exe"
  Exec "$INSTDIR\WubiPinyinServer.exe"
SectionEnd

Section "Start Menu Shortcuts"
  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\$(DISPLAYNAME)"
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORSETTING).lnk" "$INSTDIR\WubiPinyinSettings.exe" "" "$INSTDIR\WubiPinyinSettings.exe" 0
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORDICT).lnk" "$INSTDIR\WubiPinyinSettings.exe" "--page dictionary" "$INSTDIR\WubiPinyinSettings.exe" 0
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORDEPLOY).lnk" "$INSTDIR\WubiPinyinDeployer.exe" "/deploy" "$SYSDIR\shell32.dll" 144
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORSERVER).lnk" "$INSTDIR\WubiPinyinServer.exe" "" "$INSTDIR\WubiPinyinServer.exe" 0
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORAPPFOLDER).lnk" "$INSTDIR\WubiPinyinServer.exe" "/weaseldir" "$SYSDIR\shell32.dll" 19
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORUNINSTALL).lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
SectionEnd

Section "Uninstall"
  SetRegView 64
  IfFileExists "$INSTDIR\WubiPinyinServer.exe" 0 +2
  ExecWait '"$INSTDIR\WubiPinyinServer.exe" /quit'
  IfFileExists "$INSTDIR\WubiPinyinSetup.exe" 0 +2
  ExecWait '"$INSTDIR\WubiPinyinSetup.exe" /u'

  ; The product does not support a custom user-data directory.  Do not use a
  ; user-writable registry value as an elevated recursive-delete target.
  StrCpy $R0 "$APPDATA\WubiPinyin"
  IfSilent remove_product
  MessageBox MB_YESNO|MB_ICONQUESTION "$(REMOVE_USER_DATA)" IDYES remove_user_data
  Goto remove_product
remove_user_data:
  RMDir /r "$R0"
  DeleteRegKey HKCU "${PRODUCT_USER_REG_KEY}"
  SetRegView 32
  DeleteRegKey HKCU "${PRODUCT_USER_REG_KEY}"
  SetRegView 64

remove_product:
  DeleteRegKey HKLM "${PRODUCT_REG_KEY}"
  DeleteRegKey HKLM "${REG_UNINST_KEY}"
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "WubiPinyinServer"
  SetOutPath "$TEMP"
  RMDir /r /REBOOTOK "$INSTDIR"
  SetShellVarContext all
  RMDir /r "$SMPROGRAMS\$(DISPLAYNAME)"
SectionEnd
