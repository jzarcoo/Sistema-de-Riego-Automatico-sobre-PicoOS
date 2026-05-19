#include "irrigation_manager.h"
#include "kernel_drivers.h"
#include "kernel_hw_config.h"
#include "scheduler.h"
#include "pico/multicore.h"

#define PUMP_TIMEOUT_TICKS 2500
#define PUMP_MIN_TICKS 300
#define SOIL_WET_THRESHOLD 2500

static int irrigation_requested = 0;
static int irrigation_initialized = 0;
static int pumping = 0;
static uint32_t pump_start_tick = 0;

static void irrigation_manager_init(void) {
    if (!irrigation_initialized) {
        k_gpio_init(IRRIGATION_PUMP_PIN, 1);
        k_gpio_set(IRRIGATION_PUMP_PIN, 0);   // LOW = bomba apagada
        k_gpio_pulldown(IRRIGATION_PUMP_PIN); // Pull-down: float = LOW = apagada
        irrigation_initialized = 1;
    }
}

void irrigation_manager_request_water(void) {
    irrigation_requested = 1;
}

void irrigation_manager_update(void) {
    if (!irrigation_initialized)
        irrigation_manager_init();

    if (irrigation_requested && !pumping) {
        k_gpio_set(IRRIGATION_PUMP_PIN, 1);  // HIGH = bomba ON
        pumping = 1;
        pump_start_tick = core_schedulers[1].kernel_ticks;
        irrigation_requested = 0;
    }

    if (pumping) {
        uint32_t elapsed = core_schedulers[1].kernel_ticks - pump_start_tick;

        if (elapsed < PUMP_MIN_TICKS) return;

        int humidity = kernel_read_soil_sensor();

        if (humidity < SOIL_WET_THRESHOLD || elapsed > PUMP_TIMEOUT_TICKS) {
            k_gpio_set(IRRIGATION_PUMP_PIN, 0);  // LOW = bomba OFF
            pumping = 0;
        }
    }
}
