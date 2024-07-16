# FORTH80C

An processor of the programming language FORTH, which run on the OS CP/M-80.

For AltairZ80 (Altair 8800 emulator).
For XM8 (NEC PC-8801 emulator) too.

FORTH80C compliants<sup>*</sup> with FORTH-79 standard.

(<sip>*</sup> However, this does not include the rather unreasonable requirement that a "block" must contain 1024 bytes.)

-------------------------
-------------------------

# cpmadd88

This program writes a file into a CP/M disk image in .d88 format.

> ./cpmadd \<filename 1> \<filename 2\> \[ENTER\]

It writes the CP/M file \<filename 2\> into the .d88 file \<filename 1>.

Note. This program does not write into free space between data.
