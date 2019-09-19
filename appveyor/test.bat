echo on
cd "%BUILD_ROOT%"\tests\%CONFIGURATION%
for %%G in ("%APPVEYOR_BUILD_FOLDER%"\tests\tags\*.zip) do ( cmake -E tar xzvf "%%G" || exit /b 1 )
tags_tests.exe || exit /b 1
tags_tests.exe --CheckIdxFiles || exit /b 1
tags_cache_tests.exe || exit /b 1
tag_view_tests.exe || exit /b 1
composite_task_tests.exe || exit /b 1
process_tests.exe mock_process.exe --gtest_filter=-Process.PassArgumentInSingleQuotes || exit /b 1
execute_process_task.exe || exit /b 1
