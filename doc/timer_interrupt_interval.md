Periphery memory currently ends at address `11`: `0..2` are UART registers,
`3..5` map timer/custom/UART interrupt devices to ISR slots, `6..8` store
their priorities, and `9` is `timer_interrupt_interval`. Value `0` disables
the timer interrupt; values `>0` enable it as the instruction interval, and
writes to address `9` reset `timer_cnt`. Registers `10` and `11` are CPU
exception registers described in `doc/cpu_exceptions.md`. The TUI periphery
box has `UART`, `Interrupts`, and `Exceptions` pages. The timer register is
shown on `Interrupts`; the exception registers are shown on `Exceptions`.
