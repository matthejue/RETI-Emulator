# Interrupts

- Concrete addresses: `/home/areo/Documents/Studium/RETI-Emulator/doc/address_space.md`.
- `3`/`4`/`5`: assign ISR slot for timer/custom/UART interrupt; `255` disables
  that interrupt.
- `6`/`7`/`8`: set timer/custom/UART priority; higher number wins and can
  preempt lower priority.
- `9`: timer interrupt interval; `0` disables the timer interrupt, any value
  `>0` enables it and sets the instruction interval.
