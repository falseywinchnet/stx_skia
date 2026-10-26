#include "include/core/SkBitmap.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkImage.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkRefCnt.h"
#include "include/private/SkMalloc.h"
#include "include/private/chromium/SkDiscardableMemory.h"
#include "src/core/SkMipmap.h"
#include "src/core/SkMipmapBuilder.h"
#include "src/image/SkImage_Base.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

namespace {
struct Output {
    std::FILE* file = nullptr;
    uint64_t hash = 1469598103934665603ULL;
    size_t bytes = 0;
    int chains = 0;
};

bool record_pixels(const SkPixmap& pixels, Output& output) {
    const size_t rowBytes = pixels.info().minRowBytes();
    for (int y = 0; y < pixels.height(); ++y) {
        const uint8_t* row = static_cast<const uint8_t*>(pixels.addr(0, y));
        if (output.file != nullptr && std::fwrite(row, 1, rowBytes, output.file) != rowBytes) {
            return false;
        }
        for (size_t index = 0; index < rowBytes; ++index) {
            output.hash ^= row[index];
            output.hash *= 1099511628211ULL;
        }
        output.bytes += rowBytes;
    }
    return true;
}

bool chain(int width, int height, SkColorType format, Output& output) {
    sk_sp<SkColorSpace> colorSpace = SkColorSpace::MakeSRGB();
    const SkImageInfo info = SkImageInfo::Make(width, height, format, kPremul_SkAlphaType,
                                              colorSpace);
    const size_t pixelBytes = static_cast<size_t>(SkColorTypeBytesPerPixel(format));
    const size_t stride = static_cast<size_t>(width + 2) * pixelBytes;
    std::vector<uint64_t> storage((stride * height + 7) / 8, 0);
    const SkPixmap source(info, storage.data(), stride);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const SkColor color = SkColorSetARGB(255, (x * 19 + y * 7) & 255,
                                                  (y * 23 + x * 3) & 255, (x * y * 11) & 255);
            const SkIRect pixel = SkIRect::MakeXYWH(x, y, 1, 1);
            if (!source.erase(color, pixel)) return false;
        }
    }
    const std::vector<uint64_t> original = storage;
    sk_sp<SkMipmap> mipmap(SkMipmap::Build(source, nullptr));
    const int count = SkMipmap::ComputeLevelCount(width, height);
    if (count == 0) return !mipmap;
    if (!mipmap || (*mipmap).countLevels() != count) return false;
    if (!(*mipmap).validForRootLevel(info)) return false;
    int expectedWidth = width;
    int expectedHeight = height;
    for (int index = 0; index < count; ++index) {
        expectedWidth = expectedWidth > 1 ? expectedWidth / 2 : 1;
        expectedHeight = expectedHeight > 1 ? expectedHeight / 2 : 1;
        const SkISize dimensions = SkMipmap::ComputeLevelSize(width, height, index);
        if (dimensions.width() != expectedWidth || dimensions.height() != expectedHeight) return false;
        SkMipmap::Level level{};
        if (!(*mipmap).getLevel(index, &level) || !(*mipmap).getLevel(index, nullptr)) return false;
        if (level.fPixmap.dimensions() != dimensions) return false;
        if (level.fPixmap.rowBytes() != static_cast<size_t>(expectedWidth) * pixelBytes) return false;
        if (!SkColorSpace::Equals(level.fPixmap.colorSpace(), colorSpace.get())) return false;
        if (level.fScale.width() != static_cast<float>(expectedWidth) / width) return false;
        if (level.fScale.height() != static_cast<float>(expectedHeight) / height) return false;
        if (!record_pixels(level.fPixmap, output)) return false;
    }
    SkMipmap::Level unchanged{.fPixmap = source, .fScale = SkSize::Make(1.0f, 1.0f)};
    if ((*mipmap).getLevel(-1, &unchanged) || (*mipmap).getLevel(count, &unchanged)) return false;
    if ((*mipmap).extractLevel(SkSize::Make(1.0f, 1.0f), &unchanged)) return false;
    if (unchanged.fPixmap.addr() != source.addr()) return false;
    if (!(*mipmap).extractLevel(SkSize::Make(0.00001f, 0.00001f), &unchanged)) return false;
    if (unchanged.fPixmap.width() != 1 || unchanged.fPixmap.height() != 1) return false;
    if (storage != original) return false;
    ++output.chains;
    return true;
}

bool independent_average() {
    alignas(8) std::array<uint8_t, 64> storage{};
    const SkImageInfo info = SkImageInfo::Make(4, 4, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    const SkPixmap source(info, storage.data(), 16);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const size_t offset = static_cast<size_t>(y * 16 + x * 4);
            storage[offset] = static_cast<uint8_t>(x * 40);
            storage[offset + 1] = static_cast<uint8_t>(y * 40);
            storage[offset + 2] = 80;
            storage[offset + 3] = 255;
        }
    }
    sk_sp<SkMipmap> mips(SkMipmap::Build(source, nullptr));
    if (!mips) return false;
    SkMipmap::Level level{};
    if (!(*mips).getLevel(0, &level)) return false;
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            const SkColor expected = SkColorSetARGB(255, 20 + x * 80, 20 + y * 80, 80);
            if (level.fPixmap.getColor(x, y) != expected) return false;
        }
    }
    if (!(*mips).getLevel(1, &level)) return false;
    if (level.fPixmap.getColor(0, 0) != SkColorSetARGB(255, 60, 60, 80)) return false;
    SkBitmap bitmap;
    if (!bitmap.installPixels(source)) return false;
    sk_sp<SkMipmap> fromBitmap(SkMipmap::Build(bitmap, nullptr));
    if (!fromBitmap || (*fromBitmap).countLevels() != 2) return false;
    return true;
}

bool builder() {
    const SkImageInfo info = SkImageInfo::Make(9, 7, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    alignas(8) std::array<uint8_t, 9 * 7 * 4> storage{};
    const SkPixmap source(info, storage.data(), 9 * 4);
    if (!source.erase(SK_ColorGREEN)) return false;
    sk_sp<SkImage> image = SkImages::RasterFromPixmapCopy(source);
    if (!image) return false;
    sk_sp<SkImage> attached{};
    {
        SkMipmapBuilder levels(info);
        if (levels.countLevels() != 3) return false;
        if (levels.level(-1).addr() != nullptr || levels.level(3).addr() != nullptr) return false;
        for (int index = 0; index < 3; ++index) {
            const SkPixmap level = levels.level(index);
            const SkColor color = SkColorSetARGB(255, 20 + index * 30, 70, 90);
            if (!level.erase(color)) return false;
        }
        attached = levels.attachTo(image);
        if (!attached || !(*attached).hasMipmaps()) return false;
#ifndef STX_SKIA_REFERENCE
        if (levels.attachTo(nullptr)) return false;
#endif
    }
    image.reset();
    const SkImage_Base& base = *as_IB(attached);
    sk_sp<SkMipmap> mips = base.refMips();
    if (!mips) return false;
    for (int index = 0; index < 3; ++index) {
        SkMipmap::Level level{};
        if (!(*mips).getLevel(index, &level)) return false;
        const SkColor expected = SkColorSetARGB(255, 20 + index * 30, 70, 90);
        if (level.fPixmap.getColor(0, 0) != expected) return false;
    }
    const SkImageInfo tiny = info.makeWH(1, 1);
    SkMipmapBuilder empty(tiny);
    if (empty.countLevels() != 0 || empty.level(0).addr() != nullptr) return false;
    return true;
}

struct AllocationEvents {
    int calls = 0;
    int locks = 0;
    int unlocks = 0;
    int destroyed = 0;
    bool valid = true;
    bool failAllocation = false;
    bool discardOnUnlock = false;
};
// Factory API has no context argument. This fixture is single-threaded; global
// state outlives every returned block and is reset only after all owners release.
AllocationEvents events{};

class Discardable final : public SkDiscardableMemory {
public:
    explicit Discardable(size_t bytes) : fStorage(sk_malloc_throw(bytes)) {}
    ~Discardable() override {
        events.valid = events.valid && !fLocked;
        ++events.destroyed;
        sk_free(fStorage);
    }
    bool lock() override {
        events.valid = events.valid && !fLocked;
        ++events.locks;
        if (fStorage == nullptr) return false;
        fLocked = true;
        return true;
    }
    void* data() override {
        events.valid = events.valid && fLocked;
        return fStorage;
    }
    void unlock() override {
        events.valid = events.valid && fLocked;
        fLocked = false;
        ++events.unlocks;
        if (events.discardOnUnlock) {
            sk_free(fStorage);
            fStorage = nullptr;
        }
    }
private:
    void* fStorage = nullptr;
    bool fLocked = true;
};

SkDiscardableMemory* allocate(size_t bytes) {
    ++events.calls;
    if (events.failAllocation) return nullptr;
    SkDiscardableMemory* memory = new Discardable(bytes);
    return memory;
}

bool discardable(bool discard) {
    events = AllocationEvents{};
    events.discardOnUnlock = discard;
    alignas(8) std::array<uint8_t, 64> storage{};
    const SkImageInfo info = SkImageInfo::Make(4, 4, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    const SkPixmap source(info, storage.data(), 16);
    if (!source.erase(SK_ColorGREEN)) return false;
    sk_sp<SkMipmap> client(SkMipmap::Build(source, allocate));
    if (!client) return false;
    SkMipmap* cache = client.get();
    (*cache).attachToCacheAndRef();
    client.reset();
    // Cache identity remains live but no pixels may be inspected without a
    // client reference. The next ref either restores the block or reports loss.
    client = sk_ref_sp(cache);
    SkMipmap::Level selected{};
    const bool found = (*client).getLevel(0, &selected);
    const bool extracted = (*client).extractLevel(SkSize::Make(0.25f, 0.25f), nullptr);
    const bool valid = (*client).validForRootLevel(info);
    bool correct = found == !discard && extracted == !discard && valid == !discard;
    if (found && selected.fPixmap.getColor(0, 0) != SK_ColorGREEN) correct = false;
    (*cache).detachFromCacheAndUnref();
    client.reset();
    correct = correct && events.calls == 1 && events.locks == 1 &&
              events.unlocks == (discard ? 1 : 2) && events.destroyed == 1 && events.valid;
    return correct;
}

bool invalid_inputs() {
    events = AllocationEvents{};
    events.failAllocation = true;
    const SkImageInfo info = SkImageInfo::Make(4, 4, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    const SkPixmap metadata(info, nullptr, 0);
    sk_sp<SkMipmap> failed(SkMipmap::Build(metadata, allocate, false));
    if (failed || events.calls != 1) return false;
#ifndef STX_SKIA_REFERENCE
    events.calls = 0;
    alignas(8) std::array<uint8_t, 64> storage{};
    const std::array<SkPixmap, 6> invalid{{
        SkPixmap(info, nullptr, 16),
        SkPixmap(info, storage.data(), 4),
        SkPixmap(info, storage.data(), std::numeric_limits<size_t>::max()),
        SkPixmap(info.makeWH(-1, 4), storage.data(), 16),
        SkPixmap(info.makeColorType(kUnknown_SkColorType), storage.data(), 16),
        SkPixmap(info.makeWH(400000000, 400000000), nullptr, 0),
    }};
    for (size_t index = 0; index < invalid.size(); ++index) {
        const bool compute = index != invalid.size() - 1;
        sk_sp<SkMipmap> result(SkMipmap::Build(invalid[index], allocate, compute));
        if (result) return false;
    }
    if (events.calls != 0) return false;
#endif
    if (SkMipmap::ComputeLevelCount(-1, 4) != 0) return false;
    if (!SkMipmap::ComputeLevelSize(4, 4, -1).isEmpty()) return false;
    if (!SkMipmap::ComputeLevelSize(4, 4, 2).isEmpty()) return false;
    return true;
}
}

int main(int argc, char** argv) {
    if (!independent_average()) return 1;
    if (!builder()) return 2;
    if (!discardable(false) || !discardable(true)) return 3;
    if (!invalid_inputs()) return 4;
    Output output{};
    if (argc == 2) {
        output.file = std::fopen(argv[1], "wb");
        if (output.file == nullptr) return 5;
    }
    constexpr std::array<SkColorType, 4> formats{
        kAlpha_8_SkColorType, kRGB_565_SkColorType, kRGBA_8888_SkColorType, kRGBA_F16_SkColorType};
    bool passed = true;
    for (const SkColorType format : formats) {
        for (int height = 1; height <= 17; ++height) {
            for (int width = 1; width <= 17; ++width) {
                if (!chain(width, height, format, output)) passed = false;
            }
        }
    }
    if (output.file != nullptr && std::fclose(output.file) != 0) passed = false;
    if (!passed) return 6;
    std::printf("mipmap: %d chains, %zu bytes, hash=%llu PASS\n", output.chains,
                output.bytes, static_cast<unsigned long long>(output.hash));
    return 0;
}
