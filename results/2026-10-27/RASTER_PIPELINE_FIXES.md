# Raster pipeline defect fixes — 2026-10-27

These fixes follow the mechanical step in RASTER_PIPELINE_SPELLING.md (49eab37). Each is a
separate change.

## 1. Memory contexts arrive mutable

**Defect.** `appendLoad`, `appendLoadDst` and `appendStore` took
`const SkRasterPipelineContexts::MemoryCtx*`. `append(op, const void*)` cast away const for any
op. But `run()` and compiled thunks write every recorded memory context: for a row's tail, they
repoint `pixels` at biased scratch storage, run the program, and restore the pointer. Writing a
context that is actually a const object is undefined behaviour. The signatures also told
callers their context was left alone. The raster blitter relied on this. Its `const` helpers
`appendLoadDst` and `appendStore` handed out the blitter's own `fDstPtr`, which its compiled
blits then repointed on every tail.

**Fix.**
- The three helpers take a mutable `MemoryCtx*`.
- The op table that records memory use is now one named function, `memory_use()`; it was a
  switch inside `uncheckedAppend`. `uncheckedAppend` and the const overload both use it.
- `append(op, const void*)` now refuses, in Release, every op whose context `run()` patches: the
  loads, stores, scale and lerp memory ops, `srcover_rgba_8888`, the debug stores, and
  `emboss`'s context pair. Read-only contexts (constants, matrices, transfer functions) still
  pass through it.
- The blitter's `appendLoadDst` and `appendStore` are no longer `const`.
- No other caller changed: each already passed a mutable local or member.

**Evidence.**
- Compile-time checks in `tests/raster_pipeline_corpus.cpp` (candidate only) accept mutable
  contexts and reject const ones for all three helpers.
- The Release refusal has no death test. The suites exercise every retained caller without
  reaching it.
- Rendering is unchanged:
  - Candidate release 75/75 PASS; reference release 74/74 PASS.
  - The pipeline corpus (hash 13484477241382582955) and every other shared artifact match;
    `image-sampling-corrected.raw` differs, an earlier intentional correction.
  - Instrumented suite PASS; diagnostics unread.
- The kernel header is untouched, so the object-code identity check in the mechanical step
  still covers it.

## Still open

- **Shared state in compiled pipelines.** A compiled pipeline's program and patch records, and
  the contexts they repoint, are shared by every copy of the thunk. Overlapping invocations
  race. `compile()` documents this. Callers today are single-threaded blitters that own their
  thunks.
- **Any-pointer stage contexts.** The kernel header's `Ctx` adapter converts the stage's
  `void*` context to whatever pointer type a stage declares. Pairing each op with its context
  type would let both `append` and the stages check it. That changes the op list and the
  stage-defining macros, so it is a design change of its own.
