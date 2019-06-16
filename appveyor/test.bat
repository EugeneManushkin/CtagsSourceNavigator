echo on
cd "%BUILD_ROOT%"\tests\%CONFIGURATION%
for %%G in ("%APPVEYOR_BUILD_FOLDER%"\tests\tags\*.zip) do ( cmake -E tar xzvf "%%G" || exit /b 1 )
tags_tests.exe
tags_tests.exe --CheckIdxFiles
tags_cache_tests.exe
tag_view_tests.exe
