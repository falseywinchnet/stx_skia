#!/bin/sh
set -eu
root=$(pwd)
cmake -S build/gui-fixture -B build/gui-consumer-llvm22 -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$root/build/gui-fixture/cmake/llvm22.cmake" \
    -DSTX_SKIA_SOURCE_ROOT="$root" \
    -DGUI_FORMS_BUILD_GALLERY=OFF -DGUI_FORMS_ENABLE_MACOS_HOST=OFF \
    -DGUI_FORMS_BUILD_TESTS=ON -DGUI_FORMS_BUILD_PREPARED_TEXT=ON
cmake --build build/gui-consumer-llvm22 -j "${BUILD_JOBS:-6}" --target \
    gui_forms_skia_smoke gui_drawing_skia_tests gui_drawing_raster_c_api_tests \
    gui_forms_prepared_text_skia_tests gui_forms_damage_clip_tests \
    gui_forms_overlay_shadow_tests gui_forms_paint_group_tests
ctest --test-dir build/gui-consumer-llvm22 --output-on-failure \
    -R '^(gui_forms_skia_smoke|gui_drawing_skia_tests|gui_drawing_raster_c_api_tests|gui_forms_prepared_text_skia_tests|gui_forms_damage_clip_tests|gui_forms_overlay_shadow_tests|gui_forms_paint_group_tests)$'
