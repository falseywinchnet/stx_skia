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

## Still required

- Convert retained reference counting, arrays, memory utilities and remaining
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
