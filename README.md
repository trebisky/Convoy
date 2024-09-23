This is software for a Convoy S2+ flashlight.

The Convoy light uses a ATtiny13A chip (AVR architecture) to run the light.

This has 1K of flash, 64 bytes (yes bytes) of ram, 64 bytes of EEPROM.

I dug up the original "biscotti" software by ToyKeeper.
I worked up a Makefile for this and dug up various necessary header files.
I got this to where it would build.  This is biscotti_ORIG

As I began to study it and try to work with it, I realized that it contained
a lot of "dead wood" and optional code that was both distracting and confusing.
So I decided to make a copy of my own and prune it down.  This is "biscotti".


