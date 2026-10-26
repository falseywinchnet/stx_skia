#include "include/core/SkBitmap.h"
#include "include/core/SkCPURecorder.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageGenerator.h"
#include "include/core/SkPixmap.h"
#include "src/core/SkMipmap.h"
#include "src/image/SkImage_Raster.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

namespace {
constexpr int width = 9;
constexpr int height = 7;
constexpr std::size_t stride = 52;
constexpr std::size_t byteCount = (height - 1) * stride + width * 4;
using Pixels = std::array<uint8_t, byteCount>;

SkImageInfo image_info() {
    const SkImageInfo info = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                                              kPremul_SkAlphaType);
    return info;
}

void fill_pixels(Pixels& pixels) {
    pixels.fill(0xad);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>(y) * stride + x * 4U;
            pixels[offset] = static_cast<uint8_t>(x * 20);
            pixels[offset + 1] = static_cast<uint8_t>(y * 30);
            pixels[offset + 2] = 70;
            pixels[offset + 3] = 255;
        }
    }
}

bool check_pixels(const SkImage& image, int sourceX, int sourceY) {
    SkPixmap pixels;
    if (!image.peekPixels(&pixels)) return false;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const SkColor expected = SkColorSetARGB(255, (x + sourceX) * 20,
                                                    (y + sourceY) * 30, 70);
            if (pixels.getColor(x, y) != expected) return false;
        }
    }
    return true;
}

struct ReleaseState { int calls = 0; };
void release_pixels(const void*, void* context) {
    ReleaseState& state = *static_cast<ReleaseState*>(context);
    ++state.calls;
}

bool factory_ownership() {
    ReleaseState released;
    Pixels pixels{};
    fill_pixels(pixels);
    const SkImageInfo info = image_info();
    const SkPixmap source(info, pixels.data(), stride);
    sk_sp<SkImage> copy = SkImages::RasterFromPixmapCopy(source);
    if (!copy || !check_pixels(*copy, 0, 0)) return false;
    const SkImage_Base& copyBase = *as_IB(copy);
    const SkBitmap* copyBitmap = copyBase.onPeekBitmap();
    if (!copyBitmap) return false;
    const SkPixelRef* copyPixels = (*copyBitmap).pixelRef();
    if (!copyPixels) return false;
#ifdef STX_SKIA_REFERENCE
    // Capture bookkeeping keeps a second owner in the original implementation.
    if ((*copyPixels).unique()) return false;
#else
    if (!(*copyPixels).unique()) return false;
#endif
    pixels[0] = 255;
    if (!check_pixels(*copy, 0, 0)) return false;
    fill_pixels(pixels);
    sk_sp<SkImage> borrowed = SkImages::RasterFromPixmap(source, release_pixels, &released);
    if (!borrowed || !check_pixels(*borrowed, 0, 0)) return false;
    sk_sp<SkImage> secondOwner = borrowed;
    borrowed.reset();
    if (released.calls != 0) return false;
    secondOwner.reset();
    if (released.calls != 1) return false;

    const SkPixmap invalid(info, pixels.data(), 1);
    if (SkImages::RasterFromPixmap(invalid, release_pixels, &released)) return false;
    if (released.calls != 1) return false;
    sk_sp<SkData> shortData = SkData::MakeWithCopy(pixels.data(), byteCount - 1);
    if (SkImages::RasterFromData(info, shortData, stride)) return false;
    sk_sp<SkData> exactData = SkData::MakeWithCopy(pixels.data(), byteCount);
    sk_sp<SkImage> retained = SkImages::RasterFromData(info, exactData, stride);
    if (!retained) return false;
    exactData.reset();
    const bool correct = check_pixels(*retained, 0, 0);
    return correct;
}

bool bitmap_ownership() {
    Pixels pixels{};
    fill_pixels(pixels);
    const SkImageInfo info = image_info();
    SkBitmap bitmap;
    if (!bitmap.installPixels(info, pixels.data(), stride)) return false;
    sk_sp<SkImage> copied = SkImages::RasterFromBitmap(bitmap);
    if (!copied) return false;
    bitmap.eraseColor(SK_ColorBLUE);
    if (!check_pixels(*copied, 0, 0)) return false;
    fill_pixels(pixels);
    bitmap.setImmutable();
    sk_sp<SkImage> shared = SkImages::RasterFromBitmap(bitmap);
    if (!shared || (*shared).uniqueID() != bitmap.getGenerationID()) return false;
    SkPixmap view;
    if (!(*shared).peekPixels(&view) || view.addr() != pixels.data()) return false;
    SkBitmap subset;
    if (!bitmap.extractSubset(&subset, SkIRect::MakeXYWH(2, 1, 4, 3))) return false;
    sk_sp<SkImage> subsetImage = SkImages::RasterFromBitmap(subset);
    if (!subsetImage || (*subsetImage).uniqueID() == (*shared).uniqueID()) return false;
    bitmap.reset();
    subset.reset();
    if (!check_pixels(*subsetImage, 2, 1) || !check_pixels(*shared, 0, 0)) return false;

    SkBitmap legacy;
    if (!(*subsetImage).asLegacyBitmap(&legacy)) return false;
    if (legacy.getColor(0, 0) != SkColorSetARGB(255, 40, 30, 70)) return false;
    if (!legacy.isImmutable()) return false;
    return true;
}

bool mipmaps_and_subsets() {
    Pixels pixels{};
    fill_pixels(pixels);
    const SkImageInfo info = image_info();
    const SkPixmap source(info, pixels.data(), stride);
    sk_sp<SkImage> image = SkImages::RasterFromPixmapCopy(source);
    if (!image) return false;
    sk_sp<SkImage> mipmapped = (*image).withDefaultMipmaps();
    if (!mipmapped || !(*mipmapped).hasMipmaps()) return false;
    if ((*mipmapped).uniqueID() == (*image).uniqueID()) return false;
    const SkImage::RequiredProperties mipProperties{.fMipmapped = true};
    const SkIRect bounds = SkIRect::MakeWH(width, height);
    sk_sp<SkImage> fullCopy = (*mipmapped).makeSubset(nullptr, bounds, mipProperties);
    if (!fullCopy || !(*fullCopy).hasMipmaps()) return false;
    const SkImage_Base& originalBase = *as_IB(mipmapped);
    const SkImage_Base& copyBase = *as_IB(fullCopy);
    sk_sp<SkMipmap> originalMips = originalBase.refMips();
    sk_sp<SkMipmap> copiedMips = copyBase.refMips();
    if (!originalMips || !copiedMips || originalMips == copiedMips) return false;
    const int levels = (*originalMips).countLevels();
    if ((*copiedMips).countLevels() != levels) return false;
    for (int i = 0; i < levels; ++i) {
        SkMipmap::Level original{};
        SkMipmap::Level copy{};
        if (!(*originalMips).getLevel(i, &original) || !(*copiedMips).getLevel(i, &copy)) return false;
        if (original.fPixmap.dimensions() != copy.fPixmap.dimensions()) return false;
        if (original.fPixmap.addr() == copy.fPixmap.addr()) return false;
        for (int y = 0; y < original.fPixmap.height(); ++y) {
            const void* originalRow = original.fPixmap.addr(0, y);
            const void* copyRow = copy.fPixmap.addr(0, y);
            const std::size_t rowSize = original.fPixmap.info().minRowBytes();
            if (std::memcmp(originalRow, copyRow, rowSize) != 0) return false;
        }
    }
    const SkIRect subsetBounds = SkIRect::MakeXYWH(2, 1, 4, 3);
    for (int useMips = 0; useMips < 2; ++useMips) {
        const SkImage::RequiredProperties properties{.fMipmapped = useMips != 0};
        sk_sp<SkImage> subset = (*mipmapped).makeSubset(nullptr, subsetBounds, properties);
        if (!subset || !check_pixels(*subset, 2, 1)) return false;
        if ((*subset).hasMipmaps() != (useMips != 0)) return false;
    }
    if ((*image).makeSubset(nullptr, SkIRect::MakeWH(width + 1, height), {})) return false;
    if ((*image).makeSubset(nullptr, SkIRect::MakeEmpty(), {})) return false;
    return true;
}

bool conversions_and_reads() {
    Pixels pixels{};
    fill_pixels(pixels);
    const SkImageInfo info = image_info();
    const SkPixmap source(info, pixels.data(), stride);
    sk_sp<SkImage> image = SkImages::RasterFromPixmapCopy(source);
    if (!image) return false;
    sk_sp<SkImage> converted = (*image).makeColorTypeAndColorSpace(
        nullptr, kBGRA_8888_SkColorType, nullptr, {});
    if (!converted || (*converted).colorType() != kBGRA_8888_SkColorType) return false;
    if (!check_pixels(*converted, 0, 0)) return false;
    if ((*image).makeColorTypeAndColorSpace(nullptr, kUnknown_SkColorType, nullptr, {})) return false;
    sk_sp<SkColorSpace> linear = SkColorSpace::MakeSRGBLinear();
    sk_sp<SkImage> reinterpreted = (*image).reinterpretColorSpace(linear);
    if (!reinterpreted || !SkColorSpace::Equals((*reinterpreted).colorSpace(), linear.get())) return false;
    SkPixmap reinterpretedPixels;
    if (!(*reinterpreted).peekPixels(&reinterpretedPixels)) return false;
    for (int y = 0; y < height; ++y) {
        if (std::memcmp(reinterpretedPixels.addr(0, y), source.addr(0, y), width * 4) != 0) return false;
    }
    constexpr std::size_t destinationStride = 60;
    std::array<uint8_t, destinationStride * (height + 2)> output{};
    output.fill(0xe1);
    const SkPixmap destination(info.makeWH(width + 2, height + 2), output.data(), destinationStride);
    if (!(*image).readPixels(nullptr, destination, -1, -1)) return false;
    for (int y = 0; y < height + 2; ++y) {
        for (std::size_t byte = 0; byte < destinationStride; ++byte) {
            uint8_t expected = 0xe1;
            if (y > 0 && y <= height && byte >= 4 && byte < (width + 1) * 4U) {
                expected = pixels[static_cast<std::size_t>(y - 1) * stride + byte - 4];
            }
            if (output[static_cast<std::size_t>(y) * destinationStride + byte] != expected) return false;
        }
    }
    return true;
}

class SolidGenerator final : public SkImageGenerator {
public:
    explicit SolidGenerator(bool succeed) : SkImageGenerator(image_info()), fSucceed(succeed) {}
protected:
    bool onGetPixels(const SkImageInfo& info, void* pixels, size_t rowBytes,
                     const Options&) override {
        if (!fSucceed) return false;
        const SkPixmap destination(info, pixels, rowBytes);
        const bool filled = destination.erase(SK_ColorGREEN);
        return filled;
    }
private:
    const bool fSucceed;
};

struct ReadState {
    int calls = 0;
    std::unique_ptr<const SkImage::AsyncReadResult> result = nullptr;
};
void record_read(void* context, std::unique_ptr<const SkImage::AsyncReadResult> result) {
    ReadState& state = *static_cast<ReadState*>(context);
    ++state.calls;
    state.result = std::move(result);
}

bool generated_reads(bool succeed) {
    ReadState state;
    std::unique_ptr<SkImageGenerator> generator = std::make_unique<SolidGenerator>(succeed);
    sk_sp<SkImage> image = SkImages::DeferredFromGenerator(std::move(generator));
    if (!image) return false;
    const SkImageInfo inputInfo = image_info();
    const SkImageInfo outputInfo = inputInfo.makeWH(3, 2);
    const SkIRect sourceBounds = SkIRect::MakeXYWH(2, 1, 5, 4);
    (*image).asyncRescaleAndReadPixels(outputInfo, sourceBounds,
        SkImage::RescaleGamma::kSrc, SkImage::RescaleMode::kNearest, record_read, &state);
    if (state.calls != 1) return false;
    if (!succeed) {
        const bool absent = state.result == nullptr;
        return absent;
    }
    if (!state.result || (*state.result).count() != 1) return false;
    const SkPixmap result(outputInfo, (*state.result).data(0), (*state.result).rowBytes(0));
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            if (result.getColor(x, y) != SK_ColorGREEN) return false;
        }
    }
    return true;
}
}

int main() {
    if (!factory_ownership()) return 1;
    if (!bitmap_ownership()) return 2;
    if (!mipmaps_and_subsets()) return 3;
    if (!conversions_and_reads()) return 4;
    if (!generated_reads(false) || !generated_reads(true)) return 5;
    std::printf("raster images: ownership, subsets, mipmaps, conversion, reads; base %zu, raster %zu bytes; PASS\n",
                sizeof(SkImage_Base), sizeof(SkImage_Raster));
    return 0;
}
