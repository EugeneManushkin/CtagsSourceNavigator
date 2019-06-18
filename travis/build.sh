#!/bin/bash
mkdir $BUILD_DIR && cd $BUILD_DIR
cmake $TRAVIS_BUILD_DIR/tests
make
