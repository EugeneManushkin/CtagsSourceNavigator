echo on
set EXPECTED_TESTS=11
set ACTUAL_TESTS=0
cd "%BUILD_ROOT%"\tests\%CMAKE_CONFIGURATION%
for %%G in ("%~dp0..\.."\tests\tags\*.zip) do ( cmake -E tar xzvf "%%G" || exit /b 1 )

cd "%BUILD_ROOT%"\tests
for /R %%G in (*_tests.exe) do call :RunTest "%%~G" || exit /b 1
cd "%BUILD_ROOT%"\tests
for /R %%G in (*tags_tests.exe) do call :RunTest "%%~G" --CheckIdxFiles || exit /b 1
if %ACTUAL_TESTS% LSS %EXPECTED_TESTS% @echo "Fewer tests than expected." && exit /b 1

@echo Done. %ACTUAL_TESTS% tests run.
exit /b 0

:RunTest
  cd "%~dp1" || exit /b 1
  "%~1" %2 %3 %4 %5 %6 %7 %8 %9 || exit /b 1
  set /a ACTUAL_TESTS=%ACTUAL_TESTS%+1
exit /b 0
