#include "../include/datastructures.h"
#include "../include/statemachine.h"
#include <stdint.h>

void heapify_up(uint8_t idx, uint8_t heap[], uint8_t prio_map[]) {
  while (idx > 0) {
    uint8_t parent = (idx - 1) / 2;
    if (prio_map[heap[idx]] > prio_map[heap[parent]]) {
      uint8_t temp = heap[idx];
      heap[idx] = heap[parent];
      heap[parent] = temp;
      idx = parent;
    } else {
      break;
    }
  }
}

void heapify_down(uint8_t idx, uint8_t heap[], uint8_t prio_map[]) {
  while (2 * idx + 1 < heap_size) {
    uint8_t left = 2 * idx + 1;
    uint8_t right = 2 * idx + 2;
    uint8_t largest = idx;

    if (left < heap_size &&
        prio_map[heap[left]] > prio_map[heap[largest]]) {
      largest = left;
    }
    if (right < heap_size &&
        prio_map[heap[right]] > prio_map[heap[largest]]) {
      largest = right;
    }
    if (largest != idx) {
      uint8_t temp = heap[idx];
      heap[idx] = heap[largest];
      heap[largest] = temp;
      idx = largest;
    } else {
      break;
    }
  }
}

uint8_t pop_highest_prio(uint8_t heap[], uint8_t prio_map[]) {
  uint8_t highest_priority_isr = heap[0];
  heap[0] = heap[--heap_size];
  heapify_down(0, heap, prio_map);
  return highest_priority_isr;
}
