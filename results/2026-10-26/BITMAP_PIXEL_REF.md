# Bitmap pixel-reference origin boundary

The published CPU library accepted a `SkPixelRef` origin outside the reference
extent in Release builds. `SkBitmap::setPixelRef` asserted only in Debug, then
formed an address from the unchecked `dx` and `dy`. Painting a 1×1 bitmap with
an origin of `(1, 0)` into a 1×1 reference reproduced an AddressSanitizer
out-of-bounds write in the pre-fix candidate.

The corrected boundary checks that the reference dimensions and row stride can
contain the bitmap. It detaches references with impossible geometry and pins an
out-of-range origin to the valid interval in Release, matching the documented
contract. Debug retains the caller-origin assertions. Valid nonzero origins and
zero-width views remain represented. The caller still owns the backing memory
contract of a `SkPixelRef` created from an external address.

The pixel-reference fixture covers valid origins, both sides of invalid offsets,
extreme integer offsets, impossible bitmap extents and short strides. On the
current source the M4 candidate and reference Release suites each pass 44 tests;
candidate Release and Debug instrumented suites report PASS. Nine rendering
artifacts and the serialized artifact match the reference byte-for-byte.
These checks establish this boundary fix, not a repository-wide security claim.
