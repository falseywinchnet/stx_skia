#!/bin/sh
set -eu
profile=${1:-candidate}
case "$profile" in
    baseline) baseline=ON; sanitize=OFF ;;
    candidate) baseline=OFF; sanitize=OFF ;;
    candidate-sanitize) baseline=OFF; sanitize=ON ;;
    *) echo 'Expected baseline, candidate, or candidate-sanitize' >&2; exit 2 ;;
esac
cmake -S . -B "build/$profile" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="${CC:-clang}" -DCMAKE_CXX_COMPILER="${CXX:-clang++}" \
    -DSTX_SKIA_BASELINE="$baseline" -DSTX_SKIA_SANITIZE="$sanitize"
cmake --build "build/$profile" -j "${BUILD_JOBS:-6}"
if [ "$sanitize" = ON ]; then
    # Preserve diagnostics for the owner's independent reviewer; only report status.
    if env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
        ctest --test-dir "build/$profile" --output-on-failure > "build/$profile/integrity.log" 2>&1; then
        echo PASS
    else
        echo STOP
        exit 1
    fi
else
    ctest --test-dir "build/$profile" --output-on-failure
    "build/$profile/stx_skia_render_tests" "build/$profile/render.raw"
    "build/$profile/stx_skia_pixel_conversion_tests" "build/$profile/conversions.raw"
fi
