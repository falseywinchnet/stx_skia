"""Prepare a pinned, isolated GUI.Forms consumer and its optimization patch."""
import argparse
import difflib
from pathlib import Path
import subprocess

REVISION = "8d1f2be06a40c5da78bb75e45cbe1fecec2f6b62"


def read_source(repository, name):
    result = subprocess.check_output(["git", "-C", str(repository), "show", REVISION + ":" + name])
    return result.decode()


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise ValueError("Pinned source fragment did not match exactly once")
    return source.replace(old, new)


def rewrite_raster(source):
    source = replace_once(source, '#include "skia_raster.hpp"', '#include "skia_raster.hpp"\n#include "stx_skia/pixel_storage.hpp"')
    start = source.index("SkGradientData to_sk_gradient(")
    end = source.index("\nSkTileMode to_sk_tile_mode", start)
    source = source[:start] + '''// Shader construction copies these values before the next draw reuses storage.
void fill_sk_gradient(std::span<const GradientStop> stops, SkGradientData& result) {
    result.colors.resize(stops.size());
    result.positions.resize(stops.size());
    for (std::size_t i = 0; i < stops.size(); ++i) {
        result.colors[i] = SkColor4f::FromColor(to_sk_color(stops[i].color));
        result.positions[i] = static_cast<float>(stops[i].offset);
    }
}
''' + source[end:]
    source = replace_once(source, "    ImageMap images;", "    ImageMap images;\n    SkGradientData gradient_scratch;")
    old = "    const SkGradientData data = to_sk_gradient(stops);"
    if source.count(old) != 2:
        raise ValueError("Expected two gradient preparations")
    source = source.replace(old, "    SkGradientData& data = (*impl_).gradient_scratch;\n    fill_sk_gradient(stops, data);")
    source = replace_once(source, "pixel_data = SkData::MakeWithCopy(pixels.data(), pixels.size());", "pixel_data = stx_skia::take_pixel_data(pixels);")
    return source


def rewrite_executor(source):
    source = replace_once(source, '#include "skia_executor.hpp"', '#include "skia_executor.hpp"\n#include "stx_skia/pixel_storage.hpp"')
    source = replace_once(source, "const std::vector<std::byte> pixels = remapped_pixels(", "std::vector<std::byte> pixels = remapped_pixels(")
    source = replace_once(source, "SkData::MakeWithCopy(\n                    pixels.data(), pixels.size())", "stx_skia::take_pixel_data(pixels)")
    # Brush images borrow a snapshot; they cannot transfer that snapshot's owner.
    return source


def rewrite_build(source):
    start = source.index("    set(GUI_FORMS_SKIA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/skia)")
    end = source.index("    add_library(gui_forms_skia STATIC", start)
    source = source[:start] + '''    if(NOT DEFINED STX_SKIA_SOURCE_ROOT)
        message(FATAL_ERROR "This verification fixture requires STX_SKIA_SOURCE_ROOT")
    endif()
    add_subdirectory("${STX_SKIA_SOURCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/stx-skia")
    set(GUI_FORMS_SKIA_ROOT "${STX_SKIA_SOURCE_ROOT}/skia")
    add_custom_target(gui_forms_skia_dependency)
    add_library(gui_forms_skia_archive INTERFACE)
    target_link_libraries(gui_forms_skia_archive INTERFACE stx_skia::stx_skia)
    add_library(gui_forms_skcms_archive INTERFACE)
''' + source[end:]
    return source


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gui", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError("Refusing an existing fixture directory")
    args.output.mkdir(parents=True)
    archive = subprocess.Popen(["git", "-C", str(args.gui), "archive", REVISION], stdout=subprocess.PIPE)
    try:
        subprocess.run(["tar", "-x", "-C", str(args.output)], stdin=archive.stdout, check=True)
    finally:
        archive.stdout.close()
    if archive.wait() != 0:
        raise RuntimeError("Source archive failed")
    patch = []
    entries = [("src/render/skia/raster/skia_raster.cpp", rewrite_raster),
               ("src/render/skia/executor/skia_executor.cpp", rewrite_executor),
               ("CMakeLists.txt", rewrite_build)]
    for name, rewrite in entries:
        original = read_source(args.gui, name)
        modified = rewrite(original)
        (args.output / name).write_text(modified)
        patch.extend(difflib.unified_diff(original.splitlines(True), modified.splitlines(True),
                     fromfile="a/" + name, tofile="b/" + name))
    Path("integration/gui_forms_8d1f2be0.patch").write_text("".join(patch))
    print("Prepared isolated consumer at", args.output)


if __name__ == "__main__":
    main()
