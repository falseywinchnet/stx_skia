# Codec ownership belongs to stx_png/stx_cng, never the raster library.
list(FILTER stx_sources EXCLUDE REGEX "skia/src/(codec|encode|android)/")
list(FILTER stx_sources EXCLUDE REGEX "skia/src/ports/SkImageGeneratorCG\\.cpp$")
list(APPEND stx_sources src/encoded_image.cpp)
# Effects outside the admitted GUI drawing contract. Registration is disabled;
# these factories are neither called by the consumer nor needed by its filters.
list(FILTER stx_sources EXCLUDE REGEX "skia/src/effects/(Sk1DPathEffect|Sk2DPathEffect|SkCornerPathEffect|SkDiscretePathEffect|SkEmbossMask|SkEmbossMaskFilter|SkHighContrastFilter|SkTableMaskFilter|SkTrimPathEffect)\\.cpp$")

# Keep the compiler baseline (128-bit SSE2/NEON), without wider CPU dispatch.
list(FILTER stx_sources EXCLUDE REGEX "skia/src/core/.*_opts_(ssse3|ml3|lasx|avx|erms)\\.cpp$")

# General image effects belong to GUI.Forms. Factory implementations and the
# mask-to-image adapters are removed; core implementations are excluded below.
list(FILTER stx_sources EXCLUDE REGEX "skia/src/effects/imagefilters/")

# CSS/backdrop image evaluation belongs to the first-party GUI.Forms engine.
list(FILTER stx_sources EXCLUDE REGEX "skia/src/core/(SkBlurEngine|SkImageFilter|SkImageFilterCache|SkImageFilterTypes|SkLocalMatrixImageFilter)\\.cpp$")
