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
included. The unchanged upstream snapshot contains its own data and notices.
Build tools/dependencies referenced by upstream are not automatically fetched.

Skia copyright and license notices, including third-party notices distributed
inside its snapshot, remain in force. Derivation and house-style conversion do
not make that code originally authored here. The separate Git repository starts
with this attributed snapshot; upstream history remains available at the pin.
