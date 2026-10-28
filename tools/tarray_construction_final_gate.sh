#!/bin/sh
set -eu
sh tools/build_native.sh candidate
sh tools/build_native.sh baseline
build/candidate/stx_skia_tarray_construction_tests
build/baseline/stx_skia_tarray_construction_tests
build/candidate/stx_skia_raster_image_tests
build/baseline/stx_skia_raster_image_tests
build/candidate/stx_skia_mipmap_tests build/candidate/mipmap.raw
build/baseline/stx_skia_mipmap_tests build/baseline/mipmap.raw
build/candidate/stx_skia_mipmap_draw_tests build/candidate/mipmap-draw.raw
build/baseline/stx_skia_mipmap_draw_tests build/baseline/mipmap-draw.raw
for artifact in mipmap-draw mipmap shader-blends layers render conversions pixmap mask mask-filter; do
    cmp "build/baseline/$artifact.raw" "build/candidate/$artifact.raw"
done
echo 'Nine rendering/mipmap artifacts identical'
build/candidate/stx_skia_hash_storage_tests
build/baseline/stx_skia_hash_storage_tests
build/candidate/stx_skia_flattenable_stream_tests build/candidate/flattenable.raw
build/baseline/stx_skia_flattenable_stream_tests build/baseline/flattenable.raw
cmp build/candidate/flattenable.raw build/baseline/flattenable.raw
for profile in candidate-sanitize reference-views-debug-sanitize; do
    prior="build/$profile/integrity-before-tarray-construction-final.log"
    test ! -e "$prior"
    if [ -f "build/$profile/integrity.log" ]; then
        mv "build/$profile/integrity.log" "$prior"
    fi
done
sh tools/build_native.sh candidate-sanitize
cmake --build build/reference-views-debug-sanitize -j 6
if env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
    ctest --test-dir build/reference-views-debug-sanitize --output-on-failure \
    > build/reference-views-debug-sanitize/integrity.log 2>&1; then
    echo PASS
else
    echo STOP
    exit 1
fi
root=$(pwd)
test ! -e build/tarray-construction-final-install-original
test ! -e build/tarray-construction-final-install-relocated
cmake --install build/candidate --prefix "$root/build/tarray-construction-final-install-original"
mv build/tarray-construction-final-install-original build/tarray-construction-final-install-relocated
cmake -S tests/installed -B build/tarray-construction-final-consumer -G Ninja \
    -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH="$root/build/tarray-construction-final-install-relocated"
cmake --build build/tarray-construction-final-consumer
build/tarray-construction-final-consumer/consumer

python3 -B tools/check_build_scope.py build/candidate
