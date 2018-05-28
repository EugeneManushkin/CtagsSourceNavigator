:: This is the release build script that creates plugin archives for uploading to PlugRing
::
:: Requirments: cmake, 7z
::
:: Usage: run.bat cmake_generator [repository_path]. If repository_path is not specified the repository will be cloned. Examples:
::  build_script\run.bat "Visual Studio 14 2015" .\
::  <repository_root>\build_script\run.bat "Visual Studio XX XXXX" <repository_root>
::  copy build_script c:\<some_folder> && c:\<some_folder>\build_script\run.bat "Visual Studio XX XXXX"

set GENERATOR=%~1
set REPO_ROOT=%~dpnx2
set ROOT=%~dp0build
set ARCHIVER="%ProgramFiles%\7-Zip\7z.exe"

if "%GENERATOR%"=="" exit /b 1
if NOT EXIST %ARCHIVER% exit /b 1

%~d0
call :CleanRoot
if "%REPO_ROOT%"=="" call :Clone || exit /b 1
call :GetBuildNumber || exit /b 1
call :BuildPlatform x86
call :BuildPlatform x64
exit /b

:CleanRoot
  rmdir /S /Q %ROOT%
  md %ROOT%
exit /b

:Clone
  set REPO_ROOT=%ROOT%\CtagsSourceNavigator
  cd %ROOT%
  git clone https://github.com/EugeneManushkin/CtagsSourceNavigator.git || exit /b 1
  cd CtagsSourceNavigator
  git submodule update --init || exit /b 1
exit /b

:GetBuildNumber
  md %ROOT%\get_build_number && cd %ROOT%\get_build_number || exit /b 1
  cmake -DREPO_ROOT="%REPO_ROOT%\." -G "%GENERATOR%" ..\..\get_build_number || exit /b 1
  cmake --build ./ --config Release || exit /b 1
  for /F %%G in ('Release\getbuild.exe') do set BUILD_NUM=%%G
  if "%BUILD_NUM%"=="" exit /b 1
exit /b

:BuildPlatform
  set BUILD_ROOT=%ROOT%\%BUILD_NUM%\%1
  md %BUILD_ROOT% && cd %BUILD_ROOT% || exit /b 1
  if %1==x64 (
    set CMAKE_GENERATOR="%GENERATOR% Win64"
  ) else (
    set CMAKE_GENERATOR="%GENERATOR%"
  )

  cmake -G %CMAKE_GENERATOR% "%REPO_ROOT%" || exit /b 1
  cmake --build ./ --config Release  || exit /b 1
  cd Release\ctags || exit /b 1
  %ARCHIVER% a ctags-%BUILD_NUM%-%1.zip * || exit /b 1
  move ctags-%BUILD_NUM%-%1.zip %ROOT% || exit /b 1
  call :RunTests %1 "%BUILD_ROOT%"\tests\Release
exit /b

:RunTests
  cd %2
  for %%G in (%REPO_ROOT%\tests\tags\*.zip) do ( cmake -E tar xzvf "%%G" || exit 1 )
  tags_tests.exe > %ROOT%\tags_tests-%1.txt 2>&1
  tags_tests.exe --CheckIdxFiles > %ROOT%\tags_tests_idx-%1.txt 2>&1
exit /b
