This is software for a Convoy S2+ flashlight.

All of this culminated in "biscuit", which is firmware I now use
in my Convoy lights.

========================

The Convoy light uses a ATtiny13A chip (AVR architecture) to run the light.

This has 1K of flash, 64 bytes (yes bytes) of ram, 64 bytes of EEPROM.

I dug up the original "biscotti" software by ToyKeeper.
I worked up a Makefile for this and tracked down the various necessary header files.
I got this to where it would build.  This is biscotti_ORIG

As I began to study it and try to work with it, I realized that it contained
a lot of "dead wood" and optional code that for me was both distracting and confusing.

I made a series of copies, pruning each more than the one before it.

My final product that I am using in my flashlights is "biscuit"

1. biscotti_ORIG -- the original code, set up to build
2. biscotti -- pruned, but builds the same as the original
3. simple -- pruned more, all strobes removed
4. biscuit -- my own version, pruned and simplified

The final "biscuit" has no mode groups.  It has the one group I want.
It also has no strobes or blinking modes.  No battery monitor.
It does watch the battery voltage and shut down as needed.

I do development on my Fedora linux laptop (currently running Fedora 40).
I need to install the following packages to build this code:

dnf install avr-gcc

dnf install avr-libc

dnf install avrdude

The final set of brightness levels in "biscuit.hex" is:

0, 1, 7, 15, 32, 63, 127, 255

This seems to work well and the first 3 (1,7,15) are ideal for most purposes.



