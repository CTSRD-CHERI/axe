### A consistency checker for memory subsystem traces

Axe is a tool that aids automatic, black-box testing of the memory
subsystems found in modern multi-core processors. Given a trace
containing a set of top-level memory requests and responses, Axe
determines if the trace is valid according to a range of memory
consistency models. It is designed to be used as the oracle in an
automated hardware test framework (such as
[BlueCheck](https://github.com/CTSRD-CHERI/bluecheck) or
[GroundTest](https://github.com/ucb-bar/groundtest/blob/master/src/main/scala/tracegen.scala)),
quickly checking large memory traces that result from randomly-generated
sequences of memory operations.

See the [Axe
manual](https://github.com/CTSRD-CHERI/axe/raw/master/doc/manual.pdf)
for details.
