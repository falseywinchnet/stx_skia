#include "src/core/SkArenaAlloc.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
struct Tracked {
    inline static int live = 0;
    inline static int destroyed = 0;
    int value = 31;
    Tracked() { ++live; }
    explicit Tracked(int initial) : value(initial) { ++live; }
    Tracked(const Tracked& source) : value(source.value) { ++live; }
    ~Tracked() {
        --live;
        ++destroyed;
    }
};

bool heap_only_case() {
    Tracked::live = 0;
    Tracked::destroyed = 0;
    {
        SkArenaAlloc arena(64);
        uint32_t* number = arena.make<uint32_t>(73);
        Tracked* first = arena.make<Tracked>(41);
        Tracked* array = arena.makeArray<Tracked>(5);
        if (*number != 73 || (*first).value != 41 || Tracked::live != 6) return false;
        for (int index = 0; index < 5; ++index) {
            if (array[index].value != 31) return false;
        }
        void* aligned = arena.makeBytesAlignedTo(23, 64);
        const uintptr_t address = reinterpret_cast<uintptr_t>(aligned);
        if ((address & 63U) != 0) return false;
    }
    return Tracked::live == 0 && Tracked::destroyed == 6;
}

bool inline_case() {
    Tracked::live = 0;
    Tracked::destroyed = 0;
    {
        SkSTArenaAlloc<256> arena;
        Tracked* first = arena.make<Tracked>(17);
        Tracked* array = arena.makeArrayDefault<Tracked>(3);
        if ((*first).value != 17 || Tracked::live != 4) return false;
        for (int index = 0; index < 3; ++index) {
            if (array[index].value != 31) return false;
        }
    }
    return Tracked::live == 0 && Tracked::destroyed == 4;
}

bool reset_case() {
    Tracked::live = 0;
    Tracked::destroyed = 0;
    SkArenaAllocWithReset arena(nullptr, 0, 64);
    if (!arena.isEmpty()) return false;
    Tracked* first = arena.make<Tracked>(19);
    if ((*first).value != 19 || arena.isEmpty() || Tracked::live != 1) return false;
    arena.reset();
    if (!arena.isEmpty() || Tracked::live != 0 || Tracked::destroyed != 1) return false;
    Tracked* second = arena.make<Tracked>(29);
    if ((*second).value != 29 || arena.isEmpty() || Tracked::live != 1) return false;
    arena.reset();
    if (!arena.isEmpty() || Tracked::live != 0 || Tracked::destroyed != 2) return false;
    {
        SkSTArenaAllocWithReset<128> inlineArena;
        if (!inlineArena.isEmpty()) return false;
        Tracked* item = inlineArena.make<Tracked>(37);
        if ((*item).value != 37 || inlineArena.isEmpty()) return false;
        inlineArena.reset();
        if (!inlineArena.isEmpty() || Tracked::live != 0) return false;
    }
    return Tracked::destroyed == 3;
}
}

int main(int argc, char** argv) {
#ifndef STX_SKIA_REFERENCE
    if (argc == 2 && std::strcmp(argv[1], "--invalid-alignment") == 0) {
        SkArenaAlloc arena(64);
        arena.makeBytesAlignedTo(8, 3);
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--oversize-initial-block") == 0) {
        const size_t oversized = static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 1;
        SkArenaAlloc arena(nullptr, oversized, 64);
        return 0;
    }
#endif
    const bool valid = heap_only_case() && inline_case() && reset_case();
    if (!valid) {
        std::fputs("arena allocation contract mismatch\n", stderr);
        return 1;
    }
    std::puts("arena allocation: heap, inline, reset PASS");
    return 0;
}
