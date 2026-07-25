# UART Protocol Cheat Sheet

UART memory cells: `R0` send, `R1` receive, `R2` status. In `R2`, `b0 = send ready` and `b1 = receive ready`.

UART transfers are raw 8-bit bytes only. There is no datatype byte and no integer/string framing. ASCII characters are transferred as their byte values.

Send one byte: write byte to `R0`, clear `b0`; emulator later sets `b0` and
emits that byte to standard output or the debug terminal capture.

Receive one byte: clear `b1`; emulator later writes the next buffered ASCII byte to `R1` and sets `b1`.

Input is buffered. Interactive input can contain multiple characters; they are consumed one byte at a time. With `-m`, one separator space/tab after `# input:` is skipped and the rest of the line is used as the initial byte buffer, including further spaces.

## Output control frames

`ESC` below is the ASCII escape byte `27`. A control frame has the byte form
`ESC command ESC /`. Every byte in the frame, including both escape bytes and
the final slash, is consumed by the emulator and is not written to the terminal
or the currently selected output file.

- `ESC load <path> ESC /` appends a file to the UART input buffer. The emulator
  first appends the file's 32-bit big-endian word count and then the file bytes.
  Plain, unframed `load <path>` text has no special meaning and is printed
  normally.
- `ESC !<terminal_command> ESC /` runs the command through the shell in the
  emulator process's current working directory.
- `ESC <path> ESC /` creates or truncates `<path>` and routes subsequent UART
  output bytes to it.
- `ESC stdout ESC /` routes subsequent output back to standard output and thus
  to the debug terminal viewer when debug mode is active.
- `ESC stderr ESC /` routes subsequent output to standard error.

Paths in these frames are relative to the directory in which the emulator is
running unless they are absolute.

## Debug terminal output

Without `-d`, completed UART sends are written directly to standard output. With
`-d`, they are instead appended as raw bytes to
`/tmp/reti_emulator/terminal_output.bin`, preventing output from corrupting the
ncurses TUI. Capturing starts with the emulator and does not depend on the
viewer being open.

The third infobox page provides `(V)iew terminal`. Capital `V` opens the Python
GUI terminal with all output captured so far and follows new output while the
emulator runs. The viewer handles carriage return, newline, backspace, tab,
and printable ASCII output. Backspace removes the preceding displayed character.
It is available while stepping and after halt when `-K` keeps the TUI open.
