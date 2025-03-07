echo on
md %BUILD_ROOT%
cd %BUILD_ROOT% || exit /b 1

cmake^
 -DCTAGS_VERSION_MAJOR=%CTAGS_VERSION_MAJOR%^
 -DCTAGS_VERSION_MINOR=%CTAGS_VERSION_MINOR%^
 -DCTAGS_BUILD=%CTAGS_BUILD%^
 -DCTAGS_UTIL_DIR="%CTAGS_UTIL_DIR%"^
 -DPLUGIN_DIR="%PROD_DIR%"^
 %CMAKE_ARCHITECTURE_ARG%^
 -G "%CMAKE_GENERATOR%"^
 "%~dp0..\.."^
 || exit /b 1

cmake^
 --build .^
 --parallel %NUMBER_OF_PROCESSORS%^
 --verbose^
 --config %CMAKE_CONFIGURATION%^
 || exit /b 1

@echo Done.
