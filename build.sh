#!/bin/bash

source ~/emsdk/emsdk_env.sh
export platform=emscripten
export DEBUG=1
find | grep '\.o$' | xargs rm
# make clean
emmake make -f Makefile -j3
cp parallel_n64_libretro_emscripten.bc ~/RetroArch/dist-scripts/parallel_n64_libretro_emscripten.bc
