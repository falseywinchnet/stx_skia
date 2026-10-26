/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkTemplates_DEFINED
#define SkTemplates_DEFINED

#include "include/private/SkAlign.h"
#include "include/private/SkAutoTMalloc.h"
#include "include/private/SkAutoTArray.h"
#include "include/private/SkAutoSTMalloc.h"
#include "include/private/SkAssert.h"
#include "include/private/SkMalloc.h"
#include "include/private/SkTLogic.h"
#include "include/private/SkTo.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>


/** \file SkTemplates.h

    This file contains light-weight template classes for type-safe and exception-safe
    resource management.
*/

/**
 *  Marks a local variable as known to be unused (to avoid warnings).
 *  Note that this does *not* prevent the local variable from being optimized away.
 */
template<typename T> inline void sk_ignore_unused_variable(const T&) { }

/**
 * This is a general purpose absolute-value function.
 * See SkAbs32 in (SkSafe32.h) for a 32-bit int specific version that asserts.
 */
template <typename T> static inline T SkTAbs(T value) {
    if (value < 0) {
        value = -value;
    }
    return value;
}

/**
 *  Returns a pointer to a D which comes immediately after S[count].
 */
template <typename D, typename S> inline D* SkTAfter(S* ptr, size_t count = 1) {
    return reinterpret_cast<D*>(ptr + count);
}

/**
 *  Returns a pointer to a D which comes byteOffset bytes after S.
 */
template <typename D, typename S> inline D* SkTAddOffset(S* ptr, ptrdiff_t byteOffset) {
    // The intermediate char* has the same cv-ness as D as this produces better error messages.
    // This relies on the fact that reinterpret_cast can add constness, but cannot remove it.
    return reinterpret_cast<D*>(reinterpret_cast<sknonstd::same_cv_t<char, D>*>(ptr) + byteOffset);
}

template <typename T, T* P> struct SkOverloadedFunctionObject {
    template <typename... Args>
    auto operator()(Args&&... args) const -> decltype(P(std::forward<Args>(args)...)) {
        return P(std::forward<Args>(args)...);
    }
};

template <auto F> using SkFunctionObject =
    SkOverloadedFunctionObject<std::remove_pointer_t<decltype(F)>, F>;

/** \class SkAutoTCallVProc

    Call a function when this goes out of scope. The template uses two
    parameters, the object, and a function that is to be called in the destructor.
    If release() is called, the object reference is set to null. If the object
    reference is null when the destructor is called, we do not call the
    function.
*/
template <typename T, void (*P)(T*)> class [[nodiscard]] SkAutoTCallVProc
    : public std::unique_ptr<T, SkFunctionObject<P>> {
    using inherited = std::unique_ptr<T, SkFunctionObject<P>>;
public:
    using inherited::inherited;
    SkAutoTCallVProc(const SkAutoTCallVProc&) = delete;
    SkAutoTCallVProc(SkAutoTCallVProc&& that) : inherited(std::move(that)) {}

    operator T*() const { return this->get(); }
};


namespace skia_private {
/** Like AutoTArray with storage for some number of elements "nested within". The requested number
 *  of elements to fit in the storage is specified by kCountRequested. kCount is the actual number
 *  of elements that will fit in the storage. If the runtime number of elements exceeds the space of
 *  the storage, the elements will live on the heap.
 */
template <int kCountRequested, typename T> class AutoSTArray {
public:
    AutoSTArray(const AutoSTArray&) = delete;
    AutoSTArray& operator=(const AutoSTArray&) = delete;

    AutoSTArray(AutoSTArray&& that) {
        if (that.fArray == nullptr) {
            fArray = nullptr;
            fCount = 0;
        } else if (that.fArray == (T*) that.fStorage) {
            fArray = (T*) fStorage;
            fCount = that.fCount;
            std::uninitialized_move(that.fArray, that.fArray + that.fCount, fArray);
        } else {
            fArray = std::exchange(that.fArray, nullptr);
            fCount = std::exchange(that.fCount, 0);
        }
    }
    AutoSTArray& operator=(AutoSTArray&&) = delete;

    /** Initialize with no objects */
    AutoSTArray() {
        fArray = nullptr;
        fCount = 0;
    }

    /** Allocate count number of T elements */
    AutoSTArray(int count) {
        fArray = nullptr;
        fCount = 0;
        this->reset(count);
    }

    ~AutoSTArray() {
        this->reset(0);
    }

    /** Destroys previous objects in the array and default constructs count number of objects */
    [[clang::reinitializes]]
    void reset(int count) {
        T* start = begin();
        T* iter = end();
        while (iter > start) {
            (--iter)->~T();
        }

        SkASSERT(count >= 0);
        if (fCount != count) {
            if (fArray != (T*) fStorage) {
                sk_free(fArray);
            }

            if (count > kCount) {
                fArray = (T*) sk_malloc_throw(count, sizeof(T));
            } else if (count > 0) {
                fArray = (T*) fStorage;
            } else {
                fArray = nullptr;
            }

            fCount = count;
        }

        iter = begin();
        T* stop = end();
        while (iter < stop) {
            new (iter++) T;
        }
    }

    /* Removes elements with index >= count */
    void trimTo(int count) {
        SkASSERT(count >= 0);
        if (count >= fCount) {
            return;
        }
        T* start = begin() + count;
        T* iter = end();
        while (iter > start) {
            (--iter)->~T();
        }
        fCount = count;
    }

    /** Return the number of T elements in the array */
    int count() const { return fCount; }

    /** Return the array of T elements. Will be nullptr if count == 0 */
    T* get() const { return fArray; }

    T* begin() { return fArray; }

    const T* begin() const { return fArray; }

    T* end() { return fArray + fCount; }

    const T* end() const { return fArray + fCount; }

    /** Return the nth element in the array */
    T&  operator[](int index) const {
        return fArray[sk_collection_check_bounds(index, fCount)];
    }

    /** Aliases matching other types, like std::vector. */
    const T* data() const { return fArray; }
    T* data() { return fArray; }
    size_t size() const { return fCount; }

private:
#if defined(SK_BUILD_FOR_GOOGLE3)
    // Stack frame size is limited for SK_BUILD_FOR_GOOGLE3. 4k is less than the actual max,
    // but some functions have multiple large stack allocations.
    static constexpr int kMaxBytes = 4 * 1024;
    static constexpr int kMinCount = kCountRequested * sizeof(T) > kMaxBytes
        ? kMaxBytes / sizeof(T)
        : kCountRequested;
#else
    static constexpr int kMinCount = kCountRequested;
#endif

    // Because we are also storing an int, there is a tiny bit of padding that
    // the C++ compiler adds after fStorage if sizeof(T) <= alignof(T*).
    // Thus, we can expand how many elements are stored on the stack to make use of this
    // (e.g. 1 extra element for 4 byte T if kCountRequested was even).
    static_assert(alignof(int) <= alignof(T*) || alignof(int) <= alignof(T));
public:
    static constexpr int kCount =
            SkAlignTo(kMinCount*sizeof(T) + sizeof(int), std::max(alignof(T*), alignof(T))) / sizeof(T);

private:
    T* fArray;
    alignas(T) std::byte fStorage[kCount * sizeof(T)];
    int fCount;
};

using UniqueVoidPtr = std::unique_ptr<void, SkOverloadedFunctionObject<void(void*), sk_free>>;

}  // namespace skia_private

template<typename C, std::size_t... Is>
constexpr auto SkMakeArrayFromIndexSequence(C c, std::index_sequence<Is...> is)
-> std::array<decltype(c(std::declval<typename decltype(is)::value_type>())), sizeof...(Is)> {
    return {{ c(Is)... }};
}

template<size_t N, typename C> constexpr auto SkMakeArray(C c)
-> std::array<decltype(c(std::declval<typename std::index_sequence<N>::value_type>())), N> {
    return SkMakeArrayFromIndexSequence(c, std::make_index_sequence<N>{});
}

#endif
