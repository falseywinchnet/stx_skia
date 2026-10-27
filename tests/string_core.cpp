// Differential corpus for the SkString interface the candidate keeps: construction, copying and
// moving with shared storage, comparison, set, insert and append around the 4-byte allocation
// boundaries, code-point appends, printf and appendf across the 1024-byte stack buffer,
// SkStringPrintf, and SkAppendScalar in both formats. Both builds must produce identical bytes
// (string-core.raw). Each string is recorded as its size, its bytes and whether it is terminated.
// Expressions check that the members the candidate removed are absent there and present in the
// reference.

#include "differential_corpus.h"

#include "include/core/SkString.h"
#include "src/core/SkStringUtils.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

namespace {

// The candidate drops members nothing uses; the reference still has them.
template <typename T>
concept HasNumberAppends = requires(T& text) { text.appendS32(1); text.insertHex(0, 1u, 2); };
template <typename T>
concept HasPrepend = requires(T& text) { text.prepend("x"); text.prependf("%d", 1); };
template <typename T>
concept HasResizeAndRemove = requires(T& text) { text.resize(1); text.remove(0, 1); };
template <typename T>
concept HasStdStringConstruction = requires(const std::string& source) { T{source}; };
template <typename T>
concept HasSearch = requires(const T& text) { text.find("a"); text.startsWith("a"); };
template <typename T>
concept HasIndexing = requires(T& text) { text[0]; text.begin(); };
#ifdef STX_SKIA_REFERENCE
static_assert(HasNumberAppends<SkString> && HasPrepend<SkString> && HasResizeAndRemove<SkString> &&
              HasStdStringConstruction<SkString> && HasSearch<SkString> && HasIndexing<SkString>);
#else
static_assert(!HasNumberAppends<SkString> && !HasPrepend<SkString> &&
              !HasResizeAndRemove<SkString> && !HasStdStringConstruction<SkString> &&
              !HasSearch<SkString> && !HasIndexing<SkString>);
#endif

using corpus::Output;
using corpus::append;
using corpus::append_bool;

void record(Output& output, const SkString& text) {
    append(output, static_cast<uint64_t>(text.size()));
    append_bool(output, text.isEmpty());
    const uint8_t* begin = reinterpret_cast<const uint8_t*>(text.c_str());
    output.bytes.insert(output.bytes.end(), begin, begin + text.size());
    append_bool(output, text.c_str()[text.size()] == '\0');
    append_bool(output, text.c_str() == text.data());
    ++output.cases;
}

// Text of the given length, "abcd..." repeated, for sizes around the allocation boundaries.
std::string pattern(size_t length, char first) {
    std::string text(length, ' ');
    for (size_t i = 0; i < length; ++i) {
        text[i] = static_cast<char>(first + static_cast<char>(i % 26));
    }
    return text;
}

constexpr std::array<size_t, 22> kLengths{0,   1,   2,   3,    4,    5,    6,    7,    8,    9,    11,
                                          12,  13,  15,  16,   17,   63,   64,   1023, 1024, 1025, 4096};

void construction_cases(Output& output) {
    record(output, SkString());
    record(output, SkString(nullptr));
    record(output, SkString(nullptr, 0));
    for (const size_t length : kLengths) {
        const std::string text = pattern(length, 'a');
        record(output, SkString(text.c_str()));
        record(output, SkString(text.c_str(), length));
        // A length shorter than the text takes a prefix.
        record(output, SkString(text.c_str(), length / 2));
    }
    // Embedded zero bytes are kept by the length form and end the text form.
    const char withZero[] = {'a', 'b', '\0', 'c', 'd'};
    record(output, SkString(withZero, sizeof(withZero)));
    record(output, SkString(withZero));
}

void comparison_cases(Output& output) {
    const std::array<const char*, 7> texts{"", "a", "b", "ab", "abc", "abd", "abcdefghij"};
    for (const char* left : texts) {
        const SkString a(left);
        append_bool(output, a.equals(nullptr));
        for (const char* right : texts) {
            const SkString b(right);
            append_bool(output, a.equals(b));
            append_bool(output, a.equals(right));
            append_bool(output, a.equals(right, std::char_traits<char>::length(right)));
            append_bool(output, a == b);
            append_bool(output, a != b);
            ++output.cases;
        }
        for (const char c : {'a', 'b', 'j', 'z', '\0'}) {
            append_bool(output, a.contains(c));
            ++output.cases;
        }
    }
    // Shared storage compares equal; equal text in separate storage does too.
    const SkString shared("shared");
    const SkString copy = shared;
    append_bool(output, copy == shared);
    append_bool(output, copy.c_str() == shared.c_str());
    ++output.cases;
}

void value_cases(Output& output) {
    for (const size_t length : kLengths) {
        const std::string text = pattern(length, 'A');
        const SkString original(text.c_str());

        // Copies share until one is changed; the change leaves the other intact.
        SkString copy(original);
        append_bool(output, copy.c_str() == original.c_str());
        copy.append("!");
        record(output, original);
        record(output, copy);

        SkString assigned{};
        assigned = original;
        assigned.set("x");
        record(output, original);
        record(output, assigned);

        // A moved-from string is empty and reusable.
        SkString source(text.c_str());
        SkString moved(std::move(source));
        record(output, moved);
        record(output, source);
        source.append("again");
        record(output, source);

        SkString target("previous");
        SkString donor(text.c_str());
        target = std::move(donor);
        record(output, target);
        record(output, donor);

        // Assigning to itself keeps the text.
        SkString self(text.c_str());
        const SkString& alias = self;
        self = alias;
        record(output, self);

        SkString reset(text.c_str());
        reset.reset();
        record(output, reset);
    }
}

void set_cases(Output& output) {
    for (const size_t before : kLengths) {
        for (const size_t after : kLengths) {
            const std::string initial = pattern(before, 'a');
            const std::string replacement = pattern(after, 'K');
            SkString unique(initial.c_str());
            unique.set(replacement.c_str());
            record(output, unique);

            // A shared string must not change its other owner.
            SkString shared(initial.c_str());
            const SkString owner(shared);
            shared.set(replacement.c_str(), after);
            record(output, shared);
            record(output, owner);
        }
        SkString cleared(pattern(before, 'a').c_str());
        cleared.set(nullptr);
        record(output, cleared);
        SkString empty(pattern(before, 'a').c_str());
        empty.set("abc", 0);
        record(output, empty);
    }
}

void insert_cases(Output& output) {
    const std::array<size_t, 13> bases{0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 64, 1030};
    const std::array<size_t, 8> lengths{0, 1, 2, 3, 4, 5, 17, 1100};
    for (const size_t base : bases) {
        const std::string initial = pattern(base, 'a');
        const std::array<size_t, 5> offsets{0, base / 2, base, base + 1, static_cast<size_t>(-1)};
        for (const size_t offset : offsets) {
            for (const size_t length : lengths) {
                const std::string text = pattern(length, 'M');
                SkString unique(initial.c_str());
                unique.insert(offset, text.c_str(), length);
                record(output, unique);

                SkString shared(initial.c_str());
                const SkString owner(shared);
                shared.insert(offset, text.c_str());
                record(output, shared);
                record(output, owner);
            }
            SkString nothing(initial.c_str());
            nothing.insert(offset, nullptr);
            record(output, nothing);
        }
    }
}

void append_cases(Output& output) {
    SkString text{};
    for (const size_t length : kLengths) {
        const std::string piece = pattern(length, 'a');
        text.append(piece.c_str());
        record(output, text);
        text.append(piece.c_str(), length / 3);
        record(output, text);
        text.append(SkString(piece.c_str()));
        record(output, text);
    }
    // Code points of one to four UTF-8 bytes, a lone surrogate, and a value past the last code
    // point.
    const std::array<SkUnichar, 13> points{0x41, 0x7F, 0x80, 0xE9, 0x7FF, 0x800, 0xD800, 0x20AC,
                                           0xFFFF, 0x10000, 0x1F600, 0x10FFFF, 0x110000};
    for (const SkUnichar point : points) {
        SkString unichar("u");
        unichar.appendUnichar(point);
        record(output, unichar);
        SkString inserted("ab");
        inserted.insertUnichar(1, point);
        record(output, inserted);
    }
    SkString negative("n");
    negative.appendUnichar(-1);
    record(output, negative);
}

void format_cases(Output& output) {
    SkString formatted{};
    formatted.printf("%d %u %x %s %c %%", -42, 42u, 0xBEEFu, "text", 'z');
    record(output, formatted);
    formatted.printf("%.3f %g %e", 3.14159, 1e-7, 2.5e10);
    record(output, formatted);
    formatted.printf("%s", "");
    record(output, formatted);

    // Results shorter than, equal to and longer than the 1024-byte stack buffer.
    for (const size_t length : {size_t{1022}, size_t{1023}, size_t{1024}, size_t{1025}, size_t{3000}}) {
        const std::string text = pattern(length, 'a');
        SkString printed("previous");
        printed.printf("%s", text.c_str());
        record(output, printed);

        SkString appended("prefix:");
        appended.appendf("%s", text.c_str());
        record(output, appended);

        SkString fromEmpty{};
        fromEmpty.appendf("[%s]", text.c_str());
        record(output, fromEmpty);

        record(output, SkStringPrintf("%zu:%s", length, text.c_str()));
    }
    SkString chain("start");
    for (int i = 0; i < 40; ++i) {
        chain.appendf(" %d", i * 37);
    }
    record(output, chain);
}

void scalar_cases(Output& output) {
    const std::array<float, 20> values{0.0f, -0.0f, 1.0f, -1.0f, 0.5f, 0.1f, 3.0f, 100.0f, 1e7f,
                                       1.5e-7f, 123456.789f, 1.0f / 3.0f, corpus::kFloatMax,
                                       corpus::kFloatMin, corpus::kDenormal, corpus::kInf,
                                       -corpus::kInf, corpus::kNaN, 16777216.0f, 0.75f};
    for (const float value : values) {
        SkString decimal("d=");
        SkAppendScalar(&decimal, value, kDec_SkScalarAsStringType);
        record(output, decimal);
        SkString hex("h=");
        SkAppendScalar(&hex, value, kHex_SkScalarAsStringType);
        record(output, hex);
        SkString both{};
        SkAppendScalarDec(&both, value);
        SkAppendScalarHex(&both, value);
        record(output, both);
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 1 || argc > 2) return 2;
    Output output{};
    construction_cases(output);
    comparison_cases(output);
    value_cases(output);
    set_cases(output);
    insert_cases(output);
    append_cases(output);
    format_cases(output);
    scalar_cases(output);
    if (argc == 2 && !corpus::write_file(argv[1], output)) return 2;
    const unsigned long long hash = corpus::fnv1a(output.bytes);
    std::printf("string core: %zu cases, %zu bytes, hash=%llu PASS\n", output.cases,
                output.bytes.size(), hash);
    return 0;
}
