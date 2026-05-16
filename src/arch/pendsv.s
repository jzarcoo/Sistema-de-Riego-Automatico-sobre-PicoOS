.syntax unified
.cpu cortex-m0plus
.thumb

.global isr_pendsv
.type isr_pendsv, %function

isr_pendsv:
    @ --- 1. LEER EL ESTADO ACTUAL ---
    mrs r0, psp         @ Cargar el Process Stack Pointer actual en r0
    
    @ Si el PSP es 0, venimos del main (idle loop) usando MSP.
    @ No hay registros de usuario que guardar, saltamos a pedir una tarea nueva.
    cmp r0, #0
    beq call_scheduler

    @ --- 2. GUARDAR CONTEXTO POR SOFTWARE (r4 - r11) ---
    @ En Cortex-M0+ no existe "stmdb", hay que manipular la pila y registros altos manualmente.
    subs r0, #32        @ Reservar 32 bytes (8 registros * 4 bytes)
    mov r2, r0          @ Usar r2 como copia segura del SP para guardar
    stmia r2!, {r4-r7}  @ Guardar la mitad inferior (r4-r7)
    
    mov r4, r8          @ Mover registros altos a bajos para poder guardarlos
    mov r5, r9
    mov r6, r10
    mov r7, r11
    stmia r2!, {r4-r7}  @ Guardar la mitad superior (originalmente r8-r11)

call_scheduler:
    @ --- 3. INVOCAR AL PLANIFICADOR EN C ---
    push {lr}           @ Guardar el código EXC_RETURN (fundamental para no generar Hard Fault)
    bl schedule         @ Llama a uint32_t schedule(uint32_t current_sp);
    pop {r3}            @ Recuperar el EXC_RETURN temporalmente en r3
    
    @ El planificador nos devuelve el SP de la tarea entrante en r0.
    @ Si r0 sigue siendo 0, significa que la cola de procesos esta vacia.
    cmp r0, #0
    beq return_idle

    @ --- 4. PREPARAR EL RETORNO AL USUARIO ---
    @ Forzamos el EXC_RETURN a 0xFFFFFFFD para asegurarnos de que al salir
    @ de esta excepcion, el CPU utilice el PSP (modo Unprivileged/User).
    ldr r3, =0xFFFFFFFD

    @ --- 5. RESTAURAR CONTEXTO POR SOFTWARE DE LA NUEVA TAREA (r4 - r11) ---
    adds r0, #16        @ Avanzar a la segunda mitad del bloque
    ldmia r0!, {r4-r7}  @ Extraer los valores que iran a r8-r11
    mov r8, r4
    mov r9, r5
    mov r10, r6
    mov r11, r7
    
    subs r0, #32        @ Retroceder al inicio del bloque
    ldmia r0!, {r4-r7}  @ Extraer los valores para r4-r7
    adds r0, #16        @ Ajustar el puntero para saltar la seccion que ya leimos

    @ --- 6. ACTUALIZAR HARDWARE Y SALIR ---
    msr psp, r0         @ Cargar el nuevo Process Stack Pointer al hardware

    @ Forzar modo unprivileged para tareas de usuario
    movs r0, #3         @ nPRIV=1, SPSEL=1 (unprivileged, use PSP)
    msr control, r0
    isb                 @ Ensure effect before exception return

    bx r3               @ ¡Salto de fe! Volver a tarea con r3=0xFFFFFFFD (thread unprivileged)

return_idle:
    @ Si veniamos de main y seguimos sin tareas, volvemos a MSP (idle/kernel)
    @ Forzar modo privilegiado para idle/kernel
    movs r0, #0
    msr control, r0     @ Modo privilegiado, MSP
    isb

    @ Cargar explícitamente el EXC_RETURN para volver a handler mode con MSP
    ldr r3, =0xFFFFFFF9 @ Return to handler, MSP, privileged

    bx r3               @ Exception return: restaurar registros y pasar a idle
