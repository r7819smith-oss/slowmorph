# SlowMorph for the M-VAVE FM1

Nothing here yet. This folder will hold a port of SlowMorph to the FM1's open
firmware (V14 development kit).

The feasibility study found that the crossfade and granular engines should
fit on the FM1, and the spectral engine only in a reduced form that has to be
measured on the device first. The FM1's sample bank holds about 19.5 s of
mono 32 kHz audio in total.

The plan is to port to portable C, then test in the kit's browser simulator.
Nothing will be flashed to a device without a separate review.
