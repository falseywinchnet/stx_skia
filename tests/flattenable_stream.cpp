#include "include/core/SkBlendMode.h"
#include "include/core/SkBlurTypes.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkData.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkPathEffect.h"
#include "include/core/SkSerialProcs.h"
#include "include/core/SkShader.h"
#include "include/effects/SkDashPathEffect.h"
#include "include/effects/SkShaderMaskFilter.h"
#include "src/core/SkWriteBuffer.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#ifndef STX_SKIA_REFERENCE
template <typename T> concept HasFactoryRecorder = requires { &T::setFactoryRecorder; };
static_assert(!HasFactoryRecorder<SkBinaryWriteBuffer>);
template <typename T> concept HasFactoryRegistry = requires { &T::Register; };
template <typename T> concept HasFactoryLookup = requires { &T::NameToFactory; };
static_assert(!HasFactoryRegistry<SkFlattenable>);
static_assert(!HasFactoryLookup<SkFlattenable>);
#endif

namespace {
bool append_record(const SkFlattenable& effect, std::vector<uint8_t>& output) {
    sk_sp<SkData> serialized = effect.serialize();
    if (!serialized || (*serialized).size() == 0) return false;
    const size_t bytes = (*serialized).size();
    if (bytes % sizeof(uint32_t) != 0) return false;
    std::vector<uint32_t> storage(bytes / sizeof(uint32_t));
    const size_t written = effect.serialize(storage.data(), bytes);
    if (written != bytes || std::memcmp(storage.data(), (*serialized).data(), bytes) != 0) return false;
    const SkSerialProcs procs{};
    SkBinaryWriteBuffer writer(procs);
    writer.writeFlattenable(&effect);
    if (writer.bytesWritten() != bytes) return false;
    std::vector<uint32_t> direct(storage.size());
    writer.writeToMemory(direct.data());
    if (storage != direct) return false;
    sk_sp<SkFlattenable> decoded = SkFlattenable::Deserialize(
        effect.getFlattenableType(), (*serialized).data(), bytes);
#ifdef STX_SKIA_REFERENCE
    if (!decoded || (*decoded).getFlattenableType() != effect.getFlattenableType()) return false;
#else
    if (decoded) return false;
    if (SkFlattenable::Deserialize(effect.getFlattenableType(), nullptr, bytes)) return false;
    if (SkFlattenable::Deserialize(effect.getFlattenableType(), nullptr, 0)) return false;
    if (effect.serialize(nullptr, bytes) != 0) return false;
    if (effect.serialize(nullptr, 0) != 0) return false;
    std::array<uint32_t, 3> guard{0xaabbccddU, 0xaabbccddU, 0xaabbccddU};
    const std::array<uint32_t, 3> unchanged = guard;
    uint8_t* misaligned = reinterpret_cast<uint8_t*>(guard.data()) + 1;
    if (effect.serialize(misaligned, sizeof(guard) - 1) != 0 || guard != unchanged) return false;
    if (effect.serialize(guard.data(), 0) != 0 || guard != unchanged) return false;
    if (effect.serialize(guard.data() + 1, sizeof(uint32_t)) != 0) return false;
    if (guard.front() != unchanged.front() || guard.back() != unchanged.back()) return false;
#endif
    const uint8_t* data = static_cast<const uint8_t*>((*serialized).data());
    output.insert(output.end(), data, data + bytes);
    return true;
}

bool streams(std::vector<uint8_t>& output, int& cases) {
    for (int variation = 0; variation < 4; ++variation) {
        const float offset = static_cast<float>(variation) * 0.125f;
        const std::array<float, 20> matrix{
            0.75f, 0.0f, 0.0f, 0.0f, offset,
            0.0f, 0.625f, 0.0f, 0.0f, 0.0625f,
            0.0f, 0.0f, 0.5f, 0.0f, 0.125f,
            0.0f, 0.0f, 0.0f, 0.875f, 0.0f};
        sk_sp<SkShader> shader = SkShaders::Color(SkColor4f{offset, 0.5f, 0.75f, 0.625f}, nullptr);
        sk_sp<SkColorFilter> color = SkColorFilters::Matrix(matrix.data());
        sk_sp<SkMaskFilter> blur = SkMaskFilter::MakeBlur(static_cast<SkBlurStyle>(variation), 1.0f + offset);
        sk_sp<SkMaskFilter> shaderMask = SkShaderMaskFilter::Make(shader);
        const std::array<float, 4> intervals{2.0f, 3.0f, 5.0f, 7.0f};
        const SkSpan<const float> intervalView(intervals.data(), intervals.size());
        sk_sp<SkPathEffect> dash = SkDashPathEffect::Make(intervalView, offset);
        if (!shader || !color || !blur || !shaderMask || !dash) return false;
        const std::array<const SkFlattenable*, 5> effects{
            shader.get(), color.get(), blur.get(), shaderMask.get(), dash.get()};
        const SkSerialProcs procs{};
        SkBinaryWriteBuffer growing(procs);
        std::array<uint32_t, 2048> fixedStorage{};
        SkBinaryWriteBuffer fixed(fixedStorage.data(), sizeof(fixedStorage), procs);
        growing.writeFlattenable(nullptr);
        fixed.writeFlattenable(nullptr);
        for (const SkFlattenable* effect : effects) {
            if (!append_record(*effect, output)) return false;
            growing.writeFlattenable(effect);
            fixed.writeFlattenable(effect);
            ++cases;
        }
        // Reuse each type name and a nested shader name in the same stream.
        for (size_t index = effects.size(); index > 0; --index) {
            growing.writeFlattenable(effects[index - 1]);
            fixed.writeFlattenable(effects[index - 1]);
        }
        const size_t bytes = growing.bytesWritten();
        if (!fixed.usingInitialStorage() || fixed.bytesWritten() != bytes) return false;
        if (bytes % sizeof(uint32_t) != 0) return false;
        std::vector<uint32_t> contents(bytes / sizeof(uint32_t));
        growing.writeToMemory(contents.data());
        if (std::memcmp(contents.data(), fixedStorage.data(), bytes) != 0) return false;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(contents.data());
        output.insert(output.end(), data, data + bytes);
        ++cases;
    }
    return true;
}
}

int main(int argc, char** argv) {
    std::vector<uint8_t> output{};
    int cases = 0;
    if (!streams(output, cases)) return 1;
    uint64_t digest = UINT64_C(1469598103934665603);
    for (const uint8_t byte : output) {
        digest ^= byte;
        digest *= UINT64_C(1099511628211);
    }
    if (argc == 2) {
        FILE* file = std::fopen(argv[1], "wb");
        if (file == nullptr) return 2;
        const size_t written = std::fwrite(output.data(), 1, output.size(), file);
        const int closed = std::fclose(file);
        if (written != output.size() || closed != 0) return 3;
    }
    std::printf("flattenable streams: %d cases, %zu bytes, hash=%llu\n", cases, output.size(),
                static_cast<unsigned long long>(digest));
    std::printf("writer_size=%zu\n", sizeof(SkBinaryWriteBuffer));
    return 0;
}
