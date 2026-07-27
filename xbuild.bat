@echo off
setlocal enabledelayedexpansion
if not defined include (
  echo You should run this inside Developer Command Promt!
  exit /b
)
for %%a in ("%include:;=" "%") do (
  echo %%a| findstr /r "ATLMFC\\include" > nul
  if not errorlevel 1 (
    set "atl_lib_dir=%%a"
    set dpi_manifest=!atl_lib_dir:ATLMFC\include=Include\Manifest\PerMonitorHighDPIAware.manifest!
    if not exist ".\PerMonitorHighDPIAware.manifest" (
      copy ""!dpi_manifest!"" .\
    )
  )
)
rem ---------------------------------------------------------------------------
if not exist env.bat copy env.bat.template env.bat
if exist env.bat call env.bat
set PRODUCT_VERSION=
if not defined WEASEL_ROOT set WEASEL_ROOT=%CD%
if not defined VERSION_MAJOR set VERSION_MAJOR=0
if not defined VERSION_MINOR set VERSION_MINOR=17
if not defined VERSION_PATCH set VERSION_PATCH=4

if not defined WEASEL_VERSION set WEASEL_VERSION=%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH%
if not defined WEASEL_BUILD set WEASEL_BUILD=1

rem use numeric build version for release build
set PRODUCT_VERSION=%WEASEL_VERSION%.%WEASEL_BUILD%
rem for non-release build, try to use git commit hash as product build version
if not defined RELEASE_BUILD (
  rem check if git is installed and available, then get the short commit id of head
  git --version >nul 2>&1
  if not errorlevel 1 (
    for /f "delims=" %%i in ('git tag --sort=-creatordate ^| findstr /r "%WEASEL_VERSION%"') do (
      set LAST_TAG=%%i
      goto found_tag
    )
    :found_tag
    for /f "delims=" %%i in ('git rev-list %LAST_TAG%..HEAD --count') do (
      set WEASEL_BUILD=%%i
    )
    rem get short commmit id of head
    for /F %%i in ('git rev-parse --short HEAD') do (set PRODUCT_VERSION=%WEASEL_VERSION%.%WEASEL_BUILD%.%%i)
  )
)

rem FILE_VERSION is always 4 numbers; same as PRODUCT_VERSION in release build
if not defined FILE_VERSION set FILE_VERSION=%WEASEL_VERSION%.%WEASEL_BUILD%
echo PRODUCT_VERSION=%PRODUCT_VERSION%
echo WEASEL_VERSION=%WEASEL_VERSION%
echo WEASEL_BUILD=%WEASEL_BUILD%
echo WEASEL_ROOT=%WEASEL_ROOT%
echo WEASEL_BUNDLED_RECIPES=%WEASEL_BUNDLED_RECIPES%
echo BOOST_ROOT=%BOOST_ROOT%

if defined GITHUB_ENV (
	setlocal enabledelayedexpansion
	echo git_ref_name=%PRODUCT_VERSION%>>!GITHUB_ENV!
)

rem ---------------------------------------------------------------------------
rem parse the command line options
set build_config=release
set build_rebuild=0
set build_boost=0
set boost_build_variant=release
set build_data=0
set build_opencc=0
set build_rime=0
set rime_build_variant=release
set build_weasel=0
set build_installer=0
set build_clean=0
set build_commands=0
set verify_hybrid_filter=0
if /I "%VERIFY_HYBRID_FILTER%" == "1" set verify_hybrid_filter=1
:parse_cmdline_options
  if "%1" == "" goto end_parsing_cmdline_options
  if "%1" == "debug" (
    set build_config=debug
    set boost_build_variant=debug
    set rime_build_variant=debug
  )
  if "%1" == "release" (
    set build_config=release
    set boost_build_variant=release
    set rime_build_variant=release
  )
  if "%1" == "rebuild" set build_rebuild=1
  if "%1" == "boost" set build_boost=1
  if "%1" == "data" set build_data=1
  if "%1" == "opencc" set build_opencc=1
  if "%1" == "rime" set build_rime=1
  if "%1" == "librime" set build_rime=1
  if "%1" == "weasel" set build_weasel=1
  if "%1" == "installer" set build_installer=1
  if "%1" == "verify-hybrid-filter" set verify_hybrid_filter=1
  if "%1" == "arm64" (
    echo ARM64 builds are not part of the WubiPinyin MVP.
    exit /b 1
  )
  if "%1" == "clean" set build_clean=1
  if "%1" == "commands" set build_commands=1
  if "%1" == "all" (
    set build_boost=1
    set build_data=1
    set build_opencc=1
    set build_rime=1
    set build_weasel=1
    set build_installer=1
    set build_commands=1
  )
  shift
  goto parse_cmdline_options
:end_parsing_cmdline_options

if %build_weasel% == 0 (
if %build_boost% == 0 (
if %build_data% == 0 (
if %build_opencc% == 0 (
if %build_rime% == 0 (
if %build_commands% == 0 (
  set build_weasel=1
))))))

rem Product binaries must link and package the checked-in librime, including HybridFilter.
if %build_weasel% == 1 set build_rime=1
if %build_installer% == 1 set build_rime=1
if %verify_hybrid_filter% == 1 set VERIFY_HYBRID_FILTER=1
rem
rem quit WubiPinyinServer.exe before building
cd /d %WEASEL_ROOT%
if exist output\WubiPinyinServer.exe (
  output\WubiPinyinServer.exe /q
)

rem build booost
if %build_boost% == 1 (
  call build.bat boost
  if errorlevel 1 exit /b 1
  cd /d %WEASEL_ROOT%
)
if %build_rime% == 1 (
  call build.bat %rime_build_variant% rime
  if errorlevel 1 exit /b 1
  cd /d %WEASEL_ROOT%
)
if %build_data% == 1 (
  call build.bat data
  if errorlevel 1 exit /b 1
  cd /d %WEASEL_ROOT%
)
if %build_opencc% == 1 (
  call build.bat opencc
  if errorlevel 1 exit /b 1
  cd /d %WEASEL_ROOT%
)

if %build_commands% == 1 (
  echo Generating compile_commands.json
  xmake project -k compile_commands -m %build_config%
)

if defined SDKVER set build_sdk_option=--vs_sdkver=%SDKVER% -c
if not defined SDKVER set build_sdk_option=

rem if to clean
if %build_clean% == 1 ( goto clean )
if %build_weasel% == 0 ( goto end )

xmake f -a x64 -m %build_config% %build_sdk_option%
if %build_rebuild% == 1 ( xmake clean )
xmake
if errorlevel 1 goto error
xmake f -a x86 -m %build_config% %build_sdk_option%
if %build_rebuild% == 1 ( xmake clean )
xmake
if errorlevel 1 goto error

if %build_installer% == 1 (
  call :build_installer
  if errorlevel 1 goto error
)
goto end

:build_installer
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%WEASEL_ROOT%\WubiPinyinData\scripts\stage-locked-sources.ps1" ^
    -RepositoryRoot "%WEASEL_ROOT%" ^
    -OutputDirectory "%WEASEL_ROOT%\output\data\WubiPinyinData"
  if errorlevel 1 exit /b 1

  call "%WEASEL_ROOT%\WubiPinyinSettings\build_release.bat"
  if errorlevel 1 exit /b 1
  if not exist "%WEASEL_ROOT%\output\settings\Release\x64\WubiPinyinSettings.exe" (
    echo Error: WubiPinyinSettings.exe was not produced.
    exit /b 1
  )

  set "NSIS_EXE=%ProgramFiles(x86)%\NSIS\makensis.exe"
  if exist "%NSIS_EXE%" goto nsis_found
  set "NSIS_EXE=%ProgramFiles(x86)%\NSIS\Bin\makensis.exe"
  if exist "%NSIS_EXE%" goto nsis_found
  for /f "delims=" %%i in ('where makensis.exe 2^>nul') do set "NSIS_EXE=%%i"
  if exist "%NSIS_EXE%" goto nsis_found
  echo Error: NSIS makensis.exe was not found.
  exit /b 1

:nsis_found
  "%NSIS_EXE%" ^
    -INPUTCHARSET UTF8 ^
    /DWEASEL_VERSION=%WEASEL_VERSION% ^
    /DWEASEL_BUILD=%WEASEL_BUILD% ^
    /DPRODUCT_VERSION=%PRODUCT_VERSION% ^
    output\install.nsi
  exit /b %errorlevel%

:clean
  if exist build (
    rmdir /s /q build
    if errorlevel 1 (
      echo error cleaning weasel build
      goto error
    )
    goto end
  )

:error
  echo error building weasel...
  exit /b 1
  
:end
  cd %WEASEL_ROOT%

