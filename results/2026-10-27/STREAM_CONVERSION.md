# Streams: prune, corpus, rewrite and fixes — 2026-10-27

The first support-code unit. The owner chose to prune the unused stream classes and convert
the rest. The profile (`SUPPORT_CODE_PROFILE.md`) found streams at 0% of GUI.Forms' Skia
render time, so this unit converts and fixes; it makes no speed claim.

## Prune (1def781c)

The candidate keeps only the abstract interfaces that something uses:
- **`SkStream`:** `read`, `skip`, `isAtEnd`, position and length, for
  `SkData::MakeFromStream`.
- **`SkWStream`:** `write`, `flush`, `bytesWritten` and `writeText`, for `SkPath::dump` and the
  write buffers.
- **`SkStreamPriv::RemainingLengthIsBelow`,** for `SkData`.

These had no user in the library or in GUI.Forms, and are removed:
- the memory, file and dynamic-memory streams, and `SkNullWStream`;
- the rewindable, seekable, asset and memory hierarchy, and `MakeFromFile`;
- typed readers, text and packed writers, and `writeStream`;
- the other `SkStreamPriv` helpers, and `SkMD5`.

Only our own tests used them. Tests that used a concrete stream as a sink now write into
`tests/stream_sink.h`'s `ByteSink`. `tests/data.cpp` checks by expression that the removed
members are absent from the candidate and present in the reference.

`baseline/skia` holds the intake `SkStream.h`, `SkStream.cpp` and `SkStreamPriv.h` (from
490c9bd4) and `SkMD5`, and the reference build compiles them.

## Corpus (5c510650)

`tests/stream_core.cpp` writes `stream-core.raw`, 778 cases:
- **Remaining length:** `RemainingLengthIsBelow` for every combination of known or unknown
  length and position, with lengths, positions and requests around 0, 1, 7, 64 and `SIZE_MAX`;
- **Skip:** `SkStream::skip`, with position and end state afterwards;
- **Text:** `SkWStream::writeText`, with the bytes it wrote.

Both builds wrote identical bytes before any change.

## Mechanical rewrite (6e053cb8)

- **Includes:** trimmed to what the kept code uses (`SkTypes.h`, `<cstddef>`, `<cstring>`).
- **Spelling:** member calls through `(*this)`, and a defaulted `SkWStream` destructor.
- **Result:** `stream-core.raw` is identical to the pre-rewrite output and to the reference.

## Fixes

| Defect | Before | After |
|---|---|---|
| `writeText(nullptr)` | `SkASSERT` only, so Release called `strlen(nullptr)` | Writes nothing and returns `false`. The stream stays usable |
| Null stream in `RemainingLengthIsBelow` | `SkASSERT` only, so Release dereferenced null | `SkASSERT_RELEASE`: a null stream stops the program |
| Position past the length | The intake subtraction wrapped to a huge remainder, so every request looked satisfiable | Already fixed in 34b51825: nothing remains, so every length is below it. The header comment now says so |

**Checks.**
- **Candidate only:** a null text is refused, writes no bytes, and a later write still works;
  a position past the length is refused for 0, 1 and `SIZE_MAX`.
- **Stop test:** `stream_core_stops_null-stream` passes only if the null stream stops the
  program.
- **Reference:** these checks are excluded there, because the intake code is undefined at both
  inputs.

**Not changed: the helper keeps its pointer parameter.** A reference parameter would state the
non-null contract in the type, but the reference build compiles the candidate's `SkData.cpp`
against the intake `SkStreamPriv.h`, which takes a pointer. Switching would need a
reference copy of `SkData.cpp` too, and the intake `SkData` handles short reads differently
(34b51825). That belongs to an `SkData` unit.

## Verification

All on the M4 mirror, sequentially:
- **Candidate:** 91/91, including the new stop test.
- **Reference:** 82/82.
- **Corpora:** `stream-core.raw` is identical in both builds, and all shared artifacts are
  identical apart from the six intentional differences.
- **Instrumented suite:** PASS.
- **Installed package:** a fresh relocated install passes `tools/verify_installed.sh`.
- **GUI.Forms fixture:** 9/9 with `tools/verify_gui.sh` (pinned LLVM 22.1.8).
- **Style:** the lexical checker finds nothing in the five stream files.
- **Inventory:** the AST inventory falls from 801 excluded constructs in 58 files to 619 in 53,
  across the prune and the rewrite. The intake `SkStream.cpp` alone had 120 member arrows.

## Not done

- **Windows and Linux:** run by CI on push, not locally.
- **`SkData`:** the reference-parameter form of the helper waits for an `SkData` unit, as above.
