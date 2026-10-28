/*
 * Copyright 2018 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/core/SkOpts.h"

#if !defined(SK_ENABLE_OPTIMIZE_SIZE)

#define SK_OPTS_NS ml3
#include "src/opts/SkRasterPipeline_opts.h"

namespace SkOpts {
    void Init_ml3() {
        raster_pipeline_lowp_stride  = SK_OPTS_NS::raster_pipeline_lowp_stride();
        raster_pipeline_highp_stride = SK_OPTS_NS::raster_pipeline_highp_stride();

        // The stage tables are in SkRasterPipelineOp order.
        for (size_t i = 0; i < SK_OPTS_NS::kHighpStages.size(); ++i) {
            ops_highp[i] = reinterpret_cast<StageFn>(SK_OPTS_NS::kHighpStages[i].stage);
        }
        just_return_highp = reinterpret_cast<StageFn>(SK_OPTS_NS::just_return);
        start_pipeline_highp = SK_OPTS_NS::start_pipeline;

        for (size_t i = 0; i < SK_OPTS_NS::lowp::kLowpStages.size(); ++i) {
            ops_lowp[i] = reinterpret_cast<StageFn>(SK_OPTS_NS::lowp::kLowpStages[i].stage);
        }
        just_return_lowp = reinterpret_cast<StageFn>(SK_OPTS_NS::lowp::just_return);
        start_pipeline_lowp = SK_OPTS_NS::lowp::start_pipeline;
    }
}  // namespace SkOpts

#endif // SK_ENABLE_OPTIMIZE_SIZE
