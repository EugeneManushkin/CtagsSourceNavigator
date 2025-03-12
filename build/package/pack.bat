echo on
cd %PROD_DIR% || exit /b 1

7z a -mx9 ctags-%CTAGS_VERSION%-%ARCH%.zip * || exit /b 1
move ctags-%CTAGS_VERSION%-%ARCH%.zip "%BUILD_ROOT%" || exit /b 1

@echo Packed files in %PROD_DIR%.
