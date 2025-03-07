echo on
cd %BUILD_ROOT% || exit /b 1

cmake^
 -DURL=https://github.com/universal-ctags/ctags-win32/releases/download/%CTAGS_UTIL_DATE%/%CTAGS_UTIL_REV%/ctags-%CTAGS_UTIL_DATE%_%CTAGS_UTIL_REV%-%ARCH%.zip^
 -DFILENAME=./ctags.utility.zip^
 -P "%~dp0"\download.cmake^
 || exit /b 1

set TEMP_DIR=%BUILD_ROOT%\temp
md %TEMP_DIR%
md %PROD_DIR%\%CTAGS_UTIL_DIR%
7z x %BUILD_ROOT%\ctags.utility.zip -o%TEMP_DIR% || exit /b 1
move %TEMP_DIR%\ctags.exe %PROD_DIR%\%CTAGS_UTIL_DIR% || exit /b 1
move %TEMP_DIR%\license %PROD_DIR%\%CTAGS_UTIL_DIR% || exit /b 1

cd %PROD_DIR% || exit /b 1
7z a -mx9 ctags-%CTAGS_VERSION%-%ARCH%.zip * || exit /b 1
move ctags-%CTAGS_VERSION%-%ARCH%.zip "%BUILD_ROOT%" || exit /b 1

@echo Done.
