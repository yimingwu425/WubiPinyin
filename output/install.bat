@echo off
setlocal

pushd "%~dp0"

cscript //nologo check_windows_version.js
if errorlevel 1 goto unsupported

if /i "%1" == "/register" goto register

echo stopping service from an older version.
call stop_service.bat

echo configuring preset input schemas...
WubiPinyinDeployer.exe /install

echo administrative permissions required. detecting permissions...
net session >nul 2>&1
if not %errorlevel% == 0 (
  echo elevating command prompt...
  cscript //nologo sudo.js "%~f0" /register
  goto done
)

:register
echo registering WubiPinyin IME to your system.
if not exist "%APPDATA%\WubiPinyin" mkdir "%APPDATA%\WubiPinyin"
reg add "HKEY_CURRENT_USER\Software\Rime\WubiPinyin" /v RimeUserDir /t REG_SZ /d "%APPDATA%\WubiPinyin" /f /reg:64
if errorlevel 1 goto error
reg add "HKEY_CURRENT_USER\Software\Rime\WubiPinyin" /v RimeUserDir /t REG_SZ /d "%APPDATA%\WubiPinyin" /f /reg:32
if errorlevel 1 goto error
WubiPinyinSetup.exe /s
if errorlevel 1 goto error
reg add "HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Run" /v WubiPinyinServer /t REG_SZ /d "%CD%\WubiPinyinServer.exe" /f /reg:64
if errorlevel 1 goto error
start "" WubiPinyinServer.exe
goto done

:unsupported
echo WubiPinyin requires Windows 11 x64. ARM64 and x86 are not supported.
goto error

:error
popd
exit /b 1

:done
echo installed.
popd
exit /b 0
