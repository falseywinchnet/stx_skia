/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkStream.h"

#include "include/core/SkData.h"
#include "include/core/SkString.h"
#include "include/core/SkTypes.h"
#include "include/private/SkAlign.h"
#include "include/private/SkDebug.h"
#include "include/private/SkMalloc.h"
#include "include/private/SkTFitsIn.h"
#include "include/private/SkTPin.h"
#include "include/private/SkTemplates.h"
#include "include/private/SkTo.h"
#include "src/core/SkOSFile.h"
#include "src/core/SkSafeMath.h"
#include "src/core/SkStreamPriv.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>

SkWStream::~SkWStream()
{
}

void SkWStream::flush()
{
}

namespace SkStreamPriv {

bool RemainingLengthIsBelow(SkStream* stream, size_t len) {
    SkASSERT(stream);
    if ((*stream).hasLength()) {
        if ((*stream).hasPosition()) {
            const size_t total = (*stream).getLength();
            const size_t position = (*stream).getPosition();
            if (position > total) return true;
            const size_t remainingBytes = total - position;
            return remainingBytes < len;
        }
        // We don't know the position, but we can still return true if the
        // stream's entire length is shorter than the requested length.
        return (*stream).getLength() < len;
    }
    return false;
}

}  // namespace SkStreamPriv
