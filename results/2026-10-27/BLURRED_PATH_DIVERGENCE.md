# Blurred path and point divergence — 2026-10-27

The canvas and device unit (0453cb1, `../2026-10-26/CANVAS_DEVICE_CONVERSION.md`) found that
blurred paths and blurred point draws differ from the reference, and that the difference
predates that unit. This record finds where it came from.

## Verdict

The difference is **not an intended correction**. Commit c046c51 ("Convert blur composition and
optimize exact coverage products", 2026-10-25) added two input rejections to `SkBlurMask`. In
both cases the mask filter reports failure, and the draw falls back to the **unblurred**
geometry. The blur is silently lost. Neither rejection is described in
`../2026-10-25/BLUR_STYLES.md`, which states that mask output is byte-identical, and its corpus
contains neither input. The original code is memory-safe on both inputs (see below), so the
rejections remove no defect.

## Bisection

Two temporary probes were built against the library at each commit, in a separate worktree
with its own mirror. Each probe prints one hash per drawn bitmap.

- **Path probe:** 288 blurred path draws, covering the fill rules (including inverse fills),
  styles and anti-aliasing.
- **Point probe:** 216 blurred `drawPoints` cases: three point modes, stroke widths 0, 0.5, 2.5
  and 6, every cap, both anti-aliasing settings and three styles. The blur is normal style with
  sigma 1.5.

| Commit | Path cases differing | Point cases differing |
|---|---:|---:|
| reference profile | 0 | 0 |
| 0b9f2be (parent) | 0 | 0 |
| c046c51 | 144 (all inverse fills) | 12 |

The 12 point cases are all point mode at stroke width 0.5 with butt or square caps. Reference
hashes in those cases do not depend on anti-aliasing. Candidate hashes split by anti-aliasing
setting, which is what unblurred fallback drawing would produce. The probes were temporary and
are not committed.

## Mechanisms

The mechanisms come from reading the code at c046c51 and following its failure returns. They
match the probe pattern above. No build with the two rejections removed has been run yet.

1. **Inverse fills: the BoxBlur extent check.** `SkBlurMask::BoxBlur` now begins with:

       if (width <= 0 || height <= 0 || width > INT32_MAX || height > INT32_MAX) return false;

   For an inverse fill, `SkDraw`'s mask path asks the filter for its margin with a
   metadata-only mask (`fImage == nullptr`). That mask's bounds are the saturated infinite
   rectangle, so its 64-bit width is about 2^32 and the check fails. `compute_mask_bounds`
   returns false, `DrawToMask` returns false, and the filter's `filterPath` fails. The draw then
   continues unblurred. The original had no such check. Its metadata path only computes the
   border; the real mask is later sized to the clip-intersected bounds. Every blurred
   inverse-filled path is affected.

2. **Narrow rectangles: the BlurRect profile check.** `SkBlurMask::BlurRect` now rejects:

       if (width < profileSize || height < profileSize) return false;

   Here `width` and `height` are the padded mask extents. A 0.5 × 0.5 point square with sigma 1.5
   pads to 8 or 9 pixels, but the profile size is ceil(6 × 1.5) = 9. `filterRectMask` fails,
   `filterRectsToNine` returns `kFalse`, `filterRects` passes it through, and `filterPath`
   returns false. Rectangle-shaped paths therefore fall back unblurred. Round caps produce a
   non-rectangular path and are unaffected. The original accepts the case: in
   `ComputeBlurredScanline`, `sw = width - profile_size` wraps as unsigned, so the
   `profile_size <= sw` branch selects `ProfileLookup` with a negative sharp width.

## Why the original is safe on these inputs

- **Inverse fills.** The metadata-only query allocates nothing and returns the border from the
  blur filter's own destination preparation. Pixel allocation happens later, for the
  clip-bounded mask, and keeps its own size checks.
- **Narrow rectangles.** Let W < P be the padded width and the profile size, and let
  c = (P & ~1) − 1 ≤ P − 1. Then the sharp width is w = (W − P) − c. In `ProfileLookup`,
  |2x + 1 − W| ≤ W − 1, so dx ≤ P − 1 + c ≤ 2P − 2 and the index ox = max(dx >> 1, 0) ≤ P − 1.
  This stays inside the P-entry profile. Both lines of that function stay far inside `int`
  range at the extents BlurRect already admits.

## Effect on records

- The canvas corpus's exclusion of blurred paths and points (`tests/canvas_device.cpp`) is
  caused by this regression. It should be lifted once the regression is fixed.
- The c046c51 rejections that are documented remain separate decisions: invalid style and
  enumerations, non-finite inputs, padded coordinate limits, and the inconsistent-rounding
  rejection for fractional inner rectangles.

## Suggested fix (not applied)

- **BoxBlur:** apply the extent limit only when pixels are produced. A metadata-only query keeps
  the original border computation.
- **BlurRect:** remove the profile-size rejection. Restore the original narrow-rectangle result,
  the profile lookup with the negative sharp width, in signed arithmetic that is defined. Relax
  the matching `ComputeBlurredScanline` assertion.
- **Tests:** add both inputs to the mask corpus and the canvas corpus, plus candidate-only
  bound checks. Rerun the sequential gates.
