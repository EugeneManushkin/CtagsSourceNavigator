#!/bin/bash -x
cd $TRAVIS_BUILD_DIR/$BUILD_DIR

for arch in $TRAVIS_BUILD_DIR/tests/tags/*.zip; do
  unzip "$arch"
done

./tags_tests
./tags_tests --CheckIdxFiles
./tag_view_tests
./tags_cache_tests
