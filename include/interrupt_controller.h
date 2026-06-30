#include <stdbool.h>
#include <stdint.h>

#ifndef INTERRRUPT_CONTROLLER_H
#define INTERRRUPT_CONTROLLER_H

#define INVALID_ISR_NUM UINT8_MAX

typedef enum {
  INTERRUPT_TIMER = 0,
  CUSTOM
} Hardware_Interrupt_Signal_Line;

#define NUM_UART_REGISTERS 3
#define NUM_HARDWARE_INTERRUPT_SIGNAL_LINES 2
#define INTERRUPT_CONTROLLER_ISR_BASE NUM_UART_REGISTERS
#define INTERRUPT_CONTROLLER_PRIO_BASE                                      \
  (INTERRUPT_CONTROLLER_ISR_BASE + NUM_HARDWARE_INTERRUPT_SIGNAL_LINES)
#define NUM_SYSTEM_INFO_CELLS 6
#define SYSTEM_INFO_BASE                                                     \
  (INTERRUPT_CONTROLLER_PRIO_BASE + NUM_HARDWARE_INTERRUPT_SIGNAL_LINES)
#define SYSTEM_INFO_SRAM_MAX_ADDRESS SYSTEM_INFO_BASE
#define SYSTEM_INFO_TIMER_INTERRUPT_INTERVAL (SYSTEM_INFO_BASE + 1)
#define SYSTEM_INFO_OS_CS (SYSTEM_INFO_BASE + 2)
#define SYSTEM_INFO_OS_DS (SYSTEM_INFO_BASE + 3)
#define SYSTEM_INFO_KERNEL_HEAP_START (SYSTEM_INFO_BASE + 4)
#define SYSTEM_INFO_KERNEL_STACK_START (SYSTEM_INFO_BASE + 5)
#define NUM_PERIPHERY_ADDRESSES                                              \
  (NUM_UART_REGISTERS + 2 * NUM_HARDWARE_INTERRUPT_SIGNAL_LINES +             \
   NUM_SYSTEM_INFO_CELLS)
#define NUM_ISR_SLOTS INVALID_ISR_NUM

extern uint8_t device_to_isr[NUM_HARDWARE_INTERRUPT_SIGNAL_LINES];
extern uint8_t device_to_prio[NUM_HARDWARE_INTERRUPT_SIGNAL_LINES];
extern uint8_t isr_to_prio[NUM_ISR_SLOTS];

void init_interrupt_controller(void);
void sync_interrupt_controller_from_memory(void);
void set_interrupt_device_isr(Hardware_Interrupt_Signal_Line device, uint8_t isr);
void load_interrupt_controller_config(const char *path);

#endif // INTERRRUPT_CONTROLLER_H
