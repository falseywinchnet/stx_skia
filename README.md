# stx_skia

A CPU raster library derived from Skia, being narrowed to the drawing work used
by GUI.Forms and rewritten in the project's explicit C++ house style.

The target is ordinary software rendering: paths, fills, strokes, gradients,
clipping, shadows, bitmap sampling and alpha compositing. Image decoding and
font shaping belong to the consumer's existing libraries.

## Current state

This is the **baseline intake**, not a completed replacement or a performance
release. `skia/` contains the complete pinned upstream source snapshot plus the
two existing GUI.Forms build patches. No additional pruning, algorithm changes
or house-style conversion have been applied to Skia yet. Excluded source remains
in the snapshot; the CPU profiles exclude it from compilation.

The baseline comes from GUI.Forms main at
`8d1f2be06a40c5da78bb75e45cbe1fecec2f6b62`, inspected on 2026-10-25, and Skia
`2a9b593bab4b2fd019fa494c8d401ff1fab0b883` (`chrome/m152`).

- [What GUI.Forms uses, and what we can investigate removing](docs/INTAKE.md)
- [Refactoring and measurement sequence](docs/PLAN.md)
- [Programming house style](docs/PROGRAMMING_HOUSE_STYLE.md)
- [Source identities and license boundaries](PROVENANCE.md)
- [Machine-readable source inventory](baseline/inventory.json)

The first compatibility target is the existing private GUI.Forms Skia adapters,
including GUI.Drawing and the optional prepared-text path. This is not a promise
of compatibility with every upstream Skia API. GUI.Forms has not been changed
to consume this repository.

## Build baseline

`baseline/` retains the original GUI.Forms GN profiles and archive-policy check.
They are evidence, not a new portable build interface: the Unix profile defaults
to arm64 and requires the same platform/compiler setup as GUI.Forms. The Windows
profile retains its MinGW cross-build assumptions. No build has been run for this
intake. The first implementation gate establishes reproducible builds and
consumer rendering comparisons before deleting functionality.

No performance, memory-integrity or security-review claim is made by this intake.
The owner's later independent review is a separate gate.

## Licensing

Skia retains its BSD-style license in [skia/LICENSE](skia/LICENSE), also copied to
[LICENSE-Skia](LICENSE-Skia). Component-specific upstream notices remain in the
source tree. GUI.Forms-derived profiles, patches and house-style material retain
the GUI.Forms license in [LICENSE](LICENSE); that license also covers original
project documentation. Reformatting or refactoring does not remove upstream
copyright notices or change upstream component licenses.
