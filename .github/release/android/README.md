# RETI Emulator for Android

This archive supports 64-bit ARM devices running Android 7 or newer in Termux.
Extract it in Termux and run `./reti-emulator/reti_emulator -h`.

The archive includes ncurses, its terminal database, and the vendored cJSON
code used by the emulator. Normal emulation does not require Python. Source
debugging additionally needs Python 3 with tkinter. If they are missing, the
emulator prints an installation error while normal emulation keeps working.
