#!/bin/bash -xe
cd $TRAVIS_BUILD_DIR/$BUILD_DIR

for arch in $TRAVIS_BUILD_DIR/tests/tags/*.zip; do
  unzip "$arch"
done

tests/tags_tests
tests/tags_tests --CheckIdxFiles
tests/tag_view_tests
tests/tags_cache_tests
tests/composite_task_tests
tests/process_tests tests/mock_process
