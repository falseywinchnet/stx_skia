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
| Mask-filter dispatch and nine-patch ownership | `src/core/SkMaskFilterBase.cpp/.h`, `src/core/SkMaskFilter.cpp`; existing public header reviewed | Adopted cache/allocation owners, failure cleanup, bounded clipped run storage and reference rendering comparisons |
| Shader-mask filtering | `src/effects/SkShaderMaskFilterImpl.cpp/.h`, `include/effects/SkShaderMaskFilter.h` | 576 byte-exact shader masks, checked source metadata, scoped pixel ownership and failure preservation |
| Blur-filter construction and cached masks | `src/core/SkBlurMaskFilterImpl.cpp/.h` | Explicit shape alternatives, cache/allocation handoff, checked raster extents and 288 reference scenes |
| Standard blend shaders | `src/shaders/SkBlendShader.cpp/.h` | 696 reference scenes across all 29 standard modes, local/affine transforms, opacity combinations and colored vertices; invalid-mode rejection |
| Mipmap selection, storage and builder | `src/core/SkMipmapAccessor.cpp/.h`, `src/core/SkMipmap.cpp/.h`, `src/core/SkMipmapBuilder.cpp/.h` | Attached/generated level selection, checked allocation layout, discard/relock, builder lifetime and 1,152 byte-exact mip chains; downsampling kernels remain separate |
| Change listeners | `src/core/SkIDChangeListener.cpp`, `include/private/SkIDChangeListener.h` | Registration cleanup, deregistration, reset, notification and destruction |
| Points, rectangles and rounded rectangles | `include/core/SkPoint.h`, `include/core/SkRect.h`, `include/core/SkRRect.h`, `src/core/SkPoint.cpp`, `src/core/SkRect.cpp`, `src/core/SkRRect.cpp`, `src/core/SkPointPriv.h`, `src/core/SkRectPriv.h`, `src/core/SkRRectPriv.h` | 24,217-case byte-exact corpus over extreme integers, signed zeros, denormals, infinities and NaN; saturation corrections; field-wise serialization; borrow rejection |
| 3x3 matrices | `include/core/SkMatrix.h`, `src/core/SkMatrix.cpp`, `src/core/SkMatrixPriv.h`, `src/core/SkMatrixUtils.h` | 2,794-case byte-exact corpus over classification, mapping at every kernel count, concatenation, inversion, fitting and serialization; defined sprite translation test |
| Path storage, construction, iteration and serialization | `include/core/SkPath.h`, `SkPathBuilder.h`, `SkPathIter.h`, `SkPathTypes.h`; `src/core/SkPath.cpp`, `SkPathBuilder.cpp`, `SkPathPriv.cpp/.h`, `SkPathData.cpp/.h`, `SkPathRaw.cpp/.h`, `SkPathIter.cpp`, `SkPath_serial.cpp`, `SkPathRawShapes.cpp/.h`, `SkPathMakers.h`, `SkPathEnums.h`, `SkPath_interpolate.cpp`, `SkPathDump.cpp` | 4,905-case, 22.5 MB byte-exact corpus over every factory, all 30 builder operations, queries, iterators, transforms and the binary form; defined reversal and conic-weight access, checked allocation, aligned reads |

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

The lvalue-container constructor now constrains participation through a private
trait requiring data, an integral count and compatible element-pointer access.
This corrects libstdc++ variant probing of non-container alternatives. Compile-time
regressions cover non-containers, incompatible and const elements, vector<bool>,
valid const views and variant selection. Temporary-owner rejection remains.
See the cross-platform evidence in results/2026-10-26/RUNTIME_REMOVAL.md.

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

The earlier ImageFilterResult conversion named the image filter and shading effect;
that adapter is now removed under the clarified contract below. SkBlurMaskFilterImpl,
SkShaderMaskFilterImpl and SkCanvasPriv receive only signature/consumer adaptations
here; their remaining implementation is not marked converted. The unused legacy
emboss factory bridge is removed from SkLightingImageFilter, while its retained
lighting filters remain. These adapted files retain original baseline copies.
See MASK_FILTER_DISPATCH.md for tests and remaining scope.

## Shader-mask conversion notes

Shader-mask filtering uses the checked shared row-copy kernel and retains new
pixels until the bitmap and canvas have finished borrowing them. Invalid source
metadata and overlapping descriptors fail before output mutation; metadata-only
queries remain allocation-free. The shader still multiplies the input coverage
through source-in blending. Public shader-mask support remains part of this CPU
library despite upstream's deprecation notice. See SHADER_MASKS.md for scope and
verification. This completes the shader-mask implementation previously adapted
only for ImageFilterResult; blur-filter construction is now converted in the following batch.

## Blur-filter conversion notes

The blur-filter object is immutable. Raster setup chooses a rounded rectangle or
rectangle span explicitly rather than receiving anonymous callbacks. Temporary
source pixels, uncached output pixels and cache references have distinct owners;
cache-copy failure retains the uncached allocation. Source storage is released
before cache copying. NinePatch receives exactly one transferred owner.

Aligned row extents are checked before allocation; mask drawing uses local clip
bounds without negating a signed integer coordinate. Invalid factory enum values
are rejected. Sigma, Gaussian rounding, fractional rectangle phase and cache keys
remain unchanged. See BLUR_DISPATCH.md for verification. The image-filter adapter
is still present in this tested conversion but is now scheduled for removal under
the owner's clarified contract.

## Image-effect pruning under the clarified contract

All 17 general image-filter factory implementations and SkImageFilters.h are now
reference-only. Mask-filter-to-image-filter adapters and their canvas call path
are removed; the CPU bitmap device retains direct blur/shader mask execution.
The obsolete backdrop scene is removed from both differential profiles. All
remaining render output matches the original implementations for that revised
contract. No output/timing comparison across the two different scene definitions
is claimed.

That first removal left core SkImageFilter/FilterTypes/layer machinery compiled.
The subsequent CPU-layer and core-removal batches below remove it. The runtime
removal below also removes SkSL. The later picture/font removal below completes that dependency cut.
SkCanvas and SkCanvasPriv receive narrow dependency edits, not full house-style
conversion. See IMAGE_FILTER_PRUNING.md for the exact scope.

## CPU layer reduction

SaveLayerRec and Layer storage no longer contain backdrop/filter graph state.
SkCanvas's layer setup, initialization, restore and image draw path no longer
evaluate image filters. PreparedDraw captures paint without automatic offscreen
filter layers. Nested opacity, affine clipping, color-matrix restores, F16 and
initialization from previous pixels are covered by 256 new differential scenes.
The established 0.001-pixel layer-bound rounding tolerance is preserved.

This converts the rewritten layer path, not the entire canvas or recording
implementation. The temporary canvas rejection of SkPaint image filters is superseded by the
core-removal batch below. See CPU_LAYERS.md for this batch's original scope.

## Core image-filter removal

The five remaining core image-filter implementation units, their public/private
headers, cache and filtering backend are reference-only. SkPaint no longer owns
or exposes an image filter; device/image filtering entry points and dependent
branches are removed. Paint serialization preserves its historical null slot and
rejects nonzero image-filter tags. Ordinary layers retain device composition.

This is dependency removal, not full conversion of paint, devices or recording.
Original copies isolate the baseline from these changes. The paint contract test
checks API absence, ordinary serialization and unsupported-record rejection;
existing rendering comparisons cover retained effects. See IMAGE_FILTER_CORE.md.

## Runtime/compiler removal and blend-shader conversion

SkSL source and headers, runtime shaders/color filters/blenders, known effects,
gainmap shaders, arithmetic-blender factories and programmable mesh specifications
are reference-only. Canvas/device/record mesh dispatch is removed; ordinary
SkVertices drawing remains. Shader-language capabilities and deserialization
flags are removed instead of advertising a language the candidate cannot execute.
The raster pipeline no longer exposes compiler tracing callbacks.

Standard blend shaders now use named opacity tests, explicit child ownership and
arena-backed coordinate/result storage with stated initialization and failure
contracts. Their arithmetic and local-matrix ordering remain unchanged. The
SkBlender overload accepts standard modes; custom runtime blending returns null.
Invalid enum values also return null before constructing a shader.

This converts SkBlendShader.cpp/.h, not the other adapted canvas, surface, record
or raster-pipeline files. The subsequent raster-stage pruning below removes compiler-oriented arithmetic
and execution support from the raster pipeline. The later picture/font removal below removes those dependencies. See ../results/2026-10-26/RUNTIME_REMOVAL.md.

## Compiler-only raster-stage pruning

The remaining 290 compiler-only operations, their contexts, stack rewind support
and packing utility are removed. The candidate has 229 operations versus 524 in
the original reference (five tracing operations were removed earlier). Pipeline
stage calls no longer forward an unused compiler base pointer, and launch no
longer carries a shared compiler-tail pointer. Partial-row memory patching remains.

This is dependency pruning of imported pipeline code, not a conversion claim for
the full pipeline or its vector kernels. The new direct test covers 3,168 padded
row, precision, extension, reset and compiled-execution combinations with independent
expected pixels. See ../results/2026-10-26/RASTER_STAGE_PRUNING.md.

## Text drawing front end removed

Canvas text/glyph/slug drawing, device glyph dispatch, the glyph-run painter and
builder, text drawing utilities and overdraw canvas are reference-only. Text
record/capture/forwarding hooks are removed; legacy text playback operations
invalidate the reader. Unused text-blob/font interception queries are removed
with their glyph-run dependency. Canvas no longer allocates glyph-builder scratch;
bitmap devices no longer contain their glyph painter.

The new external glyph-mask test checks every A8 coverage value, four paint
colors, integer clipping and padded source/destination rows against independent
RGBA expectations. It also checks text-blob API absence in the candidate and its
presence in the original reference. This is dependency pruning, not conversion
of the adapted imported canvas/device/record code. At this boundary picture serialization still retained text blobs, typefaces and
font/strike internals; the next boundary below removes them.
See ../results/2026-10-26/TEXT_DRAWING_REMOVAL.md.

## Picture, capture and font machinery removed

Picture recording/playback/serialization, capture, bounding-box hierarchy and
record storage, picture-backed images/generators/shaders and multi-picture
writing are reference-only. Canvas picture APIs, drawable picture snapshots,
surface capture wrappers and their virtual hooks are removed. Ordinary image
snapshots, bitmap drawing and direct raster surfaces remain.

Font, typeface, glyph, text-blob, strike/cache and scaler implementations and
headers are reference-only, together with OpenType helpers and platform font
backends. Resource serialization no longer carries picture/typeface/slug callbacks
or typeface tables. Global cache APIs no longer expose font controls. Resource
cache controls remain. SkGraphics.cpp is converted to explicit execution order;
SkSerialProcs.h now states the retained synchronous image callback contract.
Other adapted imported files still await conversion.

The candidate compiles 253 units versus 492 in the original reference on arm64.
Archive inspection finds no definitions for the removed font, glyph, strike,
typeface, picture or capture classes. Sixteen tests and seven rendering artifacts
retain their reference results; the A8 test also asserts picture/font-cache API
absence. See ../results/2026-10-26/PICTURE_FONT_REMOVAL.md.

## Base, raster and null surface ownership

SkSurface_Base.cpp/.h, SkSurface_Raster.cpp/.h and SkSurface_Null.cpp now use
explicit types, named handles/results and visible callback/transfer ordering.
One unique canvas owner replaces the owner-plus-cached-pointer pair. The unused
capture-era pixel-storage owner is removed, allowing old pixel refs to end when
the last snapshot is released after copy-on-write. Snapshot/cache and canvas
back-link lifetime rules are documented. Subset snapshot and fallback-read
allocation failures return through the existing null-result mechanisms.

The surface contract test checks owned/wrapped buffers, retain/discard,
snapshot isolation and outliving, subset clipping, release timing, backing reuse,
null surfaces and callback results. On arm64 the base object is 56 bytes versus
72 in the reference. No rendering-speed gain is claimed. Public SkSurface.cpp/.h
and GPU compatibility query stubs remain separate work. See
../results/2026-10-26/SURFACE_OWNERSHIP.md.

## CPU surface facade

SkSurface.cpp/.h are converted to named implementation references, explicit
results and CPU ownership/callback documentation. Unsupported GPU context,
texture, semaphore and characterization interfaces, their base virtual stubs,
and CPU-unimplemented surface YUV reads are removed. Surface kinds now contain
only null and raster. The retained CPU recorder and pixel APIs are unchanged.

The surface test checks API removal, padded and clipped copies, drawing-state
independence, full-write snapshot preservation and compatible surfaces.
Seventeen tests and seven rendering artifacts retain reference results;
Release/Debug instrumented suites and a relocated consumer pass. See
../results/2026-10-26/SURFACE_FACADE.md. GPU-era compatibility declarations in
other public headers remain separate work.

## Raster image ownership and copies

SkImage_Base.cpp/.h, SkImage_Raster.cpp/.h and SkImage_RasterFactories.cpp now
use explicit owners, named results and checked copy/fallback paths. The obsolete
capture pixel-storage array is removed; the raster bitmap owns its pixel ref.
A const image read no longer creates a redundant bitmap owner. Mipmap/subset
identity rules remain intact, and borrowing helpers reject temporary owners.

The unused compressed-texture factory and CPU ETC/BC1 decoder are reference-only,
closing a residual decoder path outside the earlier ordinary codec removal.
Candidate compilation is 252 units on macOS arm64. Raster-image object size is
104 bytes versus 136 in the original; no throughput gain is claimed. Eighteen
tests and seven rendering artifacts retain reference results. Public SkImage and
lazy-generator conversion remain separate work. See
../results/2026-10-26/RASTER_IMAGE_OWNERSHIP.md.

## Deferred CPU generation

SkImage_Lazy.cpp/.h and SkImageGenerator.cpp/.h now expose ownership and generator
lock scopes directly. Validated requests reach the provider; complete generated
pixels enter the bitmap cache only after its lock is released. Color-conversion
cache identity and generator lifetimes retain their original behavior.

Unused YUV-plane generation/cache/layout implementations, GPU listeners and the
always-false GPU pixel proxy are reference-only. Three candidate compilation
units are removed. The new deterministic test covers cache admission/hits,
provider failure and retry, conversion lifetime and four concurrent readers
sharing one provider. Nineteen tests and seven rendering artifacts preserve
reference results. See ../results/2026-10-26/DEFERRED_IMAGES.md. The public image
facade remains work.

## CPU image facade

SkImage.cpp/.h now use explicit execution and CPU ownership/callback contracts.
Unsupported texture/protected/YUV operations, context lookup and backend image
kinds are removed. Existing public null-context reads remain compatible through
null-only overloads; private pixel reads and their cache/device callers carry no
backend argument. Shader and scaling overloads share named implementations.

The expanded image test covers compile-time API exclusion, scaling identities,
independent nearest-neighbor expectations, all normal/raw shader overloads and
translated clamp/repeat sampling. Image, surface, raster and deferred ownership
boundaries are converted; surrounding imported cache, device and sampling code
still requires conversion. See ../results/2026-10-26/IMAGE_FACADE.md.

## CPU mipmap selection

SkMipmapAccessor.cpp/.h now use named acquisition and scale helpers, explicit
selection state, and storage owners declared before their borrowed level views.
Temporary accessor views are rejected. Existing rounding, half-level bias,
cache lookup order and missing-level fallbacks are unchanged. Deterministic
checks cover 26 attached/generated selection cases, eviction/lifetime, a
one-pixel fallback and three unreadable-provider cases. Mipmap generation,
bitmap caching and the image shader remain separate conversion work. See
../results/2026-10-26/MIPMAP_ACCESSOR.md.

## Mipmap storage and builder

SkMipmap.cpp/.h and SkMipmapBuilder.cpp/.h now expose checked allocation layout,
client/cache lifetime, discardable descriptor ownership and writable-view rules.
The layout walk halves dimensions directly; checked byte sums precede allocation.
Metadata-only construction retains caller-filled levels. Invalid pixel extents
and metadata are rejected before allocation, and null builder attachment returns
null. Existing mip filters, rounding, compatibility and fallback behavior remain.

A new deterministic suite compares 1,152 chains across four formats, checks an
independent 2x2 average, builder outliving, allocator failure and discard/relock
transitions. It adds an eighth byte-identical artifact. Downsampling kernels and
bitmap cache implementation remain separate conversion work. See
../results/2026-10-26/MIPMAP_STORAGE.md.

## HQ mipmap downsampling kernels

SkMipmapHQDownSampler.cpp now uses explicit expanded pixel types, named arithmetic
results, live row/index traversal and a shared immutable table per format. The
eight box/triangle footprints retain their sum order and shared-edge reuse.
The source buffers are never advanced past their live extent in C++.

Two separately committed packed-format corrections preserve RG16's green channel
and give RGB10A2 alpha enough accumulator headroom. Independent weighted-sum tests
cover those intentional reference differences. The remaining 16 supported formats
retain byte comparisons; all common mip/render artifacts match. The selected
kernels compile to common-width native/SSE2 code, with M4 timing evidence and
limits recorded in ../results/2026-10-26/MIPMAP_KERNELS.md. The optional drawing
mipmap implementation remains separate conversion work.

## Bitmap and mipmap cache entries

SkBitmapCache.cpp/.h now use explicit block owners, immutable record identity,
mutable state under the record mutex and named local/global cache dispatch.
Installation reserves an external pixel pin under the mutex, then releases that
mutex before creating/publishing the bitmap. Replacing a bitmap that already pins
the same entry can therefore run its release callback without recursively locking
the record. Mipmap finder acquisition uses a scoped client owner so failed
relocks balance references before cache removal.

The new cache contract suite checks pinned duplicates, unpinned replacement,
lookup into an occupied output, stale IDs, purge retention, local/global mipmap
clients and actual discardable allocation loss. The general resource cache remains separate work. See
../results/2026-10-26/BITMAP_CACHE.md.

## Optional drawing mipmap implementation

SkMipmapDrawDownSampler.cpp now uses named sampling selection, explicit draw
state, synchronous borrowed buffers and owners preceding their views. Empty
views are rejected before ratios; failed bitmap wrapping leaves output unchanged.
Its exact-half bilinear and other-shape Catmull–Rom choices remain unchanged.
The default HQ implementation remains selected in the production package.

The optional upstream source had stale draw interfaces at the pinned revision.
The candidate uses the current CPU draw type/signature; the reference includes
its unchanged original through an interface-only adapter. A factory-selection
fixture distinguishes this implementation from HQ, then 2,304 cases compare
whole chains, padded output, constant preservation and unchanged source bytes.
All nine rendering/mipmap artifacts match. See
../results/2026-10-26/MIPMAP_DRAW.md.

## Resource cache and synchronization boundary

SkResourceCache.cpp/.h and SkSynchronizedResourceCache.cpp/.h now state record,
hash, key and callback lifetimes explicitly. Pending insertion and the hash use
scoped owners; list/hash unlink precedes destruction. Key byte views avoid typed
aliasing, and admission checks charge/count arithmetic before replacement.

Global message draining and direct shared-ID purge now take the synchronized
wrapper's mutex. Internal message processing uses base dispatch under its existing
access authority to avoid recursive locking. The historical LRU, duplicate and
strict-budget policies remain. New deterministic tests cover those policies,
4,000 concurrent record operations across local/global caches, and accounting
boundaries. See ../results/2026-10-26/RESOURCE_CACHE.md. Hash storage and invalidation are converted in the later entries below;
mutex/semaphore implementations remain separate conversion work.

## Remove the unused task scheduler

SkExecutor and SkTaskGroup sources/headers are reference-only. The candidate
source closure and archive contain neither scheduler; no retained raster consumer
calls them. The relocated installed consumer checks their headers are absent.
GUI.Forms' atomic batch pool is the owner's worker-execution reference; cache
mutexes remain a separate shared-state synchronization boundary. See
../results/2026-10-26/SCHEDULER_PRUNE.md.

## Owned cache invalidation

The generic SkMessageBus is reference-only. The retained broadcast use is now
SkResourceCacheInvalidationInbox.cpp/.h: explicit registry observers, inbox-owned
IDs, unregister-before-destruction, fixed lock order and no generic routing or
message-transfer modes. Resource caches recycle pending/drain allocations.

The M4 Release cache object grows 112 to 136 bytes and retains two buffers'
capacities; empty checks remain within timing spread and measured nonempty
administration becomes cheaper. Concurrent delivery/registration tests and all
nine reference render artifacts pass. See
../results/2026-10-26/CACHE_INVALIDATION.md and its raw measurement JSON. The hash
table conversion is recorded below; mutex/semaphore implementation remains separate work.

## Hash storage, probing and borrowed entries

SkTHash.h now uses explicit slot construction/destruction, checked power-of-two
capacity arithmetic, prepared replacement storage, named map visitors and
lvalue-only entry/iterator borrows. Unused policy-member detection is removed;
retained callers use the original three-quarter growth/one-quarter shrink policy.
The table and slot footprints are unchanged. Original SkTHash.h is preserved
byte-for-byte in the reference include tree.

A separate correction makes deletion terminate after manual resize fills every
slot. The measured optimization uses wrapped probe distances and direct assignment
between known live slots; full-table recovery stays outside the ordinary deletion
body. Deterministic collision/permutation and owned-value tests accompany all
nine unchanged render artifacts, 25-test release/instrumented suites, installed
consumption and seven GUI.Forms fixture tests. Small measured lookup/churn costs
are near parity; the collision-heavy churn workload takes about 7% less time in
the final M4 series. Earlier regressions are retained. See
../results/2026-10-26/HASH_STORAGE.md and its measurement files.

## Intrusive list and unused hash/cache wrappers

SkTInternalLList.h now states non-owning membership, node/iterator lifetime,
exclusive mutation and constant-time reset semantics. It retains two endpoint
pointers and a one-pointer iterator, with no allocation or new runtime state.
Temporary-list iterator initialization is rejected; unsupported traversal-start
values produce an empty iterator. Debug checks enforce existing detached-node and
distinct-list requirements. Valid link operations and ordering remain unchanged.

Compiler dependencies and retained-source inspection found no CPU consumers of
SkLRUCache.h, SkTDynamicHash.h or SkTMultiMap.h. They are reference-only, with a
candidate scope check against reintroduction. The active intrusive list remains
used by polygon triangulation and the discardable-memory pool. Its new fixtures
exercise both consumers as well as permutation, insertion, traversal and transfer
contracts. See ../results/2026-10-26/INTRUSIVE_LIST.md.

## Discardable-memory pool

SkDiscardableMemoryPool.cpp/.h, the SkDiscardableMemory interface and the default
pool entry point now state block ownership, locked data borrows, non-owning pool
membership, byte accounting, eviction and failure behavior. Each block retains
its pool. Prepared empty records and pixel allocations are created outside the
mutex; publication validates byte addition and installs membership under that
existing mutex. The pool still may exceed its budget while blocks are locked.

Usage/budget/statistics observations now take the same mutex as mutation; debug
counters saturate. Zero-byte creation is rejected. These corrections precede the
ownership conversion. A separate optimization skips removing/reinserting an
already-most-recent block. Native object layouts are unchanged (64-byte pool,
56-byte block in measured Release); synchronized getter cost is explicitly
measured. See ../results/2026-10-26/DISCARDABLE_POOL.md and its timing/layout files.

## Semaphore, mutex, once and thread identity

SkSemaphore.h/.cpp, SkMutex.h, SkOnce.h and SkThreadID.h/.cpp now state permit,
publication, blocking, callback and destruction contracts. Named lazy OS
initialization replaces lambdas; native API failures stop explicitly, and POSIX
interruption retries are visible. Valid-operation acquire/release orders are
unchanged. Native representative fast-path assembly matches the original byte
for byte; measured tiny timing differences and unchanged layouts are recorded in
../results/2026-10-26/SYNCHRONIZATION.md rather than claimed as a speed gain.

The unused SkSharedMutex reader/writer implementation is reference-only. Its sole
remaining CPU include was unused and is removed; SkFlattenable.cpp remains
otherwise unconverted. Candidate source/symbol checks reject the removed lock.
The synchronization fixture exercises permits, publication, exclusion, once
initialization and live thread identities alongside the complete rendering gate.

## Raw array storage and triangulation cells

SkTDArray.h / SkTDArray.cpp now express raw-element requirements, live length,
allocation ownership and borrowed access. Move leaves the source empty; size and
capacity arithmetic is checked before mutation; self-insertion preserves its
source through growth and tail movement without temporary allocation. The owner
retains its original fields and measured 24-byte Release layout. Existing-element
push and empty operations have explicit behavior; temporary-owner borrows are
rejected. Boundary corrections are recorded separately from unchanged behavior.

The polygon triangulation grid now uses a typed array of live intrusive lists;
its old raw storage skipped construction. This targeted adaptation is not a
whole-file conversion of SkPolyUtils.cpp. The original remains in the reference
build. Both releases pass 29 tests, nine artifacts match, and instrumented,
installed-consumer and GUI.Forms gates pass. The measured small insert/erase
regression is retained in ../results/2026-10-26/TD_ARRAY.md and its raw timings.

## Retained CPU priority queue

SkTDPQueue.h now covers only the insert/peek/pop contract used by the polygon
sweep. Indexed GPU-cache operations, mutable priorities, sorting and random access
are removed separately. The converted heap keeps a displaced entry locally and
slides one ancestor/child per level, retaining original tie choices. Explicit
bounds permit direct contiguous access without repeated indexed checks. It keeps
the same 24-byte owner and adds no persistent state.

The complete original header is reference-only. Thirty tests, nine matching
artifacts, instrumented suites, relocated consumer and GUI.Forms fixture pass.
A controlled old-header comparison keeps the current polygon source, array and
library fixed. It measures gains for larger descending/permuted queues and comb
polygons, with small ascending regressions retained. See
../results/2026-10-26/PRIORITY_QUEUE.md and its timing/assembly evidence. This does
not claim full polygon utility conversion or a universal rendering speedup.

## Geometry and raster sorting

SkTSort.h now uses named comparators, explicit count/depth conversions and
bounded heap traversal. Empty heap/insertion/null ranges are corrected separately.
The original introsort threshold, pivot, fallback and tie choices remain; sorting
operates in caller storage without retained state or an auxiliary array. Its new
fixture independently checks ordering, identity/guards, stability where promised,
owning values and the original comparator/output traces across 53,553 cases.

Both Release profiles pass 31 tests; nine artifacts match and instrumented,
installed-consumer and GUI.Forms gates pass. Large-case gains and small-case
regressions are preserved in ../results/2026-10-26/SORT.md and its raw measurements.
This converts the utility, not its path/scan-conversion callers, and does not
claim a universal speed improvement.

## Unused picture factory recording and search

The picture-only factory recorder, pointer/reference-counted ID sets, and their
otherwise-unused generic/string search helpers are reference-only. Two compiled
implementation units and their private headers are removed, along with the
writer's unused recorder setter, owner field and branch. Ordinary inline-name
and repeated-name dictionary serialization remains; effect deserialization stays
disabled. The remaining writer/flattenable implementation is not yet converted.

Both 32-test Release profiles pass; a new 24-case effect-stream fixture produces
3,344 identical bytes. It covers supplied/growing/owned writer forms, repeated
names and retained effect types. The release runner and native CI compare that
artifact. Nine rendering artifacts, instrumented suites, installed consumer and
GUI.Forms fixture also pass. See ../results/2026-10-26/RECORDER_PRUNE.md for scope
and the distinction between source removal and measured runtime improvement.

## Disabled effect registry and flattenable entry points

The candidate's unused process-wide factory registry and manual registration API
are removed, along with two initialization units, 27 registration functions and
their declarations/friendship. Original registration remains in the reference
profile. Default candidate named-effect rejection is preserved; ordinary serialized
bytes and rendering artifacts remain identical. Retained virtual-method ordering
and historical type tags are unchanged.

SkFlattenable.h/.cpp are converted with explicit procedure initialization, owned
data returns, borrowed callback/buffer contracts and checked null/empty/alignment
boundaries. No fields, registries or locks are added. The lower-level reader/writer,
private explicit factory arrays and remaining reconstruction methods still require
work. Both 32-test profiles, instrumented suites, relocated consumer and GUI.Forms
fixture pass. See ../results/2026-10-26/FACTORY_REGISTRY.md for the intentional API
removal, preserved reference behavior and exact scope.

## Reader factory playback and recursion bookkeeping

Unused private factory playback and recursion-limit controls are removed, together
with the dictionary, borrowed factory-array pointer/count and nesting counter.
Only the zero null-effect marker is accepted; other effect tokens invalidate the
reader under the already-disabled effect-deserialization policy. Primitive,
paint and callback image reading remain. No replacement tracking state is added.

A controlled M4 Release header comparison measures the reader at 96 bytes before
and 64 after. Both 32-test Release profiles, nine rendering artifacts, unchanged
24-case serialization output, instrumented suites, relocated consumer and seven
GUI.Forms integration tests pass.
The fixture also checks cursor preservation, plain paint round-trips and unsupported
effect rejection. This is scoped pruning, not a completed reader conversion or a
rendering-speed claim. See ../results/2026-10-26/READER_FACTORY_PRUNE.md.

## Retained binary reader

SkReadBuffer.h/.cpp are converted with checked borrowed extents, defined empty
state, sticky failure, typed byte copies and explicit callback/context lifetimes.
No fields are added; the Release reader remains 64 bytes. Owned byte-array reads
validate before allocation and use one combined SkData allocation instead of a
separate payload allocation plus wrapper. The original guarded primitive path is
retained after shorter variants regressed in measurements.

Both 33-test Release profiles, nine render artifacts, unchanged serialization,
instrumented suites, relocated consumer and seven GUI.Forms tests pass. Controlled
current-dependency timings reduce owned-record costs about 20–49%; the final word
loop median is about 5.5% slower with overlapping ranges. Earlier regressions are
preserved. See ../results/2026-10-26/READ_BUFFER.md and raw timing files. This does
not complete the writer or remaining private reconstruction implementations.

## Retained binary writer

SkWriteBuffer.h/.cpp are converted with explicit borrowed initial storage,
callback contexts, independent snapshots and process-lifetime type names. The
writer stays 80 bytes. Reset now clears the existing stream dictionary, so reused
writers produce fresh-stream bytes. Empty snapshots and zero-byte operations have
explicit behavior. Count/index conversions are checked without adding error state.
The unused stream-input interface is removed; stream output remains.

Both 33-test Release profiles, nine render artifacts, unchanged 24-case serialized
output, instrumented suites, relocated consumer and seven GUI.Forms tests pass.
Reuse fixtures cover four
supplied capacities, growth, guards, persistent snapshots and stream output. Image
callbacks now round-trip the base and two mip levels. No speed gain is claimed.
See ../results/2026-10-26/WRITE_BUFFER.md. SkWriter32's underlying storage and growth
implementation remains a separate required conversion.

## Storage writer and inline capacity

SkWriter32.h/.cpp are converted with bounded reservation/growth, checked string
sizes, defined empty behavior and value-returning record reads. Scalar records
begin lifetime through typed construction; inline capacity uses an aligned byte
array. The five-field owner remains 40 bytes, and SkSWriter32<24> remains 64 bytes.
No new lifetime metadata is added. Unused stream input is removed.

The first correct conversion regressed about threefold in the word loop. Moving
the remaining extent check to growth and restoring typed scalar stores recovers
the original fitting path; final paired medians are within about 1%, with overlapping
ranges. Earlier regressions, including an 8% result for full supplied capacity,
remain recorded. Both 34-test profiles, independent byte expectations across
fourteen capacities, render/serialization comparisons, instrumented suites,
relocated consumer and GUI.Forms gates pass. See ../results/2026-10-26/WRITER32.md
and its raw measurements/native probe. This is not a rendering-speed claim.

## Heap and inline allocation owners

SkAutoMalloc.h is converted with explicit transfer and borrowed-storage contracts.
Moving the heap owner now empties the source capacity as well as its pointer,
correcting reuse at the former size. Temporary-owner borrows are rejected at compile
time; inline storage remains nonmovable. Existing replacement/reuse policies and
layouts remain: 16 bytes for the heap helper and 80/88 bytes for the tested inline
variants. No additional tracking fields or allocation mechanisms are introduced.

Both 35-test Release profiles, nine render artifacts, unchanged serialization,
instrumented suites, relocated consumer and seven GUI.Forms tests pass. Fixtures
cover release, move, self-move, moved-from reuse and 120 inline/heap transitions.
No speed gain is claimed. See ../results/2026-10-26/AUTO_MALLOC.md. The typed allocation
templates and other remaining memory/raster implementation still require conversion.

## Typed heap allocation owner

AutoTMalloc is extracted into its own header and converted, retaining its single
owning pointer and eight-byte M4 layout. Reallocation checks element-count
multiplication before releasing ownership. Reset/reallocation policies remain;
compile-time borrow and type constraints replace implicit assumptions without
adding tracking fields. The original mixed template header is preserved for the
reference. Other classes in that header still require conversion.

Both 36-test Release profiles, nine render artifacts, unchanged serialization,
instrumented suites, relocated consumer and seven GUI.Forms tests pass.
New fixtures exercise 864
resize transitions plus ownership transfer and reset. No speed gain is claimed.
See ../results/2026-10-26/AUTO_TMALLOC.md.

## Constructed heap arrays

AutoTArray is extracted and converted with explicit element construction,
replacement and transfer. Signed sizes and byte products are checked before
allocation. Its unique pointer/count layout stays 16 bytes. Const and temporary
borrows have compile-time contracts; there is no new tracking state. Default
initialization and replacement-before-release behavior remain.

Both 37-test Release profiles, nine render artifacts, unchanged serialization,
instrumented suites, relocated consumer and seven GUI.Forms tests pass.
The fixture checks 192 reset
transitions and constructed-element lifetimes, including failed replacement
preserving the old array. No speed gain is claimed. See
../results/2026-10-26/AUTO_TARRAY.md. Inline array owners remain unconverted.

## Inline scratch arrays

AutoSTMalloc is extracted and converted. Aligned byte storage replaces its
one-element union view, preserving inline capacity/padding and the single pointer.
Existing reset/reallocation policies remain; count products are checked before
mutation. Inline moves copy capacity and now empty the source, while heap moves
transfer the allocation. No live-length or tracking field is added.

Both 38-test Release profiles, nine render artifacts, unchanged serialization,
instrumented suites, relocated consumer and seven GUI.Forms tests pass.
The refined fixture covers
1,024 transitions, alignment, initialized-prefix preservation, partial inline
moves and independent source reuse. Four M4 layouts match the reference. No speed
gain is claimed. See ../results/2026-10-26/AUTO_STMALLOC.md. The constructed inline
array owner remains unconverted.

## Constructed inline arrays

AutoSTArray is extracted and converted, retaining pointer/inline storage/live
count and the original capacity rounding. Destruction releases heap storage even
with no live elements after trimming. Reset-to-zero empties storage; equal
positive counts still reuse it. Inline moves now empty the source; heap moves
transfer storage. A local pending owner handles failed construction in
exception-enabled consumers without adding persistent state.

Both 39-test Release profiles, nine render artifacts, unchanged serialization,
instrumented suites, relocated consumer and seven GUI.Forms tests pass.
The fixture covers 576 reset
transitions, trimming, reverse destruction, ownership moves and candidate failure
cleanup. Three M4 layouts match the reference. No speed gain is claimed. See
../results/2026-10-26/AUTO_STARRAY.md. General template helpers remain unconverted.

## Remaining template helpers and raster-tail addresses

SkTemplates.h now contains the converted owner includes, scalar/offset helpers
and a named free-memory deleter. Unused generic callable/cleanup/array-factory
adapters and the typed-after helper are removed. SkBitSet is reference-only.
The retained macOS Core Foundation owner uses a named deleter. Original headers
remain preserved for the differential profile.

Raster tail patch/restore and stage address calculation now use integer address
bias/cancellation on the supported native CPU platforms, without adding context
fields. Only these three functions in the large pipeline header are converted.
Both 40-test Release profiles, nine render artifacts, unchanged serialization,
instrumented suites, relocated consumer and seven GUI.Forms tests pass. The
expanded pipeline fixture covers 4,224 cases and context restoration. No speed
gain is claimed. See ../results/2026-10-26/TEMPLATE_HELPERS.md. Broader raster,
geometry, canvas and utility conversion remains required.

## Allocation and byte primitives

SkMalloc.h/.cpp and the native allocator port are converted, preserving libc
allocation, null-free behavior, zero-size policy and platform size queries.
Count-product overflow now returns null directly for fallible allocation or
terminates directly for fatal operations, without submitting SIZE_MAX to the
allocator. No allocation metadata, registry or locking is added. Unsupported
Android controls and special review-build overrides remain reference-only.

Both 41-test Release profiles, nine render artifacts, unchanged serialization,
instrumented suites, relocated consumer and seven GUI.Forms tests pass.
Fixtures cover 209 allocation/
reallocation cases, three fallible overflow cases and 5,577 byte-operation cases.
No speed gain is claimed. See ../results/2026-10-26/ALLOCATION_PRIMITIVES.md.
Alignment helpers and remaining container/raster work are still required.

## Alignment and container capacity

SkAlign and SkContainers are converted. Unsigned alignment wrap is preserved for
format checks, and inline byte rounding remains distinct from runtime element
rounding. The allocator retains its two fields and 16-byte M4 layout. Count/byte
bounds and finite growth are checked at allocation boundaries; growth clamps
before integer conversion, including enormous finite factors.

Both 42-test Release profiles, nine render artifacts, unchanged serialization,
instrumented suites, relocated consumer and seven GUI.Forms tests pass. Fixtures
cover 4,161 alignment cases, 30,780 ordinary growth cases and 590 allocations.
The complete 246,240-byte ordinary growth trace matches the reference, with
fingerprint 7262172567427146524. No speed gain is claimed. See
../results/2026-10-26/CONTAINER_CAPACITY.md. Element-container storage and relocation
remain required work.

## Fixed aligned element storage

SkAlignedStorage is converted without runtime administration. It retains exactly
its aligned byte array and its non-initializing constructor. Positive capacity
and byte-product bounds are checked at compilation; temporary borrows are rejected.
The container continues to own element construction and destruction.

Both 43-test Release profiles, nine rendering artifacts, unchanged serialization,
instrumented suites, relocated consumer and seven GUI.Forms tests pass. The new
fixture covers 1,152 raw-storage object lifetimes and 289 container transition
schedules. Tested M4 inline-container sizes match the reference. See
../results/2026-10-26/ALIGNED_STORAGE.md. TArray's construction and relocation still
require conversion; the integration fixture does not establish that work complete.

## TArray construction and bitmap reference boundary

TArray's copy/default construction and self-subrange reset now keep source
elements live until copied and publish live counts after throwing construction
has succeeded. Release count validation and empty-end arithmetic are tightened.
The original stopped trial and its resolved cause remain recorded in
../results/2026-10-26/TARRAY_CONSTRUCTION.md. Move, relocation and other append
paths remain open, so the container is not yet fully converted.

SkBitmap's pixel-reference origin now has a Release boundary matching its
documented pinning behavior; impossible reference geometry detaches. The
pre-fix AddressSanitizer write, fix and regression scope are in
../results/2026-10-26/BITMAP_PIXEL_REF.md. Current native candidate/reference
Release suites and candidate Release/Debug instrumented suites pass 44 tests;
rendering and serialization artifacts remain byte-identical to reference.

## Block deque conversion integrated with TArray

SkDeque's block owner, borrowed initial storage and iterator transitions are
converted without changing its instance layout or delayed empty-block reuse.
The isolated STOP came from an invalid enum value in its new test; the corrected
fixture uses a valid unbound iterator. See ../results/2026-10-26/DEQUE.md.

In the combined candidate, native M4 candidate/reference Release suites pass
45 tests each, candidate Release instrumentation reports PASS, ten rendering
and serialization artifacts match reference byte-for-byte, the CPU build-scope
check passes, a relocated installed consumer runs, and the seven selected
GUI.Forms renderer tests pass. This establishes the current combined boundary;
it does not complete the remaining TArray relocation or larger raster conversion.

## TArray relocation and append failure states

TArray now guards new allocations and incomplete ranges during growth, move
and bulk append. It publishes live counts only after successful construction,
retains source objects until relocation succeeds, handles self-alias bulk copy
before growth, and restores unused-capacity sanitizer poisoning after an
exception. Heap move retains capacity; heap swap carries its sanitizer state.
The 16-byte heap and 32-byte inline layouts are unchanged. See
../results/2026-10-26/TARRAY_RELOCATION.md for the interruption fixtures,
reference comparison and negative performance measurements. The remaining
container interface and broader source conversion are still open.

The array access pass validates reverse indexing before subtraction, prohibits
borrows from a temporary owner, avoids alias snapshots when append fits
existing capacity, and rejects element alignment beyond the heap allocator's
guarantee. See ../results/2026-10-26/TARRAY_ACCESS.md. These fixes do
not imply that every retained renderer header has been converted.

Array equality now requires only element equality. The preserved reference
fails to compile when an element defines `==` but explicitly deletes `!=`;
the candidate compares with `==` and passes the focused regression. See
../results/2026-10-26/TARRAY_EQUALITY.md. Other container and renderer
boundaries remain open.

A scoped security review at revision `92e41ac` examined all 74 tracked files
under `skia/include/private`, including TArray and its inline-storage helpers.
It found no reportable vulnerability in that scope. The successful inline/heap
swap regression and the scan's limits are recorded in
../results/2026-10-26/TARRAY_PRIVATE_HEADER_REVIEW.md. This is not the CPU
implementation's final bill of health.

## Arena allocation boundary

The raster arena's heap-only and reset paths no longer do pointer arithmetic
on null storage. Initial extents, raw alignment and footer additions are
checked before narrowing or pointer formation. The original source is retained
for differential builds and the focused fixture covers valid heap/inline/reset
behavior and invalid Release inputs. See
../results/2026-10-26/ARENA_BOUNDARY.md. Full house-style conversion of this
arena remains open.

The next arena pass makes a footer inert until an object's constructor
completes, and records the completed prefix of a nontrivial array. Interrupted
construction can therefore be cleaned up without destroying unconstructed
storage. See ../results/2026-10-26/ARENA_CONSTRUCTION.md. Full source-style
conversion and broader renderer verification remain open.

## Block-list construction and concatenation

SkTBlockList now publishes a new element only after its constructor succeeds.
An interrupted append releases the unpublished slot and its empty heap block.
Concatenation keeps nontrivial source objects alive until all head items have
moved, and records each completed trivial segment before another allocation
can fail. The one-item path accepts a move-only element. Invalid block sizes,
negative reservations, empty front access and count overflow are rejected in
Release. The original header is retained for differential builds. See
../results/2026-10-26/TBLOCK_LIST.md. This is a targeted correctness pass;
the retained header and block allocator still need full source-style review.

The allocator constructor now validates growth policy, nonzero bounded block
increment and inline extent before alignment, narrowing or block construction.
Its original source is retained for differential builds. The focused Release
rejection cases and validation record are in
../results/2026-10-26/BLOCK_ALLOCATOR.md. This fixes the constructor boundary;
the remaining block allocation and release algorithms still need conversion.

The block allocator's resize path now poisons the complete released suffix,
including its first byte. A focused ASan shadow-boundary check verifies the
retained/released boundary. See ../results/2026-10-26/BLOCK_RESIZE.md.

## Raster clip resize state

SkRasterClipStack now recalculates its scan-converter antialiasing policy when
the root extent changes. It validates the required unsaved root state and
dimensions before updating bounds. A fractional-rectangle fixture crosses the
scanner's tiling limit in both directions. The original header remains the
reference build's version; see ../results/2026-10-26/RASTER_CLIP_RESIZE.md.
The broader clipping and scan-conversion implementations remain unconverted.

## Tiled clip shader coordinates

Translating a raster clip into a later CPU tile now carries its shader and
restores the original device coordinates when that shader's saved CTM is
inverted. The first tile can begin at a nonzero origin, so preserving only
the shader pointer would still sample spatial shaders at the wrong place.
The regression includes a narrow draw in a later tile and a composited
gradient clip. See ../results/2026-10-26/TILED_CLIP_SHADER.md. The wider
shader and tiler implementations remain in the conversion programme.

Replacing a raster clip now clears any shader mask left by an earlier
`clipShader`. Canvas clip reset, root resizing, and empty translation are
covered by a focused regression. See
../results/2026-10-26/RASTER_CLIP_REPLACEMENT.md. The broader clipping and
scan-conversion implementation remains open.

Antialiased and integer clip translations now clamp the applied offset so
their bounds retain their dimensions at signed coordinate limits. The shader
clip wrapper uses that applied offset. Original antialiased and integer region
sources remain in the reference profile; see
../results/2026-10-26/CLIP_TRANSLATION_LIMITS.md. No inner-pixel throughput
claim is made from this boundary fix.

## Serialized region and wrapped surface input

The region reader now checks the run count by division before multiplying it
by the word size. It reads serialized words from byte storage, so a valid
complex region need not start at a four-byte aligned address; internally owned
run arrays retain direct typed reads. Serialized rectangles obey the same
sentinel-edge restriction as `SkRegion::setRect`. Rejection leaves the receiving
region unchanged.

The wrapped-surface factory rejects the private ignored-stride marker before
constructing a surface. Raster validation rejects strides outside the bitmap's
signed 32-bit range and calculates the total extent with unsigned arithmetic.
Invalid arguments return null without invoking the release callback. The
candidate-only cases and verification are recorded in
../results/2026-10-26/RASTER_REGION_INPUT_BOUNDS.md. Other region and surface
implementation remains in the conversion programme.

Owned raster surfaces and recorder-created bitmap surfaces now validate the
effective stride before allocating a pixel reference. This closes the one-row
case where the allocation could succeed but bitmap setup rejected an
unrepresentable stride. See
../results/2026-10-26/OWNED_RASTER_STRIDE.md.

A focused review of all 20 image-implementation files found two failure
paths. External-write notification now returns the copy-on-write preparation
result, and rescaling reports a null result when temporary image construction
fails. The public API and large-image regressions are recorded in
../results/2026-10-26/IMAGE_PATH_REVIEW.md. This review does not complete the
remaining image, canvas, or pixel pipeline conversion.

The data-backed pixel-reference factory now validates its owner and aligned
stride before constructing a reference. The boundary check and fixture are
recorded in ../results/2026-10-26/BITMAP_PIXEL_REF.md.

The installed package now omits headers for excluded subsystems and removed
effects. A fresh installed consumer verifies representative exclusions and
the surface notification result. See
../results/2026-10-26/INSTALLED_HEADER_SCOPE.md for the measured header count
and the remaining header-review limit.

A scoped core security review found bounds defects in the compiled private
Base64 decoder, the public stream-copy helper and filtered alpha extraction.
The repaired paths and deterministic checks are recorded in
../results/2026-10-26/CORE_BOUNDS_REVIEW.md. That scan covered selected core
files and does not clear the remaining retained implementation.
The same record includes a follow-up repair for oversized mask-filter bounds
and allocation ownership on failed filtered-alpha extraction.

AA clip scanline scratch now follows the active draw width, with checked size
arithmetic and reusable ownership. The original private header is retained for
reference builds. The focused fixture, matching render output and timing limit
are recorded in ../results/2026-10-26/AA_CLIP_SCRATCH.md. This is a bounded
repair, not a full conversion of clipping or scan conversion.

## Bitmap ownership and metadata

The complete bitmap implementation and public header now follow house spelling
and explicit ownership/failure sequencing. Filtered alpha extraction releases
its temporary owners before an unfiltered fallback; metadata borrows reject
temporary bitmaps. Copy/move/reset, shared subsets, pixel transfers, release
callbacks and alpha extraction have a dedicated contract fixture. The original
pre-conversion source and header remain in the reference build. See
../results/2026-10-26/BITMAP_CONVERSION.md. This completes the bitmap facade,
while its device drawing, sampling and canvas callers remain in the programme.

## Color matrices and lookup tables

The matrix value type and lighting factory, matrix and table filters, and
immutable color-table storage are converted. Matrix aliasing and arithmetic are
preserved; table storage is copied once and retained explicitly. Temporary
coefficient/table borrows are rejected. Both release suites pass, the complete
instrumented suite returns PASS, and nine output artifacts match the reference
byte for byte. See ../results/2026-10-26/COLOR_ADJUSTMENT_CONVERSION.md.
Composition and working-space handling are recorded below. The shared conversion
plan and remaining color filters are covered by the next recorded group; skcms
and raster kernels remain in the conversion programme.

## Color-filter composition and working spaces

The color-filter facade, scalar pipeline, composition, working-format wrapper,
color-space transform filter and filtered-shader wrapper now use named ownership
transfers and explicit processing order. Base views reject temporary owners;
the shader wrapper uses its existing base owner directly. Working-format wire
coefficients are assembled as seven-element and nine-element arrays rather than
traversing separate struct members or matrix rows as one array. The reference
keeps the original implementation. See
../results/2026-10-26/COLOR_COMPOSITION_CONVERSION.md for test scope and results.

## Shared color-conversion plan and remaining color filters

SkColorSpaceXformSteps now owns initialized coefficient arrays and explicitly
separates immediate scalar conversion from pipeline construction that borrows
those arrays. Temporary plans cannot publish borrowed pipeline stages. The
blend-mode and Gaussian coverage filters are converted, preserving their mode
simplifications and approximation. The 576-plan comparison includes SDR/HDR,
alpha conventions and raster tails, with all eleven output files matching the
reference. See ../results/2026-10-26/COLOR_PLAN_CONVERSION.md. Lower-level raster
and skcms arithmetic remain separate conversion work.

## Constant-color and working-space shaders

The constant-color shader and working-space wrapper are converted. Working-space
resolution returns named owned fields; temporary shader-color views are rejected.
The wrapper preserves the incoming stage context while replacing color-dependent
fields. This also carries two separately committed corrections: use the resolved
input color space and preserve nonzero tile offsets. The reference fixture keeps
the original color error observable, separately from the common-output comparison;
offset propagation is checked against the candidate's tile-aware stage record.
Candidate/reference suites pass 55/55 and 54/54, all twelve common output files
match, and the full instrumented suite returns PASS.
See ../results/2026-10-26/SHADER_WORKING_SPACE.md. Other shader classes and their
shared base still remain in the conversion programme.

## Shader facade and coordinate wrappers

The public shader facade, local-matrix/CTM wrappers, coordinate clamp and empty
shader now use explicit ownership and named processing stages. Borrowed image
and matrix views reject temporary shader objects. Nested matrix multiplication,
coordinate clamp and captured tile offsets retain their existing behavior.
Candidate/reference suites pass 56/56 and 55/55, all thirteen common artifacts
match, and the complete instrumented suite returns PASS. See
../results/2026-10-26/SHADER_COORDINATE_CONVERSION.md for coverage and limitations.
The shared shader base and remaining sampling classes are still separate work.

## Shared shader base and matrix state

The shared shader base now makes matrix application, context borrowing and
factory ownership explicit. Gradient query fields start initialized; temporary
shared owners cannot supply persistent base views. Uncalled fragment-processor
matrix helpers are removed, with source closure and archive evidence; the CPU
context path remains. Release suites pass 56/56 and 55/55, all thirteen common
artifacts match, and the full instrumented suite returns PASS. A fresh relocated
consumer exercises the public shader interface. See
../results/2026-10-26/SHADER_BASE_CONVERSION.md for the matrix-state checks,
conditional compilation coverage and remaining scope.

## Image-shader sampling and mutable coordinate stages

Image shader setup now has named sampling/gather/color procedures, explicit
arena-backed contexts and a named draw-rectangle result. The mutable coordinate
wrapper owns an initialized matrix array; child-stage failure is propagated in a
separate fix. The expanded corpus covers 16,128 scenes and matches the pre-conversion
candidate throughout. The 15,264 common scenes match upstream; previously corrected
mipmap-format paths remain separately recorded and independently tested. Candidate/
reference suites pass 57/57 and 56/56, fourteen common artifacts match, and the
instrumented suite returns PASS. See
../results/2026-10-26/IMAGE_SAMPLING_CONVERSION.md and TRANSFORM_CHILD_FAILURE.md in
that directory. Only the two required canvas result consumers were adapted.
The integer-translation shortcut now avoids out-of-range float-to-int conversion;
eight signed boundary cases verify correct clamp-edge pixels. See
../results/2026-10-26/IMAGE_TRANSLATION_RANGE.md. Strict-subset behavior still needs
focused follow-up; the common corpus does not establish that contract.

## CPU subset paths

Strict source bounds are now covered by 180 outside-pixel perturbation cases,
including alpha masks, fractional/clipped rectangles, filtering, transforms and
blur. The CPU implementation already extracts bounded pixel views. Unused GPU
subset state and coverage-mask/analytic-blur dispatch branches are removed;
the ordinary CPU bitmap-mask and nine-patch blur paths remain. Both release
suites pass (58/58 candidate, 57/57 reference) and fifteen common artifacts match.
The full instrumented suite returns PASS.
See ../results/2026-10-26/CPU_SUBSET_PATH.md. This resolves the prior strict-subset
follow-up; the broader canvas/device implementation still needs conversion.

## Raster special-image views

The retained special-image interface and raster implementation now name their
pixel owners, bounded views and copy results. Metadata borrows reject temporary
owners. Expanded tests check sharing versus copy isolation, nested coordinates,
source-handle release and 48 additional special-image shader renders. Candidate/
reference suites pass 58/58 and 57/57, all fifteen common artifacts match, and
the full instrumented suite returns PASS. See
../results/2026-10-26/SPECIAL_IMAGE_CONVERSION.md. The surrounding canvas/device
and lower-level sampling implementation remain separate conversion work.

## Gradient geometry and factories

Linear, radial, sweep and two-point conical gradient classes are converted.
Factory early returns are explicit; sweep angles have named fields; focal setup
state starts initialized and metadata borrows reject temporary objects. A new
5,472-render corpus covers degenerate geometry, hard stops, tile modes,
interpolation spaces, alpha conventions and local transforms. Candidate/reference
suites pass 59/59 and 58/58, all sixteen common artifacts match, and the full
instrumented suite returns PASS. See
../results/2026-10-26/GRADIENT_GEOMETRY_CONVERSION.md. The shared gradient base,
color-stop storage, interpolation builder and raster kernels remain unconverted.

## Shared gradient base

The shared stop storage, interpolation transform and pipeline builder are
converted. Owners precede borrowed views; temporary views are rejected; allocation
counts are checked; powerless-hue expansion reserves its output before emission.
Unused Graphite bitmap caching and Ganesh-only position forcing are removed under
the CPU-only scope, preserving all CPU explicit-stop behavior. The expanded corpus
has 34,200 renders across all fifteen interpolation spaces and four hue methods.
Candidate/reference suites pass 59/59 and 58/58, sixteen common artifacts match,
and the full instrumented suite returns PASS. See
../results/2026-10-26/GRADIENT_BASE_CONVERSION.md. Public gradient descriptors and
raster gradient kernels remain separate conversion work.

## Points, rectangles and rounded rectangles

The three public geometry headers, their implementations and private helpers are
converted. The intersection macro with a hidden return is replaced by named
overlap helpers that keep the NaN-rejecting comparison order. Rounded-rectangle
equality and serialization read fields instead of punning arrays or object
bytes; the serialized bytes are unchanged. `SkIRect::outset` and integer point
negation saturate instead of overflowing, and an empty point list is no longer
dereferenced. GPU-only private helpers, a 32-bit bounds branch and two undefined
compatibility branches are removed; no public geometry API is removed. Borrow
accessors on `SkRRect` reject temporaries. Candidate/reference suites pass 60/60
and 59/59, the 9.2 MB corpus and every other shared artifact match except the
intentionally corrected sampling artifact. See
../results/2026-10-26/GEOMETRY_PRIMITIVES_CONVERSION.md. Matrices, paths and
curve geometry remain separate conversion work.

## 3x3 matrices

The public matrix header, its implementation and private helpers are converted.
A mechanical AST pointer rewrite is kept separate from the review. Tiny
dot-product wrappers become formulas with unchanged grouping; precision-naming
helpers remain. Equality no longer claims `restrict` for possibly identical
matrices, own-value `set9` is defined, homogeneous mapping works directly over
spans, and the single-point affine kernel no longer computes from uninitialized
lanes. `SkTreatAsSprite` no longer converts NaN, infinite or out-of-range
translations to `int`. GPU-only and dead private helpers are removed; no public
matrix API is removed. The lazily cached type mask still requires a precomputed
type before cross-thread sharing, now documented. Candidate/reference suites pass
61/61 and 60/60 and every shared artifact matches except the intentionally
corrected sampling artifact. See ../results/2026-10-26/MATRIX_CONVERSION.md.

## Path storage, construction, iteration and serialization

The public path, builder and iterator headers and the path implementation family are
converted. Tuples and pairs become named records, so no structured binding remains;
builder lambdas, dump `std::function` callbacks and three `goto`s become named
functions and structured control flow with the same iteration counts. Reversal no
longer forms a pointer before the point array, and both path iterators no longer start
their conic-weight pointer before the weight array. Allocation failure in path data
construction is reported instead of dereferenced; misaligned serialized input is
rejected; self-referential raw shapes cannot be copied; borrowing accessors reject
temporaries. The original iterator also left its close-line flag uninitialized after
non-line verbs, which the corpus avoids reading. Candidate/reference suites pass 62/62
and 61/61. The corpus and every other shared artifact match, except the intentionally
corrected sampling artifact. See ../results/2026-10-26/PATH_CONSTRUCTION_CONVERSION.md.
Contour measurement, curve geometry, stroking and dashing remain.

## Curve geometry, solvers, cubic clipping and cubic maps

Quadratic, cubic and conic evaluation and chopping, the double-precision Bezier,
quadratic and cubic solvers, the cubic clipper and the public cubic map are converted with
formulas and operand order unchanged. Strided float views of point arrays, bit casts,
lambdas, a variadic template, structured bindings, a macro and a `goto` become named
fields, overloads and structured control flow; `SkConic` and the helper records start
initialized. Routines used only by GPU tessellation, PDF, path ops, tools and upstream
tests are removed and listed. The two-parameter cubic chop ending at 1 no longer returns
the chop point as its end when the source is the destination; a probe reproduced the
original defect. Candidate/reference suites pass 63/63 and 62/62; the new corpus and
every other shared artifact match, except the intentionally corrected sampling artifact;
the instrumented suite reports PASS. See
../results/2026-10-26/CURVE_GEOMETRY_CONVERSION.md. Contour measurement, stroking and
dashing remain.

## Contour measurement

Contour measurement and the path-measure facade are converted. Segment search,
interpolation and extraction use indices, so no pointer is formed before the segment array
or walked past a verb without a bound; segments and verb measures start initialized; the
iterator returns owned measures directly. A declaration with no definition is removed.
Candidate/reference suites pass 64/64 and 63/63; the new corpus and every other shared
artifact match, except the intentionally corrected sampling artifact; the instrumented
suite reports PASS. See ../results/2026-10-26/CONTOUR_MEASURE_CONVERSION.md. Stroking and
dashing remain.

## Stroking, caps and joins, and stroke records

The path stroker, cap and join procedures and stroke records are converted. Three `goto`s
become structured control flow with the same outcomes; path records are named optional
records in loops whose `continue` still advances; stroker, quad-construction and record
members start initialized; factory tables become switches with defined fallbacks.
Debug-only stroker instrumentation is removed. Candidate/reference suites pass 65/65 and
64/64; the new corpus and every other shared artifact match, except the intentionally
corrected sampling artifact; the instrumented suite reports PASS. See
../results/2026-10-26/STROKE_CONVERSION.md. Dashing and path-effect composition remain.

## Dashing, the dash path effect and path-effect composition

Dash filtering, the dash effect and sum/compose effects are converted: axis-aligned line
clipping no longer indexes across struct fields, rectangle edges and last points are named
optional records, and dash records start initialized. Point results from the dashed-point
fast path now own their storage (the raw `new[]` record was copyable and would double
free), and a null result no longer receives a write. Candidate/reference suites pass 66/66
and 65/65; the new corpus and every other shared artifact match, except the intentionally
corrected sampling artifact; the instrumented suite reports PASS after an oversized first
corpus was reduced. See ../results/2026-10-26/DASH_CONVERSION.md.

## Still required

- Finish reference-count/view compatibility cleanup; convert arrays, memory utilities and remaining
  image/pixel views; then mask/blur, sampling, compositing and raster kernels.
- Convert retained geometry, paths, scan conversion, canvas/device state,
  transforms, clipping and surface/image ownership.
- Finish pruning unused generic serialization and unsupported compatibility
  interfaces. Picture, capture and font machinery are removed; preserve external
  glyph coverage and all admitted drawing behavior.
- Convert remaining retained filter, shader, color and skcms implementation;
  retain blur masks, matrix color filters and shader masks. General image filters
  and runtime/SkSL effects and their compiler-only raster stages are removed.
  Convert the remaining raster stage construction, execution and kernels.
- Audit every retained production header and build/source list, including
  templates, generated runtime support and platform ports. Syntax-checker success
  alone is not semantic conversion evidence.
- Complete native platform, installed-package, GUI.Forms and instrumented gates,
  including broader workload and allocation/workspace measurements.
- Complete the owner's independent review and coordinate production integration.

The ledger records completed boundaries; it is not a count-based substitute for
the full scope. Large unconverted portions remain and the goal stays active.
