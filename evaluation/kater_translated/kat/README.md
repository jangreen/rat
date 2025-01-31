The memory model files were taken from Kater and adapted to RAT's syntax.

We made a non-trivial change to three of the four Power models:

    eco*;po?;eco* --> eco;po?;eco*

We believe the former version is an error in the files as it contains the identity
which breaks the compilation tests for Power.
Kater's paper also suggests that the latter one is correct.