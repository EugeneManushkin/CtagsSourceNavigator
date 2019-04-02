echo on
md %BUILD_ROOT%
cd %BUILD_ROOT% || exit /b 1
cmake -DCTAGS_VERSION_MAJOR=%CTAGS_VERSION_MAJOR% -DCTAGS_VERSION_MINOR=%CTAGS_VERSION_MINOR% -DCTAGS_BUILD=%CTAGS_BUILD% -DCTAGS_UTIL_DIR=%CTAGS_UTIL_DIR% -G "%CMAKE_GENERATOR%" "%APPVEYOR_BUILD_FOLDER%" || exit /b 1
msbuild /target:build /property:Configuration=%CONFIGURATION% /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll" %BUILD_ROOT%\Project.sln || exit /b 1

appveyor DownloadFile https://github.com/universal-ctags/ctags-win32/releases/download/%CTAGS_UTIL_DATE%/%CTAGS_UTIL_REV%/ctags-%CTAGS_UTIL_DATE%_%CTAGS_UTIL_REV%-%ARCH%.zip -FileName ctags.utility.zip
cd %BUILD_ROOT%\%CONFIGURATION%\ctags || exit /b 1
md %CTAGS_UTIL_DIR% || exit /b 1
7z x %BUILD_ROOT%\ctags.utility.zip -o%CTAGS_UTIL_DIR% || exit /b 1
7z a ctags-%CTAGS_VERSION%-%ARCH%.zip * || exit /b 1
move ctags-%CTAGS_VERSION%-%ARCH%.zip "%APPVEYOR_BUILD_FOLDER%" || exit /b 1
