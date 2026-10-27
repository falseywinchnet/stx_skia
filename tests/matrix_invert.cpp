// Differential corpus for 4x4 matrix inversion: general, affine, 2D-perspective, permutation,
// singular, nearly singular, huge and non-finite matrices, inverted out of place and in place.
// It records whether each matrix inverts and, if it does, the inverse. Both builds must produce
// identical bytes.
//
// Every entry of a shared matrix is zero, non-finite, or at least 2^-10 in magnitude. Such an
// entry is a multiple of 2^-33, so every product of four is a multiple of 2^-132, and the double
// determinant is either zero or at least 2^-132: rounding it to float cannot reach zero. Matrices
// with smaller entries are written to the second output, whose results the determinant fix
// changes.

#include "differential_corpus.h"

#include "src/core/SkMatrixInvert.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {

using corpus::Output;
using corpus::Sequence;
using corpus::append;
using corpus::append_bool;

using Matrix = std::array<float, 16>;

bool invert(const Matrix& in, Matrix& out) {
    const bool invertible = SkInvert4x4Matrix(in.data(), out.data()) != 0.0f;
    return invertible;
}

// In-place inversion: the input and output arrays are the same.
bool invert_in_place(Matrix& matrix) {
    const bool invertible = SkInvert4x4Matrix(matrix.data(), matrix.data()) != 0.0f;
    return invertible;
}

void record(Output& output, const Matrix& in) {
    Matrix out{};
    const bool invertible = invert(in, out);
    append_bool(output, invertible);
    if (invertible) {
        for (const float value : out) append(output, value);
    }
    Matrix same = in;
    const bool sameInvertible = invert_in_place(same);
    append_bool(output, sameInvertible);
    if (sameInvertible) {
        for (const float value : same) append(output, value);
    }
    ++output.cases;
}

bool has_small_entry(const Matrix& m) {
    for (const float value : m) {
        if (value != 0 && std::isfinite(value) && std::fabs(value) < 0x1p-10f) return true;
    }
    return false;
}

constexpr std::array<float, 16> kLarge{1.0f, -1.0f, 0.5f, 2.0f, -3.0f, 1e-3f, -1e-3f, 1e3f,
                                       1e20f, -1e20f, corpus::kFloatMax, -corpus::kFloatMax,
                                       corpus::kInf, -corpus::kInf, corpus::kNaN, 0.1f};

float pick_entry(Sequence& sequence, bool special) {
    if (special && sequence.pick(3) == 0) return kLarge[sequence.pick(kLarge.size())];
    if (sequence.pick(5) == 0) return 0.0f;
    const int whole = static_cast<int>(sequence.next() % 4001u) - 2000;
    return whole == 0 ? -0.0f : static_cast<float>(whole) / 128.0f;
}

enum class Shape { kGeneral, kAffine, kPerspective2D, kPermutation, kRankDeficient, kNearSingular,
                   kInteger, kCount };

Matrix make_matrix(Sequence& sequence, Shape shape, bool special) {
    Matrix m{};
    for (float& value : m) value = pick_entry(sequence, special);
    switch (shape) {
        case Shape::kGeneral:
            break;
        case Shape::kAffine:
            m[3] = 0;
            m[7] = 0;
            m[11] = 0;
            m[15] = 1;
            break;
        case Shape::kPerspective2D:
            m[2] = 0;
            m[6] = 0;
            m[8] = 0;
            m[9] = 0;
            m[10] = 1;
            m[11] = 0;
            m[14] = 0;
            break;
        case Shape::kPermutation: {
            m = Matrix{};
            std::array<int, 4> order{0, 1, 2, 3};
            for (int i = 3; i > 0; --i) {
                const size_t j = sequence.pick(static_cast<size_t>(i) + 1);
                const int swapped = order[static_cast<size_t>(i)];
                order[static_cast<size_t>(i)] = order[j];
                order[j] = swapped;
            }
            for (int c = 0; c < 4; ++c) {
                const float sign = sequence.pick(2) == 0 ? 1.0f : -1.0f;
                m[static_cast<size_t>(c * 4 + order[static_cast<size_t>(c)])] = sign;
            }
            break;
        }
        case Shape::kRankDeficient: {
            // One column is a combination of two others.
            const size_t target = sequence.pick(4);
            const size_t first = (target + 1) % 4;
            const size_t second = (target + 2) % 4;
            const float p = static_cast<float>(static_cast<int>(sequence.next() % 9u) - 4);
            const float q = static_cast<float>(static_cast<int>(sequence.next() % 9u) - 4);
            for (size_t r = 0; r < 4; ++r) {
                m[target * 4 + r] = p * m[first * 4 + r] + q * m[second * 4 + r];
            }
            break;
        }
        case Shape::kNearSingular: {
            // Two columns differ in one entry by a few ulps.
            const size_t target = sequence.pick(4);
            const size_t source = (target + 1) % 4;
            for (size_t r = 0; r < 4; ++r) m[target * 4 + r] = m[source * 4 + r];
            const size_t row = sequence.pick(4);
            float& nudged = m[target * 4 + row];
            const int steps = 1 + static_cast<int>(sequence.pick(4));
            for (int i = 0; i < steps; ++i) nudged = std::nextafter(nudged, corpus::kInf);
            break;
        }
        case Shape::kInteger:
            for (float& value : m) {
                value = static_cast<float>(static_cast<int>(sequence.next() % 7u) - 3);
            }
            break;
        case Shape::kCount:
            break;
    }
    return m;
}

void shared_corpus(Output& output, Output& small) {
    Sequence sequence{};
    for (int i = 0; i < 6000; ++i) {
        const Shape shape = static_cast<Shape>(sequence.pick(static_cast<size_t>(Shape::kCount)));
        const Matrix m = make_matrix(sequence, shape, i % 4 == 3);
        record(has_small_entry(m) ? small : output, m);
    }
    // Scales and their products with permutations, where the inverse is exact.
    const std::array<float, 10> scales{1.0f, 2.0f, -0.5f, 0x1p-10f, 0x1p10f, 1e3f, 1e20f,
                                       corpus::kFloatMax, 0x1p63f, 0x1p-9f};
    for (const float a : scales) {
        for (const float b : scales) {
            Matrix m{};
            m[0] = a;
            m[5] = b;
            m[10] = 1;
            m[15] = 1;
            record(output, m);
            m[10] = a;
            m[15] = b;
            record(output, m);
            m[12] = 3;
            m[13] = -7;
            record(output, m);
        }
    }
}

// Matrices whose determinant can round to zero in float: tiny scales, alone and combined with
// ordinary and large entries.
void small_corpus(Output& small) {
    const std::array<float, 12> tiny{1e-4f, 1e-5f, 1e-10f, 1e-12f, 1e-15f, 1e-20f, 1e-23f, 1e-30f,
                                     3e-39f, corpus::kFloatMin, corpus::kDenormal, 0x1p-60f};
    Sequence sequence{};
    for (const float s : tiny) {
        Matrix diagonal2{};
        diagonal2[0] = s;
        diagonal2[5] = s;
        diagonal2[10] = 1;
        diagonal2[15] = 1;
        record(small, diagonal2);
        Matrix diagonal4 = diagonal2;
        diagonal4[10] = s;
        diagonal4[15] = s;
        record(small, diagonal4);
        diagonal2[12] = 5;
        diagonal2[13] = -2;
        record(small, diagonal2);
        for (int i = 0; i < 40; ++i) {
            const Shape shape = static_cast<Shape>(sequence.pick(3));
            Matrix m = make_matrix(sequence, shape, i % 5 == 4);
            for (size_t k = 0; k < 8; ++k) m[k] *= s;
            record(small, m);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 1 || argc > 3) return 2;
    Output output{};
    Output small{};
    shared_corpus(output, small);
    small_corpus(small);
    if (argc >= 2 && !corpus::write_file(argv[1], output)) return 2;
    if (argc == 3 && !corpus::write_file(argv[2], small)) return 2;
    const unsigned long long hash = corpus::fnv1a(output.bytes);
    const unsigned long long smallHash = corpus::fnv1a(small.bytes);
    std::printf("matrix invert: %zu cases, %zu bytes, hash=%llu; small %zu cases, hash=%llu PASS\n",
                output.cases, output.bytes.size(), hash, small.cases, smallHash);
    return 0;
}
