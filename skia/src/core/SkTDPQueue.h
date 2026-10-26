/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkTDPQueue_DEFINED
#define SkTDPQueue_DEFINED

#include "include/private/SkAssert.h"
#include "include/private/SkDebug.h"
#include "include/private/SkTDArray.h"
#include "include/private/SkTo.h"

#include <utility>

/**
 * This class implements a priority queue. T is the type of the elements in the queue. LESS is a
 * function that compares two Ts and returns true if the first is higher priority than the second.
 *
 */
template <typename T,
          bool (*LESS)(const T&, const T&)>
class SkTDPQueue {
public:
    SkTDPQueue() {}
    SkTDPQueue(int reserve) { fArray.reserve(reserve); }

    SkTDPQueue(SkTDPQueue&&) = default;
    SkTDPQueue& operator =(SkTDPQueue&&) = default;

    SkTDPQueue(const SkTDPQueue&) = delete;
    SkTDPQueue& operator=(const SkTDPQueue&) = delete;

    /** Number of items in the queue. */
    int count() const { return fArray.size(); }

    /** Gets the next item in the queue without popping it. */
    const T& peek() const { return fArray[0]; }
    T& peek() { return fArray[0]; }

    /** Removes the next item. */
    void pop() {
        this->validate();
        if (1 == fArray.size()) {
            fArray.pop_back();
            return;
        }

        fArray[0] = fArray[fArray.size() - 1];

        fArray.pop_back();
        this->percolateDownIfNecessary(0);

        this->validate();
    }

    /** Inserts a new item in the queue based on its priority. */
    void insert(T entry) {
        this->validate();
        int index = fArray.size();
        *fArray.append() = entry;

        this->percolateUpIfNecessary(index);
        this->validate();
    }


private:
    static int LeftOf(int x) { SkASSERT(x >= 0); return 2 * x + 1; }
    static int ParentOf(int x) { SkASSERT(x > 0); return (x - 1) >> 1; }

    bool percolateUpIfNecessary(int index) {
        SkASSERT(index >= 0);
        bool percolated = false;
        do {
            if (0 == index) {

                return percolated;
            }
            int p = ParentOf(index);
            if (LESS(fArray[index], fArray[p])) {
                using std::swap;
                swap(fArray[index], fArray[p]);

                index = p;
                percolated = true;
            } else {

                return percolated;
            }
            this->validate(index);
        } while (true);
    }

    void percolateDownIfNecessary(int index) {
        SkASSERT(index >= 0);
        do {
            int child = LeftOf(index);

            if (child >= fArray.size()) {
                // We're a leaf.

                return;
            }

            if (child + 1 >= fArray.size()) {
                // We only have a left child.
                if (LESS(fArray[child], fArray[index])) {
                    using std::swap;
                    swap(fArray[child], fArray[index]);


                    return;
                }
            } else if (LESS(fArray[child + 1], fArray[child])) {
                // The right child is the one we should swap with, if we swap.
                child++;
            }

            // Check if we need to swap.
            if (LESS(fArray[child], fArray[index])) {
                using std::swap;
                swap(fArray[child], fArray[index]);

                index = child;
            } else {
                // We're less than both our children.

                return;
            }
            this->validate(index);
        } while (true);
    }

    void validate(int excludedIndex = -1) const {
#ifdef SK_DEBUG
        for (int i = 1; i < fArray.size(); ++i) {
            int p = ParentOf(i);
            if (excludedIndex != p && excludedIndex != i) {
                SkASSERT(!(LESS(fArray[i], fArray[p])));
            }
        }
#endif
    }

    SkTDArray<T> fArray;
};

#endif
