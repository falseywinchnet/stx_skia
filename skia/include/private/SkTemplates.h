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
#include "include/private/SkAutoSTArray.h"
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
