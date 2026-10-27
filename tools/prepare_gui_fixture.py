"""Prepare a pinned, isolated GUI.Forms consumer and its integration patches.

The first patch (gui_forms_8d1f2be0.patch) is the renderer optimization. The second
(gui_forms_8d1f2be0_swizzle.patch) applies on top of it and replaces GUI.Forms' four RGBA/BGRA
byte loops with stx_skia's standalone red/blue swap kernel. --patches-only rewrites both patch
files without preparing a fixture.
"""
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


SWIZZLE_INCLUDE = '#include "stx_skia/swizzle.hpp"'


def rewrite_raster_canvas(source):
    # One reused, tightly packed buffer instead of a new vector per tile and per patch. Image
    # loads and patches copy the rows they are given, so the buffer may be refilled at once.
    source = replace_once(source, '#include "gui_forms/window.hpp"\n',
                          '#include "gui_forms/window.hpp"\n' + SWIZZLE_INCLUDE + '\n')
    start = source.index("[[nodiscard]] std::vector<std::byte> rgba_to_bgra(")
    end = source.index("\n} // namespace", start)
    source = source[:start] + '''// Fills result with width x height pixels of source rows, source_row_bytes apart, as
// tightly packed BGRA rows, reusing result's storage. False if the rows do not fit source.
[[nodiscard]] bool rgba_to_bgra(
    std::span<const std::byte> source, std::size_t source_row_bytes,
    std::uint32_t width, std::uint32_t height, std::vector<std::byte>& result) {
    const std::size_t tight_row = static_cast<std::size_t>(width) * 4U;
    result.resize(tight_row * static_cast<std::size_t>(height));
    const stx_skia::SwizzleResult swapped = stx_skia::swap_red_blue_rows(
        result, stx_skia::RowBytes(tight_row), source, stx_skia::RowBytes(source_row_bytes),
        stx_skia::PixelCount(width), stx_skia::RowCount(height));
    return swapped == stx_skia::SwizzleResult::ok;
}
''' + source[end:]
    source = replace_once(source, """    tiles_.reserve(count);
    try {""", """    tiles_.reserve(count);
    std::vector<std::byte> converted;
    try {""")
    source = replace_once(source, """                std::span<const std::byte> pixels(snapshot.pixels().data() + offset, bytes);
                std::vector<std::byte> converted;
                std::uint64_t stride = snapshot.row_bytes();
                if (snapshot.pixel_format == gui_drawing::PixelFormat::rgba32_premultiplied) {
                    converted = rgba_to_bgra(pixels, snapshot.row_bytes(),
                        static_cast<std::uint32_t>(storage.width), static_cast<std::uint32_t>(storage.height));
                } else {""", """                std::span<const std::byte> pixels(snapshot.pixels().data() + offset, bytes);
                std::uint64_t stride = snapshot.row_bytes();
                if (snapshot.pixel_format == gui_drawing::PixelFormat::rgba32_premultiplied) {
                    if (!rgba_to_bgra(pixels, static_cast<std::size_t>(snapshot.row_bytes()),
                            static_cast<std::uint32_t>(storage.width),
                            static_cast<std::uint32_t>(storage.height), converted)) {
                        last_resource_error_ = ImageResourceError::invalid_image_data;
                        release_tiles(*owner);
                        return false;
                    }
                } else {""")
    source = replace_once(source, """    const gui_drawing::ImageSnapshot snapshot = (*bitmap_).snapshot();
    for (std::size_t change = 0; change < changes.rectangles.size(); ++change) {""",
                          """    const gui_drawing::ImageSnapshot snapshot = (*bitmap_).snapshot();
    std::vector<std::byte> converted;
    for (std::size_t change = 0; change < changes.rectangles.size(); ++change) {""")
    source = replace_once(source, """            std::span<const std::byte> patch(snapshot.pixels().data() + source_offset, required);
            std::vector<std::byte> converted;
            std::uint64_t source_row_bytes = snapshot.row_bytes();
            if (snapshot.pixel_format == gui_drawing::PixelFormat::rgba32_premultiplied) {
                converted = rgba_to_bgra(patch, snapshot.row_bytes(),
                    static_cast<std::uint32_t>(rect.width), static_cast<std::uint32_t>(rect.height));
                patch = converted;""", """            std::span<const std::byte> patch(snapshot.pixels().data() + source_offset, required);
            std::uint64_t source_row_bytes = snapshot.row_bytes();
            if (snapshot.pixel_format == gui_drawing::PixelFormat::rgba32_premultiplied) {
                if (!rgba_to_bgra(patch, static_cast<std::size_t>(snapshot.row_bytes()),
                        static_cast<std::uint32_t>(rect.width),
                        static_cast<std::uint32_t>(rect.height), converted)) {
                    last_resource_error_ = ImageResourceError::invalid_image_data;
                    return false;
                }
                patch = converted;""")
    return source


def rewrite_coregraphics(source):
    source = replace_once(source, '#include "../../../core/image/resource_pixels.hpp"\n',
                          '#include "../../../core/image/resource_pixels.hpp"\n' + SWIZZLE_INCLUDE + '\n')
    source = replace_once(source, """                rgba.resize((*resource).encoded.size());
                for (std::size_t offset = 0; offset < rgba.size(); offset += 4U) {
                    rgba[offset] = (*resource).encoded[offset + 2U];
                    rgba[offset + 1U] = (*resource).encoded[offset + 1U];
                    rgba[offset + 2U] = (*resource).encoded[offset];
                    rgba[offset + 3U] = (*resource).encoded[offset + 3U];
                }
                have_rgba = true;""", """                rgba.resize((*resource).encoded.size());
                have_rgba = stx_skia::swap_red_blue(rgba, (*resource).encoded) ==
                            stx_skia::SwizzleResult::ok;""")
    return source


def rewrite_windows_drawing(source):
    source = replace_once(source, '#include "gui_forms/c_api.h"\n',
                          '#include "gui_forms/c_api.h"\n' + SWIZZLE_INCLUDE + '\n')
    # One pass instead of a copy followed by an in-place swap.
    source = replace_once(source, """void copy_as_bgra(const ImageSnapshot& snapshot, void* destination) {
    const std::span<const std::byte> source = snapshot.pixels();
    std::byte* output = static_cast<std::byte*>(destination);
    std::copy(source.begin(), source.end(), output);
    if (snapshot.pixel_format == PixelFormat::rgba32_premultiplied) {
        for (std::size_t offset = 0; offset < source.size(); offset += 4U) {
            std::swap(output[offset], output[offset + 2U]);
        }
    }
}""", """// Writes the snapshot's pixels as BGRA to destination, which holds at least as many bytes and
// does not overlap them. False only if the swap rejects the buffers; nothing is then written.
[[nodiscard]] bool copy_as_bgra(const ImageSnapshot& snapshot, void* destination) {
    const std::span<const std::byte> source = snapshot.pixels();
    const std::span<std::byte> output(static_cast<std::byte*>(destination), source.size());
    if (snapshot.pixel_format != PixelFormat::rgba32_premultiplied) {
        std::copy(source.begin(), source.end(), output.begin());
        return true;
    }
    const stx_skia::SwizzleResult swapped = stx_skia::swap_red_blue(output, source);
    return swapped == stx_skia::SwizzleResult::ok;
}""")
    source = replace_once(source, """        converted.resize(snapshot.pixels().size());
        copy_as_bgra(snapshot, converted.data());
        pixels = converted.data();""", """        converted.resize(snapshot.pixels().size());
        if (!copy_as_bgra(snapshot, converted.data())) {
            throw std::runtime_error("surface pixel conversion failed");
        }
        pixels = converted.data();""")
    source = replace_once(source, """    copy_as_bgra(snapshot, pixels);
    std::scoped_lock lock(lease_mutex);""", """    if (!copy_as_bgra(snapshot, pixels)) {
        SelectObject(device, previous);
        DeleteDC(device);
        DeleteObject(dib);
        throw std::runtime_error("bitmap-backed HDC pixel conversion failed");
    }
    std::scoped_lock lock(lease_mutex);""")
    return source


def rewrite_windows_host(source):
    source = replace_once(source, '#include "gui_forms/live_surface.hpp"\n',
                          '#include "gui_forms/live_surface.hpp"\n' + SWIZZLE_INCLUDE + '\n')
    source = replace_once(source, """            live_bgra_scratch_.resize(frame.pixels().size());
            const std::size_t byte_count = live_bgra_scratch_.size();
            for (std::size_t index = 0U; index < byte_count; index += 4U) {
                live_bgra_scratch_[index] = source[index + 2U];
                live_bgra_scratch_[index + 1U] = source[index + 1U];
                live_bgra_scratch_[index + 2U] = source[index];
                live_bgra_scratch_[index + 3U] = source[index + 3U];
            }
            source = live_bgra_scratch_.data();""", """            live_bgra_scratch_.resize(frame.pixels().size());
            const stx_skia::SwizzleResult swapped =
                stx_skia::swap_red_blue(live_bgra_scratch_, frame.pixels());
            if (swapped != stx_skia::SwizzleResult::ok) return;
            source = live_bgra_scratch_.data();""")
    return source


def rewrite_canvas_tests(source):
    # The RGBA path must produce the same tile resources as the BGRA path, through publication
    # and through edits crossing tile gutters.
    source = replace_once(source, "#include <cstdlib>\n",
                          "#include <array>\n#include <cstddef>\n#include <cstdint>\n#include <cstdlib>\n")
    source = replace_once(source, "#include <stdexcept>\n#include <string_view>\n",
                          "#include <optional>\n#include <stdexcept>\n#include <string_view>\n#include <vector>\n")
    source = replace_once(source, "void tiled_updates_include_sampling_gutters_and_recover_resources() {",
                          '''// Every tile resource of an RGBA canvas equals the BGRA canvas's for the same picture,
// after publication and after strided edits that cross tile gutters.
bool same_tile_resources(Window& first, Window& second) {
    const std::vector<ImageId> first_ids = first.image_resources().image_ids();
    const std::vector<ImageId> second_ids = second.image_resources().image_ids();
    if (first_ids.size() != second_ids.size()) return false;
    for (std::size_t index = 0; index < first_ids.size(); ++index) {
        const std::optional<ImageResourceView> a = first.image_resources().find(first_ids[index]);
        const std::optional<ImageResourceView> b = second.image_resources().find(second_ids[index]);
        if (!a || !b) return false;
        if ((*a).metadata.width != (*b).metadata.width ||
            (*a).metadata.height != (*b).metadata.height ||
            (*a).encoded.size() != (*b).encoded.size()) return false;
        for (std::size_t byte = 0; byte < (*a).encoded.size(); ++byte) {
            if ((*a).encoded[byte] != (*b).encoded[byte]) return false;
        }
    }
    return true;
}

void edit_both(gui_drawing::Bitmap& rgba, gui_drawing::Bitmap& bgra, gui_drawing::RectI area,
               std::uint32_t seed) {
    gui_drawing::BitmapEditView rgba_edit = rgba.begin_edit(area);
    gui_drawing::BitmapEditView bgra_edit = bgra.begin_edit(area);
    std::uint32_t state = seed;
    for (std::int32_t row = 0; row < area.height; ++row) {
        for (std::int32_t column = 0; column < area.width; ++column) {
            state = state * 1664525U + 1013904223U;
            const std::byte red{static_cast<unsigned char>(state >> 24)};
            const std::byte green{static_cast<unsigned char>(state >> 16)};
            const std::byte blue{static_cast<unsigned char>(state >> 8)};
            const std::size_t rgba_at = static_cast<std::size_t>(row) * rgba_edit.row_bytes +
                static_cast<std::size_t>(column) * 4U;
            const std::size_t bgra_at = static_cast<std::size_t>(row) * bgra_edit.row_bytes +
                static_cast<std::size_t>(column) * 4U;
            rgba_edit.writable_data[rgba_at] = red;
            rgba_edit.writable_data[rgba_at + 1U] = green;
            rgba_edit.writable_data[rgba_at + 2U] = blue;
            rgba_edit.writable_data[rgba_at + 3U] = std::byte{255};
            bgra_edit.writable_data[bgra_at] = blue;
            bgra_edit.writable_data[bgra_at + 1U] = green;
            bgra_edit.writable_data[bgra_at + 2U] = red;
            bgra_edit.writable_data[bgra_at + 3U] = std::byte{255};
        }
    }
    CHECK(rgba.commit_edit(rgba_edit.token) != 0U);
    CHECK(bgra.commit_edit(bgra_edit.token) != 0U);
}

void rgba_tiles_match_bgra_tiles() {
    constexpr std::uint32_t width = 1100;
    constexpr std::uint32_t height = 600;
    const std::shared_ptr<gui_drawing::Bitmap> rgba = std::make_shared<gui_drawing::Bitmap>(
        width, height, gui_drawing::PixelFormat::rgba32_premultiplied);
    const std::shared_ptr<gui_drawing::Bitmap> bgra =
        std::make_shared<gui_drawing::Bitmap>(width, height);
    std::uint32_t state = 0x9e3779b9U;
    for (std::uint32_t y = 0; y < height; y += 7) {
        for (std::uint32_t x = 0; x < width; x += 5) {
            state = state * 1664525U + 1013904223U;
            const gui_drawing::Color color = gui_drawing::Color::from_argb(state | 0xff000000U);
            (*rgba).set_pixel(x, y, color);
            (*bgra).set_pixel(x, y, color);
        }
    }
    const std::shared_ptr<RasterCanvas> rgba_canvas = make_control<RasterCanvas>(StableId("rgba.tiles"));
    const std::shared_ptr<RasterCanvas> bgra_canvas = make_control<RasterCanvas>(StableId("bgra.tiles"));
    (*rgba_canvas).set_bitmap(rgba);
    (*bgra_canvas).set_bitmap(bgra);
    Window rgba_window(rgba_canvas, {200, 200});
    Window bgra_window(bgra_canvas, {200, 200});
    rgba_window.perform_layout();
    bgra_window.perform_layout();
    CHECK(rgba_window.image_resources().image_ids().size() == 6U);
    CHECK(same_tile_resources(rgba_window, bgra_window));

    // Across the vertical and horizontal tile boundaries, one column, one row, and a
    // bottom-right edge tile.
    const std::array<gui_drawing::RectI, 4> edits{gui_drawing::RectI{500, 300, 30, 230},
                                                  gui_drawing::RectI{511, 0, 1, 600},
                                                  gui_drawing::RectI{0, 512, 1100, 1},
                                                  gui_drawing::RectI{1030, 590, 70, 10}};
    std::uint32_t seed = 7U;
    for (const gui_drawing::RectI area : edits) {
        edit_both(*rgba, *bgra, area, seed);
        seed += 101U;
        CHECK((*rgba_canvas).synchronize_bitmap());
        CHECK((*bgra_canvas).synchronize_bitmap());
        CHECK(same_tile_resources(rgba_window, bgra_window));
    }
}

void tiled_updates_include_sampling_gutters_and_recover_resources() {''')
    source = replace_once(source, """    canvas_converts_rgba_storage_without_changing_document_truth();
    tiled_updates_include_sampling_gutters_and_recover_resources();""",
                          """    canvas_converts_rgba_storage_without_changing_document_truth();
    rgba_tiles_match_bgra_tiles();
    tiled_updates_include_sampling_gutters_and_recover_resources();""")
    return source


def rewrite_windows_probe(source):
    # The probe used only BGRA bitmaps, so it never reached the RGBA swap in copy_as_bgra.
    source = replace_once(source, """    CHECK(api.bitmap_get_pixel(presented, 2, 1, &pixel) == GD_OK);
    CHECK(pixel.argb == UINT32_C(0xff0000ff));
""", """    CHECK(api.bitmap_get_pixel(presented, 2, 1, &pixel) == GD_OK);
    CHECK(pixel.argb == UINT32_C(0xff0000ff));

    /* An RGBA bitmap reaches GDI through the red/blue swap, both through a leased HDC and
       when presented into the memory DC. */
    {
        gd_handle rgba = {0};
        uintptr_t rgba_hdc = 0;
        uint64_t rgba_token = 0;
        CHECK(api.bitmap_create(3, 2, GD_PIXEL_RGBA32_PREMULTIPLIED, &rgba) == GD_OK);
        CHECK(api.bitmap_set_pixel(rgba, 0, 0, (gd_color){UINT32_C(0xffff0000), 0}) == GD_OK);
        CHECK(api.bitmap_set_pixel(rgba, 1, 0, (gd_color){UINT32_C(0xff00ff00), 0}) == GD_OK);
        CHECK(api.bitmap_set_pixel(rgba, 2, 1, (gd_color){UINT32_C(0xff0000ff), 0}) == GD_OK);
        CHECK(api.bitmap_acquire_hdc(rgba, &rgba_hdc, &rgba_token) == GD_OK);
        CHECK(GetPixel((HDC)rgba_hdc, 0, 0) == RGB(255, 0, 0));
        CHECK(GetPixel((HDC)rgba_hdc, 1, 0) == RGB(0, 255, 0));
        CHECK(GetPixel((HDC)rgba_hdc, 2, 1) == RGB(0, 0, 255));
        CHECK(api.bitmap_release_hdc(rgba, rgba_token) == GD_OK);
        /* Reported, not checked: release_hdc copies the BGRA lease back without swapping. */
        CHECK(api.bitmap_get_pixel(rgba, 0, 0, &pixel) == GD_OK);
        printf("drawing-windows-rgba-lease-roundtrip=%08x\\n", (unsigned)pixel.argb);
        CHECK(api.bitmap_set_pixel(rgba, 0, 0, (gd_color){UINT32_C(0xffff0000), 0}) == GD_OK);
        CHECK(api.bitmap_set_pixel(rgba, 2, 1, (gd_color){UINT32_C(0xff0000ff), 0}) == GD_OK);
        CHECK(api.native_surface_present((uintptr_t)memory, GD_NATIVE_SURFACE_HDC, rgba) == GD_OK);
        CHECK(GetPixel(memory, 0, 0) == RGB(255, 0, 0));
        CHECK(GetPixel(memory, 2, 1) == RGB(0, 0, 255));
        CHECK(api.release(rgba) == GD_OK);
        puts("drawing-windows-rgba=pass");
    }
""")
    return source


def rewrite_swizzle_links(source):
    if not source.endswith("\n"):
        raise ValueError("CMakeLists.txt must end with a newline")
    return source + """
# stx_skia fixture: hosts swap RGBA and BGRA rows with stx_skia's standalone kernel, which
# links no Skia code. The Windows host builds without the Skia adapter, so stx_skia is added
# here when the adapter did not add it; only the targets linked below are then built.
if(NOT TARGET stx_skia::swizzle)
    if(NOT DEFINED STX_SKIA_SOURCE_ROOT)
        message(FATAL_ERROR "The swap kernel requires STX_SKIA_SOURCE_ROOT")
    endif()
    add_subdirectory("${STX_SKIA_SOURCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/stx-skia" EXCLUDE_FROM_ALL)
endif()
# These targets compile the four converted loops.
foreach(gui_forms_swizzle_target IN ITEMS gui_forms_controls gui_forms_coregraphics
        gui_drawing_c_api gui_forms_host_windows gui_forms_windows_dib_lifecycle_tests
        gui_forms_windows_tooltip_tests gui_forms_windows_prepared_text_tests)
    if(TARGET ${gui_forms_swizzle_target})
        target_link_libraries(${gui_forms_swizzle_target} PRIVATE stx_skia::swizzle)
    endif()
endforeach()
"""


OPTIMIZATION_PATCH = Path("integration/gui_forms_8d1f2be0.patch")
SWIZZLE_PATCH = Path("integration/gui_forms_8d1f2be0_swizzle.patch")
OPTIMIZATION = [("src/render/skia/raster/skia_raster.cpp", rewrite_raster),
                ("src/render/skia/executor/skia_executor.cpp", rewrite_executor),
                ("CMakeLists.txt", rewrite_build)]
SWIZZLE = [("src/controls/raster_canvas/raster_canvas.cpp", rewrite_raster_canvas),
           ("src/render/coregraphics/raster/coregraphics_raster.cpp", rewrite_coregraphics),
           ("src/abi/drawing_platform_windows.cpp", rewrite_windows_drawing),
           ("src/host/windows/application/windows_host.cpp", rewrite_windows_host),
           ("tests/canvas_tests.cpp", rewrite_canvas_tests),
           ("tests/drawing_windows_probe.c", rewrite_windows_probe),
           ("CMakeLists.txt", rewrite_swizzle_links)]


def rewrite_stages(gui):
    """Applies both stages to the pinned sources. Returns the final text of every rewritten
    file and each stage's unified diff against the stage before it."""
    current = {}
    patches = []
    for stage in (OPTIMIZATION, SWIZZLE):
        patch = []
        for name, rewrite in stage:
            before = current[name] if name in current else read_source(gui, name)
            after = rewrite(before)
            current[name] = after
            patch.extend(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                         fromfile="a/" + name, tofile="b/" + name))
        patches.append("".join(patch))
    return current, patches


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--gui", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--patches-only", action="store_true")
    args = parser.parse_args()
    files, patches = rewrite_stages(args.gui)
    if not args.patches_only:
        if args.output is None:
            raise ValueError("--output is required unless --patches-only is given")
        if args.output.exists():
            raise ValueError("Refusing an existing fixture directory")
        args.output.mkdir(parents=True)
        archive = subprocess.Popen(["git", "-C", str(args.gui), "archive", REVISION],
                                   stdout=subprocess.PIPE)
        try:
            subprocess.run(["tar", "-x", "-C", str(args.output)], stdin=archive.stdout, check=True)
        finally:
            archive.stdout.close()
        if archive.wait() != 0:
            raise RuntimeError("Source archive failed")
        for name, text in files.items():
            # Exact bytes on every host: no text-mode newline translation on Windows.
            (args.output / name).write_text(text, encoding="utf-8", newline="")
    OPTIMIZATION_PATCH.write_text(patches[0], encoding="utf-8", newline="")
    SWIZZLE_PATCH.write_text(patches[1], encoding="utf-8", newline="")
    if args.patches_only:
        print("Wrote", OPTIMIZATION_PATCH, "and", SWIZZLE_PATCH)
    else:
        print("Prepared isolated consumer at", args.output)


if __name__ == "__main__":
    main()
