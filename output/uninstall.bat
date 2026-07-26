@echo off
setlocal

pushd "%~dp0"

cscript //nologo check_windows_version.js
if errorlevel 1 goto unsupported

if /i "%1" == "/unregister" goto unregister

echo stopping service.
call stop_service.bat

echo administrative permissions required. detecting permissions...
net session >nul 2>&1
if not %errorlevel% == 0 (
  echo elevating command prompt...
  cscript //nologo sudo.js "%~f0" /unregister
  goto done
)

:unregister
echo unregistering WubiPinyin IME.
WubiPinyinSetup.exe /u
if errorlevel 1 goto error
reg delete "HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Run" /v WubiPinyinServer /f /reg:64
goto done

:unsupported
echo WubiPinyin requires Windows 11 x64. ARM64 and x86 are not supported.
goto error

:error
popd
exit /b 1

:done
echo uninstalled.
popd
exit /b 0
