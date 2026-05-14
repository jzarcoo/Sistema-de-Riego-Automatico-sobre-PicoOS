#include "scheduler.h"
#include <stdio.h>
#include "pico/stdlib.h"

tcb_t tasks[MAX_TASKS];
int current_task = -1;
volatile uint32_t kernel_ticks = 0;

/**
 * @brief Initialize the task stack so it looks like an interrupted context.
 * @param id Task index in the task table.
 */
void init_task_stack(int id) {
    uint32_t *stack_top = tasks[id].stack + STACK_SIZE;
    
    // Hardware stack frame (simulated)
    *(--stack_top) = 0x01000000; // xPSR (Thumb bit enabled)
    *(--stack_top) = (uint32_t)tasks[id].entry_point; // PC (Program Counter)
    *(--stack_top) = 0xFFFFFFFD; // LR (Return to Thread mode using PSP)
    *(--stack_top) = 0; // R12
    *(--stack_top) = 0; // R3
    *(--stack_top) = 0; // R2
    *(--stack_top) = 0; // R1
    *(--stack_top) = 0; // R0

    // Software stack frame (simulated, R4-R11)
    for (int i = 0; i < 8; i++) {
        *(--stack_top) = 0;
    }
    
    tasks[id].sp = stack_top;
    tasks[id].state = READY;
}

/**
 * @brief Register a task in the system without enqueuing it yet.
 * @param id Task index in the task table.
 * @param entry_point Task entry function.
 */
void task_create(int id, void (*entry_point)(void)) {
    if (id >= 0 && id < MAX_TASKS) {
        tasks[id].entry_point = entry_point;
        tasks[id].state = DORMANT;
        tasks[id].quantum = 10; 
        tasks[id].remaining_ticks = 10;
        tasks[id].heartbeat = 1;
        tasks[id].last_seen = kernel_ticks;
        // TODO: CHECK  
        init_task_stack(id);
    }
}

/**
 * @brief SysTick handler called automatically by the hardware.
 */
void isr_systick() {
    kernel_ticks++;

    // ==
    // watchdog
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == DORMANT)
            continue;
        if ((kernel_ticks - tasks[i].last_seen) > TASK_TIMEOUT_TICKS) {
            printf("[WATCHDOG] Tarea %d no responde, reiniciando...\n", i + 1);
            /*
             * Reinicio de tarea
             */
            task_create(i, tasks[i].entry_point);
            printf("[WATCHDOG] Tarea %d reiniciada.\n", i + 1);
        }
    }
    // ===

    if (current_task != -1) {
        if (--tasks[current_task].remaining_ticks > 0) {
            return; 
        }
        tasks[current_task].remaining_ticks = tasks[current_task].quantum;
    }

    // Trigger PendSV to perform the context switch
    // Write to the Interrupt Control and State Register (ICSR)
    *(volatile uint32_t *)0xE000ED04 = (1 << 28);
}

/**
 * @brief Handle the EXIT syscall by terminating the current task.
 */
void k_task_exit(void) {
    if (current_task != -1) {
        tasks[current_task].state = DORMANT;
        printf("Tarea %d terminada.\n", current_task + 1);
        // Force an immediate context switch
        *(volatile uint32_t *)0xE000ED04 = (1 << 28);
    }
}

/**
 * @brief Pick the next runnable task using Round Robin.
 * @param current_sp Stack pointer of the task being switched out.
 * @return Stack pointer of the selected task, or current_sp if none is ready.
 */
uint32_t schedule(uint32_t current_sp) {
    // Save the SP of the task that was just paused
    if (current_task != -1) {
        tasks[current_task].sp = (uint32_t *)current_sp;
        if (tasks[current_task].state == RUNNING) {
            tasks[current_task].state = READY;
        }
    }

    // Round Robin algorithm
    int next_task = current_task;
    int tries = 0;

    do {
        next_task = (next_task + 1) % MAX_TASKS;
        tries++;
        // If we find a ready task, stop searching
        if (tasks[next_task].state == READY) {
            break;
        }
    } while (tries < MAX_TASKS);

    // If a runnable task was found
    if (tasks[next_task].state == READY) {
        current_task = next_task;
        tasks[current_task].state = RUNNING;
        tasks[current_task].remaining_ticks = tasks[current_task].quantum;
        return (uint32_t)tasks[current_task].sp;
    }

    // If no task is ready, keep the current context
    return current_sp; 
}