#!/usr/bin/env sh

clang-format -i src/*.c
clang-format -i src/*.h

clang-tidy -fix src/*.c
clang-tidy -fix src/*.h

clang-format -i src/*.c
clang-format -i src/*.h

clang-tidy -fix src/*.c
clang-tidy -fix src/*.h