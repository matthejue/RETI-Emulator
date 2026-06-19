# Interrupt Controller Control

- Concrete addresses: `/home/areo/Documents/Studium/RETI-Emulator/doc/address_space.md`.
- `3`/`4`: assign ISR slot for timer/custom interrupt; `255` disables that interrupt.
- `5`/`6`: set timer/custom priority; higher number wins and can preempt lower priority.
