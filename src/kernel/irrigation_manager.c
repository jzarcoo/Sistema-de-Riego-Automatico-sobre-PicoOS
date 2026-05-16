#include "irrigation_manager.h"
#include "kernel_drivers.h"
#include "kernel_hw_config.h"
#include "scheduler.h"

#define PUMP_TIMEOUT_TICKS 3000
#define SOIL_WET_THRESHOLD 2500

static int irrigation_requested = 0;
static int irrigation_initialized = 0;
static int pumping = 0;
static uint32_t pump_start_tick = 0;

static void irrigation_manager_init(void) {
    if (!irrigation_initialized) {
        k_gpio_init(IRRIGATION_PUMP_PIN, 1);
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
        k_gpio_set(IRRIGATION_PUMP_PIN, 1);
        pumping = 1;
        pump_start_tick = kernel_ticks;
        irrigation_requested = 0;
    }

    if (pumping) {
        int humidity = kernel_read_soil_sensor();
        uint32_t elapsed = kernel_ticks - pump_start_tick;

        if (humidity < SOIL_WET_THRESHOLD || elapsed > PUMP_TIMEOUT_TICKS) {
            k_gpio_set(IRRIGATION_PUMP_PIN, 0);
            pumping = 0;
        }
    }
}
