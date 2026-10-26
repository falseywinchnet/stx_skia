# Original implementations retained for differential tests during conversion.
foreach(unit IN ITEMS SkPixelRef SkMallocPixelRef SkPixelStorage SkIDChangeListener SkMatrix SkDevice
        SkAutoPixmapStorage SkSafeMath SkReadPixelsRec SkWritePixelsRec SkImageInfo SkConvertPixels SkPixmap SkPixmapDraw SkMask SkMaskBlurFilter SkBlurMask SkCachedData SkMaskFilterBase SkMaskFilter SkBlurMaskFilterImpl SkCanvasPriv SkCanvas)
    list(REMOVE_ITEM stx_sources skia/src/core/${unit}.cpp)
    list(APPEND stx_sources baseline/skia/src/core/${unit}.cpp)
endforeach()

list(REMOVE_ITEM stx_sources skia/src/effects/SkShaderMaskFilterImpl.cpp)
list(APPEND stx_sources baseline/skia/src/effects/SkShaderMaskFilterImpl.cpp)

# Removed factory sources remain reference-only at the original pin.
file(GLOB stx_reference_image_filters CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "baseline/skia/src/effects/imagefilters/*.cpp")
list(FILTER stx_sources EXCLUDE REGEX "^skia/src/effects/imagefilters/")
list(APPEND stx_sources ${stx_reference_image_filters})
