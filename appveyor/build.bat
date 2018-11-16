echo on
call "%APPVEYOR_BUILD_FOLDER%"\build_script\version.bat || exit /b 1
set BUILD_NUM=%CTAGS_VERSION_MAJOR%.%CTAGS_VERSION_MINOR%.0.%CTAGS_BUILD%

md %BUILD_ROOT%
cd %BUILD_ROOT% || exit /b 1
cmake -DCTAGS_VERSION_MAJOR=%CTAGS_VERSION_MAJOR% -DCTAGS_VERSION_MINOR=%CTAGS_VERSION_MINOR% -DCTAGS_BUILD=%CTAGS_BUILD% -G "%CMAKE_GENERATOR%" "%APPVEYOR_BUILD_FOLDER%" || exit /b 1
msbuild /target:build /property:Configuration=%CONFIGURATION% /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll" %BUILD_ROOT%\Project.sln || exit /b 1

cd %BUILD_ROOT%\%CONFIGURATION%\ctags || exit /b 1
7z a ctags-%BUILD_NUM%-%1.zip * || exit /b 1
move ctags-%BUILD_NUM%-%1.zip "%APPVEYOR_BUILD_FOLDER%" || exit /b 1
