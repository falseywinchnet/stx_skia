# stx_skia

A CPU raster library for GUI.Forms, derived from Skia and narrowed to its drawing contract.

Paths, gradients, clipping, shadows, bitmap sampling and alpha compositing stay
inside the renderer. Image decoding and font shaping stay with the consumer:
GUI.Forms uses `stx_png`/`stx_zlib` and its CNG resource path for images, and
HarfBuzz/FreeType for text. This library receives pixels and coverage masks.

## Working implementation

The first CMake implementation builds a static library and an installable package.
It preserves the private Skia drawing vocabulary used by GUI.Forms. It is **not**
a complete upstream Skia distribution, and most retained upstream implementation
still awaits house-style conversion.

- CPU surfaces, paths, strokes, dashes, clipping and affine transforms.
- Linear, radial and sweep gradients; nearest, linear, cubic and mipmap sampling.
- Layers, group opacity, blend modes, rounded corners, blur masks, shader masks and matrix color filters.
- RGBA/BGRA pixels and A8 masks, with explicit transfer of owned pixel storage.
- No compiled GPU backend, image decoder/encoder or platform font backend.

The candidate removes generic codec scaffolding, Apple's encoded-image generator,
unused effects and wider CPU-dispatch objects. Excluded GPU implementations,
unused modules and upstream infrastructure have also been removed from the tree.
All 17 general image-filter factory implementations and their public factory
header are removed from the candidate. GUI.Forms owns CSS and backdrop effects.
The core image-filter graph, cache, paint field and device/image entry points are
also removed. Ordinary layers compose devices directly. SkSL/runtime effects, programmable
meshes and shader-language capability APIs are also removed. The canvas no longer
exposes text drawing or owns glyph-run scratch; external A8 glyph-mask composition
remains. Picture recording/playback/capture, picture-backed images and shaders,
font parsing/rasterization, text blobs, typefaces and strike caches are removed.
The retained CPU pipeline has no font engine dependency. Surface APIs expose CPU
pixel ownership, snapshots and reads; unsupported GPU texture/context/semaphore
interfaces and CPU-unimplemented surface YUV reads are removed.

On the initial recorded M4/macOS build, the candidate passed seven selected GUI.Forms
renderer tests and produced byte-identical output across 24 baseline comparison
cases. Its static archive is 6.1% smaller, with essentially unchanged rendering
time in that small workload. Pixel ownership transfer removes a full buffer copy.
See the [measurement record](results/2026-10-25/README.md) for exact workloads,
times, negative results and limits. Cross-platform consumer gates and the owner's
independent review remain outstanding.

## Build and consume

Use non-Apple Clang, CMake 3.24 or later and Ninja. No dependency download is needed
for the standalone library.

```sh
cmake -S . -B build/candidate -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build/candidate
ctest --test-dir build/candidate --output-on-failure
cmake --install build/candidate --prefix /your/install/prefix
```

```cmake
find_package(stx_skia CONFIG REQUIRED)
target_link_libraries(your_private_renderer PRIVATE stx_skia::stx_skia)
```

The exported target requires C++20. Skia classes do not supply RTTI; translation
units deriving from them must respect that boundary. Consumer RTTI for its own
types remains available. Instrumented builds exclude RTTI-dependent vptr checks
at this interface. Windows Clang consumers inherit the Microsoft syntax extension
required by upstream headers. Keep Skia types behind the application's
private renderer adapter. Installed headers include upstream declarations beyond
the admitted subset; their presence does not promise those facilities are linked.
`SkImages::DeferredFromEncodedData` rejects encoded input. Decode externally.

`stx_skia::take_pixel_data` transfers a `std::vector<std::byte>` allocation into
immutable `SkData` and leaves the source empty. Callers must surrender writable
aliases. The returned data, shared subsets and images retain the storage until
its final release. Skia's existing fail-fast allocation policy still applies.

`tools/build_native.sh baseline` retains the original source closure for paired
comparison; `candidate` selects the reduced library. `candidate-sanitize` runs
the deterministic tests with instrumentation and prints only PASS or STOP for
that test execution. `tools/verify_installed.sh` checks a relocated installation.
The retained GN profiles describe the original intake, not the pruned build.

## GUI.Forms integration

[The integration patch](integration/gui_forms_8d1f2be0.patch) and its preparation
script build an isolated fixture at GUI.Forms commit
`8d1f2be06a40c5da78bb75e45cbe1fecec2f6b62`. The fixture transfers decoded/remapped
pixel vectors and reuses gradient stop storage. Borrowed texture snapshots retain
their existing copy because their ownership cannot be transferred.

This verification patch is a handoff artifact, not a completed change to
GUI.Forms' production build or dependency manifest. The authoritative GUI.Forms
checkout is unchanged. Its coordinator owns integration and platform gates.

## Design and provenance

- [Consumer scope and pruning investigation](docs/INTAKE.md)
- [Coordinator requirements, including future material capabilities](docs/COORDINATOR_REQUIREMENTS.md)
- [Implementation sequence and remaining work](docs/PLAN.md)
- [Full conversion ledger](docs/CONVERSION.md)
- [Programming house style](docs/PROGRAMMING_HOUSE_STYLE.md)
- [Source identities and modifications](PROVENANCE.md)
- [Original source inventory](baseline/inventory.json)
- [Removed-source manifest](baseline/pruned-source-files.json)

Skia is pinned to `2a9b593bab4b2fd019fa494c8d401ff1fab0b883` (`chrome/m152`).
Its BSD-style license remains in [skia/LICENSE](skia/LICENSE), also copied to
[LICENSE-Skia](LICENSE-Skia). Component notices remain with retained sources.
Original additions and GUI.Forms-derived material use [LICENSE](LICENSE).
Refactoring does not replace upstream copyright or component licenses.
