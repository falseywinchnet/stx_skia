/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/core/SkPaintPriv.h"

#include "include/core/SkBlendMode.h"
#include "include/core/SkBlender.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkColorType.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathEffect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkShader.h"
#include "include/private/SkAssert.h"
#include "include/private/SkDebug.h"
#include "src/core/SkColorSpacePriv.h"
#include "src/effects/colorfilters/SkColorFilterBase.h"
#include "src/shaders/SkColorFilterShader.h"
#include "src/shaders/SkShaderBase.h"

#include <cstdint>
#include <optional>

class SkColorSpace;

static bool changes_alpha(const SkPaint& paint) {
    SkColorFilter* cf = paint.getColorFilter();
    return cf && !as_CFB(cf)->isAlphaUnchanged();
}

enum SrcColorOpacity {
    // The src color is known to be opaque (alpha == 255)
    kOpaque_SrcColorOpacity = 0,
    // The src color is known to be fully transparent (color == 0)
    kTransparentBlack_SrcColorOpacity = 1,
    // The src alpha is known to be fully transparent (alpha == 0)
    kTransparentAlpha_SrcColorOpacity = 2,
    // The src color opacity is unknown
    kUnknown_SrcColorOpacity = 3
};

static bool blend_mode_is_opaque(SkBlendMode mode, SrcColorOpacity opacityType) {
    SkBlendModeCoeff src, dst;
    if (!SkBlendMode_AsCoeff(mode, &src, &dst)) {
        return false;
    }

    switch (src) {
        case SkBlendModeCoeff::kDA:
        case SkBlendModeCoeff::kDC:
        case SkBlendModeCoeff::kIDA:
        case SkBlendModeCoeff::kIDC:
            return false;
        default:
            break;
    }

    switch (dst) {
        case SkBlendModeCoeff::kZero:
            return true;
        case SkBlendModeCoeff::kISA:
            return kOpaque_SrcColorOpacity == opacityType;
        case SkBlendModeCoeff::kSA:
            return kTransparentBlack_SrcColorOpacity == opacityType ||
                   kTransparentAlpha_SrcColorOpacity == opacityType;
        case SkBlendModeCoeff::kSC:
            return kTransparentBlack_SrcColorOpacity == opacityType;
        default:
            return false;
    }
}

bool SkPaintPriv::Overwrites(const SkPaint* paint, ShaderOverrideOpacity overrideOpacity) {
    if (!paint) {
        // No paint means we default to SRC_OVER, so we overwrite iff our shader-override
        // is opaque, or we don't have one.
        return overrideOpacity != kNotOpaque_ShaderOverrideOpacity;
    }

    SrcColorOpacity opacityType = kUnknown_SrcColorOpacity;

    if (!changes_alpha(*paint)) {
        const unsigned paintAlpha = paint->getAlpha();
        if (0xff == paintAlpha && overrideOpacity != kNotOpaque_ShaderOverrideOpacity &&
            (!paint->getShader() || paint->getShader()->isOpaque())) {
            opacityType = kOpaque_SrcColorOpacity;
        } else if (0 == paintAlpha) {
            if (overrideOpacity == kNone_ShaderOverrideOpacity && !paint->getShader()) {
                opacityType = kTransparentBlack_SrcColorOpacity;
            } else {
                opacityType = kTransparentAlpha_SrcColorOpacity;
            }
        }
    }

    const auto bm = paint->asBlendMode();
    if (!bm) {
        return false;   // don't know for sure, so we play it safe and return false.
    }
    return blend_mode_is_opaque(bm.value(), opacityType);
}

bool SkPaintPriv::ShouldDither(const SkPaint& p, SkColorType dstCT) {
    // The paint dither flag can veto.
    if (!p.isDither()) {
        return false;
    }

    if (dstCT == kUnknown_SkColorType) {
        return false;
    }

    // We always dither 565 or 4444 when requested.
    if (dstCT == kRGB_565_SkColorType || dstCT == kARGB_4444_SkColorType) {
        return true;
    }

    // Otherwise, dither is only needed for non-const paints.
    return p.getMaskFilter() ||
           (p.getShader() && !as_SB(p.getShader())->isConstant());
}

void SkPaintPriv::RemoveColorFilter(SkPaint* p, SkColorSpace* dstCS) {
    if (SkColorFilter* filter = p->getColorFilter()) {
        if (SkShader* shader = p->getShader()) {
            // SkColorFilterShader will modulate the shader color by paint alpha
            // before applying the filter, so we'll reset it to opaque.
            p->setShader(SkColorFilterShader::Make(sk_ref_sp(shader),
                                                   p->getAlphaf(),
                                                   sk_ref_sp(filter)));
            p->setAlphaf(1.0f);
        } else {
            p->setColor(filter->filterColor4f(p->getColor4f(), sk_srgb_singleton(), dstCS), dstCS);
        }
        p->setColorFilter(nullptr);
    }
}
