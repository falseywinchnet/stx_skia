/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkStreamPriv_DEFINED
#define SkStreamPriv_DEFINED

#include "include/core/SkRefCnt.h"
#include "include/core/SkStream.h"
#include "src/core/SkEndian.h"

#include <cstdint>

class SkData;

namespace SkStreamPriv {

// If the stream supports identifying the current position and total length, this returns
// true if there are not enough bytes in the stream to fulfill a read of the given length.
// Otherwise, it returns false.
// False does *not* mean a read will succeed of the given length, but true means we are
// certain it will fail.
bool RemainingLengthIsBelow(SkStream* stream, size_t len);

}  // namespace SkStreamPriv

#endif  // SkStreamPriv_DEFINED
