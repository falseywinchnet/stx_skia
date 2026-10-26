#include "include/core/SkBlurTypes.h"
#include "include/core/SkRect.h"
#include "src/core/SkBlurMask.h"
#include "src/core/SkMask.h"
#include "src/core/SkMaskBlurFilter.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

#ifndef STX_SKIA_REFERENCE
template <typename T>
concept BorrowsMaskImage = requires(T&& view) { std::forward<T>(view).image(); };
static_assert(BorrowsMaskImage<SkMaskBuilder&>);
static_assert(!BorrowsMaskImage<SkMaskBuilder>);
#endif

namespace {
struct CloseFile {
    void operator()(std::FILE* file) const {
        if (file != nullptr) std::fclose(file);
    }
};
struct Record {
    std::FILE* file = nullptr;
    uint64_t hash = 1469598103934665603ULL;
    size_t bytes = 0;
    bool append(const void* values, size_t count) {
        const uint8_t* data = static_cast<const uint8_t*>(values);
        for (size_t i = 0; i < count; ++i) {
            hash ^= data[i];
            hash *= 1099511628211ULL;
        }
        if (file != nullptr && std::fwrite(values, 1, count, file) != count) return false;
        bytes += count;
        return true;
    }
    bool mask(const SkMask& value) {
        const std::array<int32_t, 5> shape = {value.fBounds.fLeft, value.fBounds.fTop,
                                            value.fBounds.fRight, value.fBounds.fBottom,
                                            static_cast<int32_t>(value.fRowBytes)};
        if (!append(shape.data(), sizeof(shape))) return false;
        if (value.fImage == nullptr) return true;
        const size_t extent = value.computeTotalImageSize();
        const bool saved = append(value.fImage, extent);
        return saved;
    }
};

bool sizes_and_bounds() {
    for (int format = 0; format < SkMask::kCountMaskFormats; ++format) {
        const SkMask::Format type = static_cast<SkMask::Format>(format);
        for (int height = 0; height < 33; ++height) {
            for (uint32_t stride = 0; stride < 129; ++stride) {
                const SkMask mask(nullptr, SkIRect::MakeXYWH(-20, 7, 9, height), stride, type);
                const size_t expected = static_cast<size_t>(height) * stride;
                const size_t planes = type == SkMask::k3D_Format ? 3 : 1;
                if (mask.computeImageSize() != expected || mask.computeTotalImageSize() != planes * expected) return false;
            }
        }
    }
    const std::array<uint32_t, 5> strides = {1, 3, 65535, 0x7FFFFFFFU, 0xFFFFFFFFU};
    for (const uint32_t stride : strides) {
        for (const int32_t height : {1, 3, 65535, INT32_MAX}) {
            const SkMask mask(nullptr, SkIRect::MakeWH(1, height), stride, SkMask::kA8_Format);
            const uint64_t product = static_cast<uint64_t>(height) * stride;
            const size_t expected = product > INT32_MAX ? 0 : static_cast<size_t>(product);
            if (mask.computeImageSize() != expected) return false;
        }
    }
    for (const int x : {-20, 0, 18}) {
        for (const int y : {-30, 0, 27}) {
            for (const int width : {0, 1, 17}) {
                for (const int height : {0, 1, 13}) {
                    const SkMask src(nullptr, SkIRect::MakeXYWH(x, y, width, height), 0, SkMask::kA8_Format);
                    for (const int rx : {0, 1, 7}) {
                        for (const int ry : {0, 2, 5}) {
                            SkMaskBuilder dst = SkMaskBuilder::PrepareDestination(rx, ry, src);
                            if (dst.fImage != nullptr || dst.fFormat != SkMask::kA8_Format) return false;
                            const SkIRect expected = SkIRect::MakeLTRB(x - rx, y - ry, x + width + rx, y + height + ry);
                            if (dst.fBounds != expected || dst.fRowBytes != static_cast<uint32_t>(width + 2 * rx)) return false;
                        }
                    }
                }
            }
        }
    }
    return true;
}

bool addresses_and_iterators() {
    alignas(uint32_t) std::array<uint8_t, 512> pixels{};
    for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<uint8_t>(i * 73 + 19);
    const SkIRect bounds = SkIRect::MakeXYWH(-11, 17, 23, 4);
    for (const SkMask::Format format : {SkMask::kBW_Format, SkMask::kA8_Format, SkMask::k3D_Format,
                                       SkMask::kARGB32_Format, SkMask::kLCD16_Format, SkMask::kSDF_Format}) {
        const SkMask mask(pixels.data(), bounds, 128, format);
        const size_t bpp = format == SkMask::kARGB32_Format ? 4 : (format == SkMask::kLCD16_Format ? 2 : 1);
        for (int y = bounds.fTop; y < bounds.fBottom; ++y) {
            for (int x = bounds.fLeft; x < bounds.fRight; ++x) {
                const size_t row = static_cast<size_t>(y - bounds.fTop) * 128;
                const size_t column = static_cast<size_t>(x - bounds.fLeft);
                const size_t offset = row + (format == SkMask::kBW_Format ? column / 8 : column * bpp);
                const void* expected = pixels.data() + offset;
                if (format == SkMask::kBW_Format) {
                    if (mask.getAddr1(x, y) != expected) return false;
                } else {
                    if (mask.getAddr(x, y) != expected) return false;
                    if ((format == SkMask::kA8_Format || format == SkMask::kSDF_Format) && mask.getAddr8(x, y) != expected) return false;
                    if (format == SkMask::kLCD16_Format && mask.getAddrLCD16(x, y) != expected) return false;
                    if (format == SkMask::kARGB32_Format && mask.getAddr32(x, y) != expected) return false;
                }
            }
        }
    }
    SkMask::AlphaIter<SkMask::kBW_Format> bit(pixels.data(), 0);
    const SkMask::AlphaIter<SkMask::kBW_Format> end(pixels.data() + 8, 0);
    for (int i = 0; i < 64; ++i) {
        const int shift = 7 - i % 8;
        const uint8_t expected = (pixels[static_cast<size_t>(i / 8)] & (1U << shift)) != 0 ? 255 : 0;
        if (!(bit < end) || *bit != expected) return false;
        ++bit;
    }
    if (bit < end) return false;
    for (int i = 63; i >= 0; --i) {
        --bit;
        const uint8_t expected = (pixels[static_cast<size_t>(i / 8)] & (1U << (7 - i % 8))) != 0 ? 255 : 0;
        if (*bit != expected) return false;
    }
    bit >>= 128;
    if (*bit != ((pixels[128] & 0x80) != 0 ? 255 : 0)) return false;
    SkMask::AlphaIter<SkMask::kA8_Format> alpha(pixels.data());
    for (int i = 0; i < 23; ++i) {
        if (*alpha != pixels[static_cast<size_t>(i)]) return false;
        ++alpha;
    }
    --alpha;
    alpha >>= 128;
    if (*alpha != pixels[150]) return false;
    const std::array<uint32_t, 4> argb = {0x00112233U, 0x87112233U, 0xFF112233U, 0x40112233U};
    SkMask::AlphaIter<SkMask::kARGB32_Format> argb_it(argb.data());
    for (const uint32_t pixel : argb) {
        if (*argb_it != pixel >> 24) return false;
        ++argb_it;
    }
    const std::array<uint16_t, 4> lcd = {0, 0xFFFF, 0xF800, 0x07E0};
    SkMask::AlphaIter<SkMask::kLCD16_Format> lcd_it(lcd.data());
    const std::array<uint8_t, 4> expected_lcd = {0, 255, 85, 85};
    for (const uint8_t expected : expected_lcd) {
        if (*lcd_it != expected) return false;
        ++lcd_it;
    }
    return true;
}

bool allocation_and_moves() {
    // The descriptor is a borrower. Allocate/retain owners before its lifetime.
    SkAutoMaskFreeImage pixels(SkMaskBuilder::AllocImage(17, SkMaskBuilder::kZeroInit_Alloc));
    if (!pixels) return false;
    for (size_t i = 0; i < 20; ++i) {
        if (pixels.get()[i] != 0) return false;
    }
    SkMaskBuilder view(pixels.get(), SkIRect::MakeWH(5, 3), 5, SkMask::kA8_Format);
    view.bounds().offset(-4, 7);
    if (view.image() != pixels.get() || view.getAddr8(-4, 7) != pixels.get()) return false;
    SkAutoMaskFreeImage prepared_owner;
    SkMaskBuilder prepared = SkMaskBuilder::PrepareDestination(2, 3, view);
    prepared_owner.reset(prepared.image());
    if (!prepared_owner || prepared.fBounds != SkIRect::MakeLTRB(-6, 4, 3, 13) || prepared.fRowBytes != 9) return false;
#ifndef STX_SKIA_REFERENCE
    SkMaskBuilder moved(std::move(view));
    if (moved.image() != pixels.get() || view.fImage != nullptr || !view.isEmpty() || view.fRowBytes != 0) return false;
    moved = std::move(moved);
    if (moved.image() != pixels.get()) return false;
    // Replacing a view does not free either externally owned allocation.
    prepared = std::move(moved);
    if (prepared.image() != pixels.get() || moved.fImage != nullptr || !moved.isEmpty()) return false;
    prepared.getAddr8(-4, 7)[0] = 93;
    if (pixels.get()[0] != 93) return false;
    const SkMask bad(nullptr, SkIRect::MakeLTRB(0, 3, 1, 2), UINT32_MAX, SkMask::kA8_Format);
    if (bad.computeImageSize() != 0) return false;
    const SkMask extreme(nullptr, SkIRect::MakeLTRB(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX), UINT32_MAX, SkMask::k3D_Format);
    if (extreme.computeImageSize() != 0 || extreme.computeTotalImageSize() != 0) return false;
    const SkMask edge(nullptr, SkIRect::MakeLTRB(INT32_MIN, 0, INT32_MIN + 2, 2), 2, SkMask::kA8_Format);
    SkMaskBuilder overflow = SkMaskBuilder::PrepareDestination(1, 0, edge);
    if (!overflow.isEmpty() || overflow.fImage != nullptr || overflow.fRowBytes != 0) return false;
    SkMaskBuilder negative = SkMaskBuilder::PrepareDestination(-1, 0, prepared);
    if (!negative.isEmpty() || negative.fImage != nullptr || negative.fRowBytes != 0) return false;
    const SkMask bw(pixels.get(), SkIRect::MakeWH(5, 3), 1, SkMask::kBW_Format);
    const SkMask invalid(pixels.get(), SkIRect::MakeWH(5, 3), 5, static_cast<SkMask::Format>(255));
    if (bw.getAddr(0, 0) != nullptr || invalid.getAddr(0, 0) != nullptr) return false;
#endif
    return true;
}

bool blur_comparison(Record& record) {
    for (const SkMask::Format format : {SkMask::kBW_Format, SkMask::kA8_Format,
                                       SkMask::kARGB32_Format, SkMask::kLCD16_Format}) {
        for (const int width : {1, 7, 8, 17, 33}) {
            for (const int height : {1, 5, 19}) {
                const size_t bpp = format == SkMask::kARGB32_Format ? 4 : (format == SkMask::kLCD16_Format ? 2 : 1);
                const size_t active = format == SkMask::kBW_Format ? static_cast<size_t>((width + 7) / 8) : static_cast<size_t>(width) * bpp;
                const size_t stride = (active + 7) & ~size_t{3};
#ifdef STX_SKIA_REFERENCE
                // The reference advances row iterators after its last live row.
                const size_t extent = static_cast<size_t>(height + 1) * stride;
#else
                // The candidate must work with no padding after the last pixel.
                const size_t extent = static_cast<size_t>(height - 1) * stride + active;
#endif
                std::unique_ptr<uint8_t[]> input(new uint8_t[extent]);
                for (size_t i = 0; i < extent; ++i) input[i] = static_cast<uint8_t>(i * 43 + 11);
                const SkMask src(input.get(), SkIRect::MakeXYWH(-13, 7, width, height),
                                 static_cast<uint32_t>(stride), format);
                for (const double sigma : {0.5, 1.5, 2.0, 4.0, 8.0}) {
                    SkAutoMaskFreeImage owner;
                    SkMaskBuilder dst;
                    const SkMaskBlurFilter filter(sigma, sigma);
                    const SkIPoint border = filter.blur(src, &dst);
                    owner.reset(dst.image());
                    if (!owner || dst.fFormat != SkMask::kA8_Format || border.x() < 0 || border.y() < 0) return false;
                    if (!record.mask(dst)) return false;
                }
            }
        }
    }
    const std::array<uint8_t, 35> input = {0, 31, 79, 127, 191, 223, 255};
    const SkMask src(input.data(), SkIRect::MakeWH(7, 5), 7, SkMask::kA8_Format);
    for (const SkBlurStyle style : {kNormal_SkBlurStyle, kSolid_SkBlurStyle, kOuter_SkBlurStyle, kInner_SkBlurStyle}) {
        SkAutoMaskFreeImage owner;
        SkMaskBuilder dst;
        SkIVector margin = {0, 0};
        const bool blurred = SkBlurMask::BoxBlur(&dst, src, 2.5f, style, &margin);
        owner.reset(dst.image());
        if (!blurred || !owner || !record.mask(dst)) return false;
    }
    return true;
}
}

// Allocation and temporary storage are included; source preparation is excluded.
static int benchmark_blur() {
    uint64_t checksum = 0;
    for (const SkMask::Format format : {SkMask::kBW_Format, SkMask::kA8_Format,
                                       SkMask::kARGB32_Format, SkMask::kLCD16_Format}) {
        for (const int width : {33, 257}) {
            const int height = width == 33 ? 19 : 127;
            const int repeats = width == 33 ? 2000 : 80;
            const size_t bpp = format == SkMask::kARGB32_Format ? 4 : (format == SkMask::kLCD16_Format ? 2 : 1);
            const size_t active = format == SkMask::kBW_Format ? static_cast<size_t>((width + 7) / 8) : static_cast<size_t>(width) * bpp;
            const size_t stride = (active + 7) & ~size_t{3};
            const size_t extent = static_cast<size_t>(height + 1) * stride;
            std::unique_ptr<uint8_t[]> input(new uint8_t[extent]);
            for (size_t i = 0; i < extent; ++i) input[i] = static_cast<uint8_t>(i * 43 + 11);
            const SkMask src(input.get(), SkIRect::MakeWH(width, height), static_cast<uint32_t>(stride), format);
            for (const double sigma : {1.5, 8.0}) {
                const SkMaskBlurFilter filter(sigma, sigma);
                const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
                for (int i = 0; i < repeats; ++i) {
                    SkAutoMaskFreeImage owner;
                    SkMaskBuilder dst;
                    filter.blur(src, &dst);
                    owner.reset(dst.image());
                    if (!owner) return 1;
                    const size_t bytes = dst.computeImageSize();
                    checksum += owner.get()[static_cast<size_t>(i) % bytes];
                }
                const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                const std::chrono::duration<double> elapsed = end - start;
                const double seconds = elapsed.count();
                std::printf("%d,%d,%d,%.1f,%d,%.9f\n", static_cast<int>(format), width, height, sigma, repeats, seconds);
            }
        }
    }
    std::printf("checksum=%llu\n", static_cast<unsigned long long>(checksum));
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--benchmark") == 0) {
        const int result = benchmark_blur();
        return result;
    }
    std::unique_ptr<std::FILE, CloseFile> file;
    if (argc == 2) {
        file.reset(std::fopen(argv[1], "wb"));
        if (!file) return 2;
    }
    Record record{.file = file.get()};
    if (!sizes_and_bounds()) { std::puts("size/bounds failed"); return 1; }
    if (!addresses_and_iterators()) { std::puts("address/iterator failed"); return 1; }
    if (!allocation_and_moves()) { std::puts("allocation/move failed"); return 1; }
    if (!blur_comparison(record)) { std::puts("blur failed"); return 1; }
    if (file && std::fflush(file.get()) != 0) return 2;
    std::printf("mask: %zu bytes hash=%llu PASS\n", record.bytes, static_cast<unsigned long long>(record.hash));
    return 0;
}
