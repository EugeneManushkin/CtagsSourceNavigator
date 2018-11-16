echo on
cd "%BUILD_ROOT%"\tests\%CONFIGURATION%
tags_tests.exe
tags_tests.exe --CheckIdxFiles
