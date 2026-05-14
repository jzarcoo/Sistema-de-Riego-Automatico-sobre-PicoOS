cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(ProyectoFinal C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(ProyectoFinal
    src/main.c
    
    # === ARCHIVOS DE ENSAMBLADOR ===
    src/arch/pendsv.s
    src/arch/svc_handler.s
    src/arch/syscalls.s
    
    # === ARCHIVOS DEL KERNEL ===
    src/kernel/filesystem.c
    src/kernel/kernel_drivers.c
    src/kernel/kernel_service.c
    src/kernel/log_memory.c
    src/kernel/logger.c
    src/kernel/message_queue.c
    src/kernel/scheduler.c
    src/kernel/semaphore.c
    src/kernel/watchdog_supervisor.c

    # === ARCHIVOS DE USUARIO ===
    src/user/user_app.c
    src/user/tasks/display_task.c
    src/user/tasks/irrigation_task.c
    src/user/tasks/logger_task.c
    src/user/tasks/sensor_task.c
    src/user/tasks/trigger_task.c
)

target_include_directories(ProyectoFinal PRIVATE include)

pico_enable_stdio_usb(ProyectoFinal 1)
pico_enable_stdio_uart(ProyectoFinal 1) # 

pico_add_extra_outputs(ProyectoFinal)

target_link_libraries(ProyectoFinal
    pico_stdlib #
    hardware_adc
    hardware_exception #
    hardware_flash # -
    hardware_uart
    hardware_watchdog
    hardware_sync # -
)