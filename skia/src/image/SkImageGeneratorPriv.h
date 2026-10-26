/*
 * Copyright 2023 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkImageGeneratorPriv_DEFINED
#define SkImageGeneratorPriv_DEFINED

#include "include/core/SkRefCnt.h" // IWYU pragma: keep

#include <memory>
#include <optional>

class SkColorSpace;
class SkData;
class SkImageGenerator;
class SkMatrix;
class SkPaint;
class SkSurfaceProps;
enum SkAlphaType : int;
struct SkISize;

namespace SkImageGenerators {
/**
 *  If the default image decoder system can interpret the specified (encoded) data, then
 *  this returns a new ImageGenerator for it. Otherwise this returns NULL. Either way
 *  the caller is still responsible for managing their ownership of the data.
 *  By default, images will be converted to premultiplied pixels. The alpha type can be
 *  overridden by specifying kPremul_SkAlphaType or kUnpremul_SkAlphaType. Specifying
 *  kOpaque_SkAlphaType is not supported, and will return NULL.
 */
std::unique_ptr<SkImageGenerator> MakeFromEncoded(sk_sp<const SkData>,
                                                  std::optional<SkAlphaType> = std::nullopt);
}

#endif
