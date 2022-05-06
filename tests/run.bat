:: Run functional tests for both i686 and x86_64 platforms
::
:: Usage: run.bat cmake_generator_i686

set GENERATOR=%~1
set REPO_ROOT=%~dp0\..
set ROOT=%~dp0.temp

if "%GENERATOR%"=="" set GENERATOR=Visual Studio 14 2015

%~d0
call :CleanRoot
call :BuildPlatform x86 "%GENERATOR%"
call :BuildPlatform x64 "%GENERATOR% Win64"
exit /b

:CleanRoot
  rmdir /S /Q "%ROOT%"
  md "%ROOT%"
exit /b

:BuildPlatform
  set BUILD_ROOT=%ROOT%\bin\%1
  md "%BUILD_ROOT%" && cd "%BUILD_ROOT%" || exit /b 1
  cmake -G "%~2" "%REPO_ROOT%" || exit /b 1
  cmake --build ./ --config Release -j 6 || exit /b 1
  call :RunTests %1 "%BUILD_ROOT%"\tests\Release
exit /b

:RunTests
  cd %2
  for %%G in ("%REPO_ROOT%"\tests\tags\*.zip) do ( cmake -E tar xzvf "%%G" || exit 1 )
  tags_tests.exe > "%ROOT%"\tags_tests-%1.txt 2>&1
  tags_tests.exe --CheckIdxFiles > "%ROOT%"\tags_tests-%1-idx.txt 2>&1
exit /b
