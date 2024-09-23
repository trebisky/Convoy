This is software for a Convoy S2+ flashlight.

The Convoy light uses a ATtiny13A chip (AVR architecture) to run the light.

This has 1K of flash, 64 bytes (yes bytes) of ram, 64 bytes of EEPROM.

I dug up the original "biscotti" software by ToyKeeper.
I worked up a Makefile for this and dug up various necessary header files.
I got this to where it would build.  This is biscotti_ORIG

As I began to study it and try to work with it, I realized that it contained
a lot of "dead wood" and optional code that was both distracting and confusing.

I made a series of copies, pruning each more than the one before it.

My final product that I am testing in a flashlight is "biscuit"

1. biscotti_ORIG -- the original code, set up to build
2. biscotti -- pruned, but builds the same as the original
3. simple -- pruned more, all strobes removed
4. biscuit -- my own version, pruned and simplified

The final "biscuit" has no mode groups.  It has the one group I want.
It also has no strobes or blinking modes.  No battery monitor.
It does watch the battery voltage and shut down as needed.


