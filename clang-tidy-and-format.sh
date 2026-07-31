#!/usr/bin/env sh

clang-format -i src/*.c

clang-tidy -fix src/*.c

