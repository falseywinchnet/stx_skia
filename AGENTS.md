# stx_skia working contract

Read docs/INTAKE.md, docs/PLAN.md and docs/PROGRAMMING_HOUSE_STYLE.md before edits.
The owner requested a CPU Skia derivative, pruning followed by house-style
refactoring and measured optimization. Preserve the actual consumer contract;
do not substitute a rectangle-only renderer for the required drawing library.

The imported skia/ baseline is third-party source awaiting conversion. Do not
claim it already conforms to house style. New or rewritten C++ follows the full
house style. Keep mechanical rewrites separate from algorithm changes and keep
upstream notices and source provenance. No GPU backend is admitted. Codec and
font-backend ownership stays outside this library.

Source inventory is not a link dependency proof. A function absent from direct
consumer calls can still be an internal prerequisite. Preserve upstream test
sources relevant to retained behavior. Record negative measurements too.

Use the user's /Users/ultimussecundai/.local/bin/m4host selector and m4build
helper for heavy builds. Do not edit remote mirrors as authoritative sources or
resynchronize a mirror while it is in use. Put generated output under build/.
Repository-specific build commands will be recorded with the first build gate.

No new automated memory integrity review is authorized by the intake. At the
requested review gate, expose only PASS or STOP in this chat and leave detailed
investigation to the owner's Daybreak sibling. Ordinary deterministic correctness
and rendering comparisons remain part of implementation work.

Do not modify GUI.Forms integration during this intake. Its existing branch and
unrelated local files must remain intact. No blanket fast-math, guessed restrict
contracts, or unmeasured SIMD speed claims. New SIMD work targets the common
128-bit SSE/NEON width and portable compiler-visible kernels, following the
owner's BFFT direction; imported upstream optimized code is retained as baseline.
