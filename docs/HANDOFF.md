# Handoff to the GUI.Forms sibling sessions — 2026-10-27

This note is for the sessions continuing the stx_skia port from GUI.Forms. Read AGENTS.md,
docs/INTAKE.md, docs/PLAN.md and docs/PROGRAMMING_HOUSE_STYLE.md first. Then read this note,
docs/CONVERSION.md (the ledger; its "Still required" section lists open work) and
docs/OPTIMIZATION_STUDY.md (before any performance work).

## Where things stand

- **Branch:** `codex/cpu-library`, PR #1 on falseywinchnet/stx_skia. Every unit is pushed, and
  CI (native Linux x64/arm64, Windows CLANG64, GUI.Forms fixture on Windows) passed through the
  dead-code removal. The support-leftover commits were pushed with this note.
- **Last local gates (M4 mirror):** candidate 123/123 and reference 90/90, with no warnings. The
  instrumented suite gave PASS; the installed package passes; the GUI.Forms fixture gives 9/9.
- **Inventory:** 249 excluded constructs in 32 files. That count measures spelling only.
  Unconverted code with no flagged construct does not appear in it.
- **Converted and fixed units:** each has a record in `results/`. The latest are, in order:
  - `STREAM_CONVERSION.md` and `STRING_CONVERSION.md`;
  - `TARRAY_CONVERSION.md`, `BLOCK_LIST_CONVERSION.md` and `ARENA_CONVERSION.md`;
  - `DEAD_CODE_REMOVAL.md` and `SUPPORT_LEFTOVERS.md`.

## Owner decisions to respect

- **Fix, don't just respell.** "Conversion" means fixing or replacing bad code, not respelling
  it.
- **Prune lightly.** Keep general-purpose APIs whole (`TArray` and the like); ask before removing
  members. Whole unused internal subsystems may go, with build proof. Public APIs with no caller
  (`SkDataTable`, `SkPoint3`, `SkTiledImageUtils`, the Perlin-noise shaders, `SkTime`,
  `SkFloatInterpFunc`, `SkMaskFilter::Deserialize`) stay and are converted with their units.
- **Android pieces stay.** The owner may use them.
- **No GPU backend.** Codecs and font backends stay outside the library.
- **Performance rules.** No fast-math, no guessed `restrict`, no unmeasured SIMD claims. New SIMD
  targets 128-bit portable kernels. Imported optimized code is the baseline; measure with the
  GUI.Forms renderer A/B (docs/OPTIMIZATION_STUDY.md).
- **Integrity diagnostics are the owner's.** Report only PASS or STOP from the instrumented
  suite, and never read `build/candidate-sanitize/integrity.log`. No automated memory review.
- **Keep GUI.Forms untouched.** Don't modify the authoritative GUI.Forms checkout. Integration
  goes through the pinned fixture and the patches in `integration/`.

## How a unit is done

1. **Find users.** Run `tools/link_closure.py` (symbol level, compiled objects only) on the
   build host. Check GUI.Forms with grep over `build/gui-fixture`, and fetch `main` for reading
   only.
2. **Intake copies first.** Before changing any file, put its intake copy in `baseline/skia`:
   `git show 490c9bd4:<path> > baseline/<path>`. Add `.cpp` files to the
   `foreach(unit IN ITEMS ...)` lists in `cmake/reference.cmake`. Without this, the reference
   build compiles the candidate's file, and identical artifacts prove nothing. Headers resolve
   baseline-first on the reference include path.
3. **Corpus.** Write a `tests/<unit>_core.cpp` on `tests/differential_corpus.h`. Register it in
   four places:
   - `CMakeLists.txt`: the executable and test, the `STX_SKIA_REFERENCE=1` definition and the
     `-fno-rtti` options;
   - `tools/build_native.sh`;
   - `.github/workflows/native.yml`, in both of its compare steps.

   Rules for what the corpus records:
   - Never record addresses.
   - Keep alignments at 8 or below. `alignof(std::max_align_t)` is 8 on Apple arm64, MSVC and
     wasm, so 16-byte alignment is address-dependent there.
   - Use only inputs both builds define. Put intentional differences in a second artifact
     passed as `argv[2]`, and show that every difference has the stated cause.
   - Commit the corpus with both builds identical.
4. **Respell.** Make a mechanical respelling, prove the artifacts byte-identical to the
   pre-rewrite output and to the reference, and commit.
5. **Fix.** Make the fixes, with candidate-only checks under `#ifndef STX_SKIA_REFERENCE` and
   Release-check stop tests. Stop tests are registered with `cmake/expect_stop.cmake`, which
   accepts any signal: it shows that the program does not continue, not that a check fired.
6. **Negative control.** Run the new checks against the unfixed sources. After restoring the
   fixed files, `touch` them. The mirror keeps file times, and a restore in the same second as
   a build leaves stale objects that ninja won't rebuild.
7. **Close out.** Run the full gates in order (candidate, baseline, candidate-sanitize), the
   installed check (`tools/verify_installed.sh` on fresh `build/install-*` directories), the
   GUI.Forms fixture, and the inventory. Write a record in `results/` and a ledger entry, then
   commit and push.

Commands (from the stx_skia checkout; m4build syncs the tree to the mirror, deletions
included):

```sh
/Users/ultimussecundai/.local/bin/m4build -- env PATH=/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/bin:/bin sh tools/build_native.sh candidate
/Users/ultimussecundai/.local/bin/m4build -- env PATH=/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/bin:/bin sh tools/build_native.sh baseline
/Users/ultimussecundai/.local/bin/m4build -- env PATH=/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/bin:/bin sh tools/build_native.sh candidate-sanitize
/Users/ultimussecundai/.local/bin/m4build -- env PATH=/opt/homebrew/opt/llvm@22/bin:/opt/homebrew/bin:/usr/bin:/bin GUI_FORMS_LLVM_RUNTIME=<pinned LLVM 22.1.8 runtime> sh tools/verify_gui.sh
python3 ~/gui_forms/tools/check_house_style.py <files>
```

**Comparing the profiles.** Use `cmp` on `build/candidate/*.raw` against `build/baseline/*.raw`.
The intentional differences are listed below.

**Inventory.** Run `inventory.sh` from `house-policy-check-tool` on the mirror. Count findings
whose `constructKind` is one of: `pointer_member_arrow`, `auto_type`, `lambda`,
`decltype_expression`, `trailing_return` or `structured_binding`.

## Intentional differences between the profiles (nine)

| Artifact | Why it differs |
|---|---|
| `android-clip-reset.raw` | A canvas reset now clears the Android clip restriction |
| `image-sampling-corrected.raw` | Corrected image-sampling behaviour |
| `m44-corrected.raw` | Corrected 4×4 matrix behaviour |
| `matrix-invert-small.raw` | Inversion of very small matrices |
| `patch-utils-large.raw` | Large patches are reduced instead of refused |
| `raster-clip-stack-replaced.raw` | The replaced raster clip stack |
| `tarray-core-capacity.raw` | The intake move constructor reported size as capacity |
| `arena-core-layout.raw` | Nontrivial arena arrays carry a 4-byte constructed count |
| `utf-core-strict.raw` | UTF decoding now rejects non-Unicode input |

Each unit's record in `results/` explains its entry in full.

## Open work, roughly in order

1. **Support leftovers.**
   - **Code:** `SkRandom`, `SkChecksum`, CPU detection (`SkCpu`), time, debug, assertion and
     log helpers, the base headers (`SkTypes.h`, `SkFeatures.h`, `SkMacros.h`,
     `SkAttributes.h`) and the OS file ports (`SkOSFile_*`, used by `SkData`'s file loaders).
   - **Compatibility remnants:** `sk_sp::operator->` and the `SkSpan` traits.
2. **Colour.** `SkColor`, `SkColorSpace`, `SkBlendMode` and `SkColorData`. Ask the owner whether
   skcms (about 5,600 lines of third-party colour management) is converted or kept as an
   imported module before starting on it.
3. **Pixel kernels.** `SkVx`, `Sk4px`, row blending, mask blitting and swizzles. These are the
   optimized baseline: give them corpora and a respelling, and change an algorithm only with
   measurement. The wasm build (SSE4.1 over SIMD128) and the planned Android NDK build (NEON)
   must keep compiling.
4. **Canvas-adjacent code.** `SkRSXform`, `SkPoint3`, lattice iteration, `SkTiledImageUtils`,
   `SkNoDrawCanvas`, `SkPathUtils`, and the out-of-line `SkCanvasPriv` lattice helpers (unused).
5. **Deferred defects.** The never-released CPU recorder and context singletons;
   `SkMatrix::normalizePerspective` with infinite entries; about a dozen unregistered stop
   modes.
6. **Swap kernel follow-ups on the GUI.Forms side:**
   - `release_hdc` copies a BGRA lease back into an RGBA bitmap without swapping (red comes back
     blue, confirmed on Windows CI);
   - the fifth byte loop, in `material_image.cpp`;
   - wasm object-code inspection.
7. **Build profiles.** An Android NDK build (aarch64, and x86_64 for the emulator), and a check
   that GUI.Forms' wasm configuration still compiles.
8. **Optimization.** Follow docs/OPTIMIZATION_STUDY.md. Fills are near store bandwidth, so the
   open lever is overdraw.
9. **Close out.** A full audit of every retained header and source list, then the owner's
   independent review (PASS or STOP only), then integration through GUI.Forms' private renderer
   seam.

## Pitfalls already met

- **Shell.** zsh doesn't word-split variables and treats `$P:s` as a modifier. Run loops under
  `bash -c` and write `${P}:path`.
- **Stale objects.** `tools/link_closure.py` counts only objects of sources in
  `compile_commands.json`; objects left in the build directory misled one earlier check.
- **`SkUNREACHABLE` traps.** It is `__builtin_trap()` (or `__fastfail` with MSVC), so bounds and
  emptiness checks built on it already stop in Release.
- **`SK_MinS32` is `-INT32_MAX`,** not `INT32_MIN`. The saturating helpers clamp there, so don't
  reuse them where `INT32_MIN` is a valid result.
- **Fixture on the mirror.** The GUI.Forms fixture must be copied to the mirror separately,
  because m4build excludes `build/`.
- **CI polling.** Don't poll CI. Auto-fix is on for PR #1.
