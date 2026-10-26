# Original implementations retained for differential tests during conversion.
foreach(unit IN ITEMS SkPixelRef SkMallocPixelRef SkPixelStorage SkIDChangeListener SkMatrix SkDevice
        SkAutoPixmapStorage SkSafeMath SkReadPixelsRec SkWritePixelsRec SkImageInfo SkConvertPixels SkPixmap SkPixmapDraw SkMask SkMaskBlurFilter SkBlurMask SkCachedData SkMaskFilterBase SkMaskFilter SkBlurMaskFilterImpl SkCanvasPriv)
    list(REMOVE_ITEM stx_sources skia/src/core/${unit}.cpp)
    list(APPEND stx_sources baseline/skia/src/core/${unit}.cpp)
endforeach()

list(REMOVE_ITEM stx_sources skia/src/effects/SkShaderMaskFilterImpl.cpp)
list(APPEND stx_sources baseline/skia/src/effects/SkShaderMaskFilterImpl.cpp)

list(REMOVE_ITEM stx_sources skia/src/effects/imagefilters/SkLightingImageFilter.cpp)
list(APPEND stx_sources baseline/skia/src/effects/imagefilters/SkLightingImageFilter.cpp)
