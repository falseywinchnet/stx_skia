# Working-space shader context corrections

The public shader contract says a null input color space inherits the destination.
The wrapper resolved that input for its conversion plans, then gave its child
fInputSpace.get(), which was still null. A constant sRGB shader on a linear
surface consequently produced 0.5 for a red component that should be about
0.213993073. A reproducer failed before the repair and returned the same red
component as the unwrapped shader after passing the resolved input to the child.

The wrapper also omitted the caller's device offset when creating its child
stage record. That offset carries tile placement to clip shaders. The child now
receives the original offset. A stage observer checks nonzero positive/negative
offsets and preserves the existing stop-on-child-failure behavior.

These are context-propagation corrections, separate from the following house
style rewrite. The reference fixture explicitly reproduces the old color-space error;
it does not count that as correct rendering. The original reference stage record
predates the device-offset field, so offset forwarding is a candidate-only check. The byte-comparison artifact holds
216 explicit-input-space cases that should remain unchanged, with inherited and
explicitly equal output spaces compared within each case.
