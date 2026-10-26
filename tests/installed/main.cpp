#include <stx_skia/pixel_storage.hpp>
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/core/SkImage.h"
#include "include/core/SkColorSpace.h"
#include <vector>
#include <cstddef>

template <typename T>
concept HasImageFiltering = requires(T& paint) { paint.getImageFilter(); };
static_assert(!HasImageFiltering<SkPaint>);
template <typename T>
concept HasProgrammableMesh = requires { &T::drawMesh; };
static_assert(!HasProgrammableMesh<SkCanvas>);
#if __has_include("include/effects/SkRuntimeEffect.h")
#error Runtime compiler headers must not be installed.
#endif

namespace {
struct ConsumerBase {
    virtual ~ConsumerBase() = default;
};
struct ConsumerDerived final : ConsumerBase {};
}

int main() {
    // Skia's private no-RTTI build must not disable RTTI for application types.
    ConsumerDerived consumer;
    ConsumerBase* base = &consumer;
    ConsumerDerived* recovered = dynamic_cast<ConsumerDerived*>(base);
    if (recovered != &consumer) return 4;
    std::vector<std::byte> pixels(16U * 16U * 4U, std::byte{0xFF});
    sk_sp<SkData> data = stx_skia::take_pixel_data(pixels);
    if (!data || !pixels.empty()) return 1;
    const SkImageInfo info = SkImageInfo::MakeN32Premul(16, 16, SkColorSpace::MakeSRGB());
    sk_sp<SkImage> image = SkImages::RasterFromData(info, data, 16U * 4U);
    if (!image) return 2;
    sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
    if (!surface) return 3;
    SkCanvas& canvas = *(*surface).getCanvas();
    canvas.drawImage(image, 0, 0);
    return 0;
}
