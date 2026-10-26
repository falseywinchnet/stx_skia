#include "include/core/SkData.h"
#include "include/core/SkStream.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>

namespace {
#ifndef STX_SKIA_REFERENCE
class ChunkedStream final : public SkStream {
public:
    ChunkedStream(const std::uint8_t* bytes, std::size_t length, std::size_t chunk,
                  bool knownLength = true, bool invalidPosition = false)
        : fBytes(bytes), fLength(length), fChunk(chunk), fKnownLength(knownLength),
          fInvalidPosition(invalidPosition) {}

    std::size_t read(void* buffer, std::size_t size) override {
        std::size_t count = size;
        if (count > fChunk) count = fChunk;
        const std::size_t remaining = fLength - fPosition;
        if (count > remaining) count = remaining;
        if (buffer != nullptr && count != 0) {
            std::memcpy(buffer, fBytes + fPosition, count);
        }
        fPosition += count;
        return count;
    }

    bool isAtEnd() const override { return fPosition == fLength; }
    bool hasLength() const override { return fKnownLength; }
    std::size_t getLength() const override { return fLength; }
    bool hasPosition() const override { return fKnownLength; }
    std::size_t getPosition() const override {
        if (fInvalidPosition) return fLength + 1;
        return fPosition;
    }

private:
    const std::uint8_t* fBytes;
    std::size_t fLength;
    std::size_t fChunk;
    bool fKnownLength;
    bool fInvalidPosition;
    std::size_t fPosition = 0;
};
#endif

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
#ifndef STX_SKIA_REFERENCE
    ChunkedStream chunked(input.data(), input.size(), 1);
    data = SkData::MakeFromStream(&chunked, input.size());
    if (!data || std::memcmp((*data).data(), input.data(), input.size()) != 0) return false;
    ChunkedStream shortInput(input.data(), 3, 1, false);
    if (SkData::MakeFromStream(&shortInput, input.size())) return false;
    ChunkedStream invalidPosition(input.data(), input.size(), 1, true, true);
    if (SkData::MakeFromStream(&invalidPosition, 1)) return false;
    if (SkData::MakeFromStream(nullptr, 0)) return false;
#endif
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

bool memory_stream_move() {
    SkMemoryStream empty;
    std::uint8_t untouched = 0x5a;
    if (empty.read(&untouched, 1) != 0 || untouched != 0x5a || empty.getAtPos() != nullptr) {
        return false;
    }
    const std::array<std::uint8_t, 6> bytes = {1, 2, 3, 4, 5, 6};
    SkMemoryStream stream(bytes.data(), bytes.size(), false);
    if (!stream.seek(4) || !stream.move(-2) || stream.getPosition() != 2) return false;
    if (!stream.move(-1) || stream.getPosition() != 1) return false;
    if (!stream.move(-std::numeric_limits<long>::max()) || stream.getPosition() != 0) return false;
    if (!stream.move(std::numeric_limits<long>::min()) || stream.getPosition() != 0) return false;
    if (!stream.move(std::numeric_limits<long>::max()) ||
        stream.getPosition() != bytes.size()) return false;
    if (!stream.move(-1) || stream.getPosition() != bytes.size() - 1) return false;
    if (!stream.move(1) || stream.getPosition() != bytes.size()) return false;
    return true;
}

#ifndef STX_SKIA_REFERENCE
bool block_stream_bounds() {
    const std::array<std::uint8_t, 7> bytes = {3, 5, 7, 11, 13, 17, 19};
    SkDynamicMemoryWStream writer;
    if (!writer.write(bytes.data(), bytes.size())) return false;

    std::array<std::uint8_t, 7> untouched{};
    untouched.fill(0xa5);
    if (writer.read(untouched.data(), 1, std::numeric_limits<std::size_t>::max())) {
        return false;
    }
    for (std::uint8_t value : untouched) {
        if (value != 0xa5) return false;
    }

    std::unique_ptr<SkStreamAsset> reader = writer.detachAsStream();
    if (!reader || writer.bytesWritten() != 0) return false;
    std::array<std::uint8_t, 7> output{};
    if ((*reader).read(output.data(), std::numeric_limits<std::size_t>::max()) != bytes.size()) {
        return false;
    }
    if (output != bytes || (*reader).getPosition() != bytes.size()) return false;
    if ((*reader).read(output.data(), 1) != 0 || (*reader).read(output.data(), 0) != 0) {
        return false;
    }
    if (!(*reader).move(-2) || (*reader).getPosition() != bytes.size() - 2) return false;
    if (!(*reader).move(-std::numeric_limits<long>::max()) || (*reader).getPosition() != 0) {
        return false;
    }
    if (!(*reader).move(std::numeric_limits<long>::min()) || (*reader).getPosition() != 0) {
        return false;
    }
    if (!(*reader).move(std::numeric_limits<long>::max()) ||
        (*reader).getPosition() != bytes.size()) return false;
    if (!(*reader).seek(std::numeric_limits<std::size_t>::max()) ||
        (*reader).getPosition() != bytes.size()) return false;
    return true;
}
#endif
}

int main() {
    if (!subsets() || !ownership_and_streams() || !memory_stream_move()) return 1;
#ifndef STX_SKIA_REFERENCE
    if (!block_stream_bounds()) return 1;
#endif
    std::puts("data subsets, ranges, ownership and streams: PASS");
    return 0;
}
