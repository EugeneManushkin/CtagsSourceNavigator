#!/bin/bash -xe
mkdir $BUILD_DIR && cd $BUILD_DIR
cmake -DCMAKE_CXX_STANDARD=11 $TRAVIS_BUILD_DIR/tests
make
