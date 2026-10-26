# Interrupted inline TArray swap: preserved STOP

This branch adds five deterministic move-construction interruption points to
the inline `TArray::swap` regression. The test checks the documented basic
failure state, live-object counts, subsequent append, and unused-capacity
poisoning where the instrumented build exposes it.

The M4 candidate Release build and complete 50-test suite passed. The
candidate instrumented runner printed `STOP` and exited with status 1. Its
diagnostics were **not read in this chat**, following `AGENTS.md`. The original
log and a preserved copy are in the M4 mirror at
`build/candidate-sanitize/integrity.log` and
`build/candidate-sanitize/integrity-tarray-interrupted-swap-stop.log`.
The mirror workspace is
`/Users/joshuahkuttenkuler/Developer/CodexBuilds/stx_skia-853b87f3f8f5`.
The test source SHA-256 at the stopped run was
`71c2a50accec4de09ea61519349849c9a75d9448359dbb0a40e7f33a0a9cdcdc`.

The spelling and lifetime checkers found zero issues in the changed test file.
This branch is a preserved reproducer for the owner's independent Daybreak
review. No cause or security conclusion is inferred from `STOP` alone.
