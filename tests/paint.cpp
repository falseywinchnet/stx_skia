#include "include/core/SkPaint.h"
#include "include/core/SkSerialProcs.h"
#include "src/core/SkPaintPriv.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkWriteBuffer.h"

#include <array>
#include <cstdint>
#include <cstdio>

template <typename T>
concept HasImageFiltering = requires(T& paint) {
    paint.getImageFilter();
    paint.refImageFilter();
    paint.setImageFilter(nullptr);
};
#ifdef STX_SKIA_REFERENCE
static_assert(HasImageFiltering<SkPaint>);
#else
static_assert(!HasImageFiltering<SkPaint>);
#endif

namespace {
bool round_trip() {
    for (int style = 0; style < 3; ++style) {
        SkPaint paint;
        paint.setColor(0xc04280a2U);
        paint.setStrokeWidth(2.25f);
        paint.setStrokeMiter(3.5f);
        paint.setStyle(static_cast<SkPaint::Style>(style));
        paint.setAntiAlias(true);
        paint.setBlendMode(SkBlendMode::kMultiply);
        const SkSerialProcs procs{};
        SkBinaryWriteBuffer writer(procs);
        SkPaintPriv::Flatten(paint, writer);
        std::array<uint32_t, 32> storage{};
        const size_t bytes = writer.bytesWritten();
        if (bytes > sizeof(storage)) return false;
        writer.writeToMemory(storage.data());
        SkReadBuffer reader(storage.data(), bytes);
        const SkPaint decoded = SkPaintPriv::Unflatten(reader);
        if (!reader.isValid() || decoded != paint) return false;
    }
    return true;
}

bool effect_slot(uint32_t imageTag, bool admitted) {
    // Historical v68 paint record: width, miter, RGBA, packed flags, followed
    // by path/shader/mask/color/image/blender entries. Null entries are zero.
    const SkSerialProcs procs{};
    SkBinaryWriteBuffer writer(procs);
    writer.writeScalar(2.0f);
    writer.writeScalar(4.0f);
    writer.writeColor4f(SkColor4f{0.25f, 0.5f, 0.75f, 1.0f});
    constexpr uint32_t hasEffects = 2U << 24;
    const uint32_t mode = static_cast<uint32_t>(SkBlendMode::kSrcOver) << 8;
    writer.write32(hasEffects | mode);
    for (int slot = 0; slot < 4; ++slot) writer.write32(0);
    writer.write32(imageTag);
    writer.write32(0);
    std::array<uint32_t, 32> storage{};
    const size_t bytes = writer.bytesWritten();
    if (bytes > sizeof(storage)) return false;
    writer.writeToMemory(storage.data());
    SkReadBuffer reader(storage.data(), bytes);
    const SkPaint decoded = SkPaintPriv::Unflatten(reader);
    if (reader.isValid() != admitted) return false;
    if (admitted) {
        if (decoded.getStrokeWidth() != 2.0f || decoded.getColor4f() != SkColor4f{0.25f, 0.5f, 0.75f, 1.0f}) return false;
    } else {
        const SkPaint empty;
        if (decoded != empty) return false;
    }
    return true;
}
}

int main() {
    if (!round_trip() || !effect_slot(0, true)) return 1;
#ifndef STX_SKIA_REFERENCE
    if (!effect_slot(1, false)) return 1;
#endif
    std::printf("paint: %zu bytes; API and serialized effect slot PASS\n", sizeof(SkPaint));
    return 0;
}
