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
