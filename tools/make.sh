#!/usr/bin/env sh

make clean && \
make -j$(nproc) && \
make windows -j$(nproc) && \
ls -lh b3dv* && \
echo 'build done'