#include "include/core/SkStream.h"
#include "include/core/SkPathBuilder.h"
#include "src/core/SkAAClip.h"
#include "src/core/SkBase64.h"
#include "src/core/SkMask.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
class ShortStream final : public SkStream {
public:
    ShortStream(const uint8_t* bytes, size_t length) : fBytes(bytes), fLength(length) {}

    size_t read(void* buffer, size_t size) override {
        if (size == 0 || fPosition == fLength) return 0;
        if (buffer != nullptr) {
            std::memcpy(buffer, fBytes + fPosition, 1);
        }
        ++fPosition;
        return 1;
    }

    bool isAtEnd() const override { return fPosition == fLength; }

private:
    const uint8_t* fBytes;
    size_t fLength;
    size_t fPosition = 0;
};

bool decode_expected(const char* source, size_t sourceLength,
                     const char* expected, size_t expectedLength) {
    size_t required = 99;
    const SkBase64::Error sized = SkBase64::Decode(source, sourceLength, nullptr, &required);
    if (sized != SkBase64::kNoError || required != expectedLength) return false;
    std::array<unsigned char, 16> output{};
    size_t written = 99;
    const SkBase64::Error decoded = SkBase64::Decode(source, sourceLength,
                                                    output.data(), &written);
    if (decoded != SkBase64::kNoError || written != expectedLength) return false;
    return std::memcmp(output.data(), expected, expectedLength) == 0;
}

bool base64_bounds() {
    const std::array<char, 4> exact = {'T', 'W', 'F', 'u'};
    if (!decode_expected(exact.data(), exact.size(), "Man", 3)) return false;
    const std::array<char, 8> doubled = {'T', 'W', 'F', 'u', 'T', 'W', 'F', 'u'};
    if (!decode_expected(doubled.data(), doubled.size(), "ManMan", 6)) return false;
    if (!decode_expected("TQ==", 4, "M", 1)) return false;
    if (!decode_expected("TWE=", 4, "Ma", 2)) return false;
    if (!decode_expected("TQ", 2, "M", 1)) return false;
    if (!decode_expected("TWE", 3, "Ma", 2)) return false;
    if (!decode_expected("TQ== \n", 6, "M", 1)) return false;
    if (!decode_expected(" \t", 2, "", 0)) return false;
    if (!decode_expected(nullptr, 0, "", 0)) return false;

    const std::array<char, 5> extra = {'A', 'A', 'A', 'A', 'A'};
    const std::array<char, 4> badPad = {'T', 'Q', '=', 'A'};
    const std::array<char, 2> badNul = {'A', 0};
    std::array<unsigned char, 16> output{};
    output.fill(0xa5);
    size_t length = 99;
    if (SkBase64::Decode(extra.data(), extra.size(), output.data(), &length) !=
        SkBase64::kPadError || length != 0) return false;
    if (SkBase64::Decode(badPad.data(), badPad.size(), output.data(), &length) !=
        SkBase64::kPadError || length != 0) return false;
    if (SkBase64::Decode(badNul.data(), badNul.size(), output.data(), &length) !=
        SkBase64::kBadCharError || length != 0) return false;
    for (const unsigned char value : output) {
        if (value != 0xa5) return false;
    }
    return true;
}

bool stream_counts() {
    const std::array<uint8_t, 5> bytes = {11, 22, 33, 44, 55};
    ShortStream complete(bytes.data(), bytes.size());
    SkDynamicMemoryWStream completeOutput;
    if (!completeOutput.writeStream(&complete, bytes.size())) return false;
    const std::vector<uint8_t> copied = completeOutput.detachAsVector();
    if (copied.size() != bytes.size()) return false;
    for (size_t index = 0; index < bytes.size(); ++index) {
        if (copied[index] != bytes[index]) return false;
    }

    ShortStream shortInput(bytes.data(), 3);
    SkDynamicMemoryWStream shortOutput;
    if (shortOutput.writeStream(&shortInput, bytes.size())) return false;
    const std::vector<uint8_t> prefix = shortOutput.detachAsVector();
    if (prefix.size() != 3) return false;
    for (size_t index = 0; index < prefix.size(); ++index) {
        if (prefix[index] != bytes[index]) return false;
    }
    return true;
}

bool file_stream_failure_states() {
    std::array<uint8_t, 1> output = {0x5a};
    SkFILEStream reader;
    if (reader.isValid() || reader.read(output.data(), output.size()) != 0) return false;
    if (output[0] != 0x5a) return false;
    reader.close();
    if (reader.read(output.data(), output.size()) != 0) return false;

    SkFILEWStream writer(nullptr);
    if (writer.isValid() || writer.bytesWritten() != 0) return false;
    if (writer.write(output.data(), output.size())) return false;
    return writer.bytesWritten() == 0;
}

bool aliased_path_points() {
    SkPathBuilder polygon;
    polygon.moveTo(0, 0);
    for (int index = 1; index < 32; ++index) {
        polygon.lineTo(static_cast<float>(index), static_cast<float>(index + 1));
    }
    const SkSpan<const SkPoint> polygonInput = polygon.points();
    polygon.addPolygon(polygonInput, false);
    const SkSpan<const SkPoint> polygonOutput = polygon.points();
    if (polygonOutput.size() != 64) return false;
    for (int index = 0; index < 32; ++index) {
        const SkPoint expected = polygonOutput[index];
        const SkPoint actual = polygonOutput[index + 32];
        if (actual.fX != expected.fX || actual.fY != expected.fY) return false;
    }

    SkPathBuilder polyline;
    polyline.moveTo(0, 0);
    for (int index = 1; index < 32; ++index) {
        polyline.lineTo(static_cast<float>(index), static_cast<float>(index + 1));
    }
    const SkSpan<const SkPoint> lineInput = polyline.points();
    polyline.polylineTo(lineInput);
    const SkSpan<const SkPoint> lineOutput = polyline.points();
    if (lineOutput.size() != 64) return false;
    for (int index = 0; index < 32; ++index) {
        const SkPoint expected = lineOutput[index];
        const SkPoint actual = lineOutput[index + 32];
        if (actual.fX != expected.fX || actual.fY != expected.fY) return false;
    }
    return true;
}

bool oversized_clip_mask() {
    SkAAClip clip;
    if (!clip.setRect(SkIRect::MakeWH(50000, 50000))) return false;
    SkMaskBuilder mask;
    clip.copyToMask(&mask);
    return mask.isEmpty() && mask.image() == nullptr && mask.rowBytes() == 0;
}
}

int main() {
    if (!base64_bounds()) {
        std::fprintf(stderr, "Base64 bounds contract failed\n");
        return 1;
    }
    if (!stream_counts()) {
        std::fprintf(stderr, "stream copy count contract failed\n");
        return 2;
    }
    if (!file_stream_failure_states()) {
        std::fprintf(stderr, "file stream failure contract failed\n");
        return 3;
    }
    if (!aliased_path_points()) {
        std::fprintf(stderr, "aliased path point contract failed\n");
        return 4;
    }
    if (!oversized_clip_mask()) {
        std::fprintf(stderr, "oversized clip mask contract failed\n");
        return 5;
    }
    return 0;
}
