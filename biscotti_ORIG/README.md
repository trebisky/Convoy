This is my original copy of ToyKeepers biscotti firmware for
the Convoy S2+ flashlight (with NANJG driver)

To be able to build this I needed to do the following

1) I copied a Makefile from one of Lpodkalicki's attiny13 demos
and used it as a starting point

2) All inline functions needed to have "static" added (so they now say static inline ...)

After this it is 18 bytes too big, but otherwise compiles clean.

3) To make it fit, I clumsily comment out all the strobe stuff.
	Now it fits into 968 bytes.
