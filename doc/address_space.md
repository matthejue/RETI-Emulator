# Address Space Cheat Sheet

The top two address bits select the memory-mapped section. The left bit is bit
32, the right bit is bit 31; all remaining bits are the address inside that
section.

- `00`: EPROM
- `01`: periphery:
  `0 uart_send`, `1 uart_receive`, `2 uart_status`,
  `3 timer_device_to_isr`, `4 custom_device_to_isr`,
  `5 timer_device_to_prio`, `6 custom_device_to_prio`,
  `7 timer_interrupt_interval` (`0 = timer interrupt disabled`)
  Interrupt-controller meaning: `doc/interrupt_controller_control.md`.
- `10` + `11`: SRAM
