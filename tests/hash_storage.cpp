#include "src/core/SkTHash.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <utility>

namespace {
struct Entry {
    int key = 0;
    int value = 0;
    static int GetKey(const Entry& entry) { return entry.key; }
    static uint32_t Hash(int key) { return static_cast<uint32_t>(key); }
};
struct CollidingEntry : Entry {
    static uint32_t Hash(int) { return 3; }
};

template <typename Item>
bool full_tables() {
    for (const int capacity : std::array<int, 5>{1, 2, 4, 8, 16}) {
        for (int victim = 0; victim < capacity; ++victim) {
            skia_private::THashTable<Item, int> table{};
            for (int key = 0; key < capacity; ++key) {
                Item entry{};
                entry.key = key;
                entry.value = key * 7;
                table.set(entry);
            }
            table.resize(capacity);
            if (table.count() != capacity || table.capacity() != capacity) return false;
            if (!table.removeIfExists(victim) || table.find(victim) != nullptr) return false;
            for (int key = 0; key < capacity; ++key) {
                if (key == victim) continue;
                const Item* found = table.find(key);
                if (found == nullptr || (*found).value != key * 7) return false;
            }
            for (int key = capacity - 1; key >= 0; --key) {
                if (key == victim) continue;
                if (!table.removeIfExists(key)) return false;
            }
            if (table.count() != 0 || table.find(100) != nullptr) return false;
        }
    }
    return true;
}

bool ordinary() {
    skia_private::THashTable<Entry, int> table{};
    for (int key = 0; key < 100; ++key) table.set(Entry{key, key * 3});
    for (int key = 0; key < 100; key += 2) table.remove(key);
    for (int key = 0; key < 100; ++key) {
        const Entry* found = table.find(key);
        if (key % 2 == 0) {
            if (found != nullptr) return false;
        } else if (found == nullptr || (*found).value != key * 3) return false;
    }
    return true;
}
}
int main() {
    if (!ordinary()) return 1;
#ifndef STX_SKIA_REFERENCE
    if (!full_tables<Entry>() || !full_tables<CollidingEntry>()) return 2;
#endif
    std::puts("hash storage: ordinary lookup/deletion and admitted full-table boundaries PASS");
    return 0;
}
