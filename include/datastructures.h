#include <stdint.h>
#include <stdbool.h>

#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H
#define HEAP_SIZE UINT8_MAX

extern uint8_t heap[];
extern uint8_t heap_size;

void heapify_up(uint8_t idx, uint8_t heap[], uint8_t prio_map[]);

void heapify_down(uint8_t idx, uint8_t heap[], uint8_t prio_map[]);

uint8_t pop_highest_prio(uint8_t heap[], uint8_t prio_map[]);

#endif // DATASTRUCTURES_H
