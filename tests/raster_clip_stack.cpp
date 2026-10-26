#include "include/core/SkClipOp.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkRect.h"
#include "src/core/SkRasterClipStack.h"

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
#endif
    if (!valid) {
        std::fputs("raster clip stack mismatch\n", stderr);
        return 1;
    }
    std::puts("raster clip stack PASS");
    return 0;
}
