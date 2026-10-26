# From intake to an owned CPU raster library

1. **Reproduce the baseline.** Build the pinned source with the recorded CPU
   profiles using the non-Apple LLVM toolchain. Retain compile commands, source
   list, archive symbols and linked consumer map. Run GUI.Forms control rendering,
   GUI.Drawing, prepared text, shadow, damage and image-composition comparisons.
   Establish identical inputs and pixel hashes before assigning improvements.
2. **Prune by subsystem.** Start with uncalled serialization/recording and codec
   infrastructure, then investigate unused Skia text internals and general image
   filters. Resolve dependent SkSL/runtime machinery explicitly. Each removal
   gets an independent change and the same rendering gate. Keep useful upstream
   tests and document the exact supported subset.
3. **Convert to house style.** Work through ownership/buffer boundaries, then
   pixel kernels, masks/blur, scan conversion and geometry. Separate mechanical
   naming/spelling changes from altered lifetime, allocation or arithmetic.
   Preserve precision and exceptional behavior unless a measured change is
   explicitly accepted. Do not mass-rewrite all upstream syntax in one commit.
4. **Optimize measured work.** Reduce redundant copies and allocation, hoist
   invariant dispatch, separate interior/edge kernels and establish alias rules.
   Inspect generated common-width vector code and measure small, awkward-stride,
   clipped, translucent and large inputs. Preserve fast paths that already work.
5. **Independent review and integration.** The owner brings a Daybreak sibling
   for detailed review after the relevant boundary is stable. Findings are fixed
   and rechecked. Integrate through GUI.Forms' private renderer seam only after
   its cross-platform and stage gates pass. A review is evidence with a scope,
   not a guarantee that no defects can remain.

This first intake does not silently choose a new public API, drop GUI.Drawing,
remove shadows or alter the user's planned material capabilities. The immediate
conversation is about which upstream facilities the independent library should
retain beyond GUI.Forms' demonstrated usage. The existing consumer surface is
the minimum compatibility target.
