#include "include/private/SkTArray.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

using skia_private::STArray;
using skia_private::TArray;

namespace {
struct ConstructionFailure {};
struct TrivialNonCopy {
    uint32_t value = 0;
    TrivialNonCopy() = default;
    TrivialNonCopy(const TrivialNonCopy&) = delete;
};
static_assert(std::is_trivially_copyable_v<TrivialNonCopy>);

bool trivial_copy_without_constructor() {
    std::array<TrivialNonCopy, 9> input{};
    for (size_t index = 0; index < input.size(); ++index) input[index].value = static_cast<uint32_t>(index + 21);
    TArray<TrivialNonCopy> owner(input.data(), static_cast<int>(input.size()));
    STArray<4, TrivialNonCopy> inlineOwner(input.data(), static_cast<int>(input.size()));
    for (int index = 0; index < owner.size(); ++index) {
        if (owner[index].value != input[index].value || inlineOwner[index].value != input[index].value) return false;
    }
    return true;
}
#ifndef STX_SKIA_REFERENCE
bool empty_end_contract() {
    TArray<uint32_t> empty;
    const TArray<uint32_t>& readOnly = empty;
    if (empty.end() != nullptr || readOnly.end() != nullptr) return false;
    if (empty.push_back_n(0) != nullptr) return false;
    if (empty.push_back_n(0, uint32_t{7}) != nullptr) return false;
    if (empty.size() != 0) return false;
    return true;
}
#endif
struct Element {
    inline static int live = 0;
    inline static int attempts = 0;
    inline static int failAt = 0;
    int value = 17;
    Element() {
        ++attempts;
        if (attempts == failAt) throw ConstructionFailure{};
        ++live;
    }
    Element(const Element& source) : value(source.value) {
        ++attempts;
        if (attempts == failAt) throw ConstructionFailure{};
        ++live;
    }
    Element(Element&& source) noexcept : value(source.value) { ++live; }
    ~Element() { --live; }
};

template <typename Array> bool successful_construction(size_t& total) {
    for (int count = 0; count <= 96; ++count) {
        std::array<uint32_t, 96> input{};
        for (int index = 0; index < count; ++index) input[index] = static_cast<uint32_t>(index * 19 + count);
        Array owner(input.data(), count);
        Array copied(owner);
        Array spanCopy(SkSpan<const uint32_t>(input.data(), count));
        if (owner.size() != count || copied.size() != count || spanCopy.size() != count) return false;
        for (int index = 0; index < count; ++index) {
            if (owner[index] != input[index] || copied[index] != input[index] || spanCopy[index] != input[index]) return false;
        }
        Array assigned;
        assigned = copied;
        assigned.reset(SkSpan<const uint32_t>(input.data(), count));
        for (int index = 0; index < count; ++index) {
            if (assigned[index] != input[index]) return false;
        }
        assigned.reset(count);
        for (int index = 0; index < count; ++index) assigned[index] = static_cast<uint32_t>(index + 9);
        uint32_t* unchangedEnd = assigned.end();
        if (assigned.push_back_n(0) != unchangedEnd) return false;
        uint32_t* appended = assigned.push_back_n(count);
        for (int index = 0; index < count; ++index) appended[index] = static_cast<uint32_t>(index + 33);
        if (assigned.size() != count * 2) return false;
        for (int index = 0; index < count; ++index) {
            if (assigned[index] != static_cast<uint32_t>(index + 9)) return false;
            if (assigned[index + count] != static_cast<uint32_t>(index + 33)) return false;
        }
        assigned.resize(static_cast<size_t>(count));
        if (assigned.size() != count) return false;
        for (int index = 0; index < count; ++index) {
            if (assigned[index] != static_cast<uint32_t>(index + 9)) return false;
        }
        Array listed{2, 3, 5, 7};
        if (listed.size() != 4 || listed[2] != 5) return false;
        ++total;
    }
    return true;
}

bool successful_objects(size_t& total) {
    for (int count = 0; count <= 32; ++count) {
        {
            TArray<Element> source;
            source.reset(count);
            for (int index = 0; index < count; ++index) source[index].value = index + 23;
            STArray<4, Element> copied(source.data(), count);
            STArray<4, Element> assigned;
            assigned = copied;
            if (Element::live != count * 3) return false;
            for (int index = 0; index < count; ++index) {
                if (copied[index].value != index + 23 || assigned[index].value != index + 23) return false;
            }
            copied.reset(3);
            if (Element::live != count * 2 + 3) return false;
            assigned.push_back();
            if (assigned.back().value != 17) return false;
        }
        if (Element::live != 0) return false;
        ++total;
    }
    return true;
}

#ifndef STX_SKIA_REFERENCE
// The original publishes live counts before construction; do not ask it to
// clean up failed construction. These cases exercise only the corrected paths.
template <typename Array> bool failed_construction(size_t& total) {
    for (const int count : {3, 19}) {
        for (int failure = 1; failure <= count; ++failure) {
            TArray<Element> source;
            source.reset(count);
            const int sourceLive = Element::live;
            Element::failAt = Element::attempts + failure;
            bool caught = false;
            try {
                Array copy(source.data(), count);
            } catch (const ConstructionFailure&) {
                caught = true;
            }
            Element::failAt = 0;
            if (!caught || Element::live != sourceLive) return false;
            Array target;
            target.reset(2);
            Element::failAt = Element::attempts + failure;
            caught = false;
            try {
                target.reset(count);
            } catch (const ConstructionFailure&) {
                caught = true;
            }
            Element::failAt = 0;
            if (!caught || !target.empty() || Element::live != sourceLive) return false;
            target.reset(2);
            Element::failAt = Element::attempts + failure;
            caught = false;
            try {
                target.reset(SkSpan<const Element>(source.data(), count));
            } catch (const ConstructionFailure&) {
                caught = true;
            }
            Element::failAt = 0;
            if (!caught || !target.empty() || Element::live != sourceLive) return false;
            target.reset(2);
            const int beforeAppend = Element::live;
            Element::failAt = Element::attempts + failure;
            caught = false;
            try {
                target.push_back_n(count);
            } catch (const ConstructionFailure&) {
                caught = true;
            }
            Element::failAt = 0;
            if (!caught || target.size() != 2 || Element::live != beforeAppend) return false;
            if (target[0].value != 17 || target[1].value != 17) return false;
            target.clear();
            Array copiedSource(source.data(), count);
            Element::failAt = Element::attempts + failure;
            caught = false;
            try {
                target = copiedSource;
            } catch (const ConstructionFailure&) {
                caught = true;
            }
            Element::failAt = 0;
            if (!caught || !target.empty() || Element::live != sourceLive + count) return false;
            target.reset(1);
            Element::failAt = Element::attempts + 1;
            caught = false;
            try {
                target.push_back();
            } catch (const ConstructionFailure&) {
                caught = true;
            }
            Element::failAt = 0;
            if (!caught || target.size() != 1) return false;
            total += 6;
        }
    }
    return true;
}

template <typename Array> bool alias_reset(size_t& total) {
    for (int count = 1; count <= 32; ++count) {
        for (int offset = 0; offset < count; ++offset) {
            Array owner;
            owner.reset(count);
            for (int index = 0; index < count; ++index) owner[index].value = index + 51;
            const int retained = count - offset;
            owner.reset(SkSpan<const Element>(owner.data() + offset, retained));
            if (owner.size() != retained) return false;
            for (int index = 0; index < retained; ++index) {
                if (owner[index].value != index + offset + 51) return false;
            }
            ++total;
        }
    }
    return true;
}
template <typename Array> bool trivial_alias_reset(size_t& total) {
    for (int count = 1; count <= 32; ++count) {
        for (int offset = 0; offset < count; ++offset) {
            Array owner;
            owner.reset(count);
            for (int index = 0; index < count; ++index) owner[index] = static_cast<uint32_t>(index + 51);
            const int retained = count - offset;
            uint32_t* originalStorage = owner.data();
            owner.reset(SkSpan<const uint32_t>(owner.data() + offset, retained));
            if (owner.data() != originalStorage || owner.size() != retained) return false;
            for (int index = 0; index < retained; ++index) {
                if (owner[index] != static_cast<uint32_t>(index + offset + 51)) return false;
            }
            ++total;
        }
    }
    return true;
}

#endif

uint64_t benchmark_sink = 0;
void benchmark() {
    for (const int count : {16, 256, 4096}) {
        TArray<uint32_t> input;
        input.reset(count);
        for (int index = 0; index < count; ++index) input[index] = static_cast<uint32_t>(index + 1);
        TArray<uint32_t> target(count);
        const int iterations = 64000000 / count;
        for (int mode = 0; mode < 2; ++mode) {
            const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            uint64_t checksum = 0;
            for (int iteration = 0; iteration < iterations; ++iteration) {
                input[0] = static_cast<uint32_t>(iteration);
                if (mode == 0) {
                    target = input;
                } else {
                    target.reset(SkSpan<const uint32_t>(input.data(), count));
                }
                checksum += target[iteration % count];
            }
            const std::chrono::steady_clock::time_point finish = std::chrono::steady_clock::now();
            const std::chrono::duration<double> elapsed = finish - start;
            benchmark_sink += checksum;
            std::printf("%d,%d,%d,%.9f,%llu\n", count, mode, iterations, elapsed.count(),
                        static_cast<unsigned long long>(checksum));
        }
    }
}
}

int main(int argc, char** argv) {
    size_t ordinary = 0;
    bool valid = trivial_copy_without_constructor();
#ifndef STX_SKIA_REFERENCE
    valid = empty_end_contract() && valid;
#endif
    valid = successful_construction<TArray<uint32_t>>(ordinary) && valid;
    valid = successful_construction<STArray<1, uint32_t>>(ordinary) && valid;
    valid = successful_construction<STArray<4, uint32_t>>(ordinary) && valid;
    valid = successful_construction<STArray<64, uint32_t>>(ordinary) && valid;
    valid = successful_objects(ordinary) && valid;
#ifndef STX_SKIA_REFERENCE
    size_t failures = 0;
    size_t aliases = 0;
    valid = failed_construction<TArray<Element>>(failures) && valid;
    valid = failed_construction<STArray<4, Element>>(failures) && valid;
    valid = alias_reset<TArray<Element>>(aliases) && valid;
    valid = alias_reset<STArray<4, Element>>(aliases) && valid;
    valid = trivial_alias_reset<TArray<uint32_t>>(aliases) && valid;
    valid = trivial_alias_reset<STArray<4, uint32_t>>(aliases) && valid;
    if (valid) std::printf("candidate: %zu construction failures, %zu alias resets PASS\n", failures, aliases);
#endif
    if (!valid || Element::live != 0) {
        std::fputs("TArray construction: contract mismatch\n", stderr);
        return 1;
    }
    std::printf("TArray construction: %zu ordinary schedules PASS; heap=%zu inline=%zu bytes\n",
                ordinary, sizeof(TArray<uint32_t>), sizeof(STArray<4, uint32_t>));
    if (argc == 2 && std::strcmp(argv[1], "--benchmark") == 0) benchmark();
    return 0;
}
