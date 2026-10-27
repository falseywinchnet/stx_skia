// Differential corpus for 4x4 matrices and their vectors: construction, element access,
// equality, concatenation with 4x4 and 3x3 matrices, pre/post translation and scaling, rotation
// about arbitrary axes, perspective normalization, inversion, point and vector mapping,
// conversion to and from SkMatrix, and SkMatrixPriv::MapRect with and without perspective. The
// matrices include signed zeros, NaN, infinities, denormals and points behind the w = 0 plane.
// Both builds must produce identical bytes.

#include "differential_corpus.h"

#include "include/core/SkM44.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkRect.h"
#include "src/core/SkMatrixPriv.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

using corpus::Output;
using corpus::Sequence;
using corpus::append;
using corpus::append_bool;

constexpr std::array<float, 22> kSpecial{0.0f, -0.0f, 1.0f, -1.0f, 0.5f, 2.0f, 3.0f, -7.25f,
                                         1e-3f, 1e3f, 1e20f, -1e20f, 1e-20f, 3e-39f,
                                         corpus::kDenormal, corpus::kFloatMin,
                                         corpus::kFloatMax, -corpus::kFloatMax,
                                         corpus::kInf, -corpus::kInf, corpus::kNaN, 0.1f};

// NaN payloads and signs depend on operand order, which a respelling may change; every NaN is
// recorded as one canonical value.
void append_float(Output& output, float value) {
    const float canonical = std::isnan(value) ? corpus::kNaN : value;
    append(output, canonical);
}

float pick_float(Sequence& sequence) {
    if (sequence.pick(4) == 0) return kSpecial[sequence.pick(kSpecial.size())];
    const int whole = static_cast<int>(sequence.next() % 2001u) - 1000;
    return static_cast<float>(whole) / 64.0f;
}

// A finite value, most often small, for matrices that should stay ordinary.
float pick_ordinary(Sequence& sequence) {
    const int whole = static_cast<int>(sequence.next() % 401u) - 200;
    return static_cast<float>(whole) / 32.0f;
}

void append_m44(Output& output, const SkM44& m) {
    std::array<float, 16> values{};
    m.getColMajor(values.data());
    for (const float value : values) append_float(output, value);
}

void append_v4(Output& output, const SkV4& v) {
    append_float(output, v.x);
    append_float(output, v.y);
    append_float(output, v.z);
    append_float(output, v.w);
}

void append_v3(Output& output, const SkV3& v) {
    append_float(output, v.x);
    append_float(output, v.y);
    append_float(output, v.z);
}

void append_matrix(Output& output, const SkMatrix& m) {
    for (int i = 0; i < 9; ++i) append_float(output, m[i]);
}

std::array<float, 16> column_major(const SkM44& m) {
    std::array<float, 16> values{};
    m.getColMajor(values.data());
    return values;
}

SkM44 from_column_major(const std::array<float, 16>& values) {
    const SkM44 m = SkM44::ColMajor(values.data());
    return m;
}

// Kinds of matrix the generator produces: general, affine (bottom row 0 0 0 1), affine with a
// scaled bottom-right entry, and 2D perspective as SkMatrix would produce.
enum class Shape { kGeneral, kAffine, kScaledW, kPerspective2D, kCount };

SkM44 make_matrix(Sequence& sequence, Shape shape, bool special) {
    std::array<float, 16> values{};
    for (float& value : values) {
        value = special ? pick_float(sequence) : pick_ordinary(sequence);
    }
    switch (shape) {
        case Shape::kGeneral:
            break;
        case Shape::kAffine:
            values[3] = 0;
            values[7] = 0;
            values[11] = 0;
            values[15] = 1;
            break;
        case Shape::kScaledW:
            values[3] = 0;
            values[7] = 0;
            values[11] = 0;
            break;
        case Shape::kPerspective2D:
            values[2] = 0;
            values[6] = 0;
            values[8] = 0;
            values[9] = 0;
            values[10] = 1;
            values[11] = 0;
            values[14] = 0;
            break;
        case Shape::kCount:
            break;
    }
    return from_column_major(values);
}

SkRect make_rect(Sequence& sequence, bool special) {
    const float a = special ? pick_float(sequence) : pick_ordinary(sequence);
    const float b = special ? pick_float(sequence) : pick_ordinary(sequence);
    const float c = special ? pick_float(sequence) : pick_ordinary(sequence);
    const float d = special ? pick_float(sequence) : pick_ordinary(sequence);
    const SkRect rect = SkRect::MakeLTRB(a, b, c, d);
    return rect;
}

void construction_corpus(Output& output) {
    append_m44(output, SkM44());
    append_m44(output, SkM44::Translate(1.5f, -2.0f));
    append_m44(output, SkM44::Translate(1.5f, -2.0f, 7.0f));
    append_m44(output, SkM44::Scale(3.0f, -0.25f));
    append_m44(output, SkM44::Scale(3.0f, -0.25f, 0.0f));
    output.cases += 5;

    Sequence sequence{};
    for (int i = 0; i < 64; ++i) {
        const SkMatrix m = SkMatrix::MakeAll(pick_float(sequence), pick_float(sequence),
                                             pick_float(sequence), pick_float(sequence),
                                             pick_float(sequence), pick_float(sequence),
                                             pick_float(sequence), pick_float(sequence),
                                             pick_float(sequence));
        const SkM44 m44(m);
        append_m44(output, m44);
        append_matrix(output, m44.asM33());
        ++output.cases;
    }

    // Element access and assignment, row by column.
    for (int i = 0; i < 16; ++i) {
        SkM44 m = make_matrix(sequence, Shape::kGeneral, true);
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) append_float(output, m.rc(r, c));
        }
        m.setRC(i / 4, i % 4, 42.5f);
        append_m44(output, m);
        SkM44 set = m;
        set.setIdentity();
        append_m44(output, set);
        set.setTranslate(m.rc(0, 0), m.rc(1, 1), m.rc(2, 2));
        append_m44(output, set);
        set.setScale(m.rc(0, 1), m.rc(1, 2));
        append_m44(output, set);
        set.setScale(m.rc(0, 1), m.rc(1, 2), m.rc(3, 0));
        append_m44(output, set);
        append_bool(output, m.isFinite());
        ++output.cases;
    }
}

void equality_corpus(Output& output) {
    Sequence sequence{};
    for (int i = 0; i < 128; ++i) {
        const SkM44 a = make_matrix(sequence, static_cast<Shape>(sequence.pick(4)), true);
        std::array<float, 16> values = column_major(a);
        const size_t index = sequence.pick(16);
        switch (sequence.pick(4)) {
            case 0:
                break;
            case 1:
                values[index] = -values[index];
                break;
            case 2:
                values[index] = corpus::kNaN;
                break;
            default:
                values[index] = std::nextafter(values[index], corpus::kInf);
                break;
        }
        const SkM44 b = from_column_major(values);
        const SkM44 copy = a;
        append_bool(output, a == b);
        append_bool(output, a != b);
        append_bool(output, a == copy);
        ++output.cases;
    }
    const SkM44 zero = SkM44::Scale(0.0f, 0.0f, 0.0f);
    const SkM44 negativeZero = SkM44::Scale(-0.0f, -0.0f, -0.0f);
    append_bool(output, zero == negativeZero);
    ++output.cases;
}

void concat_corpus(Output& output) {
    Sequence sequence{};
    for (int i = 0; i < 400; ++i) {
        const bool special = i % 4 == 3;
        const SkM44 a = make_matrix(sequence, static_cast<Shape>(sequence.pick(4)), special);
        const SkM44 b = make_matrix(sequence, static_cast<Shape>(sequence.pick(4)), special);
        append_m44(output, a * b);
        append_m44(output, SkM44(a, b));
        SkM44 set = a;
        set.setConcat(a, b);
        append_m44(output, set);
        SkM44 pre = a;
        pre.preConcat(b);
        append_m44(output, pre);
        SkM44 post = a;
        post.postConcat(b);
        append_m44(output, post);
        // Aliased operands.
        SkM44 square = a;
        square.setConcat(square, square);
        append_m44(output, square);

        const SkMatrix m3 = SkMatrix::MakeAll(pick_ordinary(sequence), pick_ordinary(sequence),
                                              pick_ordinary(sequence), pick_ordinary(sequence),
                                              pick_ordinary(sequence), pick_ordinary(sequence),
                                              special ? pick_float(sequence) : 0.0f,
                                              special ? pick_float(sequence) : 0.0f,
                                              special ? pick_float(sequence) : 1.0f);
        SkM44 pre3 = a;
        pre3.preConcat(m3);
        append_m44(output, pre3);

        const float x = special ? pick_float(sequence) : pick_ordinary(sequence);
        const float y = special ? pick_float(sequence) : pick_ordinary(sequence);
        const float z = special ? pick_float(sequence) : pick_ordinary(sequence);
        SkM44 preT = a;
        preT.preTranslate(x, y);
        append_m44(output, preT);
        preT.preTranslate(x, y, z);
        append_m44(output, preT);
        SkM44 postT = a;
        postT.postTranslate(x, y);
        append_m44(output, postT);
        postT.postTranslate(x, y, z);
        append_m44(output, postT);
        SkM44 preS = a;
        preS.preScale(x, y);
        append_m44(output, preS);
        preS.preScale(x, y, z);
        append_m44(output, preS);
        ++output.cases;
    }
}

void rotation_corpus(Output& output) {
    const std::array<SkV3, 18> axes{SkV3{1, 0, 0}, SkV3{0, -1, 0}, SkV3{0, 0, 1},
                                    SkV3{1, 1, 1}, SkV3{3, -4, 12}, SkV3{0, 0, 0},
                                    SkV3{1e-20f, 0, 0}, SkV3{1e-23f, 2e-23f, 0},
                                    SkV3{1e20f, 1e20f, 0}, SkV3{3e38f, 0, 0},
                                    SkV3{corpus::kDenormal, 0, 0},
                                    SkV3{corpus::kInf, 0, 0}, SkV3{corpus::kNaN, 1, 0},
                                    SkV3{-0.0f, -0.0f, -1.0f}, SkV3{0.6f, 0.8f, 0},
                                    SkV3{1e-30f, 1e-30f, 1e-30f}, SkV3{2e19f, -3e19f, 6e19f},
                                    SkV3{-5, 0.25f, 9}};
    const std::array<float, 10> angles{0.0f, 0.5f, -1.25f, 1.5707964f, 3.1415927f, 6.2831855f,
                                       100.0f, corpus::kInf, corpus::kNaN, -0.0f};
    for (const SkV3& axis : axes) {
        for (const float angle : angles) {
            append_m44(output, SkM44::Rotate(axis, angle));
            SkM44 set = SkM44::Translate(5, 6, 7);
            set.setRotate(axis, angle);
            append_m44(output, set);
            set.setRotateUnit(axis, angle);
            append_m44(output, set);
            set.setRotateUnitSinCos(axis, std::sin(angle), std::cos(angle));
            append_m44(output, set);
            ++output.cases;
        }
    }
}

void normalize_corpus(Output& output) {
    const std::array<float, 16> bottoms{1.0f, 0.0f, -0.0f, 2.0f, -3.0f, 0.1f, 7.0f, 1e-20f,
                                        3e-39f, corpus::kDenormal, 1e30f, corpus::kInf,
                                        -corpus::kInf, corpus::kNaN, 1.0000001f, 0.33333334f};
    Sequence sequence{};
    for (const float bottom : bottoms) {
        for (int i = 0; i < 24; ++i) {
            const bool special = i % 3 == 2;
            std::array<float, 16> values = column_major(make_matrix(sequence, Shape::kScaledW,
                                                                    special));
            values[15] = bottom;
            if (i % 6 == 5) values[7] = 0.5f;  // Genuine perspective is left alone.
            SkM44 m = from_column_major(values);
            m.normalizePerspective();
            append_m44(output, m);
            ++output.cases;
        }
    }
}

void inversion_corpus(Output& output) {
    Sequence sequence{};
    for (int i = 0; i < 400; ++i) {
        const bool special = i % 5 == 4;
        const SkM44 m = make_matrix(sequence, static_cast<Shape>(sequence.pick(4)), special);
        SkM44 inverse = SkM44::Translate(9, 9, 9);
        const bool invertible = m.invert(&inverse);
        append_bool(output, invertible);
        append_m44(output, inverse);
        SkM44 self = m;
        const bool selfInvertible = self.invert(&self);
        append_bool(output, selfInvertible);
        append_m44(output, self);
        ++output.cases;
    }
    // Singular, nearly singular and tiny-scale matrices.
    const std::array<SkM44, 6> special{SkM44::Scale(0, 1, 1), SkM44::Scale(1e-12f, 1e-12f),
                                       SkM44::Scale(1e-20f, 1e-20f), SkM44::Scale(1e20f, 1e20f),
                                       SkM44(1, 2, 3, 4, 2, 4, 6, 8, 0, 0, 1, 0, 0, 0, 0, 1),
                                       SkM44::Scale(corpus::kFloatMin, 1)};
    for (const SkM44& m : special) {
        SkM44 inverse{};
        append_bool(output, m.invert(&inverse));
        append_m44(output, inverse);
        ++output.cases;
    }
}

void map_corpus(Output& output) {
    Sequence sequence{};
    for (int i = 0; i < 300; ++i) {
        const bool special = i % 4 == 3;
        const SkM44 m = make_matrix(sequence, static_cast<Shape>(sequence.pick(4)), special);
        const float x = special ? pick_float(sequence) : pick_ordinary(sequence);
        const float y = special ? pick_float(sequence) : pick_ordinary(sequence);
        const float z = special ? pick_float(sequence) : pick_ordinary(sequence);
        const float w = special ? pick_float(sequence) : pick_ordinary(sequence);
        append_v4(output, m.map(x, y, z, w));
        append_v4(output, m * SkV4{x, y, z, w});
        append_v3(output, m * SkV3{x, y, z});
        ++output.cases;
    }
}

void map_rect_corpus(Output& output) {
    Sequence sequence{};
    for (int i = 0; i < 1200; ++i) {
        const bool special = i % 6 == 5;
        const Shape shape = static_cast<Shape>(sequence.pick(4));
        SkM44 m = make_matrix(sequence, shape, special);
        if (shape == Shape::kPerspective2D && i % 2 == 0) {
            // Small perspective terms put some corners behind the w = 0 plane.
            m.setRC(3, 0, pick_ordinary(sequence) / 16.0f);
            m.setRC(3, 1, pick_ordinary(sequence) / 16.0f);
            m.setRC(3, 3, pick_ordinary(sequence) / 4.0f);
        }
        const SkRect src = make_rect(sequence, special);
        const SkRect mapped = SkMatrixPriv::MapRect(m, src);
        append_float(output, mapped.fLeft);
        append_float(output, mapped.fTop);
        append_float(output, mapped.fRight);
        append_float(output, mapped.fBottom);
        ++output.cases;
    }
}

void vector_corpus(Output& output) {
    Sequence sequence{};
    for (int i = 0; i < 200; ++i) {
        const bool special = i % 4 == 3;
        const SkV3 a{special ? pick_float(sequence) : pick_ordinary(sequence),
                     pick_ordinary(sequence), pick_ordinary(sequence)};
        const SkV3 b{pick_ordinary(sequence), special ? pick_float(sequence) : 0.5f,
                     pick_ordinary(sequence)};
        const float s = special ? pick_float(sequence) : pick_ordinary(sequence);
        append_bool(output, a == b);
        append_bool(output, a != b);
        append_float(output, SkV3::Dot(a, b));
        append_v3(output, SkV3::Cross(a, b));
        append_v3(output, SkV3::Normalize(a));
        append_v3(output, -a);
        append_v3(output, a + b);
        append_v3(output, a - b);
        append_v3(output, a * b);
        append_v3(output, a * s);
        append_v3(output, s * a);
        SkV3 c = a;
        c += b;
        c -= SkV3{s, s, s};
        c *= b;
        c *= s;
        append_v3(output, c);
        append_float(output, a.lengthSquared());
        append_float(output, a.length());
        append_float(output, a.dot(b));
        append_v3(output, a.cross(b));
        append_v3(output, a.normalize());
        append_float(output, a.ptr()[2]);

        const SkV4 p{a.x, a.y, a.z, s};
        const SkV4 q{b.x, b.y, b.z, pick_ordinary(sequence)};
        append_bool(output, p == q);
        append_bool(output, p != q);
        append_float(output, SkV4::Dot(p, q));
        append_v4(output, SkV4::Normalize(p));
        append_v4(output, -p);
        append_v4(output, p + q);
        append_v4(output, p - q);
        append_v4(output, p * q);
        append_v4(output, p * s);
        append_v4(output, s * p);
        append_float(output, p.lengthSquared());
        append_float(output, p.length());
        append_float(output, p.dot(q));
        append_v4(output, p.normalize());
        SkV4 r = p;
        for (int k = 0; k < 4; ++k) {
            append_float(output, p[k]);
            r[k] = q[k] + 1.0f;
        }
        append_v4(output, r);
        ++output.cases;
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 1 && argc != 2) return 2;
    Output output{};
    construction_corpus(output);
    equality_corpus(output);
    concat_corpus(output);
    rotation_corpus(output);
    normalize_corpus(output);
    inversion_corpus(output);
    map_corpus(output);
    map_rect_corpus(output);
    vector_corpus(output);
    if (argc == 2 && !corpus::write_file(argv[1], output)) return 2;
    const unsigned long long hash = corpus::fnv1a(output.bytes);
    std::printf("m44: %zu cases, %zu bytes, hash=%llu PASS\n", output.cases, output.bytes.size(),
                hash);
    return 0;
}
