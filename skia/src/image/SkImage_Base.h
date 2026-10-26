/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkImage_Base_DEFINED
#define SkImage_Base_DEFINED

#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkTypes.h"
#include "src/core/SkMipmap.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

class GrDirectContext;
class GrImageContext;
class SkBitmap;
class SkColorSpace;
class SkPixmap;
class SkRecorder;
class SkSurface;
enum SkYUVColorSpace : int;
struct SkIRect;
struct SkISize;
struct SkImageInfo;

enum {
    kNeedNewImageUniqueID = 0
};

class SkImage_Base : public SkImage {
public:
    ~SkImage_Base() override;

    // From SkImage.h
    sk_sp<SkImage> makeColorSpace(SkRecorder*,
                                  sk_sp<SkColorSpace>,
                                  RequiredProperties) const override;

    sk_sp<SkImage> makeSubset(SkRecorder*, const SkIRect&, RequiredProperties) const override;
    size_t textureSize() const override { return 0; }

    // Methods that we want to use elsewhere in Skia, but not be a part of the public API.
    virtual bool onPeekPixels(SkPixmap*) const { return false; }

    virtual const SkBitmap* onPeekBitmap() const { return nullptr; }

    virtual bool onReadPixels(GrDirectContext*,
                              const SkImageInfo& dstInfo,
                              void* dstPixels,
                              size_t dstRowBytes,
                              int srcX,
                              int srcY,
                              CachingHint) const = 0;

    virtual bool readPixelsGraphite(SkRecorder*, const SkPixmap& dst, int srcX, int srcY) const {
        return false;
    }

    virtual bool onHasMipmaps() const = 0;
    virtual bool onIsProtected() const = 0;

    virtual SkMipmap* onPeekMips() const { return nullptr; }

    sk_sp<SkMipmap> refMips() const {
        SkMipmap* borrowed = onPeekMips();
        sk_sp<SkMipmap> mips = sk_ref_sp(borrowed);
        return mips;
    }

    /**
     * Default implementation does a rescale/read and then calls the callback.
     */
    virtual void onAsyncRescaleAndReadPixels(const SkImageInfo&,
                                             SkIRect srcRect,
                                             RescaleGamma,
                                             RescaleMode,
                                             ReadPixelsCallback,
                                             ReadPixelsContext) const;
    /**
     * Unsupported CPU YUV conversion reports one null-result callback.
     */
    virtual void onAsyncRescaleAndReadPixelsYUV420(SkYUVColorSpace,
                                                   bool readAlpha,
                                                   sk_sp<SkColorSpace> dstColorSpace,
                                                   SkIRect srcRect,
                                                   SkISize dstSize,
                                                   RescaleGamma,
                                                   RescaleMode,
                                                   ReadPixelsCallback,
                                                   ReadPixelsContext) const;

    virtual GrImageContext* context() const { return nullptr; }

    /** this->context() try-casted to GrDirectContext. Useful for migrations – avoid otherwise! */
    virtual GrDirectContext* directContext() const { return nullptr; }

    // If this image is the current cached image snapshot of a surface then this is called when the
    // surface is destroyed to indicate no further writes may happen to surface backing store.
    virtual void generatingSurfaceIsDeleted() {}

    // return a read-only copy of the pixels. We promise to not modify them,
    // but only inspect them (or encode them).
    virtual bool getROPixels(GrDirectContext*, SkBitmap*,
                             CachingHint = kAllow_CachingHint) const = 0;

    virtual sk_sp<SkImage> onMakeSubset(SkRecorder*, const SkIRect&, RequiredProperties) const = 0;

    virtual sk_sp<const SkData> onRefEncoded() const { return nullptr; }

    virtual bool onAsLegacyBitmap(GrDirectContext*, SkBitmap*) const;

    // Create the CPU surface used by makeScaled.
    virtual sk_sp<SkSurface> onMakeSurface(SkRecorder*, const SkImageInfo&) const = 0;

    enum class Type {
        kRaster,
        kRasterPinnable,
        kLazy,
        kLazyPicture,
        kLazyTexture,
        kGanesh,
        kGaneshYUVA,
        kGraphite,
        kGraphiteYUVA,
    };

    virtual Type type() const = 0;

    // Deferred generator-backed images; compatibility kinds remain for now.
    bool isLazyGenerated() const override {
        const Type kind = type();
        const bool lazy = kind == Type::kLazy || kind == Type::kLazyPicture ||
                          kind == Type::kLazyTexture;
        return lazy;
    }

    bool isRasterBacked() const {
        const Type kind = type();
        const bool raster = kind == Type::kRaster || kind == Type::kRasterPinnable;
        return raster;
    }

    // True for images instantiated by Ganesh in GPU memory
    bool isGaneshBacked() const {
        const Type kind = type();
        const bool ganesh = kind == Type::kGanesh || kind == Type::kGaneshYUVA;
        return ganesh;
    }

    // True for images instantiated by Graphite in GPU memory
    bool isGraphiteBacked() const {
        const Type kind = type();
        const bool graphite = kind == Type::kGraphite || kind == Type::kGraphiteYUVA;
        return graphite;
    }

    bool isYUVA() const {
        const Type kind = type();
        const bool yuva = kind == Type::kGaneshYUVA || kind == Type::kGraphiteYUVA;
        return yuva;
    }

    bool isTextureBacked() const override {
        const bool texture = isGaneshBacked() || isGraphiteBacked();
        return texture;
    }

    // Call when this image is part of the key to a resourcecache entry. This allows the cache
    // to know automatically those entries can be purged when this SkImage deleted.
    virtual void notifyAddedToRasterCache() const {
        fAddedToRasterCache.store(true);
    }

    virtual sk_sp<SkImage> onReinterpretColorSpace(sk_sp<SkColorSpace>) const = 0;

    // on failure, returns nullptr
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    virtual sk_sp<SkImage> onMakeWithMipmaps(sk_sp<SkMipmap>) const {
        return nullptr;
    }

protected:
    SkImage_Base(const SkImageInfo& info, uint32_t uniqueID);
private:
    // Set true by caches when they cache content that's derived from the current pixels.

    mutable std::atomic<bool> fAddedToRasterCache{false};
};

static inline SkImage_Base* as_IB(SkImage* image) {
    SkImage_Base* base = static_cast<SkImage_Base*>(image);
    return base;
}

static inline SkImage_Base* as_IB(const sk_sp<SkImage>& image) {
    SkImage_Base* base = static_cast<SkImage_Base*>(image.get());
    return base;
}

static inline const SkImage_Base* as_IB(const SkImage* image) {
    const SkImage_Base* base = static_cast<const SkImage_Base*>(image);
    return base;
}

static inline const SkImage_Base* as_IB(const sk_sp<const SkImage>& image) {
    const SkImage_Base* base = static_cast<const SkImage_Base*>(image.get());
    return base;
}

// A returned borrow cannot extend the lifetime of a temporary owning handle.
static SkImage_Base* as_IB(sk_sp<SkImage>&&) = delete;
static SkImage_Base* as_IB(const sk_sp<SkImage>&&) = delete;
static const SkImage_Base* as_IB(sk_sp<const SkImage>&&) = delete;
static const SkImage_Base* as_IB(const sk_sp<const SkImage>&&) = delete;

#endif
