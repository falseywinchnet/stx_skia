/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/core/SkRasterPipeline.h"

#include "include/core/SkColorType.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkMatrix.h"
#include "include/private/SkDebug.h"
#include "include/private/SkTemplates.h"
#include "modules/skcms/skcms.h"
#include "src/core/SkImageInfoPriv.h"
#include "src/core/SkOpts.h"
#include "src/core/SkRasterPipelineOpContexts.h"
#include "src/core/SkRasterPipelineOpList.h"
#include "src/core/SkVx.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <vector>

using namespace skia_private;
using Op = SkRasterPipelineOp;

bool gForceHighPrecisionRasterPipeline = false;

SkRasterPipeline::SkRasterPipeline(SkArenaAlloc* alloc) : fAlloc(alloc) {
    reset();
}

void SkRasterPipeline::reset() {
    // We intentionally leave the alloc alone here; we don't own it.
    fStages      = nullptr;
    fNumStages   = 0;
    fMemoryCtxInfos.clear();
}

void SkRasterPipeline::append(SkRasterPipelineOp op, void* ctx) {
    SkASSERT(op != Op::uniform_color);            // Please use appendConstantColor().
    SkASSERT(op != Op::unbounded_uniform_color);  // Please use appendConstantColor().
    SkASSERT(op != Op::set_rgb);                  // Please use appendSetRGB().
    SkASSERT(op != Op::unbounded_set_rgb);        // Please use appendSetRGB().
    SkASSERT(op != Op::parametric);               // Please use appendTransferFunction().
    SkASSERT(op != Op::gamma_);                   // Please use appendTransferFunction().
    SkASSERT(op != Op::PQish);                    // Please use appendTransferFunction().
    SkASSERT(op != Op::HLGish);                   // Please use appendTransferFunction().
    SkASSERT(op != Op::HLGinvish);                // Please use appendTransferFunction().
    uncheckedAppend(op, ctx);
}

void SkRasterPipeline::uncheckedAppend(SkRasterPipelineOp op, void* ctx) {
    bool isLoad = false;
    bool isStore = false;
    SkColorType ct = kUnknown_SkColorType;

    switch (op) {
        // Loads (including _dst variants) and stores record their memory context's pixel size.
        case Op::load_a8:
        case Op::load_a8_dst:
            ct = kAlpha_8_SkColorType;
            isLoad = true;
            break;
        case Op::store_a8:
            ct = kAlpha_8_SkColorType;
            isStore = true;
            break;
        case Op::load_565:
        case Op::load_565_dst:
            ct = kRGB_565_SkColorType;
            isLoad = true;
            break;
        case Op::store_565:
            ct = kRGB_565_SkColorType;
            isStore = true;
            break;
        case Op::load_4444:
        case Op::load_4444_dst:
            ct = kARGB_4444_SkColorType;
            isLoad = true;
            break;
        case Op::store_4444:
            ct = kARGB_4444_SkColorType;
            isStore = true;
            break;
        case Op::load_8888:
        case Op::load_8888_dst:
            ct = kRGBA_8888_SkColorType;
            isLoad = true;
            break;
        case Op::store_8888:
            ct = kRGBA_8888_SkColorType;
            isStore = true;
            break;
        case Op::load_rg88:
        case Op::load_rg88_dst:
            ct = kR8G8_unorm_SkColorType;
            isLoad = true;
            break;
        case Op::store_rg88:
            ct = kR8G8_unorm_SkColorType;
            isStore = true;
            break;
        case Op::load_16161616:
        case Op::load_16161616_dst:
            ct = kR16G16B16A16_unorm_SkColorType;
            isLoad = true;
            break;
        case Op::store_16161616:
            ct = kR16G16B16A16_unorm_SkColorType;
            isStore = true;
            break;
        case Op::load_a16:
        case Op::load_a16_dst:
            ct = kA16_unorm_SkColorType;
            isLoad = true;
            break;
        case Op::store_a16:
            ct = kA16_unorm_SkColorType;
            isStore = true;
            break;
        case Op::load_r16:
        case Op::load_r16_dst:
            ct = kR16_unorm_SkColorType;
            isLoad = true;
            break;
        case Op::store_r16:
            ct = kR16_unorm_SkColorType;
            isStore = true;
            break;
        case Op::load_rg1616:
        case Op::load_rg1616_dst:
            ct = kR16G16_unorm_SkColorType;
            isLoad = true;
            break;
        case Op::store_rg1616:
            ct = kR16G16_unorm_SkColorType;
            isStore = true;
            break;
        case Op::load_f16:
        case Op::load_f16_dst:
            ct = kRGBA_F16_SkColorType;
            isLoad = true;
            break;
        case Op::store_f16:
            ct = kRGBA_F16_SkColorType;
            isStore = true;
            break;
        case Op::load_af16:
        case Op::load_af16_dst:
            ct = kA16_float_SkColorType;
            isLoad = true;
            break;
        case Op::store_af16:
            ct = kA16_float_SkColorType;
            isStore = true;
            break;
        case Op::load_rf16:
        case Op::load_rf16_dst:
            ct = kR16_float_SkColorType;
            isLoad = true;
            break;
        case Op::store_rf16:
            ct = kR16_float_SkColorType;
            isStore = true;
            break;
        case Op::load_rgf16:
        case Op::load_rgf16_dst:
            ct = kR16G16_float_SkColorType;
            isLoad = true;
            break;
        case Op::store_rgf16:
            ct = kR16G16_float_SkColorType;
            isStore = true;
            break;
        case Op::load_f32:
        case Op::load_f32_dst:
            ct = kRGBA_F32_SkColorType;
            isLoad = true;
            break;
        case Op::store_f32:
            ct = kRGBA_F32_SkColorType;
            isStore = true;
            break;
        case Op::load_1010102:
        case Op::load_1010102_dst:
            ct = kRGBA_1010102_SkColorType;
            isLoad = true;
            break;
        case Op::store_1010102:
            ct = kRGBA_1010102_SkColorType;
            isStore = true;
            break;
        case Op::load_1010102_xr:
        case Op::load_1010102_xr_dst:
            ct = kBGR_101010x_XR_SkColorType;
            isLoad = true;
            break;
        case Op::store_1010102_xr:
            ct = kBGR_101010x_XR_SkColorType;
            isStore = true;
            break;
        case Op::load_10101010_xr:
        case Op::load_10101010_xr_dst:
            ct = kBGRA_10101010_XR_SkColorType;
            isLoad = true;
            break;
        case Op::store_10101010_xr:
            ct = kBGRA_10101010_XR_SkColorType;
            isStore = true;
            break;
        case Op::load_10x6:
        case Op::load_10x6_dst:
            ct = kRGBA_10x6_SkColorType;
            isLoad = true;
            break;
        case Op::store_10x6:
            ct = kRGBA_10x6_SkColorType;
            isStore = true;
            break;
        case Op::debug_r:
        case Op::debug_g:
        case Op::debug_b:
        case Op::debug_a:
        case Op::debug_r_255:
        case Op::debug_g_255:
        case Op::debug_b_255:
        case Op::debug_a_255:
        case Op::debug_x:
        case Op::debug_y: {
            ct = kRGBA_8888_SkColorType;
            isStore = true;
            break;
        }
        // Odd stage that doesn't have a load variant (appendLoad uses load_a8 + alpha_to_red)
        case Op::store_r8: {
            ct = kR8_unorm_SkColorType;
            isStore = true;
            break;
        }
        case Op::srcover_rgba_8888: {
            ct = kRGBA_8888_SkColorType;
            isLoad = true;
            isStore = true;
            break;
        }
        case Op::scale_u8:
        case Op::lerp_u8: {
            ct = kAlpha_8_SkColorType;
            isLoad = true;
            break;
        }
        case Op::scale_565:
        case Op::lerp_565: {
            ct = kRGB_565_SkColorType;
            isLoad = true;
            break;
        }
        case Op::emboss: {
            // Special-case, this op uses a context that holds *two* MemoryCtxs
            SkRasterPipelineContexts::EmbossCtx& embossCtx =
                    *static_cast<SkRasterPipelineContexts::EmbossCtx*>(ctx);
            addMemoryContext(&embossCtx.add,
                             SkColorTypeBytesPerPixel(kAlpha_8_SkColorType),
                             /*load=*/true, /*store=*/false);
            addMemoryContext(&embossCtx.mul,
                             SkColorTypeBytesPerPixel(kAlpha_8_SkColorType),
                             /*load=*/true, /*store=*/false);
            break;
        }
        default:
            break;
    }

    fStages = (*fAlloc).make<StageList>(StageList{fStages, op, ctx});
    fNumStages += 1;

    if (isLoad || isStore) {
        SkASSERT(ct != kUnknown_SkColorType);
        addMemoryContext(static_cast<SkRasterPipelineContexts::MemoryCtx*>(ctx),
                         SkColorTypeBytesPerPixel(ct),
                         isLoad,
                         isStore);
    }
}

// Packs an immediate value into the context field without forming a pointer from it.
void SkRasterPipeline::append(SkRasterPipelineOp op, uintptr_t ctx) {
    static_assert(sizeof(void*) == sizeof(uintptr_t));
    void* ptrCtx = nullptr;
    std::memcpy(&ptrCtx, &ctx, sizeof(ctx));
    append(op, ptrCtx);
}

void SkRasterPipeline::extend(const SkRasterPipeline& src) {
    if (src.empty()) {
        return;
    }
    // Copy src's list into one array, newest stage last, and chain its oldest stage onto ours.
    StageList* stages = (*fAlloc).makeArrayDefault<StageList>(src.fNumStages);

    const StageList* st = src.fStages;
    for (int n = src.fNumStages - 1; n >= 1; --n) {
        stages[n]      = *st;
        stages[n].prev = &stages[n-1];

        st = (*st).prev;
    }
    stages[0]      = *st;
    stages[0].prev = fStages;

    fStages = &stages[src.fNumStages - 1];
    fNumStages += src.fNumStages;
    for (const SkRasterPipelineContexts::MemoryCtxInfo& info : src.fMemoryCtxInfos) {
        addMemoryContext(info.context, info.bytesPerPixel, info.load, info.store);
    }
}

const char* SkRasterPipeline::GetOpName(SkRasterPipelineOp op) {
    const char* name = "";
    // The op list is an X-macro; expanding it is the only way to name every op.
    switch (op) {
    #define M(x) case Op::x: name = #x; break;
        SK_RASTER_PIPELINE_OPS_ALL(M)
    #undef M
    }
    // An op outside the enumeration keeps the empty name.
    return name;
}

void SkRasterPipeline::dump() const {
    SkDebugf("SkRasterPipeline, %d stages\n", fNumStages);
    std::vector<const char*> stages;
    for (const StageList* st = fStages; st != nullptr; st = (*st).prev) {
        stages.push_back(GetOpName((*st).stage));
    }
    std::reverse(stages.begin(), stages.end());
    for (const char* name : stages) {
        SkDebugf("\t%s\n", name);
    }
    SkDebugf("\n");
}

void SkRasterPipeline::appendSetRGB(SkArenaAlloc* alloc, const float rgb[3]) {
    float* arg = (*alloc).makeArrayDefault<float>(3);
    arg[0] = rgb[0];
    arg[1] = rgb[1];
    arg[2] = rgb[2];

    Op op = Op::unbounded_set_rgb;
    if (0 <= rgb[0] && rgb[0] <= 1 &&
        0 <= rgb[1] && rgb[1] <= 1 &&
        0 <= rgb[2] && rgb[2] <= 1)
    {
        op = Op::set_rgb;
    }

    uncheckedAppend(op, arg);
}

void SkRasterPipeline::appendConstantColor(SkArenaAlloc* alloc, const float rgba[4]) {
    // r,g,b might be outside [0,1], but alpha should probably always be in [0,1].
    SkASSERT(0 <= rgba[3] && rgba[3] <= 1);

    if (rgba[0] == 0 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 1) {
        append(Op::black_color);
    } else if (rgba[0] == 1 && rgba[1] == 1 && rgba[2] == 1 && rgba[3] == 1) {
        append(Op::white_color);
    } else {
        SkRasterPipelineContexts::UniformColorCtx* ctx =
                (*alloc).make<SkRasterPipelineContexts::UniformColorCtx>();
        skvx::float4 color = skvx::float4::Load(rgba);
        // Stores r, g, b and a: the context's four leading float fields.
        color.store(&(*ctx).r);

        // uniform_color requires colors in range and can go lowp,
        // while unbounded_uniform_color supports out-of-range colors too but not lowp.
        if (0 <= rgba[0] && rgba[0] <= rgba[3] &&
            0 <= rgba[1] && rgba[1] <= rgba[3] &&
            0 <= rgba[2] && rgba[2] <= rgba[3]) {
            // To make loads more direct, we store 8-bit values in 16-bit slots.
            color = color * 255.0f + 0.5f;
            (*ctx).rgba[0] = static_cast<uint16_t>(color[0]);
            (*ctx).rgba[1] = static_cast<uint16_t>(color[1]);
            (*ctx).rgba[2] = static_cast<uint16_t>(color[2]);
            (*ctx).rgba[3] = static_cast<uint16_t>(color[3]);
            uncheckedAppend(Op::uniform_color, ctx);
        } else {
            uncheckedAppend(Op::unbounded_uniform_color, ctx);
        }
    }
}

void SkRasterPipeline::appendMatrix(SkArenaAlloc* alloc, const SkMatrix& matrix) {
    const SkMatrix::TypeMask mt = matrix.getType();

    if (mt == SkMatrix::kIdentity_Mask) {
        return;
    }
    if (mt == SkMatrix::kTranslate_Mask) {
        float* trans = (*alloc).makeArrayDefault<float>(2);
        trans[0] = matrix.getTranslateX();
        trans[1] = matrix.getTranslateY();
        append(Op::matrix_translate, trans);
    } else if ((mt | (SkMatrix::kScale_Mask | SkMatrix::kTranslate_Mask)) ==
                     (SkMatrix::kScale_Mask | SkMatrix::kTranslate_Mask)) {
        float* scaleTrans = (*alloc).makeArrayDefault<float>(4);
        scaleTrans[0] = matrix.getScaleX();
        scaleTrans[1] = matrix.getScaleY();
        scaleTrans[2] = matrix.getTranslateX();
        scaleTrans[3] = matrix.getTranslateY();
        append(Op::matrix_scale_translate, scaleTrans);
    } else {
        float* storage = (*alloc).makeArrayDefault<float>(9);
        matrix.get9(storage);
        if (!matrix.hasPerspective()) {
            // note: asAffine and the 2x3 stage really only need 6 entries
            append(Op::matrix_2x3, storage);
        } else {
            append(Op::matrix_perspective, storage);
        }
    }
}

void SkRasterPipeline::appendLoad(SkColorType ct, const SkRasterPipelineContexts::MemoryCtx* ctx) {
    switch (ct) {
        case kUnknown_SkColorType: SkASSERT(false); break;

        case kAlpha_8_SkColorType:           append(Op::load_a8,      ctx); break;
        case kA16_unorm_SkColorType:         append(Op::load_a16,     ctx); break;
        case kA16_float_SkColorType:         append(Op::load_af16,    ctx); break;
        case kRGB_565_SkColorType:           append(Op::load_565,     ctx); break;
        case kARGB_4444_SkColorType:         append(Op::load_4444,    ctx); break;
        case kR8G8_unorm_SkColorType:        append(Op::load_rg88,    ctx); break;
        case kR16_unorm_SkColorType:         append(Op::load_r16,     ctx); break;
        case kR16_float_SkColorType:         append(Op::load_rf16,    ctx); break;
        case kR16G16_unorm_SkColorType:      append(Op::load_rg1616,  ctx); break;
        case kR16G16_float_SkColorType:      append(Op::load_rgf16,   ctx); break;
        case kRGBA_8888_SkColorType:         append(Op::load_8888,    ctx); break;
        case kRGBA_1010102_SkColorType:      append(Op::load_1010102, ctx); break;
        case kR16G16B16A16_unorm_SkColorType:append(Op::load_16161616,ctx); break;
        case kRGBA_F16Norm_SkColorType:
        case kRGBA_F16_SkColorType:          append(Op::load_f16,     ctx); break;
        case kRGBA_F32_SkColorType:          append(Op::load_f32,     ctx); break;
        case kRGBA_10x6_SkColorType:         append(Op::load_10x6,    ctx); break;

        case kGray_8_SkColorType:            append(Op::load_a8, ctx);
                                             append(Op::alpha_to_gray);
                                             break;

        case kR8_unorm_SkColorType:          append(Op::load_a8, ctx);
                                             append(Op::alpha_to_red);
                                             break;

        case kRGB_888x_SkColorType:          append(Op::load_8888, ctx);
                                             append(Op::force_opaque);
                                             break;

        case kBGRA_1010102_SkColorType:      append(Op::load_1010102, ctx);
                                             append(Op::swap_rb);
                                             break;

        case kRGB_101010x_SkColorType:       append(Op::load_1010102, ctx);
                                             append(Op::force_opaque);
                                             break;

        case kBGR_101010x_SkColorType:       append(Op::load_1010102, ctx);
                                             append(Op::force_opaque);
                                             append(Op::swap_rb);
                                             break;

        case kBGRA_10101010_XR_SkColorType:  append(Op::load_10101010_xr, ctx);
                                             append(Op::swap_rb);
                                             break;

        case kBGR_101010x_XR_SkColorType:    append(Op::load_1010102_xr, ctx);
                                             append(Op::force_opaque);
                                             append(Op::swap_rb);
                                             break;
        case kRGB_F16F16F16x_SkColorType:    append(Op::load_f16, ctx);
                                             append(Op::force_opaque);
                                             break;

        case kBGRA_8888_SkColorType:         append(Op::load_8888, ctx);
                                             append(Op::swap_rb);
                                             break;

        case kSRGBA_8888_SkColorType:
            append(Op::load_8888, ctx);
            appendTransferFunction(*skcms_sRGB_TransferFunction());
            break;
    }
}

void SkRasterPipeline::appendLoadDst(SkColorType ct,
                                     const SkRasterPipelineContexts::MemoryCtx* ctx) {
    switch (ct) {
        case kUnknown_SkColorType: SkASSERT(false); break;

        case kAlpha_8_SkColorType:            append(Op::load_a8_dst,      ctx); break;
        case kA16_unorm_SkColorType:          append(Op::load_a16_dst,     ctx); break;
        case kA16_float_SkColorType:          append(Op::load_af16_dst,    ctx); break;
        case kRGB_565_SkColorType:            append(Op::load_565_dst,     ctx); break;
        case kARGB_4444_SkColorType:          append(Op::load_4444_dst,    ctx); break;
        case kR8G8_unorm_SkColorType:         append(Op::load_rg88_dst,    ctx); break;
        case kR16_unorm_SkColorType:          append(Op::load_r16_dst,     ctx); break;
        case kR16_float_SkColorType:          append(Op::load_rf16_dst,    ctx); break;
        case kR16G16_unorm_SkColorType:       append(Op::load_rg1616_dst,  ctx); break;
        case kR16G16_float_SkColorType:       append(Op::load_rgf16_dst,   ctx); break;
        case kRGBA_8888_SkColorType:          append(Op::load_8888_dst,    ctx); break;
        case kRGBA_1010102_SkColorType:       append(Op::load_1010102_dst, ctx); break;
        case kR16G16B16A16_unorm_SkColorType: append(Op::load_16161616_dst,ctx); break;
        case kRGBA_F16Norm_SkColorType:
        case kRGBA_F16_SkColorType:           append(Op::load_f16_dst,     ctx); break;
        case kRGBA_F32_SkColorType:           append(Op::load_f32_dst,     ctx); break;
        case kRGBA_10x6_SkColorType:          append(Op::load_10x6_dst,    ctx); break;

        case kGray_8_SkColorType:             append(Op::load_a8_dst, ctx);
                                              append(Op::alpha_to_gray_dst);
                                              break;

        case kR8_unorm_SkColorType:           append(Op::load_a8_dst, ctx);
                                              append(Op::alpha_to_red_dst);
                                              break;

        case kRGB_888x_SkColorType:           append(Op::load_8888_dst, ctx);
                                              append(Op::force_opaque_dst);
                                              break;

        case kBGRA_1010102_SkColorType:       append(Op::load_1010102_dst, ctx);
                                              append(Op::swap_rb_dst);
                                              break;

        case kRGB_101010x_SkColorType:        append(Op::load_1010102_dst, ctx);
                                              append(Op::force_opaque_dst);
                                              break;

        case kBGR_101010x_SkColorType:        append(Op::load_1010102_dst, ctx);
                                              append(Op::force_opaque_dst);
                                              append(Op::swap_rb_dst);
                                              break;

        case kBGR_101010x_XR_SkColorType:     append(Op::load_1010102_xr_dst, ctx);
                                              append(Op::force_opaque_dst);
                                              append(Op::swap_rb_dst);
                                              break;

        case kBGRA_10101010_XR_SkColorType:   append(Op::load_10101010_xr_dst, ctx);
                                              append(Op::swap_rb_dst);
                                              break;
        case kRGB_F16F16F16x_SkColorType:     append(Op::load_f16_dst, ctx);
                                              append(Op::force_opaque_dst);
                                              break;

        case kBGRA_8888_SkColorType:          append(Op::load_8888_dst, ctx);
                                              append(Op::swap_rb_dst);
                                              break;

        case kSRGBA_8888_SkColorType:
            // TODO: We could remove the double-swap if we had _dst versions of all the TF stages
            append(Op::load_8888_dst, ctx);
            append(Op::swap_src_dst);
            appendTransferFunction(*skcms_sRGB_TransferFunction());
            append(Op::swap_src_dst);
            break;
    }
}

void SkRasterPipeline::appendStore(SkColorType ct, const SkRasterPipelineContexts::MemoryCtx* ctx) {
    switch (ct) {
        case kUnknown_SkColorType: SkASSERT(false); break;

        case kAlpha_8_SkColorType:            append(Op::store_a8,      ctx); break;
        case kR8_unorm_SkColorType:           append(Op::store_r8,      ctx); break;
        case kA16_unorm_SkColorType:          append(Op::store_a16,     ctx); break;
        case kA16_float_SkColorType:          append(Op::store_af16,    ctx); break;
        case kRGB_565_SkColorType:            append(Op::store_565,     ctx); break;
        case kARGB_4444_SkColorType:          append(Op::store_4444,    ctx); break;
        case kR8G8_unorm_SkColorType:         append(Op::store_rg88,    ctx); break;
        case kR16_unorm_SkColorType:          append(Op::store_r16,     ctx); break;
        case kR16_float_SkColorType:          append(Op::store_rf16,    ctx); break;
        case kR16G16_unorm_SkColorType:       append(Op::store_rg1616,  ctx); break;
        case kR16G16_float_SkColorType:       append(Op::store_rgf16,   ctx); break;
        case kRGBA_8888_SkColorType:          append(Op::store_8888,    ctx); break;
        case kRGBA_1010102_SkColorType:       append(Op::store_1010102, ctx); break;
        case kR16G16B16A16_unorm_SkColorType: append(Op::store_16161616,ctx); break;
        case kRGBA_F16Norm_SkColorType:
        case kRGBA_F16_SkColorType:           append(Op::store_f16,     ctx); break;
        case kRGBA_F32_SkColorType:           append(Op::store_f32,     ctx); break;
        case kRGBA_10x6_SkColorType:          append(Op::store_10x6,    ctx); break;

        case kRGB_888x_SkColorType:           append(Op::force_opaque);
                                              append(Op::store_8888, ctx);
                                              break;

        case kBGRA_1010102_SkColorType:       append(Op::swap_rb);
                                              append(Op::store_1010102, ctx);
                                              break;

        case kRGB_101010x_SkColorType:        append(Op::force_opaque);
                                              append(Op::store_1010102, ctx);
                                              break;

        case kBGR_101010x_SkColorType:        append(Op::force_opaque);
                                              append(Op::swap_rb);
                                              append(Op::store_1010102, ctx);
                                              break;

        case kBGR_101010x_XR_SkColorType:     append(Op::force_opaque);
                                              append(Op::swap_rb);
                                              append(Op::store_1010102_xr, ctx);
                                              break;
        case kRGB_F16F16F16x_SkColorType:     append(Op::force_opaque);
                                              append(Op::store_f16, ctx);
                                              break;

        case kBGRA_10101010_XR_SkColorType:   append(Op::swap_rb);
                                              append(Op::store_10101010_xr, ctx);
                                              break;

        case kGray_8_SkColorType:             append(Op::bt709_luminance_or_luma_to_alpha);
                                              append(Op::store_a8, ctx);
                                              break;

        case kBGRA_8888_SkColorType:          append(Op::swap_rb);
                                              append(Op::store_8888, ctx);
                                              break;

        case kSRGBA_8888_SkColorType:
            appendTransferFunction(*skcms_sRGB_Inverse_TransferFunction());
            append(Op::store_8888, ctx);
            break;
    }
}

void SkRasterPipeline::appendTransferFunction(const skcms_TransferFunction& tf) {
    void* ctx = const_cast<void*>(static_cast<const void*>(&tf));
    switch (skcms_TransferFunction_getType(&tf)) {
        default: SkASSERT(false); break;

        case skcms_TFType_sRGBish:
            if (tf.a == 1 && tf.b == 0 && tf.c == 0 && tf.d == 0 && tf.e == 0 && tf.f == 0) {
                uncheckedAppend(Op::gamma_, ctx);
            } else {
                uncheckedAppend(Op::parametric, ctx);
            }
            break;
        case skcms_TFType_PQish:     uncheckedAppend(Op::PQish,     ctx); break;
        case skcms_TFType_HLGish:    uncheckedAppend(Op::HLGish,    ctx); break;
        case skcms_TFType_HLGinvish: uncheckedAppend(Op::HLGinvish, ctx); break;
    }
}

// GPUs clamp all color channels to the limits of the format just before the blend step. To match
// that auto-clamp, the RP blitter uses this helper immediately before appending blending stages.
void SkRasterPipeline::appendClampIfNormalized(const SkImageInfo& info) {
    if (SkColorTypeIsNormalized(info.colorType())) {
        uncheckedAppend(Op::clamp_01, nullptr);
    }
}

// Writes one stage immediately before ip and moves ip onto it. The caller sizes the program
// for every stage it prepends.
static void prepend_to_pipeline(SkRasterPipelineStage*& ip, SkOpts::StageFn stageFn, void* ctx) {
    --ip;
    (*ip).fn = stageFn;
    (*ip).ctx = ctx;
}

bool SkRasterPipeline::buildLowpPipeline(SkRasterPipelineStage* ip) const {
    if (gForceHighPrecisionRasterPipeline) {
        return false;
    }
    // Stages are stored backwards in fStages; to compensate, we assemble the pipeline in reverse
    // here, back to front.
    prepend_to_pipeline(ip, SkOpts::just_return_lowp, /*ctx=*/nullptr);
    for (const StageList* st = fStages; st != nullptr; st = (*st).prev) {
        const int opIndex = static_cast<int>((*st).stage);
        SkASSERT_RELEASE(opIndex >= 0 && opIndex < kNumRasterPipelineHighpOps);
        if (opIndex >= kNumRasterPipelineLowpOps || !SkOpts::ops_lowp[opIndex]) {
            // This program contains a stage that doesn't exist in lowp.
            return false;
        }
        prepend_to_pipeline(ip, SkOpts::ops_lowp[opIndex], (*st).ctx);
    }
    return true;
}

void SkRasterPipeline::buildHighpPipeline(SkRasterPipelineStage* ip) const {
    // We assemble the pipeline in reverse, since the stage list is stored backwards.
    prepend_to_pipeline(ip, SkOpts::just_return_highp, /*ctx=*/nullptr);
    for (const StageList* st = fStages; st != nullptr; st = (*st).prev) {
        const int opIndex = static_cast<int>((*st).stage);
        SkASSERT_RELEASE(opIndex >= 0 && opIndex < kNumRasterPipelineHighpOps);
        prepend_to_pipeline(ip, SkOpts::ops_highp[opIndex], (*st).ctx);
    }
}

SkRasterPipeline::StartPipelineFn SkRasterPipeline::buildPipeline(SkRasterPipelineStage* ip) const {
    // We try to build a lowp pipeline first; if that fails, we fall back to a highp float pipeline.
    if (buildLowpPipeline(ip)) {
        return SkOpts::start_pipeline_lowp;
    }

    buildHighpPipeline(ip);
    return SkOpts::start_pipeline_highp;
}

int SkRasterPipeline::stagesNeeded() const {
    // Add 1 to budget for a `just_return` stage at the end.
    const int stages = fNumStages + 1;

    return stages;
}

void SkRasterPipeline::run(size_t x, size_t y, size_t w, size_t h) const {
    if (empty()) {
        return;
    }

    const int stageCount = stagesNeeded();

    // Best to not use fAlloc here... we can't bound how often run() will be called.
    AutoSTMalloc<32, SkRasterPipelineStage> program(stageCount);

    const size_t numMemoryCtxs = fMemoryCtxInfos.size();
    AutoSTMalloc<2, SkRasterPipelineContexts::MemoryCtxPatch> patches(numMemoryCtxs);
    for (size_t i = 0; i < numMemoryCtxs; ++i) {
        patches[i].info = fMemoryCtxInfos[i];
        patches[i].backup = nullptr;
        std::memset(patches[i].scratch, 0, sizeof(patches[i].scratch));
    }

    // The program is assembled backwards from its end.
    const StartPipelineFn start_pipeline = buildPipeline(program.get() + stageCount);
    const SkSpan<SkRasterPipelineContexts::MemoryCtxPatch> patchSpan(patches.data(), numMemoryCtxs);
    start_pipeline(x, y, x + w, y + h, program.get(), patchSpan);
}

namespace {

// The compiled thunk for an empty pipeline does nothing.
struct EmptyCompiledPipeline {
    void operator()(size_t, size_t, size_t, size_t) const {}
};

// A compiled pipeline borrows its program and memory-context patches from the pipeline's arena.
// Copies share that storage; invocations must not overlap, because they patch the same contexts.
struct CompiledPipeline {
    void (*fStart)(size_t, size_t, size_t, size_t, SkRasterPipelineStage*,
                   SkSpan<SkRasterPipelineContexts::MemoryCtxPatch>) = nullptr;
    SkRasterPipelineStage* fProgram = nullptr;
    SkRasterPipelineContexts::MemoryCtxPatch* fPatches = nullptr;
    size_t fPatchCount = 0;

    void operator()(size_t x, size_t y, size_t w, size_t h) const {
        const SkSpan<SkRasterPipelineContexts::MemoryCtxPatch> patches(fPatches, fPatchCount);
        fStart(x, y, x + w, y + h, fProgram, patches);
    }
};

}  // namespace

std::function<void(size_t, size_t, size_t, size_t)> SkRasterPipeline::compile() const {
    if (empty()) {
        const std::function<void(size_t, size_t, size_t, size_t)> nothing =
                EmptyCompiledPipeline{};
        return nothing;
    }

    const int stageCount = stagesNeeded();

    SkRasterPipelineStage* program = (*fAlloc).makeArray<SkRasterPipelineStage>(stageCount);

    const size_t numMemoryCtxs = fMemoryCtxInfos.size();
    SkRasterPipelineContexts::MemoryCtxPatch* patches =
            (*fAlloc).makeArray<SkRasterPipelineContexts::MemoryCtxPatch>(numMemoryCtxs);
    for (size_t i = 0; i < numMemoryCtxs; ++i) {
        patches[i].info = fMemoryCtxInfos[i];
        patches[i].backup = nullptr;
        std::memset(patches[i].scratch, 0, sizeof(patches[i].scratch));
    }

    // The program is assembled backwards from its end.
    const StartPipelineFn start_pipeline = buildPipeline(program + stageCount);
    const CompiledPipeline compiled{start_pipeline, program, patches, numMemoryCtxs};
    const std::function<void(size_t, size_t, size_t, size_t)> thunk = compiled;
    return thunk;
}

void SkRasterPipeline::addMemoryContext(SkRasterPipelineContexts::MemoryCtx* ctx,
                                        int bytesPerPixel,
                                        bool load,
                                        bool store) {
    // Each context is recorded once; later uses widen its load and store flags.
    for (SkRasterPipelineContexts::MemoryCtxInfo& info : fMemoryCtxInfos) {
        if (info.context == ctx) {
            SkASSERT(bytesPerPixel == info.bytesPerPixel);
            info.load = info.load || load;
            info.store = info.store || store;
            return;
        }
    }
    fMemoryCtxInfos.push_back(
            SkRasterPipelineContexts::MemoryCtxInfo{ctx, bytesPerPixel, load, store});
}
