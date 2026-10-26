#include "include/core/SkBlurTypes.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkImage.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkShader.h"
#include "include/core/SkSurface.h"
#include "src/core/SkDevice.h"
#include "src/shaders/SkImageShader.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

template <typename T>
concept HasCoverageMaskDispatch = requires(T& value) { value.useDrawCoverageMaskForMaskFilters(); };
template <typename T>
concept HasShaderSubsetFactory = requires { &T::MakeSubset; };
#ifndef STX_SKIA_REFERENCE
static_assert(!HasCoverageMaskDispatch<SkDevice>);
static_assert(!HasShaderSubsetFactory<SkImageShader>);
#endif

namespace {
constexpr int sourceWidth = 12;
constexpr int sourceHeight = 10;
constexpr int width = 32;
constexpr int height = 26;
using Pixels = std::array<uint32_t, width * height>;

sk_sp<SkImage> make_image(const SkIRect& admitted, bool altered, bool alphaOnly) {
    const SkColorType type = alphaOnly ? kAlpha_8_SkColorType : kRGBA_8888_SkColorType;
    const SkImageInfo info = SkImageInfo::Make(sourceWidth, sourceHeight, type, kPremul_SkAlphaType);
    std::vector<uint8_t> data(info.computeMinByteSize(), 0);
    const SkPixmap pixels(info, data.data(), info.minRowBytes());
    for (int y = 0; y < sourceHeight; ++y) {
        for (int x = 0; x < sourceWidth; ++x) {
            const bool inside = admitted.contains(x, y);
            const bool change = altered && !inside;
            const SkColor color = change ? 0xf0f010d0U : SkColorSetARGB(
                    64 + (x * 13 + y * 17) % 192, (x * 37) % 256, (y * 43) % 256, (x + y) * 11);
            const SkIRect pixel = SkIRect::MakeXYWH(x, y, 1, 1);
            if (!pixels.erase(color, pixel)) return nullptr;
        }
    }
    sk_sp<SkImage> image = SkImages::RasterFromPixmapCopy(pixels);
    return image;
}

bool render(const sk_sp<SkImage>& image, const SkRect& source,
            const SkSamplingOptions& sampling, int transform, bool blur,
            SkCanvas::SrcRectConstraint constraint, Pixels& pixels) {
    pixels.fill(0);
    const SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
    const sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(info, pixels.data(), width * sizeof(uint32_t));
    if (!surface) return false;
    SkCanvas& canvas = *(*surface).getCanvas();
    if (transform == 1) {
        canvas.translate(1.25f, -0.5f);
        canvas.skew(0.125f, -0.0625f);
    } else if (transform == 2) {
        SkMatrix matrix{};
        matrix.setPerspX(0.01f);
        canvas.concat(matrix);
    }
    SkPaint paint{};
    paint.setAntiAlias(true);
    paint.setShader(SkShaders::Color(0xd049a3e1U));
    if (blur) paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 1.25f));
    const SkRect destination = SkRect::MakeLTRB(5.25f, 4.5f, 27.75f, 21.25f);
    canvas.drawImageRect(image.get(), source, destination, sampling, &paint, constraint);
    return true;
}

bool corpus(std::vector<uint32_t>& output) {
    const std::array<SkRect, 3> sources{SkRect::MakeLTRB(2, 2, 9, 8),
                                      SkRect::MakeLTRB(2.25f, 1.75f, 8.5f, 7.25f),
                                      SkRect::MakeLTRB(-2, -1, 8, 7)};
    const std::array<SkSamplingOptions, 5> options{
        SkSamplingOptions{}, SkSamplingOptions(SkFilterMode::kLinear),
        SkSamplingOptions(SkCubicResampler::Mitchell()),
        SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kLinear),
        SkSamplingOptions::Aniso(4)};
    size_t fastDifferences = 0;
    for (const SkRect& source : sources) {
        const SkIRect admitted = source.roundOut();
        for (int alpha = 0; alpha < 2; ++alpha) {
            const sk_sp<SkImage> original = make_image(admitted, false, alpha != 0);
            const sk_sp<SkImage> changed = make_image(admitted, true, alpha != 0);
            if (!original || !changed) return false;
            for (const SkSamplingOptions& sampling : options) {
                for (int transform = 0; transform < 3; ++transform) {
                    for (int blur = 0; blur < 2; ++blur) {
                        Pixels first{};
                        Pixels second{};
                        if (!render(original, source, sampling, transform, blur != 0,
                                    SkCanvas::kStrict_SrcRectConstraint, first) ||
                            !render(changed, source, sampling, transform, blur != 0,
                                    SkCanvas::kStrict_SrcRectConstraint, second)) return false;
                        if (first != second) return false;
                        bool drawn = false;
                        for (const uint32_t pixel : first) {
                            if (pixel != 0) drawn = true;
                        }
                        if (!drawn) return false;
                        output.insert(output.end(), first.begin(), first.end());
                        if (!render(original, source, sampling, transform, blur != 0,
                                    SkCanvas::kFast_SrcRectConstraint, first) ||
                            !render(changed, source, sampling, transform, blur != 0,
                                    SkCanvas::kFast_SrcRectConstraint, second)) return false;
                        if (first != second) ++fastDifferences;
                        output.insert(output.end(), first.begin(), first.end());
                    }
                }
            }
        }
    }
    std::printf("subset perturbations affect %zu fast cases and zero strict cases\n", fastDifferences);
    const bool exercisedOutsideSamples = fastDifferences > 0;
    return exercisedOutsideSamples;
}
}

int main(int argc, char** argv) {
    if (argc != 1 && argc != 2) return 2;
    std::vector<uint32_t> pixels{};
    if (!corpus(pixels)) return 1;
    if (argc == 2) {
        FILE* file = std::fopen(argv[1], "wb");
        if (file == nullptr) return 2;
        const size_t written = std::fwrite(pixels.data(), sizeof(uint32_t), pixels.size(), file);
        const int closed = std::fclose(file);
        if (written != pixels.size() || closed != 0) return 2;
    }
    std::printf("image subsets: %zu pixels PASS\n", pixels.size());
    return 0;
}
