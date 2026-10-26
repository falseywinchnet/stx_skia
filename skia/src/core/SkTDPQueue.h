/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkTDPQueue_DEFINED
#define SkTDPQueue_DEFINED

#include "include/private/SkAssert.h"
#include "include/private/SkTDArray.h"

#include <utility>

// Owns a contiguous binary min-heap of raw-copyable values. LESS is a strict weak
// ordering, may only observe its operands, and must not throw or reenter the queue.
// Equal priorities retain the original
// heap's tie choices; insertion order is not a stability guarantee.
// Mutation requires exclusive access. peek borrows until the next mutation or
// destruction. Pointer-valued entries do not own their pointees. Allocation uses
// SkTDArray's abort-on-failure policy. No indexed membership or priority updates.
template <typename T, bool (*LESS)(const T&, const T&)>
class SkTDPQueue {
public:
    SkTDPQueue() {}
    explicit SkTDPQueue(int reserve) { fArray.reserve(reserve); }

    SkTDPQueue(SkTDPQueue&& source) : fArray(std::move(source.fArray)) {}
    SkTDPQueue& operator=(SkTDPQueue&& source) {
        fArray = std::move(source.fArray);
        return *this;
    }
    SkTDPQueue(const SkTDPQueue&) = delete;
    SkTDPQueue& operator=(const SkTDPQueue&) = delete;

    int count() const {
        const int result = fArray.size();
        return result;
    }
    const T& peek() const & { return fArray[0]; }
    const T& peek() const && = delete;

    void insert(T entry) {
        validate();
        int index = fArray.size();
        fArray.append();
        T* elements = fArray.data();
        // Reserve one hole and slide ancestors down. The inserted value stays
        // local until its final position, avoiding a full swap at each level.
        while (index > 0) {
            const int parent = (index - 1) / 2;
            if (!LESS(entry, elements[parent])) break;
            elements[index] = elements[parent];
            index = parent;
        }
        elements[index] = entry;
        validate();
    }

    void pop() {
        validate();
        const int oldCount = fArray.size();
        SkASSERT_RELEASE(oldCount > 0);
        if (oldCount == 1) {
            fArray.pop_back();
            return;
        }
        T* elements = fArray.data();
        const T entry = elements[oldCount - 1];
        fArray.pop_back();
        const int count = oldCount - 1;
        int index = 0;
        // Prove a left child exists before doubling the index. Even a capacity
        // of INT_MAX cannot overflow the child-index arithmetic.
        while (index < count / 2) {
            int child = 2 * index + 1;
            if (child + 1 < count && LESS(elements[child + 1], elements[child])) ++child;
            if (!LESS(elements[child], entry)) break;
            elements[index] = elements[child];
            index = child;
        }
        elements[index] = entry;
        validate();
    }

private:
    void validate() const {
#ifdef SK_DEBUG
        const int count = fArray.size();
        const T* elements = fArray.data();
        for (int index = 1; index < count; ++index) {
            const int parent = (index - 1) / 2;
            SkASSERT(!LESS(elements[index], elements[parent]));
        }
#endif
    }
    SkTDArray<T> fArray{};
};

#endif
