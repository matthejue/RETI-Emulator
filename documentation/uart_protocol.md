# UART Protocol Cheat Sheet

UART memory cells: `R0` send, `R1` receive, `R2` status. In `R2`,
`b0 = send ready` and `b1 = receive ready`.

UART transfers are raw 8-bit bytes only. There is no datatype byte and no
integer/string framing. ASCII characters are transferred as their byte values.

Send one byte: write byte to `R0`, clear `b0`; emulator later sets `b0` and
emits that byte to standard output or the debug terminal capture.

Receive one byte: clear `b1`; emulator later writes the next buffered ASCII byte
to `R1` and sets `b1`.

Input is buffered. Interactive input can contain multiple characters; they are
consumed one byte at a time. With `-m`, one separator space/tab after `# input:`
is skipped and the rest of the line is used as the initial byte buffer,
including further spaces.

## Interactive UART terminal

Without `-d`, the invoking terminal always supplies UART character input and
receives UART output. No additional command-line option is required.

Each input byte is written to `R1`, sets the receive-ready bit in `R2`, and
triggers the `UART` hardware interrupt. Further input is buffered while an
earlier UART interrupt is pending. Without `-d`, `Escape` is delivered as an
ordinary UART input byte.

With `-d`, lowercase `v` and uppercase `V` suspend ncurses and switch the
invoking terminal to a UART terminal view. The normal `v` view keeps host
terminal signal handling enabled and uses `Escape` to restore the debug TUI.
The raw `V` view disables terminal handling of control keys such as `Ctrl+C`,
`Ctrl+\`, and `Ctrl+Z`; those bytes and `Escape` are delivered as UART input,
while `Ctrl+]` restores the TUI. This lets arrow-key escape sequences and
PicoOS control-key shortcuts reach the guest. The MSYS2 Windows build uses the
same byte and terminal-mode behavior as the Linux and macOS builds

Opening either view from a paused debugger keeps execution paused and only
displays output captured so far. Both keys are also polled while a `c` continue
run is active; execution and live UART input continue in the terminal view.
Closing the view redraws the debug TUI and returns to the still-running
debugger. Capital `E` stops that run at its current program address.

## UART control frames

`<esc>` below is the ASCII escape byte `27`. A control command has the byte form
`<esc>command<esc>/`. Every byte in the command, including both escape bytes and
the final slash, is consumed by the emulator and is not written to the terminal
or the currently selected output file.

- `<esc>load <path><esc>/` appends a file to the UART input buffer. The emulator
  first appends the file's 32-bit big-endian word count and then the file bytes.
  A missing or unreadable path, a non-regular file, or an unrepresentable word
  count returns `UINT32_MAX`. An existing empty regular file returns a zero word
  count, so clients can distinguish it from failure.
- `<esc>read-range <offset> <count> <path><esc>/` appends the returned slice's
  32-bit big-endian byte count and at most `count` bytes beginning at `offset`.
  A range reaching past the end of the file returns fewer bytes. A missing or
  unreadable file returns `UINT32_MAX` instead of file data.
- `<esc>file-size <path><esc>/` appends the regular file's 32-bit big-endian
  byte size. A missing or unreadable file returns `UINT32_MAX`.
  Plain, unframed `load`, `read-range`, or `file-size` text has no
  special meaning and is printed normally.
- `<esc>pwd<esc>/` calls `getcwd()` and returns a 32-bit byte count followed by
  the absolute path bytes.
- `<esc>is-directory <path><esc>/` uses `stat()` to check whether the absolute
  path names a directory and returns `0`, or `UINT32_MAX` if it does not. It
  does not change the emulator's working directory.
- `<esc>mkdir <path><esc>/` calls `mkdir()` and returns `0`, or `UINT32_MAX`.
- `<esc>ls<esc>/` lists the current directory. PicoOS normally sends
  `<esc>ls <path><esc>/`. The response is a byte count followed by one
  `d name` or `- name` line per entry in `readdir()` order. Hidden entries are
  always included; sizes and other metadata are not returned.
- `<esc>unlink <path><esc>/` calls `unlink()` and returns `0`, or `UINT32_MAX`.
- `<esc>rmdir <path><esc>/` calls `rmdir()` and returns `0`, or `UINT32_MAX`.

PicoOS's `file_exists()` and `SEEK_END` use `file-size`. Regular file reads and
bounded text configuration reads use `read-range`, whose response does not
repeat the complete file size.

The exact byte count is required because configuration files are text and may
not have a size divisible by four. The explicit failure value is required
because a missing file must not leave the OS waiting indefinitely for a length.

- `<esc>write <path><esc>/` creates or truncates `<path>` and routes subsequent
  UART output bytes to it.
- `<esc>append <path><esc>/` creates `<path>` if needed and routes subsequent
  UART output bytes to its end without truncating existing data.
- `<esc>write stdout<esc>/` routes subsequent output back to standard output and
  thus to the terminal view when debug mode is active.
- `<esc>write stderr<esc>/` routes subsequent output to standard error.

PicoOS normally resolves relative operands from the calling process's PCB and
sends absolute paths. The initial `pwd` response lets PID 1 discover the
emulator startup directory. A later PicoOS `chdir()` updates only the calling
process's PCB after `is-directory` accepts the path. There is no generic
host-command frame.

## Debug terminal output

Without `-d`, completed UART sends are written directly to standard output. With
`-d`, they are instead appended as raw bytes to
`.reti_emulaor/terminal_output.bin`, preventing output from corrupting the
ncurses TUI. Capturing starts with the emulator and does not depend on the
viewer being open.

The third infobox page provides `(v)iew terminal` and `(V)iew raw terminal`.
Both clear the invoking terminal, replay all raw output captured since emulator
startup, and then display new output directly. Capturing continues while the
debug TUI is visible, so entering either view never starts in the middle of an
otherwise missing message. Terminal handling of carriage return, newline,
backspace, tab, and printable output is the same as during a non-debug run.
Both views are available without advancing the program while stepping, during
a `c` continue run, and after halt when `-K` keeps the TUI open.
