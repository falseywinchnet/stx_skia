# Full conversion ledger

The objective is the complete owned CPU raster library described by the intake
and coordinator requirements. A compatible first package is not completion.
Every retained production implementation and required header must follow the
house style, with explicit ownership, arithmetic, callbacks and failure behavior.
Unused code must be removed through demonstrated dependency closure, not hidden
behind an unsupported success path. Imported baseline files are reference-only.

## Converted boundaries

| Boundary | Implementation and headers | Evidence |
|---|---|---|
| Immutable data storage | `src/core/SkData.cpp`, `include/core/SkData.h` | Subset bounds, copy ranges, shared ownership, release timing, streams, zero initialization |
| Pixel storage identity | `src/core/SkPixelStorage.cpp`, `include/private/SkPixelStorage.h` | Stable storage identity and concurrent content counters |
| Pixel ownership and generation | `src/core/SkPixelRef.cpp`, `include/core/SkPixelRef.h`, `src/core/SkPixelRefPriv.h` | Borrowed/callback ownership, one-shot invalidation, generation replacement, shared identity reads |
| Pixel allocation/data retention | `src/core/SkMallocPixelRef.cpp`, `include/core/SkMallocPixelRef.h` | Zeroed allocation, padded stride, insufficient extent rejection, retained immutable data |
| Strong/weak reference-count implementation | `include/core/SkRefCnt.h`, `include/private/SkWeakRefCnt.h` | Copy/move/reset, reentrant release, concurrent sharing, weak lock/expiry; compatibility declarations remain as noted below |
| Borrowed view implementation | `include/core/SkSpan.h` | Empty views, all valid slices of 0–33 elements, reverse traversal, const conversion, compile-time rejection of temporary owning containers |
| Pixel extent and temporary allocation | `src/core/SkAutoPixmapStorage.cpp/.h`, `src/core/SkSafeMath.cpp/.h`, `src/core/SkReadPixelsRec.cpp/.h`, `src/core/SkWritePixelsRec.cpp/.h`, `src/core/SkImageInfo.cpp`, `include/core/SkImageInfo.h` | Checked arithmetic boundaries, clipped transfers, allocation/move/failure states and image metadata |
| Pixel conversion and row copies | `src/core/SkConvertPixels.cpp/.h`, `src/core/SkRectMemcpy.h` | 21,420 byte-exact format/alpha/color-space cases, row padding, exact extents, in-place swaps and invalid-input rejection |
| Pixel views and scaling | `include/core/SkPixmap.h`, `src/core/SkPixmap.cpp`, `src/core/SkPixmapDraw.cpp` | Color getters, alpha/opacity, clipped reads/fills/subsets, named fill kernels, temporary-metadata borrow rejection and five sampling modes |
| Mask descriptors and Gaussian blur | `src/core/SkMask.cpp/.h`, `src/core/SkMaskBlurFilter.cpp/.h` | Checked padded bounds, explicit borrowed storage, move-empty descriptors, exact last-row extents and 304 reference blur cases |
| Blur style composition and analytic rectangles | `src/core/SkBlurMask.cpp/.h` | All four styles, exact analytic profiles, checked temporary extents, padded reference convolution, bounded 16-bit coverage arithmetic |
| Cached pixel/mask storage | `src/core/SkCachedData.cpp/.h` | Cache/client reference distinction, discard/reacquire and failure states, callback ordering, final release after unlocking and concurrent reference sharing |
| Mask-filter dispatch and nine-patch ownership | `src/core/SkMaskFilterBase.cpp/.h`, `src/core/SkMaskFilter.cpp`; existing public header reviewed | Adopted cache/allocation owners, failure cleanup, bounded clipped run storage, named image-filter result and 240 rendering comparisons |
| Change listeners | `src/core/SkIDChangeListener.cpp`, `include/private/SkIDChangeListener.h` | Registration cleanup, deregistration, reset, notification and destruction |

Paths in this table are relative to `skia/`. The second conversion batch preserves
arithmetic, callback order and atomic memory ordering. It replaces procedure-local
owner classes with named implementation classes and initializes generation and
mutability state directly. It does not claim a new pixel-kernel optimization.

Listeners execute under their list mutex and may not reenter it. Destruction
requires exclusive ownership. A custom pixel release executes before base-class
cache invalidation; listeners cannot inspect released pixels. Mutability/dimension
updates and pixel edits require external synchronization. Atomic generation-ID
acquisition is not a promise of concurrent pixel mutation support.

Original versions of converted files live under `baseline/skia/`, with the first
SkData implementation at `baseline/SkData.cpp`. `cmake/reference.cmake` selects
original implementations; the baseline include path selects original converted
headers. Unchanged dependencies are shared. This prevents a conversion from
silently changing both sides of its differential test.

## Reference and view conversion notes

Reference count increments/decrements and weak locking retain the original atomic
memory orders. Reset publishes replacement storage before releasing the previous
owner, preserving reentrant release behavior. The named weak-lock retry state
makes failed compare/exchange updates explicit.

SkSpan now accepts owning containers only as lvalues. Temporary SkSpan/std::span
views remain admitted because copying a view does not own or shorten its backing
allocation. Initializer-list views remain statement-lifetime borrows, annotated
for the compiler. Their documentation now shows the actual lifetime distinction:
a named initializer_list extends its backing-array lifetime, while a span initialized
directly from a temporary list does not extend that lifetime.

The three temporary corner-array call sites in SkMatrix.h, SkMatrix.cpp and
SkDevice.cpp now bind named arrays before forming views. Those files are only
adapted here; they are not marked as fully converted.

The spelling checker reports four compatibility declarations in these headers:
`sk_sp::operator->` and three class-template deduction guides. They are not member
arrow expressions or trailing-return function definitions. The operator remains
needed by unconverted source; its removal and explicit template spelling at
remaining callers are tracked as outstanding, without suppressing the checker.

## Pixel extent conversion notes

Temporary pixmap storage now releases an occupied destination before a move,
preserves borrowed pixel views during a move, and treats self-move as a no-op.
Failed allocation leaves both ownership and the published view empty; aliased
metadata is retained before reset. Negative dimensions and invalid format enums
are rejected before size calculation. These are intentional corrections, with
candidate-only regression tests; valid common behavior is checked in both builds.

Checked size arithmetic retains its overflow state and the image extent's signed
32-bit byte limit. Clipped transfers calculate row/column byte offsets before
publishing pointer and rectangle updates. `SkColorTypeValidateAlphaType` rejects
unknown enum values without changing the caller's output. These changes are not
claimed as a new rendering optimization. See the pixel-extent measurement record.

## Pixel conversion notes

Pixel conversion names its dispatch sequence, snapshots stable dimensions before
pixel loops, and derives row addresses without forming a pointer beyond the last
active extent. It validates format enums, matching shapes, null storage, stride
units/ranges and extent arithmetic before writing. One-row arrays retain the real
internal stride-zero convention used by vertex and patch color conversion.
Caller storage must satisfy the documented alignment, extent and overlap rules;
floating alpha retains its finite [0,1] input contract. Identical row copies are
explicit no-ops; partial overlap is not supported.

Capturing loop bounds enabled native Clang vectorization of several alpha kernels.
The measured RGBA8-to-A8 gain and small-call validation costs are reported in
PIXEL_CONVERSION.md. This is a specific kernel result, not a whole-renderer claim.

## Pixmap conversion notes

Pixmap remains a caller-backed view: no pixel storage is acquired or retained.
Its documentation distinguishes metadata lifetime, writable storage authority,
alignment and overlap. Metadata references reject temporary views. Address
calculations name byte offsets; floating-format opacity scans form only live row
addresses. Scaling keeps its unpremultiplied-color convention and makes image,
shader, surface and canvas lifetimes explicit.

Behavior corrections are separated from common reference comparisons: null/empty
and malformed fill/read inputs fail before accessing pixels, unknown/null generic
addresses return null, and the RGBA1010102 float-color getter now distinguishes
red from blue. The BGRA getter retains its existing order. Integer and float
conversion rounding otherwise remains the reference behavior. The upstream XR
color-getter omissions remain omissions; this conversion does not claim added HDR
accessor support.

## Mask conversion notes

Masks remain borrowed descriptors, with separately owned allocations. Builder
metadata is actually mutable instead of changed through casts of const objects.
Moving a builder clears its source and never releases either caller-owned buffer.
Padded destinations check dimensions, coordinates, byte products and allocation
alignment before use. Invalid padding/extent inputs produce empty A8 descriptors.

Gaussian kernels retain their original fixed-point arithmetic and common-width
vector representation. The small-radius coefficient arrays initialize their full
capacity before loading vector arguments. Large-radius scans retain counts for
strided columns instead of constructing a logical end pointer beyond the backing
allocation. Source rows are borrowed individually; small-radius tail writes form
only live row addresses. Results and measurement limits are in MASKS.md.
SkBlurMask is now converted separately: style-composition loops form live row
addresses; analytic rectangle generation removes unused scanline allocations and
selects its profile formula before traversal. Coverage multiplication uses proven
16-bit intermediate bounds with exactly the original divide-by-255 rounding.
The direct Gaussian comparison routine now respects source stride and checks
scratch extents. Private failure/ownership contracts and exceptional geometry
are recorded in BLUR_STYLES.md. Blur-filter dispatch and remaining mask
rasterizers are still unconverted; this does not mark the subsystem complete.

## Cached storage conversion notes

SkCachedData adopts either a malloc-compatible allocation or locked discardable
storage. The immutable storage identity is initialized directly. Reference count,
lock/cache state and its published borrowed pointer are explicitly mutable;
const ref/unref no longer casts away the object's const qualification. Mutex
scopes end before final deletion. Cache attachment and client reference operations
retain their distinct transitions, including failed reacquisition with null data.

Data-change callbacks run under the mutex, before publication, and cannot reenter
reference/cache methods or throw. The sole production override in SkMipmap.h now
uses a const callback and a mutable borrowed level pointer. That header receives
only this adapter change, not a full-conversion claim. Diagnostics require
quiescent reference transitions; a cache-only reference cannot use pixel data.
Details and coverage are in CACHED_DATA.md.

## Mask-filter dispatch conversion notes

NinePatch retains one adopted cache reference or one owned pixel allocation,
declared before its borrowed descriptor. The generic path owns returned masks
before testing success, including failed filters that already produced storage.
Horizontal nine-patch run payload is allocated only when those clipped strips
exist, and capped at 98,304 bytes by splitting lengths above 32,767. Side-only
clips do not construct that scratch owner. Source offsets use size_t.

ImageFilterResult names the image filter and shading effect. SkBlurMaskFilterImpl,
SkShaderMaskFilterImpl and SkCanvasPriv receive only signature/consumer adaptations
here; their remaining implementation is not marked converted. The unused legacy
emboss factory bridge is removed from SkLightingImageFilter, while its retained
lighting filters remain. These adapted files retain original baseline copies.
See MASK_FILTER_DISPATCH.md for tests and remaining scope.

## Still required

- Finish reference-count/view compatibility cleanup; convert arrays, memory utilities and remaining
  image/pixel views; then mask/blur, sampling, compositing and raster kernels.
- Convert retained geometry, paths, scan conversion, canvas/device state,
  transforms, clipping and surface/image ownership.
- Remove unused picture/text/serialization paths through actual dependency
  work. Preserve external glyph coverage and all admitted drawing behavior.
- Convert remaining retained filter, shader, color and skcms implementation;
  preserve the planned material capabilities while resolving their dependencies.
- Audit every retained production header and build/source list, including
  templates, generated runtime support and platform ports. Syntax-checker success
  alone is not semantic conversion evidence.
- Complete native platform, installed-package, GUI.Forms and instrumented gates,
  including broader workload and allocation/workspace measurements.
- Complete the owner's independent review and coordinate production integration.

The ledger records completed boundaries; it is not a count-based substitute for
the full scope. Large unconverted portions remain and the goal stays active.
