# Sistema de Riego Automatico sobre PicoOS

**Laboratorio de Sistemas Operativos (0713)**
**Plataforma:** Raspberry Pi Pico (RP2040 - Dual-core ARM Cortex-M0+ @ 125MHz)

---

## 1. Introduccion

Este proyecto implementa un sistema operativo minimalista (PicoOS) sobre la Raspberry Pi Pico para controlar un sistema de riego automatico. El sistema lee un sensor de humedad, activa una bomba cuando el suelo esta seco, permite riego manual por boton, muestra estado en un LCD, y registra eventos en Flash. Todo esto corriendo sobre un kernel con separacion de privilegios, proteccion de memoria por hardware, y planificacion preemptiva dual-core.

---

## 2. Objetivos

- Implementar un kernel con planificacion Round-Robin preemptiva sobre dos cores independientes.
- Aplicar proteccion de memoria via MPU para aislar tareas de usuario del hardware.
- Demostrar IPC por paso de mensajes (productor-consumidor) sin variables compartidas.
- Implementar page cache con reemplazo LRU y write-back diferido a Flash.
- Construir un sistema de archivos minimalista sobre Flash (PicoFS).
- Garantizar tolerancia a fallos: watchdog, HardFault handler, reinicio automatico de tareas.
- Integrar los conceptos de las 10 practicas del laboratorio en un sistema funcional.

---

## 3. Marco Teorico

### 3.1 Planificacion de Procesos

Un scheduler Round-Robin asigna un quantum fijo a cada tarea. Cuando el quantum expira, el scheduler preempta la tarea y selecciona la siguiente en estado READY. En sistemas embebidos con multiples cores, cada core puede tener su propio scheduler independiente (Silberschatz, cap. 5).

### 3.2 Proteccion de Memoria

La MPU (Memory Protection Unit) del ARM Cortex-M0+ permite definir regiones de memoria con permisos diferenciados entre modo privilegiado (kernel) y no privilegiado (tareas). A diferencia de una MMU, no hace traduccion de direcciones ni paginacion — solo control de acceso. Si una tarea viola los permisos, el hardware genera un HardFault (Silberschatz, cap. 9).

### 3.3 Comunicacion entre Procesos (IPC)

El modelo productor-consumidor usa colas de mensajes tipados. Las tareas productoras envian mensajes a una cola; las consumidoras los extraen. Este esquema desacopla las tareas y evita condiciones de carrera sin necesidad de memoria compartida explicita (Silberschatz, cap. 3).

### 3.4 Semaforos y Sincronizacion

Semaforos de conteo con cola de espera FIFO. Si el recurso no esta disponible, la tarea se bloquea y se encola. Al liberar el recurso, se despierta la primera tarea en espera. Se usan como mutex (inicializados a 1) para exclusion mutua en bomba, logger y display (Silberschatz, cap. 7).

### 3.5 Page Cache y Politicas de Escritura

- **Write-Through:** Escribe a cache y a disco simultaneamente. Seguro pero lento.
- **Write-Back:** Escribe solo a cache. El disco se actualiza despues (cuando la pagina es desalojada o hay flush). Rapido pero se pierden datos ante corte de energia.
- **LRU (Least Recently Used):** Cuando la cache esta llena, se desaloja la pagina que no se ha usado por mas tiempo (Silberschatz, cap. 10).

### 3.6 Sistemas de Archivos

Un filesystem minimalista sobre Flash requiere considerar que Flash solo puede borrarse por sectores completos y programarse por paginas. Las operaciones de escritura necesitan read-modify-write. En un sistema multicore, las operaciones Flash deben pausar el otro core (Silberschatz, cap. 14).

### 3.7 Tolerancia a Fallos

El watchdog detecta tareas que dejaron de reportar actividad. El HardFault handler atrapa violaciones de memoria y mata la tarea ofensora sin afectar al sistema. La combinacion permite que el kernel sobreviva a fallos de tareas individuales (Silberschatz, cap. 2).

### 3.8 Conceptos de SO Implementados

| Concepto | Implementacion | Archivo(s) |
|----------|---------------|------------|
| Planificacion de procesos | Round-Robin preemptivo por core | `scheduler.c`, `pendsv.s` |
| Cambio de contexto | PendSV con save/restore manual R4-R11 | `pendsv.s` |
| Llamadas al sistema | SVC trap + dispatcher | `syscalls.s`, `svc_handler.s`, `kernel_service.c` |
| Proteccion de memoria | MPU con regiones privilegiadas | `mpu.c` |
| Modo dual (kernel/user) | CONTROL.nPRIV + PSP/MSP | `pendsv.s`, `scheduler.c` |
| Multiprocesamiento | Dual-core asimetrico | `main.c:core1_entry()` |
| IPC por mensajes | Colas circulares thread-safe | `message_queue.c` |
| Sincronizacion | Semaforos de conteo con cola FIFO | `semaphore.c` |
| Interrupciones de HW | GPIO IRQ con debounce y dispatch | `kernel_events.c` |
| Entrada/Salida | Jerarquia: syscall -> manager -> driver -> HW | `kernel_service.c`, managers |
| I/O bufferizado | Write+flush, diff buffer, batch I2C | `display_manager.c` |
| Memoria virtual (sim.) | Page cache LRU + Write-Back diferido | `log_memory.c`, `flash_queue.c` |
| Sistema de archivos | Filesystem plano sobre Flash | `filesystem.c` |
| Tolerancia a fallos | Watchdog heartbeat + HardFault handler | `watchdog_supervisor.c`, `main.c` |
| Deteccion de deadlock | Timeout en tareas BLOCKED sin wake_tick | `scheduler.c:isr_systick()` |

---

## 4. Desarrollo

### 4.1 Arquitectura Dual-Core

| Core | Rol | Tareas |
|------|-----|--------|
| Core 0 | Gestion, UI, logs | `logger_task`, `display_task`, `mpu_test_task` |
| Core 1 | Control critico, bomba, sensor | `irrigation_task`, `sensor_task`, `irrigation_update_task` |

Cada core tiene su scheduler independiente con SysTick a 10ms. No comparten planificador.

**Comunicacion inter-core:** Exclusivamente por colas de mensajes con deshabilitacion de interrupciones para atomicidad.

```
[sensor_task] --MSG_SOIL_DRY/WET----> [irrigation_queue] --> [irrigation_task]
[sensor_task] --MSG_DISPLAY_TEXT----> [display_queue]    --> [display_task]
[boton IRQ]   --MSG_MANUAL_TRIGGER--> [irrigation_queue] --> [irrigation_task]
[irrigation_task] --MSG_LOG_TEXT-----> [log_queue]        --> [logger_task]
[irrigation_task] --MSG_DISPLAY_TEXT-> [display_queue]    --> [display_task]
```

### 4.2 Memory Protection Unit (MPU)

5 regiones configuradas:

| Region | Direccion | Tamano | Acceso | Protege |
|--------|-----------|--------|--------|---------|
| 0 | 0x00000000 | 4GB | Full | Background (ROM, RAM, SIO) |
| 1 | 0x40014000 | 16KB | Solo kernel | IO_BANK0: funcion de pin |
| 2 | 0x4001C000 | 4KB | Solo kernel | PADS_BANK0: config electrica |
| 3 | 0x40044000 | 4KB | Solo kernel | I2C0: bus LCD |
| 4 | 0x40034000 | 4KB | Solo kernel | UART0: serial output |

Si una tarea de usuario intenta acceder a estos perifericos directamente, la MPU genera HardFault. El handler mata la tarea y el sistema continua.

**Nota sobre SIO (0xD0000000):** No se protege porque contiene el registro CPUID que el Pico SDK consulta desde `get_core_num()` en muchas funciones internas. La MPU del M0+ no permite proteger parte de un bloque. La bomba se protege via IO_BANK0 — sin acceso a IO_BANK0, una tarea no puede reconfigurar el pin de la bomba.

**PRIVDEFENA=1:** Modo privilegiado tiene acceso total. Tareas acceden perifericos solo via syscalls.

### 4.3 Planificador

- Round-Robin preemptivo, quantum 10 ticks (100ms).
- Conmutacion de contexto via PendSV.
- Estados: DORMANT, READY, RUNNING, BLOCKED.
- Stack: 512 words (2KB) por tarea.
- El scheduler guarda R4-R11 manualmente (M0+ no tiene STMDB).

### 4.4 Syscalls

17 servicios via instruccion SVC:

```
Tarea (unprivileged, PSP)
    | SVC #0 (r7 = id, r0-r3 = args)
    v
wrapper_svc [ASM] -> extrae args del stack frame
    v
kernel_service() [C] -> despacha (GPIO, ADC, semaforos, sleep, print, etc.)
    v
Retorno a tarea (resultado en r0)
```

### 4.5 Subsistema de I/O

```
User Task --> syscall --> Device Manager --> Device Driver --> Hardware
```

| Periferico | Tecnica | Justificacion |
|-----------|---------|---------------|
| ADC (sensor) | Polling cada 5s | Conversion tarda ~2us, no justifica interrupt |
| GPIO boton | IRQ + triple sampling + debounce 500ms | Evento asincronico, filtra EMI del relay |
| GPIO bomba | Write directo (SIO) | Un ciclo de reloj |
| I2C LCD | Double buffer + diff + batch por fila | Solo envia filas modificadas |
| UART | sys_print (MPU protege UART0) | Fuerza uso de syscall |

**Display LCD 2004A:** Driver HD44780 via PCF8574T a 400kHz. Double buffer (front/back), diff por fila con memcmp, batch I2C. Tolerante a desconexion (timeout I2C, se desactiva sin afectar sistema).

### 4.6 Semaforos

3 mutex (inicializados a 1):
- `irrigation_pump_sem`: Exclusion mutua bomba.
- `logger_sem`: Exclusion mutua Flash.
- `display_sem`: Exclusion mutua LCD.

Cola de espera FIFO. Timeout por watchdog detecta posible deadlock.

### 4.7 Page Cache LRU + Write-Back Diferido

6 frames de 64 bytes en RAM. Politica Write-Back:

1. **Hit:** Escribe en frame libre.
2. **Miss (cache llena):** LRU selecciona victima. Si dirty, se encola en `flash_work_queue`.
3. **Flush periodico (50 msgs):** Encola todas las paginas dirty.
4. **Write-Back:** El idle loop del kernel drena la cola y escribe a Flash en modo privilegiado (thread mode).

El flush diferido evita deadlock inter-core: las flash ops necesitan `multicore_lockout` que requiere que Core 1 responda. Si Core 1 esta en SVC handler, no puede responder. El idle loop corre en thread mode donde Core 1 siempre puede responder al lockout.

### 4.8 Sistema de Archivos (PicoFS)

Filesystem plano sobre Flash:
- Tabla de metadatos (1 sector) + datos contiguos.
- Operaciones: create, read, write, append, delete, compact, format.
- Seguridad multicore: `multicore_lockout` + disable interrupts para cada operacion Flash.
- Lectura via XIP (Execute-In-Place, lectura directa del mapa de memoria).

### 4.9 Tolerancia a Fallos

**Watchdog:** Cada tarea reporta `sys_heartbeat()`. Si no reporta en 500 ticks (5s), se reinicia con stack fresco.

**HardFault Handler:** Violacion MPU → identifica tarea y PC → marca DORMANT → PendSV → sistema continua.

**Proteccion bomba:** Timeout maximo 25s, tiempo minimo 3s, pull-down (pin flotante = bomba apagada).

### 4.10 Prioridades de Excepciones (Core 1)

| Excepcion | Prioridad | Funcion |
|-----------|-----------|---------|
| SIO FIFO IRQ | 0 (alta) | Responde a multicore_lockout |
| SysTick | 0 | Timekeeping del scheduler |
| IO_IRQ_BANK0 | 0 | Deteccion boton |
| SVCall | 3 (baja) | Puede ser interrumpido por FIFO IRQ |
| PendSV | 3 (baja) | Context switch |

SVCall con prioridad baja permite que la FIFO IRQ lo interrumpa para responder al lockout de Flash, eliminando el deadlock.

### 4.11 Uso del SDK

| Biblioteca | Uso |
|-----------|-----|
| `pico_stdlib` | Boot, USB, clocks |
| `pico_multicore` | Core 1 launch, lockout |
| `hardware_adc` | Sensor humedad |
| `hardware_i2c` | LCD |
| `hardware_flash` | Erase/program (relocado a RAM) |
| `hardware_exception` | Registro de handlers |
| `hardware_sync` | Atomicidad (PRIMASK) |
| `hardware_irq` | NVIC GPIO |

Codigo nativo (sin SDK): GPIO driver, scheduler, PendSV/SVC handlers, MPU, syscalls.

---

## 5. Resultados

### 5.1 Funcionamiento del Sistema

El sistema opera correctamente:
- El sensor lee humedad cada 5s y envia MSG_SOIL_DRY/WET a la cola.
- La bomba se activa cuando el suelo esta seco (ADC > 2500) y se apaga al detectar humedad.
- El boton manual dispara un ciclo de riego independiente del sensor.
- El display LCD muestra humedad, estado de bomba y ultimo evento.
- Los logs se persisten a Flash via page cache LRU.
- La demo MPU genera HardFault controlado y el sistema sobrevive.
- El watchdog detecta y reinicia tareas colgadas.

### 5.2 Mapeo de Practicas

| Practica | Concepto | Implementacion |
|----------|----------|----------------|
| P2 | Syscalls | 17 servicios SVC activos |
| P3 | Context Switch + IRQ | PendSV (R4-R11) + GPIO IRQ boton |
| P4 | Scheduler | Round-Robin dual-core, quantum 100ms |
| P5 | Semaforos | 3 mutex con cola FIFO via syscall |
| P6 | Memoria Virtual | Page cache 6 frames, LRU, write-back diferido |
| P7 | MPU + Recovery | 5 regiones HW + reinicio automatico |
| P8 | Filesystem | PicoFS: create/write/read/delete/compact |
| P9 | I/O | Polling, IRQ, batch I2C, UART protegida |
| P10 | Dual-Core | Core 0 (UI) + Core 1 (control), colas inter-core |

### 5.3 Problemas Encontrados y Soluciones

**1. EMI del relay en GPIO del boton:**
El relay genera ruido electromagnetico al conmutar que activa GPIO 14 falsamente. Solucion: triple sampling en ISR (3 lecturas con ~1ms entre cada una) + debounce 500ms + drain de mensajes post-riego.

**2. Deadlock inter-core por Flash lockout:**
`multicore_lockout_start_blocking()` espera que Core 1 responda via FIFO IRQ. Si Core 1 esta en SVC handler, la IRQ no puede interrumpirlo (misma prioridad en M0+). Solucion: configurar SVCall a prioridad 3 para que FIFO IRQ (prioridad 0) pueda preemptarlo, + write-back diferido en idle loop.

**3. SIO no protegible por MPU:**
El bloque SIO (0xD0000000) contiene CPUID que el SDK usa desde `get_core_num()`. Proteger SIO completo causa HardFault en funciones basicas. La MPU M0+ no permite proteger sub-bloques. Solucion: proteger IO_BANK0 (que controla la funcion del pin) en vez de SIO.

**4. printf en ISR/SVC handler:**
`printf` usa UART que es bloqueante. Dentro de un ISR o SVC handler, bloquea el sistema entero. Solucion: las tareas usan `sys_print` (syscall), el ISR no imprime nada.

---

## 6. Conclusiones

El proyecto demuestra que los conceptos teoricos de sistemas operativos (Silberschatz) se aplican directamente sobre hardware real de bajo costo. La planificacion Round-Robin, la proteccion de memoria por MPU, la comunicacion por colas de mensajes, y la persistencia por filesystem con page cache LRU operan sobre un microcontrolador de $4 USD cumpliendo requisitos de tiempo real.

Las restricciones del hardware (M0+ sin MMU, prioridades fijas de excepciones, Flash compartida entre cores) obligan a decisiones de diseno que no aparecen en la teoria pero son fundamentales en la practica: el write-back diferido para evitar deadlock, la imposibilidad de proteger SIO, y el filtrado de EMI por software son ejemplos de problemas que solo se descubren al implementar sobre hardware fisico.

---

## 7. Conexionado Fisico

| Componente | GPIO | Pin Pico | Notas |
|-----------|------|----------|-------|
| Bomba (rele) | 6 | Pin 9 | Activa en LOW, pull-down |
| Sensor humedad | 26 (ADC0) | Pin 31 | Analogico |
| Boton manual | 14 | Pin 19 | Pull-down, rising edge |
| LCD SDA | 8 | Pin 11 | I2C0, PCF8574 |
| LCD SCL | 9 | Pin 12 | I2C0, 400kHz |

---

## 8. Estructura del Proyecto

```
├── include/
│   ├── kernel/          # scheduler, mpu, syscalls, drivers, events, queues
│   ├── filesystem.h     # PicoFS API
│   ├── flash_queue.h    # Cola write-back diferido
│   ├── log_memory.h     # Page cache LRU
│   └── logger.h         # Logger persistente
├── src/
│   ├── main.c           # Boot dual-core, idle loop (flash worker)
│   ├── arch/            # pendsv.s, svc_handler.s, syscalls.s
│   ├── kernel/          # scheduler, mpu, drivers, filesystem, flash_queue
│   └── user/tasks/      # irrigation, sensor, logger, display, mpu_test
├── diagrams.py          # Generador de diagramas
└── CMakeLists.txt
```

---

## 9. Referencias

- Silberschatz, A., Galvin, P., Gagne, G. *Operating System Concepts* (10th Edition). Wiley, 2018.
- Raspberry Pi Ltd. *RP2040 Datasheet*. 2021.
- Raspberry Pi Ltd. *Raspberry Pi Pico C/C++ SDK Documentation*. 2021.
- ARM Ltd. *ARMv6-M Architecture Reference Manual*. 2017.
