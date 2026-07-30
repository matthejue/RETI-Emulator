# CPU Exceptions

The emulator implements three synchronous CPU exceptions:

- `1`: divide by zero
- `2`: stack overflow
- `3`: illegal instruction

All exceptions enter interrupt-vector slot `3`. This slot is fixed and is not
assigned or prioritized through the interrupt controller. Periphery register
`11` contains the current exception cause. It is read-only and is reset to `0`
when the emulator starts.

Periphery register `10` contains the inclusive stack/heap boundary for the
currently executing context. Writing `0` disables stack-overflow detection.
For every instruction that writes a lower value to `SP`, the emulator raises a
stack-overflow exception if the new value would be below the boundary. A value
equal to the boundary is valid because `SP` identifies the cell below the
lowest occupied stack cell.

The operating system must update register `10` before switching to a context
with a different heap boundary. This includes switching between a process and
the kernel. The kernel boundary should be the last cell occupied or reserved by
the kernel heap; a process boundary should be the last cell occupied or
reserved by that process's heap.

An exception is detected before the faulting instruction commits its register
or memory result. Exception entry then follows the existing interrupt
convention: it decrements `SP`, stores the faulting `PC - 1` at `SP + 1`, and
loads the handler address from vector slot `3`. Since `RTI` advances `PC`, an
exception handler that returns retries the faulting instruction. PicoOS is
expected to terminate the affected process instead; a stack overflow in kernel
context should cause a kernel panic.

If vector slot `3` is not present, the emulator reports the exception as
unhandled and stops.

In the debugger, focus the UART box and press `a` to cycle through its `UART`,
`Interrupts`, and `Exceptions` pages. The `Exceptions` page displays periphery
registers `10` and `11`.
