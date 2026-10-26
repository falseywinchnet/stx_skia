#include "include/core/SkCanvas.h"
#include "include/core/SkClipOp.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkRect.h"
#include "include/core/SkShader.h"
#include "include/core/SkSurface.h"
#include "src/core/SkCanvasPriv.h"
#include "src/core/SkRasterClipStack.h"

#include <climits>
#include <cstdio>
#include <cstring>

namespace {
bool fractional_clip_is_aa(SkRasterClipStack& stack) {
    const SkMatrix transform = SkMatrix::I();
    const SkRect rectangle = SkRect::MakeLTRB(0.25f, 0.25f, 7.75f, 7.75f);
    stack.clipRect(transform, rectangle, SkClipOp::kIntersect, true);
    return stack.rc().isAA();
}

bool ordinary_clip_case() {
    SkRasterClipStack small(16, 16);
    if (!fractional_clip_is_aa(small)) return false;
    SkRasterClipStack large(20000, 16);
    if (fractional_clip_is_aa(large)) return false;
    return true;
}

#ifndef STX_SKIA_REFERENCE
bool resized_clip_case() {
    SkRasterClipStack grown(16, 16);
    grown.setNewSize(20000, 16);
    if (fractional_clip_is_aa(grown)) return false;

    SkRasterClipStack shrunk(20000, 16);
    shrunk.setNewSize(16, 16);
    if (!fractional_clip_is_aa(shrunk)) return false;
    return true;
}

bool replacement_drops_shader_case() {
    SkRasterClipStack stack(8, 8);
    stack.clipShader(SkShaders::Color(SkColorSetARGB(128, 0, 0, 0)));
    stack.replaceClip(SkIRect::MakeWH(8, 8));
    if (stack.rc().clipShader()) return false;

    stack.clipShader(SkShaders::Color(SkColorSetARGB(128, 0, 0, 0)));
    stack.setNewSize(16, 8);
    if (stack.rc().clipShader()) return false;

    const SkImageInfo info = SkImageInfo::MakeN32Premul(8, 8);
    sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
    if (!surface) return false;
    SkCanvas& canvas = *(*surface).getCanvas();
    canvas.clear(SK_ColorTRANSPARENT);
    canvas.clipShader(SkShaders::Color(SkColorSetARGB(128, 0, 0, 0)));
    SkCanvasPriv::ResetClip(&canvas);
    SkPaint paint;
    paint.setColor(SK_ColorWHITE);
    canvas.drawRect(SkRect::MakeWH(8, 8), paint);
    SkPixmap pixels;
    if (!(*surface).peekPixels(&pixels)) return false;
    return SkColorGetA(pixels.getColor(1, 1)) == 255;
}

bool aa_translation_preserves_bounds_case() {
    const SkRect fractional = SkRect::MakeLTRB(0.25f, 0.25f, 4.75f, 4.75f);
    const SkPath path = SkPath::Rect(fractional);
    const SkIRect limit = SkIRect::MakeWH(8, 8);
    const SkRasterClip source(path, limit, true);
    if (!source.isAA() || source.isEmpty()) return false;
    const int originalWidth = source.getBounds().width();
    const int originalHeight = source.getBounds().height();

    SkAAClip direct;
    source.aaRgn().translate(INT_MAX, INT_MAX, &direct);
    if (direct.isEmpty() || direct.getBounds().width() != originalWidth ||
        direct.getBounds().height() != originalHeight) {
        std::fputs("direct AA translation changed bounds\n", stderr);
        return false;
    }

    SkRasterClip raster;
    source.translate(INT_MAX, INT_MAX, &raster);
    if (raster.isEmpty() || raster.getBounds().width() != originalWidth ||
        raster.getBounds().height() != originalHeight) {
        std::fputs("raster AA translation changed bounds\n", stderr);
        return false;
    }

    SkAAClip inPlace = source.aaRgn();
    inPlace.translate(INT_MAX, INT_MAX, &inPlace);
    if (inPlace.isEmpty() || inPlace.getBounds().width() != originalWidth ||
        inPlace.getBounds().height() != originalHeight) {
        std::fputs("in-place AA translation changed bounds\n", stderr);
        return false;
    }

    const SkRect negativeFractional = SkRect::MakeLTRB(-4.75f, -4.75f, -0.25f, -0.25f);
    const SkPath negativePath = SkPath::Rect(negativeFractional);
    const SkIRect negativeLimit = SkIRect::MakeLTRB(-8, -8, 0, 0);
    const SkRasterClip negativeSource(negativePath, negativeLimit, true);
    if (!negativeSource.isAA() || negativeSource.isEmpty()) {
        std::fputs("negative AA source was empty\n", stderr);
        return false;
    }
    SkRasterClip negativeRaster;
    negativeSource.translate(INT_MIN, INT_MIN, &negativeRaster);
    if (negativeRaster.isEmpty() ||
        negativeRaster.getBounds().width() != negativeSource.getBounds().width() ||
        negativeRaster.getBounds().height() != negativeSource.getBounds().height()) {
        std::fputs("negative AA translation changed bounds\n", stderr);
        return false;
    }
    return true;
}

bool bw_translation_preserves_bounds_case() {
    const SkIRect original = SkIRect::MakeLTRB(-5, -5, 0, 0);
    const SkRasterClip source(original);
    SkRasterClip translated;
    source.translate(INT_MIN, INT_MIN, &translated);
    if (translated.isEmpty()) return false;
    if (translated.getBounds().width() != original.width() ||
        translated.getBounds().height() != original.height()) return false;
    return true;
}
#endif
}

int main(int argc, char** argv) {
#ifndef STX_SKIA_REFERENCE
    if (argc == 2 && std::strcmp(argv[1], "--resize-saved") == 0) {
        SkRasterClipStack invalid(16, 16);
        invalid.save();
        invalid.setNewSize(32, 32);
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--negative-size") == 0) {
        SkRasterClipStack invalid(16, 16);
        invalid.setNewSize(-1, 16);
        return 0;
    }
#endif
    bool valid = ordinary_clip_case();
#ifndef STX_SKIA_REFERENCE
    valid = resized_clip_case() && valid;
    valid = replacement_drops_shader_case() && valid;
    valid = aa_translation_preserves_bounds_case() && valid;
    valid = bw_translation_preserves_bounds_case() && valid;
#endif
    if (!valid) {
        std::fputs("raster clip stack mismatch\n", stderr);
        return 1;
    }
    std::puts("raster clip stack PASS");
    return 0;
}
