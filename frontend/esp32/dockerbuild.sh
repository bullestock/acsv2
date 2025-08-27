#!/bin/bash
docker run --rm -v $PWD:/project -w /project -u $UID -e HOME=/tmp -e GIT_TEST_DEBUG_UNSAFE_DIRECTORIES=true espressif/idf:release-v5.4 idf.py build
