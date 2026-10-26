# Source provenance

Initial intake: 2026-10-25.

- Upstream: https://github.com/google/skia (official mirror of
  https://skia.googlesource.com/skia.git).
- Exact Skia commit: `2a9b593bab4b2fd019fa494c8d401ff1fab0b883`, GUI.Forms'
  existing chrome/m152 pin. This is deliberately not an upgrade to Skia tip.
- GUI.Forms: https://github.com/falseywinchnet/gui_forms at
  `8d1f2be06a40c5da78bb75e45cbe1fecec2f6b62`.
- Import: `git archive` of that Skia commit into `skia/`, followed by the
  existing `skia_png_only.patch` and `skia_windows_mingw.patch` under `baseline/`.
  Their historical names are retained; the active profile now admits no codec.
- GUI.Forms build profiles, archive check and programming-house-style document
  are copied verbatim. Their original license is retained at the repository root.
- `baseline/inventory.json` records the source hashes, direct consumer includes,
  and imported source identity. Direct include inventory is not transitive closure.

No external dependency checkout, generated executable, downloaded tool, private
asset, credential, local .claude material or GUI.Forms working-tree content is
included. The imported upstream snapshot contains its own data and notices.
Build tools/dependencies referenced by upstream are not automatically fetched.

Skia copyright and license notices, including third-party notices distributed
inside its snapshot, remain in force. Derivation and house-style conversion do
not make that code originally authored here. The separate Git repository starts
with this attributed snapshot; upstream history remains available at the pin.


## First CMake implementation

`cmake/sources.cmake` records the dependency closure of the original macOS CPU
GN target (491 source units including skcms). CMake applies explicit platform
port selection; `cmake/prune.cmake` describes candidate exclusions. The retained
baseline uses `baseline/SkData.cpp`, copied unchanged from the original import.

Changes to imported implementation:

- `skia/src/core/SkData.cpp`: explicit ownership, named release procedures,
  bounds/allocation arithmetic, house-style spelling. Original BSD notice kept.
- `SkOpts.cpp`, `SkBitmapProcState_opts.cpp`, `SkBlitMask_opts.cpp`,
  `SkBlitRow_opts.cpp`, `SkMemset_opts.cpp`, `SkSwizzler_opts.cpp`, under
  `skia/src/core/`: candidate-only conditional to omit higher-ISA dispatch.
  The low-precision raster pipeline remains enabled.
- 2,016 files (63,499,658 source bytes) removed as listed in
  `baseline/pruned-source-files.json`: GPU implementation, unused modules,
  platform demo/tool infrastructure and website. They remain recoverable from
  initial repository commit `490c9bd4a97884d268a5b3f9924748ebf31954bb`.

New `src/`, `include/stx_skia/`, tests and tools are original project additions.
`integration/gui_forms_8d1f2be0.patch` is derived from the pinned GUI.Forms sources
and retains their license. Its generated fixture and dependency checkouts are
excluded from version control. Benchmark archives/binaries are not distributed.


## Pixel ownership conversion

The second batch converts SkData's header and the SkPixelStorage, SkPixelRef,
SkMallocPixelRef and SkIDChangeListener implementations and headers. Original
versions are retained under `baseline/skia/` and used by the reference build.
Callback sequencing, identity arithmetic and atomic ordering are preserved.
The callback-owned, allocated and data-backed pixel references now have named
implementation classes; listener traversal has explicit types and mutation order.
Upstream notices remain on every derivative file. The new pixel_ref test is an
original deterministic contract test, not an imported upstream test suite.
