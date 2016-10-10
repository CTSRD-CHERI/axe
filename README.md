# Axe

Axe is a tool that aids automatic black-box testing of the memory
subsystems found in modern multi-core processors.

Given a trace containing a set of top-level memory requests and
responses, Axe determines if the trace is valid according to a range
of memory consistency models.

It is designed to be used as the oracle in an automated test
framework, and has been applied to various open-source processors,
including:

1. [BERI](http://www.cl.cam.ac.uk/research/security/ctsrd/beri/),
with traces generated using an RTL-level
[BlueCheck](https://github.com/CTSRD-CHERI/bluecheck) test
bench, as well as auto-generated ISA-level tests.

2. [Rocket Chip](https://github.com/ucb-bar/rocket-chip), with traces
generated using the RTL-level
[groundtest](https://github.com/ucb-bar/rocket-chip/tree/master/src/main/scala/groundtest) framework.

A [paper](https://www.repository.cam.ac.uk/handle/1810/260225) about
Axe was published at [FMCAD
2016](http://www.cs.utexas.edu/users/hunt/FMCAD/FMCAD16). Slides from
the presentation are
[here](http://www.cs.utexas.edu/users/hunt/FMCAD/FMCAD16/slides/s7t4.pdf).

Further details may be found in the [Axe
manual](https://github.com/CTSRD-CHERI/axe/raw/master/doc/manual.pdf).
