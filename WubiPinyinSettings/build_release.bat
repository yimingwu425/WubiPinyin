@echo off
setlocal

where msbuild >nul 2>nul
if errorlevel 1 (
  echo MSBuild was not found. Run this from a Visual Studio Developer Command Prompt.
  exit /b 1
)

msbuild "%~dp0WubiPinyinSettings.vcxproj" "/t:Restore;Build" /p:Configuration=Release /p:Platform=x64
exit /b %errorlevel%
