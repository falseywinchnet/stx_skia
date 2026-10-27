/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkString.h"

#include "include/private/SkDebug.h"
#include "include/private/SkFloatingPoint.h"
#include "include/private/SkMalloc.h"
#include "include/private/SkTPin.h"
#include "include/private/SkTo.h"
#include "src/core/SkSafeMath.h"
#include "src/core/SkUTF.h"
#include "src/core/SkUtils.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>
#include <string_view>
#include <utility>

// number of bytes (on the stack) to receive the printf result
static const size_t kBufferSize = 1024;

struct StringBuffer {
    char*  fText;
    int    fLength;
};

template <int SIZE>
static StringBuffer apply_format_string(const char* format, va_list args, char (&stackBuffer)[SIZE],
                                        SkString* heapBuffer) SK_PRINTF_LIKE(1, 0);

template <int SIZE>
static StringBuffer apply_format_string(const char* format, va_list args, char (&stackBuffer)[SIZE],
                                        SkString* heapBuffer) {
    // First, attempt to print directly to the stack buffer.
    va_list argsCopy;
    va_copy(argsCopy, args);
    int outLength = std::vsnprintf(stackBuffer, SIZE, format, args);
    if (outLength < 0) {
        SkDebugf("SkString: vsnprintf reported error.");
        va_end(argsCopy);
        return {stackBuffer, 0};
    }
    if (outLength < SIZE) {
        va_end(argsCopy);
        return {stackBuffer, outLength};
    }

    // Our text was too long to fit on the stack! However, we now know how much space we need to
    // format it. Format the string into our heap buffer. `set` automatically reserves an extra
    // byte at the end of the buffer for a null terminator, so we don't need to add one here.
    heapBuffer->set(nullptr, outLength);
    char* heapBufferDest = heapBuffer->data();
    SkDEBUGCODE(int checkLength =) std::vsnprintf(heapBufferDest, outLength + 1, format, argsCopy);
    SkASSERT(checkLength == outLength);
    va_end(argsCopy);
    return {heapBufferDest, outLength};
}

///////////////////////////////////////////////////////////////////////////////

const SkString::Rec SkString::gEmptyRec(0, 0);

#define SizeOfRec()     (gEmptyRec.data() - (const char*)&gEmptyRec)

static uint32_t trim_size_t_to_u32(size_t value) {
    if (sizeof(size_t) > sizeof(uint32_t)) {
        if (value > UINT32_MAX) {
            value = UINT32_MAX;
        }
    }
    return (uint32_t)value;
}

static size_t check_add32(size_t base, size_t extra) {
    SkASSERT(base <= UINT32_MAX);
    if (sizeof(size_t) > sizeof(uint32_t)) {
        if (base + extra > UINT32_MAX) {
            extra = UINT32_MAX - base;
        }
    }
    return extra;
}

sk_sp<SkString::Rec> SkString::Rec::Make(const char text[], size_t len) {
    if (0 == len) {
        return sk_sp<SkString::Rec>(const_cast<Rec*>(&gEmptyRec));
    }

    SkSafeMath safe;
    // We store a 32bit version of the length
    uint32_t stringLen = safe.castTo<uint32_t>(len);
    // Add SizeOfRec() for our overhead and 1 for null-termination
    size_t allocationSize = safe.add(len, SizeOfRec() + sizeof(char));
    // Align up to a multiple of 4
    allocationSize = safe.alignUp(allocationSize, 4);

    SkASSERT_RELEASE(safe.ok());

    void* storage = ::operator new (allocationSize);
    sk_sp<Rec> rec(new (storage) Rec(stringLen, 1));
    if (text) {
        memcpy(rec->data(), text, len);
    }
    rec->data()[len] = 0;
    return rec;
}

void SkString::Rec::ref() const {
    if (this == &SkString::gEmptyRec) {
        return;
    }
    SkAssertResult(this->fRefCnt.fetch_add(+1, std::memory_order_relaxed));
}

void SkString::Rec::unref() const {
    if (this == &SkString::gEmptyRec) {
        return;
    }
    int32_t oldRefCnt = this->fRefCnt.fetch_add(-1, std::memory_order_acq_rel);
    SkASSERT(oldRefCnt);
    if (1 == oldRefCnt) {
        delete this;
    }
}

bool SkString::Rec::unique() const {
    return fRefCnt.load(std::memory_order_acquire) == 1;
}

#ifdef SK_DEBUG
int32_t SkString::Rec::getRefCnt() const {
    return fRefCnt.load(std::memory_order_relaxed);
}

const SkString& SkString::validate() const {
    // make sure no one has written over our global
    SkASSERT(0 == gEmptyRec.fLength);
    SkASSERT(0 == gEmptyRec.getRefCnt());
    SkASSERT(0 == gEmptyRec.data()[0]);

    if (fRec.get() != &gEmptyRec) {
        SkASSERT(fRec->fLength > 0);
        SkASSERT(fRec->getRefCnt() > 0);
        SkASSERT(0 == fRec->data()[fRec->fLength]);
    }
    return *this;
}

SkString& SkString::validate() {
    const_cast<const SkString*>(this)->validate();
    return *this;
}
#endif

///////////////////////////////////////////////////////////////////////////////

SkString::SkString() : fRec(const_cast<Rec*>(&gEmptyRec)) {
}

SkString::SkString(size_t len) {
    fRec = Rec::Make(nullptr, len);
}

SkString::SkString(const char text[]) {
    size_t  len = text ? strlen(text) : 0;

    fRec = Rec::Make(text, len);
}

SkString::SkString(const char text[], size_t len) {
    fRec = Rec::Make(text, len);
}

SkString::SkString(const SkString& src) : fRec(src.validate().fRec) {}

SkString::SkString(SkString&& src) : fRec(std::move(src.validate().fRec)) {
    src.fRec.reset(const_cast<Rec*>(&gEmptyRec));
}

SkString::~SkString() {
    this->validate();
}

bool SkString::equals(const SkString& src) const {
    return fRec == src.fRec || this->equals(src.c_str(), src.size());
}

bool SkString::equals(const char text[]) const {
    return this->equals(text, text ? strlen(text) : 0);
}

bool SkString::equals(const char text[], size_t len) const {
    SkASSERT(len == 0 || text != nullptr);

    return fRec->fLength == len && !sk_careful_memcmp(fRec->data(), text, len);
}

SkString& SkString::operator=(const SkString& src) {
    this->validate();
    fRec = src.fRec;  // sk_sp<Rec>::operator=(const sk_sp<Ref>&) checks for self-assignment.
    return *this;
}

SkString& SkString::operator=(SkString&& src) {
    this->validate();

    if (fRec != src.fRec) {
        this->swap(src);
    }
    return *this;
}

void SkString::reset() {
    this->validate();
    fRec.reset(const_cast<Rec*>(&gEmptyRec));
}

char* SkString::data() {
    this->validate();

    if (fRec->fLength) {
        if (!fRec->unique()) {
            fRec = Rec::Make(fRec->data(), fRec->fLength);
        }
    }
    return fRec->data();
}

void SkString::set(const char text[]) {
    this->set(text, text ? strlen(text) : 0);
}

void SkString::set(const char text[], size_t len) {
    len = trim_size_t_to_u32(len);
    if (0 == len) {
        this->reset();
    } else if (fRec->unique() && ((len >> 2) <= (fRec->fLength >> 2))) {
        // Use less of the buffer we have without allocating a smaller one.
        char* p = this->data();
        if (text) {
            memcpy(p, text, len);
        }
        p[len] = '\0';
        fRec->fLength = SkToU32(len);
    } else {
        SkString tmp(text, len);
        this->swap(tmp);
    }
}

void SkString::insert(size_t offset, const char text[]) {
    this->insert(offset, text, text ? strlen(text) : 0);
}

void SkString::insert(size_t offset, const char text[], size_t len) {
    if (len) {
        size_t length = fRec->fLength;
        if (offset > length) {
            offset = length;
        }

        // Check if length + len exceeds 32bits, we trim len
        len = check_add32(length, len);
        if (0 == len) {
            return;
        }

        /*  If we're the only owner, and we have room in our allocation for the insert,
            do it in place, rather than allocating a new buffer.

            To know we have room, compare the allocated sizes
            beforeAlloc = SkAlign4(length + 1)
            afterAlloc  = SkAligh4(length + 1 + len)
            but SkAlign4(x) is (x + 3) >> 2 << 2
            which is equivalent for testing to (length + 1 + 3) >> 2 == (length + 1 + 3 + len) >> 2
            and we can then eliminate the +1+3 since that doesn't affec the answer
        */
        if (fRec->unique() && (length >> 2) == ((length + len) >> 2)) {
            char* dst = this->data();

            if (offset < length) {
                memmove(dst + offset + len, dst + offset, length - offset);
            }
            memcpy(dst + offset, text, len);

            dst[length + len] = 0;
            fRec->fLength = SkToU32(length + len);
        } else {
            /*  Seems we should use realloc here, since that is safe if it fails
                (we have the original data), and might be faster than alloc/copy/free.
            */
            SkString    tmp(fRec->fLength + len);
            char*       dst = tmp.data();

            if (offset > 0) {
                memcpy(dst, fRec->data(), offset);
            }
            memcpy(dst + offset, text, len);
            if (offset < fRec->fLength) {
                memcpy(dst + offset + len, fRec->data() + offset,
                       fRec->fLength - offset);
            }

            this->swap(tmp);
        }
    }
}

void SkString::insertUnichar(size_t offset, SkUnichar uni) {
    char    buffer[SkUTF::kMaxBytesInUTF8Sequence];
    size_t  len = SkUTF::ToUTF8(uni, buffer);

    if (len) {
        this->insert(offset, buffer, len);
    }
}

///////////////////////////////////////////////////////////////////////////////

void SkString::printf(const char format[], ...) {
    va_list args;
    va_start(args, format);
    this->printVAList(format, args);
    va_end(args);
}

void SkString::printVAList(const char format[], va_list args) {
    char stackBuffer[kBufferSize];
    StringBuffer result = apply_format_string(format, args, stackBuffer, this);

    if (result.fText == stackBuffer) {
        this->set(result.fText, result.fLength);
    }
}

void SkString::appendf(const char format[], ...) {
    va_list args;
    va_start(args, format);
    this->appendVAList(format, args);
    va_end(args);
}

void SkString::appendVAList(const char format[], va_list args) {
    if (this->isEmpty()) {
        this->printVAList(format, args);
        return;
    }

    SkString overflow;
    char stackBuffer[kBufferSize];
    StringBuffer result = apply_format_string(format, args, stackBuffer, &overflow);

    this->append(result.fText, result.fLength);
}

void SkString::swap(SkString& other) {
    this->validate();
    other.validate();

    using std::swap;
    swap(fRec, other.fRec);
}

///////////////////////////////////////////////////////////////////////////////

SkString SkStringPrintf(const char* format, ...) {
    SkString formattedOutput;
    va_list args;
    va_start(args, format);
    formattedOutput.printVAList(format, args);
    va_end(args);
    return formattedOutput;
}
