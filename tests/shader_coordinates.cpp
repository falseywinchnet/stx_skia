#include "include/core/SkCanvas.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkShader.h"
#include "include/core/SkSurface.h"
#include "src/shaders/SkLocalMatrixShader.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

namespace {
template <typename T>
concept TemporaryLocalMatrix = requires(T&& value) { std::move(value).localMatrix(); };
template <typename T>
concept TemporaryCTM = requires(T&& value) { std::move(value).ctm(); };
template <typename T>
concept TemporaryImage = requires(T&& value) { std::move(value).isAImage(nullptr, nullptr); };
#ifndef STX_SKIA_REFERENCE
static_assert(!TemporaryLocalMatrix<SkLocalMatrixShader>);
static_assert(!TemporaryCTM<SkCTMShader>);
static_assert(!TemporaryImage<SkLocalMatrixShader>);
#endif

constexpr int width = 19;
constexpr int height = 13;
constexpr uint32_t background = 0xff173963U;
using Pixels = std::array<uint32_t, width * height>;

bool render(const sk_sp<SkShader>& shader, const SkMatrix& transform, Pixels& pixels) {
    pixels.fill(background);
    const SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
    const sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(info, pixels.data(), width * sizeof(uint32_t));
    if (!surface) return false;
    SkCanvas& canvas = *(*surface).getCanvas();
    canvas.concat(transform);
    SkPaint paint{};
    paint.setShader(shader);
    paint.setBlendMode(SkBlendMode::kSrc);
    canvas.drawPaint(paint);
    return true;
}

sk_sp<SkImage> make_image() {
    const SkImageInfo info = SkImageInfo::MakeN32Premul(7, 5);
    const sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
    if (!surface) return nullptr;
    SkCanvas& canvas = *(*surface).getCanvas();
    SkPaint paint{};
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 7; ++x) {
            const SkColor color = SkColorSetARGB(255, x * 31, y * 47, (x + y) * 19);
            paint.setColor(color);
            const SkRect rectangle = SkRect::MakeXYWH(static_cast<float>(x), static_cast<float>(y), 1, 1);
            canvas.drawRect(rectangle, paint);
        }
    }
    sk_sp<SkImage> image = (*surface).makeImageSnapshot();
    return image;
}

bool coordinate_cases(std::vector<uint32_t>& output) {
    const sk_sp<SkImage> image = make_image();
    if (!image) return false;
    SkMatrix first{};
    first.setScaleTranslate(1.5f, 0.75f, 2.0f, -1.0f);
    SkMatrix second{};
    second.setSkew(0.25f, -0.125f);
    const SkMatrix combined = SkShaderBase::ConcatLocalMatrices(second, first);
    const std::array<SkTileMode, 3> modes{SkTileMode::kClamp, SkTileMode::kRepeat, SkTileMode::kMirror};
    const std::array<SkFilterMode, 2> filters{SkFilterMode::kNearest, SkFilterMode::kLinear};
    SkMatrix perspective{};
    perspective.setPerspX(0.015625f);
    perspective.setPerspY(-0.0078125f);
    const std::array<SkMatrix, 3> transforms{SkMatrix::I(), combined, perspective};
    for (const SkTileMode mode : modes) {
        for (const SkFilterMode filter : filters) {
            const SkSamplingOptions sampling(filter);
            const sk_sp<SkShader> base = (*image).makeShader(mode, mode, sampling);
            if (!base) return false;
            const sk_sp<SkShader> inner = (*base).makeWithLocalMatrix(first);
            const sk_sp<SkShader> nested = (*inner).makeWithLocalMatrix(second);
            const sk_sp<SkShader> flattened = (*base).makeWithLocalMatrix(combined);
            SkMatrix reported{};
            std::array<SkTileMode, 2> reportedModes{};
            SkImage* reportedImage = (*nested).isAImage(&reported, reportedModes.data());
            if (reportedImage != image.get() || reported != combined ||
                reportedModes[0] != mode || reportedModes[1] != mode) return false;
            const SkRect subset = SkRect::MakeLTRB(1.5f, 1.5f, 4.5f, 3.5f);
            const sk_sp<SkShader> clamped = SkShaders::CoordClamp(nested, subset);
            if (!clamped) return false;
            for (const SkMatrix& transform : transforms) {
                Pixels nestedPixels{};
                Pixels flatPixels{};
                Pixels clampPixels{};
                if (!render(nested, transform, nestedPixels) ||
                    !render(flattened, transform, flatPixels) ||
                    !render(clamped, transform, clampPixels)) return false;
                if (nestedPixels != flatPixels) return false;
                output.insert(output.end(), nestedPixels.begin(), nestedPixels.end());
                output.insert(output.end(), clampPixels.begin(), clampPixels.end());
            }
        }
    }
    // With identity coordinates, all pixels beyond each clamp boundary sample
    // the same edge. This checks clamp behavior independently of the reference.
    const SkSamplingOptions nearest(SkFilterMode::kNearest);
    const sk_sp<SkShader> base = (*image).makeShader(SkTileMode::kRepeat, SkTileMode::kRepeat, nearest);
    const SkRect subset = SkRect::MakeLTRB(2.5f, 1.5f, 4.5f, 3.5f);
    const sk_sp<SkShader> clamped = SkShaders::CoordClamp(base, subset);
    Pixels pixels{};
    if (!render(clamped, SkMatrix::I(), pixels)) return false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int edgeX = x < 2 ? 2 : (x > 4 ? 4 : x);
            const int edgeY = y < 1 ? 1 : (y > 3 ? 3 : y);
            if (pixels[y * width + x] != pixels[edgeY * width + edgeX]) return false;
        }
    }
    const SkRect inverted = SkRect::MakeLTRB(4, 0, 1, 3);
    if (SkShaders::CoordClamp(base, inverted) || SkShaders::CoordClamp(nullptr, subset)) return false;
    return true;
}

bool empty_and_singular() {
    const sk_sp<SkShader> empty = SkShaders::Empty();
    SkMatrix singular{};
    singular.setScale(0.0f, 1.0f);
    const sk_sp<SkImage> image = make_image();
    if (!image) return false;
    const SkSamplingOptions sampling{};
    const sk_sp<SkShader> base = (*image).makeShader(SkTileMode::kClamp, SkTileMode::kClamp, sampling);
    const sk_sp<SkShader> wrapped = (*base).makeWithLocalMatrix(singular);
    const std::array<sk_sp<SkShader>, 2> shaders{empty, wrapped};
    for (const sk_sp<SkShader>& shader : shaders) {
        Pixels pixels{};
        if (!render(shader, SkMatrix::I(), pixels)) return false;
        for (const uint32_t pixel : pixels) {
            if (pixel != background) return false;
        }
    }
    return true;
}
}

int main(int argc, char** argv) {
    std::vector<uint32_t> output{};
    if (!coordinate_cases(output)) return 1;
    if (!empty_and_singular()) return 2;
    if (argc == 2) {
        FILE* file = std::fopen(argv[1], "wb");
        if (file == nullptr) return 3;
        const size_t written = std::fwrite(output.data(), sizeof(uint32_t), output.size(), file);
        const int closed = std::fclose(file);
        if (written != output.size() || closed != 0) return 3;
    }
    std::printf("shader coordinates: %zu pixels PASS\n", output.size());
    return 0;
}
