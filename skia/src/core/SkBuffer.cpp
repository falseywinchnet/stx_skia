/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/core/SkBuffer.h"

#include "include/private/SkAlign.h"
#include "include/private/SkMalloc.h"

#include <cstddef>
#include <cstdint>

///////////////////////////////////////////////////////////////////////////////////////////////////

const void* SkRBuffer::skip(size_t size) {
    if (fValid && size <= (*this).available()) {
        const void* pos = fPos;
        fPos += size;
        return pos;
    }
    fValid = false;
    return nullptr;
}

bool SkRBuffer::read(void* buffer, size_t size) {
    if (const void* src = (*this).skip(size)) {
        sk_careful_memcpy(buffer, src, size);
        return true;
    }
    return false;
}

bool SkRBuffer::skipToAlign4() {
    const intptr_t pos = reinterpret_cast<intptr_t>(fPos);
    const size_t n = static_cast<size_t>(SkAlign4(pos) - pos);
    if (fValid && n <= (*this).available()) {
        fPos += n;
        return true;
    } else {
        fValid = false;
        return false;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void* SkWBuffer::skip(size_t size) {
    void* result = fPos;
    writeNoSizeCheck(nullptr, size);
    return fData == nullptr ? nullptr : result;
}

void SkWBuffer::writeNoSizeCheck(const void* buffer, size_t size) {
    SkASSERT(fData == nullptr || fStop == nullptr || fPos + size <= fStop);
    if (fData && buffer) {
        sk_careful_memcpy(fPos, buffer, size);
    }
    fPos += size;
}

size_t SkWBuffer::padToAlign4() {
    const size_t pos = (*this).pos();
    const size_t n = SkAlign4(pos) - pos;

    if (n && fData) {
        char* p = fPos;
        char* stop = p + n;
        do {
            *p++ = 0;
        } while (p < stop);
    }
    fPos += n;
    return n;
}
