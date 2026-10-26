/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "include/core/SkFlattenable.h"

#include "include/core/SkData.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkSerialProcs.h"
#include "include/core/SkTypes.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkWriteBuffer.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <utility>

namespace {

struct Entry {
    const char*             fName;
    SkFlattenable::Factory  fFactory;
};

struct EntryComparator {
    bool operator()(const Entry& a, const Entry& b) const {
        return strcmp(a.fName, b.fName) < 0;
    }
    bool operator()(const Entry& a, const char* b) const {
        return strcmp(a.fName, b) < 0;
    }
    bool operator()(const char* a, const Entry& b) const {
        return strcmp(a, b.fName) < 0;
    }
};

int gCount = 0;
Entry gEntries[128];

}  // namespace

void SkFlattenable::Finalize() {
    std::sort(gEntries, gEntries + gCount, EntryComparator());
}

void SkFlattenable::Register(const char name[], Factory factory) {
    SkASSERT(name);
    SkASSERT(factory);
    SkASSERT(gCount < (int)std::size(gEntries));

    gEntries[gCount].fName = name;
    gEntries[gCount].fFactory = factory;
    gCount += 1;
}

SkFlattenable::Factory SkFlattenable::NameToFactory(const char name[]) {
    RegisterFlattenablesIfNeeded();

    SkASSERT(std::is_sorted(gEntries, gEntries + gCount, EntryComparator()));
    auto pair = std::equal_range(gEntries, gEntries + gCount, name, EntryComparator());
    if (pair.first == pair.second) {
        return nullptr;
    }
    return pair.first->fFactory;
}

const char* SkFlattenable::FactoryToName(Factory fact) {
    RegisterFlattenablesIfNeeded();

    const Entry* entries = gEntries;
    for (int i = gCount - 1; i >= 0; --i) {
        if (entries[i].fFactory == fact) {
            return entries[i].fName;
        }
    }
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkData> SkFlattenable::serialize(const SkSerialProcs* procs) const {
    SkSerialProcs p;
    if (procs) {
        p = *procs;
    }
    SkBinaryWriteBuffer writer(p);

    writer.writeFlattenable(this);
    size_t size = writer.bytesWritten();
    auto data = SkData::MakeUninitialized(size);
    writer.writeToMemory(data->writable_data());
    return data;
}

size_t SkFlattenable::serialize(void* memory, size_t memory_size,
                                const SkSerialProcs* procs) const {
    SkSerialProcs p;
    if (procs) {
        p = *procs;
    }
    SkBinaryWriteBuffer writer(memory, memory_size, p);
    writer.writeFlattenable(this);
    return writer.usingInitialStorage() ? writer.bytesWritten() : 0u;
}

sk_sp<SkFlattenable> SkFlattenable::Deserialize(SkFlattenable::Type type, const void* data,
                                                size_t size, const SkDeserialProcs* procs) {
    SkReadBuffer buffer(data, size);
    if (procs) {
        buffer.setDeserialProcs(*procs);
    }
    return sk_sp<SkFlattenable>(buffer.readFlattenable(type));
}
