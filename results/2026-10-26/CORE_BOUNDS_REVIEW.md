# Retained core bounds and stream review

The Codex Security Standard scan at source commit
`544d24e0932b9194180557b88f8b9375bb405eb8` reviewed selected paths under
`skia/src/core`. Its scan ID is `22cb8ae0-3021-4678-aef6-7da4f5b26517`.
It reported four findings in three operations. Coverage is partial: reviewers
fully inspected named parser, pixel, cache and geometry files, not all 339
tracked core files. It does not establish a library-wide security clearance.

| Operation | Defect and repair | Reachability |
| --- | --- | --- |
| `SkBase64::Decode` | The inner loop read beyond the explicit input extent and could write a fifth symbol into a four-byte stack array. The decoder now checks every input read, completes each quartet within its four slots, validates padding before writing, and reports zero length on failure. | Compiled private decoder; no retained core caller or installed declaration was found. Source-tree integrations could call it. |
| `SkWStream::writeStream` | The copy discarded the byte count returned by `read`, then wrote uninitialized scratch bytes. It now writes and advances by the actual count, continues after positive short reads and fails at early EOF or an invalid count. | Public stream API; exposure requires a source that ends or fails before the requested copy length. |
| `SkBitmap::extractAlpha` | A padded filter mask could provide `height * rowBytes` bytes while the bitmap allocation reserves only the active final-row pixels. The copy now uses the destination allocation extent and checks the source extent. | Retained blur and shader filters return tight rows. A source-tree custom padded filter can exercise the path. |

The new candidate-only `core_bounds_contract` covers exact-size Base64 input,
multiple quartets, padding errors, whitespace, positive short reads and early
EOF. `mask_filter_contract` uses a padded custom filter at widths 1, 2 and 3
to verify the extracted active pixels. On the M4 Mini the candidate release
suite passed 50/50 tests, the reference release suite passed 49/49, and the
candidate sanitizer runner reported only `PASS`. The paired render-artifact
comparison passed with identical output. The render timing trial is not a
claim about the performance of the modified functions. A fresh-prefix installed
consumer also compiled, linked and ran.

The review also raised an extreme-coordinate clip translation question.
Rechecking the branch inequalities showed the suspected narrowing and
subtraction values stay within signed `int` whenever those branches execute.
No change to clip translation is justified by that candidate.
