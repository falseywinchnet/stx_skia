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
    "build/$profile/stx_skia_gradient_geometry_tests" "build/$profile/gradient-geometry.raw"
    "build/$profile/stx_skia_image_subset_tests" "build/$profile/image-subsets.raw"
    "build/$profile/stx_skia_image_sampling_tests" "build/$profile/image-sampling.raw" "build/$profile/image-sampling-corrected.raw"
    "build/$profile/stx_skia_shader_coordinate_tests" "build/$profile/shader-coordinates.raw"
    "build/$profile/stx_skia_shader_working_space_tests" "build/$profile/shader-working-space.raw"
    "build/$profile/stx_skia_color_plan_tests" "build/$profile/color-plans.raw"
    "build/$profile/stx_skia_color_composition_tests" "build/$profile/color-composition.raw"
    "build/$profile/stx_skia_color_adjustment_tests" "build/$profile/color-adjustments.raw"
    "build/$profile/stx_skia_shader_blend_tests" "build/$profile/shader-blends.raw"
    "build/$profile/stx_skia_layer_tests" "build/$profile/layers.raw"
    "build/$profile/stx_skia_render_tests" "build/$profile/render.raw"
    "build/$profile/stx_skia_pixel_conversion_tests" "build/$profile/conversions.raw"
    "build/$profile/stx_skia_pixmap_tests" "build/$profile/pixmap.raw"
    "build/$profile/stx_skia_mask_tests" "build/$profile/mask.raw"
    "build/$profile/stx_skia_mask_filter_tests" "build/$profile/mask-filter.raw"
    "build/$profile/stx_skia_flattenable_stream_tests" "build/$profile/flattenable.raw"
fi
