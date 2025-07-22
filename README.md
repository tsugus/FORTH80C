# FORTH80C

An processor of the programming language FORTH, which run on the OS CP/M-80.

For AltairZ80 (Altair 8800 emulator).
For XM8 (NEC PC-8801 emulator) too.

FORTH80C complies with FORTH-79 standard.

(Since as little assembly code as possible has been written, this implementation may be slightly slower.)

-------------------------
-------------------------

# cpmadd88

This program writes a file into a CP/M disk image in .d88 format.

> ./cpmadd \<filename 1> \<filename 2\> \[ENTER\]

It writes the CP/M file \<filename 2\> into the .d88 file \<filename 1>.

Note. This program does not write into free space between data.
