.syntax unified
.cpu cortex-m0plus
.thumb

@ IDs definitions as seen in kernel_driver.c
.equ SYS_GPIO_SET, 1
.equ SYS_GPIO_GET, 2
.equ SYS_GPIO_DIR, 3
.equ SYS_EXIT,     4
.equ SYS_SEM_WAIT, 5
.equ SYS_SEM_POST, 6
.equ SYS_PUMP_ON,      7
.equ SYS_PUMP_OFF,     8
.equ SYS_READ_SOIL_SENSOR,  9
.equ SYS_LOG_EVENT,    10
.equ SYS_HEARTBEAT,    11
.equ SYS_SEM_INIT,     12
.equ SYS_ADC_INIT,     13
.equ SYS_PULLUP,       14
.equ SYS_GPIO_IRQ_REGISTER, 15
.equ SYS_REQUEST_IRRIGATION, 17
.equ SYS_SLEEP, 18

@ --- Function: void sys_gpio_dir(int pin, int out) ---
.global sys_gpio_dir
.type sys_gpio_dir, %function
sys_gpio_dir:
    @ r0: pin
    @ r1: out (1 = output, 0 = input)
    @ Save r7 in r12 (caller-saved) to avoid touching the user stack
    mov r12, r7
    movs r7, #SYS_GPIO_DIR
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_gpio_set(int pin, int value) ---
.global sys_gpio_set
.type sys_gpio_set, %function
sys_gpio_set:
    @ r0: pin (Argument 1) - Already in place
    @ r1: value (Argument 2) - Already in place
    @ Save r7 in r12 to avoid modifying user stack
    mov r12, r7
    movs r7, #SYS_GPIO_SET @ Load syscall ID (1)
    svc #0              @ System Call! (Jump to Kernel)
    mov r7, r12         @ Restore r7
    bx lr               @ Return

@ --- Function: int sys_gpio_get(int pin) ---
.global sys_gpio_get
.type sys_gpio_get, %function
sys_gpio_get:
    @ r0: pin (Argument 1)
    @ Save r7 in r12 to avoid modifying user stack
    mov r12, r7
    movs r7, #SYS_GPIO_GET @ Load syscall ID (2)
    svc #0              @ Kernel will place the result in r0
    mov r7, r12
    bx lr

@ --- Function: void sys_exit(void) ---
.global sys_exit
.type sys_exit, %function
sys_exit:
    @ Save r7 in r12 to avoid modifying user stack
    mov r12, r7
    movs r7, #SYS_EXIT  @ Load syscall ID (4)
    svc #0              @ Jump to Kernel
    mov r7, r12         @ Restore r7
    bx lr               @ Return (though we wont actually return)

@ --- Function: void sys_sem_wait(kernel_semaphore_t *sem) ---
.global sys_sem_wait
.type sys_sem_wait, %function
sys_sem_wait:
    @ r0: pointer to semaphore 
    mov r12, r7
    movs r7, #SYS_SEM_WAIT  @ Load syscall ID (5)
    svc #0                  @ Jump to Kernel
    mov r7, r12             @ Restore r7
    bx lr                   @ Return

@ --- Function: void sys_sem_post(kernel_semaphore_t *sem) ---
.global sys_sem_post
.type sys_sem_post, %function
sys_sem_post:
    @ r0: pointer to semaphore 
    mov r12, r7
    movs r7, #SYS_SEM_POST  @ Load syscall ID (6)
    svc #0                  @ Jump to Kernel
    mov r7, r12             @ Restore r7
    bx lr                   @ Return

@ --- Function: void sys_pump_on(void) ---
.global sys_pump_on
.type sys_pump_on, %function
sys_pump_on:
    mov r12, r7
    movs r7, #SYS_PUMP_ON
    svc #0
    mov r7, r12
    bx lr


@ --- Function: void sys_pump_off(void) ---
.global sys_pump_off
.type sys_pump_off, %function
sys_pump_off:
    mov r12, r7
    movs r7, #SYS_PUMP_OFF
    svc #0
    mov r7, r12
    bx lr


@ --- Function: int sys_read_soil_sensor(void) ---
.global sys_read_soil_sensor
.type sys_read_soil_sensor, %function
sys_read_soil_sensor:
    mov r12, r7
    movs r7, #SYS_READ_SOIL_SENSOR
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_gpio_irq_register(int pin, message_queue_t *queue, int msg_type) ---
.global sys_gpio_irq_register
.type sys_gpio_irq_register, %function
sys_gpio_irq_register:
    @ r0: pin, r1: queue pointer, r2: message type
    mov r12, r7
    movs r7, #SYS_GPIO_IRQ_REGISTER
    svc #0
    mov r7, r12
    bx lr


@ --- Function: void sys_log_event(int code) ---
.global sys_log_event
.type sys_log_event, %function
sys_log_event:
    @ r0 = event code
    mov r12, r7
    movs r7, #SYS_LOG_EVENT
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_heartbeat(void) ---
.global sys_heartbeat
.type sys_heartbeat, %function
sys_heartbeat:
    mov r12, r7
    movs r7, #SYS_HEARTBEAT
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_sem_init(kernel_semaphore_t *sem, int value) ---
.global sys_sem_init
.type sys_sem_init, %function
sys_sem_init:
    @ r0: pointer to semaphore
    @ r1: initial value
    mov r12, r7
    movs r7, #SYS_SEM_INIT
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_adc_init(void) ---
.global sys_adc_init
.type sys_adc_init, %function
# sys_adc_init:
#     mov r3, r7
#     movs r7, #SYS_ADC_INIT
#     svc #0
#     mov r7, r3
#     bx lr
sys_adc_init:
    mov r12, r7
    movs r7, #SYS_ADC_INIT
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_gpio_pullup(int pin) ---
.global sys_gpio_pullup
.type sys_gpio_pullup, %function
sys_gpio_pullup:
    @ r0: pin number
    mov r12, r7
    movs r7, #SYS_PULLUP
    svc #0
    mov r7, r12
    bx lr


@ --- Function: void sys_request_irrigation(void) ---
.global sys_request_irrigation
.type sys_request_irrigation, %function
sys_request_irrigation:
    mov r12, r7
    movs r7, #SYS_REQUEST_IRRIGATION
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_sleep(uint32_t ms) ---
.global sys_sleep
.type sys_sleep, %function
sys_sleep:
    @ r0: milliseconds to sleep
    mov r12, r7
    movs r7, #SYS_SLEEP
    svc #0
    mov r7, r12
    bx lr
