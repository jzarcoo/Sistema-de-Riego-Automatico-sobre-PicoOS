#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

/**
 * @brief Busy-wait for an exact number of core cycles.
 * @param cycles Number of cycles to delay.
 */
static inline void delay_cycles_exact(uint32_t cycles)
{
    __asm volatile(
        "1: \n"
        "sub %[c], %[c], #1 \n"
        "bne 1b \n"
        : [c] "+r"(cycles)
        :
        : "cc");
}

#endif