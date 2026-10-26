#include "include/core/SkCanvas.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPixelRef.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"
#include "src/image/SkSurface_Raster.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <utility>

namespace {
constexpr int width = 9;
constexpr int height = 7;
constexpr std::size_t stride = 52;

struct ReleaseState {
    int calls = 0;
};

void record_release(void*, void* context) {
    ReleaseState& state = *static_cast<ReleaseState*>(context);
    ++state.calls;
}

// Storage and release state belong to each test scope and outlive the pixel ref.
// This tracks its lifetime without an allocator hook or a dangling observation.
class TrackedPixels final : public SkPixelRef {
public:
    TrackedPixels(void* pixels, ReleaseState& state)
        : SkPixelRef(9, 7, pixels, stride), fState(state) {}
    ~TrackedPixels() override { ++fState.calls; }
private:
    ReleaseState& fState;
};

SkImageInfo image_info() {
    const SkImageInfo info = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                                              kPremul_SkAlphaType);
    return info;
}

bool verify_image(const SkImage& image, bool greenCenter) {
    SkPixmap pixels;
    if (!image.peekPixels(&pixels)) return false;
    if (pixels.width() != width || pixels.height() != height) return false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool green = greenCenter && x >= 2 && x < 6 && y >= 1 && y < 5;
            const SkColor expected = green ? SK_ColorGREEN : SK_ColorRED;
            if (pixels.getColor(x, y) != expected) return false;
        }
    }
    return true;
}

bool snapshots(bool wrapped, bool discard) {
    ReleaseState release;
    std::array<uint8_t, stride * height> storage{};
    const SkImageInfo info = image_info();
    sk_sp<SkSurface> surface;
    if (wrapped) {
        surface = SkSurfaces::WrapPixels(info, storage.data(), stride, record_release, &release);
    } else {
        surface = SkSurfaces::Raster(info, stride, nullptr);
    }
    if (!surface) return false;
    SkCanvas& canvas = *(*surface).getCanvas();
    if ((*surface).getCanvas() != &canvas) return false;
    canvas.clear(SK_ColorRED);
    const uint32_t generation = (*surface).generationID();
    if (generation != (*surface).generationID()) return false;
    sk_sp<SkImage> oldImage = (*surface).makeImageSnapshot();
    if (!oldImage || !verify_image(*oldImage, false)) return false;
    sk_sp<SkImage> repeated = (*surface).makeImageSnapshot();
    if (repeated != oldImage) return false;
    repeated.reset();
    sk_sp<SkImage> subset = (*surface).makeImageSnapshot(SkIRect::MakeXYWH(-2, -2, 5, 4));
    if (!subset || (*subset).width() != 3 || (*subset).height() != 2) return false;
    if ((*surface).makeImageSnapshot(SkIRect::MakeXYWH(20, 20, 4, 4))) return false;

    if (discard) {
        (*surface).notifyContentWillChange(SkSurface::kDiscard_ContentChangeMode);
        canvas.clear(SK_ColorRED);
    }
    SkPaint paint;
    paint.setColor(SK_ColorGREEN);
    canvas.drawRect(SkRect::MakeXYWH(2, 1, 4, 4), paint);
    if ((*surface).generationID() == generation) return false;
    sk_sp<SkImage> current = (*surface).makeImageSnapshot();
    if (!current || current == oldImage || !verify_image(*current, true)) return false;
    if (!verify_image(*oldImage, false)) return false;
    if (release.calls != 0) return false;
    surface.reset();
    if (release.calls != (wrapped ? 1 : 0)) return false;
    if (!verify_image(*oldImage, false) || !verify_image(*current, true)) return false;
    SkPixmap subsetPixels;
    if (!(*subset).peekPixels(&subsetPixels)) return false;
    return subsetPixels.getColor(0, 0) == SK_ColorRED;
}

bool old_allocation_release() {
    ReleaseState release;
    std::array<uint8_t, stride * height> storage{};
    const SkImageInfo info = image_info();
    sk_sp<SkPixelRef> pixels = sk_make_sp<TrackedPixels>(storage.data(), release);
    sk_sp<SkSurface> surface = sk_make_sp<SkSurface_Raster>(info, std::move(pixels), nullptr);
    SkCanvas& canvas = *(*surface).getCanvas();
    canvas.clear(SK_ColorRED);
    sk_sp<SkImage> snapshot = (*surface).makeImageSnapshot();
    if (!snapshot) return false;
    SkPaint paint;
    paint.setColor(SK_ColorGREEN);
    canvas.drawRect(SkRect::MakeXYWH(2, 1, 4, 4), paint);
    if (release.calls != 0 || !verify_image(*snapshot, false)) return false;
    snapshot.reset();
#ifdef STX_SKIA_REFERENCE
    // Original capture bookkeeping retains the initial pixel ref until destruction.
    if (release.calls != 0) return false;
#else
    if (release.calls != 1) return false;
#endif
    sk_sp<SkImage> current = (*surface).makeImageSnapshot();
    if (!current || !verify_image(*current, true)) return false;
    surface.reset();
    return release.calls == 1;
}

bool restore_mutability() {
    const SkImageInfo info = image_info();
    sk_sp<SkSurface> surface = SkSurfaces::Raster(info, stride, nullptr);
    if (!surface) return false;
    SkCanvas& canvas = *(*surface).getCanvas();
    canvas.clear(SK_ColorRED);
    SkPixmap before;
    if (!(*surface).peekPixels(&before)) return false;
    sk_sp<SkImage> temporary = (*surface).makeImageSnapshot();
    if (!temporary) return false;
    temporary.reset();
    canvas.clear(SK_ColorGREEN);
    SkPixmap after;
    if (!(*surface).peekPixels(&after)) return false;
    if (before.addr() != after.addr()) return false;
    return after.getColor(4, 3) == SK_ColorGREEN;
}

struct ReadState {
    int calls = 0;
    std::unique_ptr<const SkSurface::AsyncReadResult> result = nullptr;
};

void record_read(void* context, std::unique_ptr<const SkSurface::AsyncReadResult> result) {
    ReadState& state = *static_cast<ReadState*>(context);
    ++state.calls;
    state.result = std::move(result);
}

bool callback_reads() {
    ReadState state;
    const SkImageInfo info = image_info();
    const SkImageInfo small = info.makeWH(3, 2);
    const SkIRect bounds = SkIRect::MakeWH(width, height);
    sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
    if (!surface) return false;
    SkCanvas& canvas = *(*surface).getCanvas();
    canvas.clear(SK_ColorGREEN);
    (*surface).asyncRescaleAndReadPixels(small, bounds, SkSurface::RescaleGamma::kSrc,
        SkSurface::RescaleMode::kNearest, record_read, &state);
    if (state.calls != 1 || !state.result || (*state.result).count() != 1) return false;
    {
        const SkPixmap pixels(small, (*state.result).data(0), (*state.result).rowBytes(0));
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 3; ++x) {
                if (pixels.getColor(x, y) != SK_ColorGREEN) return false;
            }
        }
    }
    (*surface).asyncRescaleAndReadPixels(small, SkIRect::MakeWH(width + 1, height),
        SkSurface::RescaleGamma::kSrc, SkSurface::RescaleMode::kNearest, record_read, &state);
    if (state.calls != 2 || state.result) return false;
#ifndef STX_SKIA_REFERENCE
    // The reference aborts when allocating an unknown-format fallback bitmap.
    sk_sp<SkSurface> empty = SkSurfaces::Null(width, height);
    (*empty).asyncRescaleAndReadPixels(small, bounds, SkSurface::RescaleGamma::kSrc,
        SkSurface::RescaleMode::kNearest, record_read, &state);
    return state.calls == 3 && !state.result;
#else
    return true;
#endif
}

bool invalid_and_null() {
    const SkImageInfo info = image_info();
    std::array<uint8_t, stride * height> storage{};
    ReleaseState release;
    if (SkSurfaces::WrapPixels(info, nullptr, stride, record_release, &release)) return false;
    if (SkSurfaces::WrapPixels(info, storage.data(), 1, record_release, &release)) return false;
    if (SkSurfaces::Raster(info, 1, nullptr)) return false;
    if (release.calls != 0) return false;
    if (SkSurfaces::Null(0, height)) return false;
    sk_sp<SkSurface> surface = SkSurfaces::Null(width, height);
    if (!surface || (*surface).makeImageSnapshot()) return false;
    SkCanvas& canvas = *(*surface).getCanvas();
    canvas.clear(SK_ColorRED);
    SkPixmap pixels;
    if ((*surface).peekPixels(&pixels)) return false;
    return (*surface).getCanvas() == &canvas;
}
}

int main() {
    for (int wrapped = 0; wrapped < 2; ++wrapped) {
        for (int discard = 0; discard < 2; ++discard) {
            if (!snapshots(wrapped != 0, discard != 0)) return 1;
        }
    }
    if (!old_allocation_release()) return 2;
    if (!restore_mutability()) return 3;
    if (!invalid_and_null()) return 4;
    if (!callback_reads()) return 5;
    std::printf("surface ownership, snapshots, retain/discard, release and null behavior; base %zu bytes; PASS\n",
                sizeof(SkSurface_Base));
    return 0;
}
