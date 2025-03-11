@cd %~dp0..\..
@cmake -DCTAGS_VERSION_BUILD=%CTAGS_VERSION_BUILD% -P build\version\run.cmake 2>&1
