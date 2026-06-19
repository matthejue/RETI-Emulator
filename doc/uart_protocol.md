# UART Protocol Cheat Sheet

UART memory cells: `R0` send, `R1` receive, `R2` status. In `R2`, `b0 = send ready` and `b1 = receive ready`.

UART transfers are raw 8-bit bytes only. There is no datatype byte and no integer/string framing. ASCII characters are transferred as their byte values.

Send one byte: write byte to `R0`, clear `b0`; emulator later sets `b0` and prints that byte as an ASCII character.

Receive one byte: clear `b1`; emulator later writes the next buffered ASCII byte to `R1` and sets `b1`.

Input is buffered. Interactive input can contain multiple characters; they are consumed one byte at a time. With `-m`, one separator space/tab after `# input:` is skipped and the rest of the line is used as the initial byte buffer, including further spaces.
