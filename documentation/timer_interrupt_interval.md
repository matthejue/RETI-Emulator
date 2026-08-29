The always-present periphery memory ends at address `12`: `0..2` are UART registers,
`3..5` map timer/custom/UART interrupt devices to ISR slots, `6..8` store
their priorities, and `9` is `timer_interrupt_interval`. Value `0` disables
the timer interrupt; values `>0` enable it as the instruction interval, and
writes to address `9` reset `timer_cnt`. Registers `10` and `11` are CPU
exception registers described in `documentation/cpu_exceptions.md`. The TUI periphery
box has `UART`, `Interrupts`, `Exceptions`, and `DMA` pages. The timer register
is shown on `Interrupts`; the exception registers are shown on `Exceptions`.
Register `12` enables DMA. While it is `0`, it is the only register on the DMA
page. While it is `1`, registers `13..16` expose source, destination, word
count, and status/control.
