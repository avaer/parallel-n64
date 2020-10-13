#!/bin/bash
# export DEBUG=1
emmake make -f Makefile platform=emscripten
cp parallel_n64_libretro_emscripten.bc ../RetroArch/dist-scripts/
pushd ../RetroArch/dist-scripts/
emmake ./dist-cores.sh emscripten
popd
cp ../RetroArch/pkg/emscripten/parallel_n64_libretro.* ../download/RetroArch/
sed -i 's/ _glScissor(x0,x1,x2,x3){/ _glScissor(x0,x1,x2,x3){x0=0;x1=0;x2=800;x3=600;/g' ../download/RetroArch/parallel_n64_libretro.js
js-beautify -r ../download/RetroArch/parallel_n64_libretro.js