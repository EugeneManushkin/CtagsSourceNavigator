echo on
md %BUILD_ROOT%
cd %BUILD_ROOT% || exit /b 1
cmake -DCTAGS_VERSION_MAJOR=%CTAGS_VERSION_MAJOR% -DCTAGS_VERSION_MINOR=%CTAGS_VERSION_MINOR% -DCTAGS_BUILD=%CTAGS_BUILD% -G "%CMAKE_GENERATOR%" "%APPVEYOR_BUILD_FOLDER%" || exit /b 1
msbuild /target:build /property:Configuration=%CONFIGURATION% /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll" %BUILD_ROOT%\Project.sln || exit /b 1

cd %BUILD_ROOT%\%CONFIGURATION%\ctags || exit /b 1
set BUILD_NUM=%CTAGS_VERSION_MAJOR%.%CTAGS_VERSION_MINOR%.0.%CTAGS_BUILD%
7z a ctags-%BUILD_NUM%-%ARCH%.zip * || exit /b 1
move ctags-%BUILD_NUM%-%ARCH%.zip "%APPVEYOR_BUILD_FOLDER%" || exit /b 1
