echo on
cd "%BUILD_ROOT%"\tests\%CMAKE_CONFIGURATION%
for %%G in ("%~dp0..\.."\tests\tags\*.zip) do ( cmake -E tar xzvf "%%G" || exit /b 1 )
tags_tests.exe || exit /b 1
tags_tests.exe --CheckIdxFiles || exit /b 1
tags_cache_tests.exe || exit /b 1
tags_name_path_line_tests.exe || exit /b 1
tag_view_tests.exe || exit /b 1
tags_repository_storage_tests.exe || exit /b 1
safe_call_tests.exe || exit /b 1

cd "%BUILD_ROOT%"\tests\facade\far3\%CMAKE_CONFIGURATION%
break_keys.exe || exit /b 1

@echo Done.
