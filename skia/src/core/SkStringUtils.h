/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkStringUtils_DEFINED
#define SkStringUtils_DEFINED

#include "include/core/SkScalar.h"
#include "include/core/SkString.h"
#include "include/private/SkTArray.h"

#include <cstddef>
#include <cstdint>

enum SkScalarAsStringType {
    kDec_SkScalarAsStringType,
    kHex_SkScalarAsStringType,
};

void SkAppendScalar(SkString*, SkScalar, SkScalarAsStringType);

static inline void SkAppendScalarDec(SkString* str, SkScalar value) {
    SkAppendScalar(str, value, kDec_SkScalarAsStringType);
}

static inline void SkAppendScalarHex(SkString* str, SkScalar value) {
    SkAppendScalar(str, value, kHex_SkScalarAsStringType);
}

#endif
