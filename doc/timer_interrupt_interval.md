Periphery memory currently ends at address `9`: `0..2` are UART registers,
`3..5` map timer/custom/UART interrupt devices to ISR slots, `6..8` store
their priorities, and `9` is `timer_interrupt_interval`. Value `0` disables
the timer interrupt; values `>0` enable it as the instruction interval, and
writes to address `9` reset `timer_cnt`. The TUI periphery box has the `UART`
and `Interrupts` pages, and the timer interrupt interval is shown on
`Interrupts`.
