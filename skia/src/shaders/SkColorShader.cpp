/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/shaders/SkColorShader.h"

#include "include/core/SkAlphaType.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkData.h"
#include "include/core/SkFlattenable.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkShader.h"
#include "include/private/SkFloatingPoint.h"
#include "src/core/SkColorSpacePriv.h"
#include "src/core/SkColorSpaceXformSteps.h"
#include "src/core/SkEffectPriv.h"
#include "src/core/SkPicturePriv.h"
#include "src/core/SkRasterPipeline.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkWriteBuffer.h"
#include "src/shaders/SkShaderBase.h"

#include <utility>

sk_sp<SkFlattenable> SkColorShader::CreateProc(SkReadBuffer& buffer) {
    if (buffer.isVersionLT(SkPicturePriv::Version::kCombineColorShaders)) {
        // Stored an 8-bit color only.
        return SkShaders::Color(buffer.readColor());
    } else {
        // Stores a floating-point color in sRGB.
        SkColor4f color;
        buffer.readColor4f(&color);
        return SkShaders::Color(color, SkColorSpace::MakeSRGB());
    }
}

void SkColorShader::flatten(SkWriteBuffer& buffer) const {
    buffer.writeColor4f(fColor);
}

bool SkColorShader::appendStages(const SkStageRec& rec, const SkShaders::MatrixRec&) const {
    SkColor4f color = fColor;
    SkColorSpaceXformSteps(sk_srgb_singleton(), kUnpremul_SkAlphaType,
                           rec.fDstCS,          kPremul_SkAlphaType).apply(color.vec());
    rec.fPipeline->appendConstantColor(rec.fAlloc, color.vec());
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////


namespace SkShaders {
sk_sp<SkShader> Color(SkColor color) {
    return Color(SkColor4f::FromColor(color), SkColorSpace::MakeSRGB());
}

sk_sp<SkShader> Color(const SkColor4f& color, sk_sp<SkColorSpace> space) {
    if (!SkIsFinite(color.vec(), 4)) {
        return nullptr;
    }

    // Convert to sRGB to simplify what must be stored, remaining unpremul until the final dst
    // color space is known during actual shading. Also pin the alpha to [0,1].
    SkColor4f srgb = color.pinAlpha();
    SkColorSpaceXformSteps(space.get(),         kUnpremul_SkAlphaType,
                           sk_srgb_singleton(), kUnpremul_SkAlphaType).apply(srgb.vec());

    return sk_make_sp<SkColorShader>(srgb);
}
}  // namespace SkShaders
