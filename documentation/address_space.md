# Address Space Cheat Sheet

The top two address bits select the memory-mapped section. The left bit is bit
32, the right bit is bit 31; all remaining bits are the address inside that
section.

- `00`: EPROM
- `01`: periphery:
  `0 uart_send`, `1 uart_receive`, `2 uart_status`,
  `3 timer_device_to_isr`, `4 custom_device_to_isr`,
  `5 uart_device_to_isr`,
  `6 timer_device_to_prio`, `7 custom_device_to_prio`,
  `8 uart_device_to_prio`,
  `9 timer_interrupt_interval` (`0 = timer interrupt disabled`),
  `10 stack_heap_boundary` (`0 = stack-overflow detection disabled`),
  `11 cpu_exception_cause` (read-only)
  Interrupt-controller meaning: `documentation/interrupt_controller_control.md`.
  CPU-exception meaning:
  `../../RETI-Emulator/documentation/cpu_exceptions.md`.
- `10` + `11`: SRAM
