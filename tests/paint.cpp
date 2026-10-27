#include "include/core/SkPaint.h"
#include "src/core/SkPaintPriv.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkWriteBuffer.h"

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

// Paint serialization had no production caller once pictures were removed; each entry point
// is gone.
template <typename Priv, typename Writer>
concept FlattensPaint = requires(const SkPaint& paint, Writer& writer) { Priv::Flatten(paint, writer); };
template <typename Priv, typename Reader>
concept UnflattensPaint = requires(Reader& reader) { Priv::Unflatten(reader); };
template <typename Reader>
concept ReadsPaint = requires(Reader& reader) { reader.readPaint(); };
template <typename Writer>
concept WritesPaint = requires(const SkPaint& paint, Writer& writer) { writer.writePaint(paint); };
static_assert(!FlattensPaint<SkPaintPriv, SkBinaryWriteBuffer>);
static_assert(!UnflattensPaint<SkPaintPriv, SkReadBuffer>);
static_assert(!ReadsPaint<SkReadBuffer>);
static_assert(!WritesPaint<SkBinaryWriteBuffer>);
#endif

int main() {
    std::printf("paint: %zu bytes; API PASS\n", sizeof(SkPaint));
    return 0;
}
