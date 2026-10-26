#include "include/core/SkData.h"
#include "include/core/SkStream.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
void count_release(const void*, void* context) {
    unsigned int& count = *static_cast<unsigned int*>(context);
    ++count;
}

bool subsets() {
    std::array<std::uint8_t, 33> input{};
    for (std::size_t i = 0; i < input.size(); ++i) input[i] = static_cast<std::uint8_t>(i * 7U);
    sk_sp<SkData> data = SkData::MakeWithCopy(input.data(), input.size());
    if (!data) return false;
    for (std::size_t offset = 0; offset <= 34; ++offset) {
        for (std::size_t length = 0; length <= 34; ++length) {
            const bool valid = offset <= input.size() && length <= input.size() - offset;
            sk_sp<SkData> shared = (*data).shareSubset(offset, length);
            sk_sp<SkData> copied = (*data).copySubset(offset, length);
            if (static_cast<bool>(shared) != valid || static_cast<bool>(copied) != valid) return false;
            if (valid) {
                if ((*shared).size() != length || (*copied).size() != length) return false;
                if (!((*shared) == (*copied))) return false;
                if (length != 0 && std::memcmp((*shared).data(), input.data() + offset, length) != 0) return false;
            }
            std::array<std::uint8_t, 40> output{};
            const std::size_t count = (*data).copyRange(offset, length, output.data());
            const std::size_t available = offset < input.size() ? input.size() - offset : 0;
            const std::size_t expected = length < available ? length : available;
            if (count != expected || (*data).copyRange(offset, length, nullptr) != expected) return false;
            if (count != 0 && std::memcmp(output.data(), input.data() + offset, count) != 0) return false;
        }
    }
    if ((*data).shareSubset(std::numeric_limits<std::size_t>::max(), 1)) return false;
    if ((*data).copySubset(1, std::numeric_limits<std::size_t>::max())) return false;
    const SkData& constant = *data;
    sk_sp<const SkData> const_subset = constant.shareSubset(1, 7);
    return const_subset && (*const_subset).size() == 7;
}

bool ownership_and_streams() {
    unsigned int releases = 0;
    const std::array<std::uint8_t, 8> input = {1, 3, 5, 7, 9, 11, 13, 15};
    sk_sp<SkData> data = SkData::MakeWithProc(input.data(), input.size(), count_release, &releases);
    sk_sp<SkData> subset = (*data).shareSubset(2, 4);
    data.reset();
    if (releases != 0 || (*subset).bytes()[0] != 5) return false;
    subset.reset();
    if (releases != 1) return false;
    SkMemoryStream stream(input.data(), input.size(), false);
    data = SkData::MakeFromStream(&stream, input.size());
    if (!data || std::memcmp((*data).data(), input.data(), input.size()) != 0) return false;
    if (SkData::MakeFromStream(&stream, 1)) return false;
    data = SkData::MakeZeroInitialized(31);
    for (std::size_t i = 0; i < (*data).size(); ++i) {
        if ((*data).bytes()[i] != 0) return false;
    }
    data = SkData::MakeWithCString(nullptr);
    if ((*data).size() != 1 || (*data).bytes()[0] != 0) return false;
    data = SkData::MakeEmpty();
    subset = (*data).copySubset(0, 0);
    return subset && (*subset).size() == 0;
}
}

int main() {
    if (!subsets() || !ownership_and_streams()) return 1;
    std::puts("data subsets, ranges, ownership and streams: PASS");
    return 0;
}
