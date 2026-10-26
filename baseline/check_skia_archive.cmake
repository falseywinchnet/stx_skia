if(NOT DEFINED SKIA_ARCHIVE OR NOT EXISTS "${SKIA_ARCHIVE}")
    message(FATAL_ERROR "Skia archive is absent: ${SKIA_ARCHIVE}")
endif()

execute_process(
    COMMAND nm -gU "${SKIA_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "nm failed for ${SKIA_ARCHIVE}: ${nm_error}")
endif()

# Skia carries no codec at all (Z-stage I5): PNG is stx_png, and Skia's
# libpng and zlib are not built.
set(forbidden_decoder_patterns
    "SkPngDecoder"
    "SkPngCodec"
    "SkPngEncoder"
    "png_create_read_struct"
    "png_create_write_struct"
    "Cr_z_"
    "SkBmpDecoder"
    "SkBmpCodec"
    "SkIcoCodec"
    "SkWbmpCodec"
    "SkJpegDecoder"
    "SkWebpDecoder"
    "SkAvif"
)
foreach(pattern IN LISTS forbidden_decoder_patterns)
    if(symbols MATCHES "${pattern}")
        message(FATAL_ERROR "Forbidden codec or zlib symbol found: ${pattern}")
    endif()
endforeach()

# Undefined compatibility signatures do not establish a backend. Defined
# Ganesh/Graphite device implementations do, and are forbidden.
if(symbols MATCHES "[0-9a-fA-F]+ T __ZN(2Gr|5skgpu8graphite)")
    message(FATAL_ERROR "Defined GPU backend symbol found in CPU-only Skia archive")
endif()

# ADR-004 phase 7: text is GUI.Forms' own HarfBuzz/FreeType engine. Skia's
# font managers, platform typefaces and its bundled FreeType are forbidden.
set(forbidden_font_backend_patterns
    "SkFontMgr_New_"
    "SkFontMgr_Custom"
    "SkTypeface_Mac"
    "SkTypeface_FreeType"
    "SkScalerContext_Mac"
    "SkScalerContext_FreeType"
    "FT_Init_FreeType"
)
foreach(pattern IN LISTS forbidden_font_backend_patterns)
    if(symbols MATCHES "${pattern}")
        message(FATAL_ERROR "Forbidden font backend symbol found: ${pattern}")
    endif()
endforeach()

message(STATUS "Skia archive policy passed: no codec or zlib, no GPU backend implementation, no font backend")

