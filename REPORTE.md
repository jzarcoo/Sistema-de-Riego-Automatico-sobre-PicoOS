# Reporte: Sistema de Riego Automatico sobre PicoOS

## Laboratorio de Sistemas Operativos (0713)
**Plataforma:** Raspberry Pi Pico (RP2040 - Dual-core ARM Cortex-M0+ @ 125MHz)

---

## 1. Descripcion General

Este proyecto implementa un sistema operativo minimalista (PicoOS) que gestiona un sistema de riego automatico. El diseno separa estrictamente el **plano de control critico** (tiempo real duro, Core 1) del **plano de gestion/usuario** (tiempo real blando, Core 0), aplicando conceptos avanzados de sistemas operativos sobre hardware bare-metal.

### Objetivos del Sistema
- Monitorear humedad del suelo mediante sensor capacitivo (ADC).
- Activar/desactivar bomba de riego automaticamente segun umbrales.
- Permitir riego manual mediante boton fisico con debounce por hardware.
- Registrar eventos en un sistema de archivos persistente sobre Flash.
- Garantizar aislamiento de memoria y tolerancia a fallos.

---

## 2. Arquitectura del Sistema

### 2.1 Multiprocesamiento Asimetrico (Dual-Core)

El RP2040 posee dos cores Cortex-M0+ independientes. Se asignan asimetricamente:

| Core | Rol | Tareas |
|------|-----|--------|
| Core 0 | Planificador general, UI, logs | `logger_task`, `display_task`, `mpu_test_task` |
| Core 1 | Monitoreo critico, control de bomba | `irrigation_task`, `sensor_task`, `irrigation_update_task` |

Cada core ejecuta su propio scheduler Round-Robin independiente con SysTick a 10ms. No comparten planificador, lo que elimina la necesidad de locks complejos para el scheduling.

**Comunicacion entre cores:** Se realiza exclusivamente mediante colas de mensajes (`message_queue_t`) que usan deshabilitacion de interrupciones para atomicidad. No se usan spinlocks ni mutexes inter-core.

### 2.2 Modelo de Comunicacion: Productor-Consumidor

```
[sensor_task] --MSG_SOIL_DRY/WET----> [irrigation_queue] --> [irrigation_task]
[sensor_task] --MSG_DISPLAY_TEXT----> [display_queue]    --> [display_task]
[boton IRQ]   --MSG_MANUAL_TRIGGER--> [irrigation_queue] --> [irrigation_task]
[irrigation_task] --MSG_LOG_TEXT-----> [log_queue]        --> [logger_task]
[irrigation_task] --MSG_DISPLAY_TEXT-> [display_queue]    --> [display_task]
```

Las tareas se comunican unicamente por paso de mensajes tipados, evitando variables compartidas y condiciones de carrera.

### 2.3 Flujo de Interrupciones

```
GPIO Pin 14 (rising edge)
    |
    v
IO_IRQ_BANK0 (NVIC Core 1)
    |
    v
gpio_irq_dispatcher() [ISR]
    |-- debounce check (300ms)
    |-- gpio_acknowledge_irq()
    |-- mq_send_from_isr(&irrigation_queue, MSG_MANUAL_TRIGGER)
    v
irrigation_task consume el mensaje
```

---

## 3. Administracion de Recursos del Microcontrolador

### 3.1 Memory Protection Unit (MPU)

La MPU del RP2040 (8 regiones) se configura en su propio modulo (`mpu.c`) para aislar el kernel de las tareas de usuario:

| Region | Direccion Base | Tamano | Acceso | Proposito |
|--------|---------------|--------|--------|-----------|
| 0 | 0x00000000 | 4GB | Full (priv+unpriv) | Background: ROM, Flash, RAM, SIO |
| 1 | 0x40014000 | 16KB | Solo privilegiado | IO_BANK0: registros GPIO (bomba) |
| 2 | 0x4001C000 | 4KB | Solo privilegiado | PADS_BANK0: config electrica pines |
| 3 | 0x40044000 | 4KB | Solo privilegiado | I2C0: bus del display LCD |

**Secuencia de boot:** Los managers de hardware inicializan los perifericos *antes* de activar la MPU. Una vez activa, las tareas de usuario solo pueden acceder a perifericos protegidos mediante syscalls que ejecutan en modo privilegiado.

**Proteccion de perifericos por hardware (requisito del profesor):**

Las tareas corren en modo no privilegiado (CONTROL.nPRIV=1). Si `display_task` o cualquier tarea de usuario intenta escribir directamente en el registro GPIO que controla la bomba (ej. `*(volatile uint32_t*)0x40014000 = ...`), la MPU detecta la violacion y lanza un HardFault por hardware. El handler mata la tarea ofensora sin afectar al kernel ni al plano de control critico.

Esto NO es proteccion logica por software — es la MPU fisica del ARM Cortex-M0+ la que bloquea el acceso a nivel de bus. El RP2040 no tiene MMU, pero la MPU cumple el rol de aislamiento de perifericos.

**PRIVDEFENA=1:** El modo privilegiado (kernel, handlers, managers) tiene acceso total al mapa de memoria por defecto. Las tareas solo acceden a GPIO/I2C via syscalls que elevan el privilegio de forma controlada (SVC trap).

### 3.2 Planificador (Scheduler)

- **Algoritmo:** Round-Robin preemptivo con quantum fijo de 10 ticks (100ms).
- **Conmutacion de contexto:** Via PendSV (prioridad mas baja), disparado por SysTick o syscalls bloqueantes.
- **Estados de tarea:** DORMANT, READY, RUNNING, BLOCKED.
- **Stack por tarea:** 512 words (2KB) dedicados, separados del MSP del kernel.

El scheduler guarda/restaura registros R4-R11 por software (Cortex-M0+ no tiene instrucciones STMDB como M3/M4) y el hardware guarda R0-R3, R12, LR, PC, xPSR automaticamente en el exception frame.

### 3.3 Syscalls (Llamadas al Sistema)

Las tareas en modo no privilegiado acceden a servicios del kernel mediante la instruccion `SVC`:

```
Tarea (unprivileged, PSP)
    |
    | SVC #0 (r7 = syscall_id, r0-r3 = args)
    v
wrapper_svc [ASM] -> determina PSP/MSP, extrae args
    |
    v
kernel_service() [C] -> despacha al servicio (privilegiado)
    |
    v
Retorno a tarea (resultado en r0)
```

Servicios disponibles: GPIO set/get, semaforos (init/wait/post), ADC, bomba (request + update), sleep, heartbeat, exit, print, display write/flush, logger (init/write/flush).

### 3.4 Subsistema de Entrada/Salida (I/O)

El kernel implementa una jerarquia de I/O inspirada en sistemas operativos reales (Silberschatz cap. 13):

```
User Task (unprivileged)
    |
    | syscall (SVC)
    v
Kernel Service (dispatcher)
    |
    v
Device Manager (irrigation_manager, display_manager)
    |
    v
Device Driver (kernel_drivers: GPIO, ADC, I2C)
    |
    v
Hardware (RP2040 peripherals)
```

**Tecnicas de I/O implementadas:**

| Periferico | Tecnica | Justificacion |
|-----------|---------|---------------|
| ADC (sensor humedad) | Polling | Lectura cada 5s, conversion de ~2us. No justifica interrupt. |
| GPIO boton | Interrupts (IRQ) | Evento asincronico, el CPU no debe encuestar continuamente. |
| GPIO bomba | Write directo | Operacion instantanea, un ciclo de reloj. |
| I2C display LCD | Double buffer + diff | Solo envia filas modificadas; reduce trafico I2C. |

**I/O Bufferizado (Write + Flush) con layout fijo:**

El display tiene 4 filas con roles fijos:
- Fila 0: Humedad (sensor_task actualiza cada 5s).
- Fila 1: Estado de la bomba (irrigation_task).
- Fila 2: Ultimo evento del sistema.
- Fila 3: Reservada.

Las tareas productoras envian mensajes tipados `MSG_DISPLAY_TEXT` a `display_queue` con `msg.data` indicando la fila destino. La tarea `display_task` (consumidor) recibe el mensaje y ejecuta:
- `sys_display_write(row, text)`: copia el texto a la fila indicada del back buffer (O(1), nanosegundos).
- `sys_display_flush()`: envia al LCD solo las filas que cambiaron vs. el front buffer.

**Diff Buffer por fila:**

El driver mantiene dos buffers (20 chars x 4 filas cada uno):
- `back_buf`: estado deseado (lo que se quiere mostrar).
- `front_buf`: estado actual del LCD.

Al flush, se compara `memcmp()` fila por fila. Solo las filas donde `back != front` se reescriben. Cada fila se envia como batch de 84 bytes en una sola transaccion I2C (vs. 4 transacciones por caracter sin batch).

**Por que no DMA:**

El LCD 2004A usa un controlador HD44780 accesible via expansor I2C PCF8574 en modo 4-bit. Cada caracter requiere 4 transacciones I2C separadas (high nibble + enable pulse + low nibble + enable pulse) con timing critico entre ellas. DMA es eficiente para transferencias bulk continuas (como un framebuffer OLED), pero no para protocolos conversacionales donde el CPU debe orquestar cada paso.

**Display LCD 2004A (JHD-204A):**

Driver del controlador HD44780 via PCF8574T a 400kHz. Soporta:
- Inicializacion del modo 4-bit segun datasheet HD44780.
- Escritura de texto con el charset integrado del LCD (ASCII completo).
- 20 caracteres x 4 lineas con backlight controlable.
- Deteccion de conexion I2C con timeout (no bloquea si desconectado).
- Tolerancia a fallos I2C: si el LCD deja de responder, el driver
  se desactiva automaticamente sin afectar al resto del sistema.

### 3.5 Semaforos

Primitivas del kernel, creadas durante el boot y expuestas a user tasks via syscall. Semaforos de conteo con cola de espera FIFO (hasta 10 tareas). Usados como mutex (inicializados a 1) para:
- `irrigation_pump_sem`: Exclusion mutua para activar la bomba.
- `logger_sem`: Exclusion mutua para escritura en Flash.
- `display_sem`: Exclusion mutua para el display LCD.

Cuando una tarea hace `k_sem_wait` y el recurso no esta disponible, se bloquea (BLOCKED) y se encola. Al hacer `k_sem_post`, se despierta la primera tarea en espera.

### 3.6 Simulacion de Memoria Virtual (Page Cache LRU)

El modulo `log_memory.c` implementa un buffer de 6 paginas en RAM que simula una cache de memoria:

1. **Page Hit:** El log se escribe en un frame libre disponible.
2. **Page Fault:** No hay frames libres. Se aplica LRU (Least Recently Used):
   - Se busca la pagina con menor `last_use`.
   - Si esta dirty (modificada), se vuelca a Flash via `logger_write()`.
   - Se reutiliza el frame para el nuevo mensaje.
3. **Flush periodico:** Cada 10 mensajes, `log_flush_all()` vuelca todas las paginas dirty.

Esto simula el comportamiento de un subsistema de memoria virtual con algoritmos de reemplazo de paginas.

### 3.7 Sistema de Archivos (PicoFS)

Filesystem plano sobre los ultimos sectores de Flash del RP2040:

- **Estructura:** Tabla de metadatos (1 sector) + datos contiguos.
- **Operaciones:** create, read, write, append, delete (logico), compact, format.
- **Seguridad multicore:** Toda operacion Flash pausa Core 1 (`multicore_lockout`) y deshabilita interrupciones.
- **Lectura XIP:** Los datos se leen directamente del mapa de memoria (Execute-In-Place) sin operaciones especiales.

### 3.8 Tolerancia a Fallos

#### Watchdog por Heartbeat
Cada tarea debe llamar `sys_heartbeat()` periodicamente. El SysTick verifica en cada tick:
- Si una tarea RUNNING no reporta heartbeat en 500 ticks (5 seg): **se reinicia automaticamente**.
- Si una tarea BLOCKED (en semaforo, posible deadlock) excede el timeout: se marca DORMANT y se reinicia con stack fresco.

#### HardFault Handler
Cuando una tarea genera un HardFault (violacion MPU, instruccion invalida):
1. Identifica la tarea ofensora y el PC de la falla.
2. Marca la tarea como DORMANT (la mata).
3. Dispara PendSV para cambiar a la siguiente tarea.
4. NO causa kernel panic — el sistema continua operando.

#### Proteccion de la Bomba
- Timeout maximo de bombeo (25 seg): previene inundacion.
- Tiempo minimo de bombeo (3 seg): previene ciclos cortos daninos para el motor.
- Pull-down en pin de bomba: si el pin flota, la bomba permanece apagada.

---

## 4. Uso de Bibliotecas del SDK

### Bibliotecas Utilizadas

| Biblioteca | Uso | Justificacion |
|-----------|-----|---------------|
| `pico_stdlib` | Inicializacion de sistema, USB serial, sleep durante boot | Configura clocks, PLLs y USB que requieren secuencias complejas de inicializacion no triviales de reimplementar. |
| `pico_multicore` | Lanzamiento de Core 1, lockout para Flash | El protocolo de lanzamiento del segundo core requiere mailbox FIFO del SIO; reimplementar seria propenso a errores. |
| `hardware_adc` | Lectura del sensor de humedad | El ADC del RP2040 requiere calibracion interna y secuencias de muestreo especificas. |
| `hardware_i2c` | Comunicacion con LCD 2004A via PCF8574 | El periferico I2C requiere manejo de FIFO, ACK/NAK y clock stretching a nivel de hardware. |
| `hardware_exception` | Registro de handlers (SVC, PendSV, HardFault) | Modifica la tabla de vectores en RAM de forma segura. |
| `hardware_flash` | Erase/program de sectores Flash | Las operaciones de Flash requieren ejecutar desde RAM (XIP se desactiva); el SDK provee funciones relocadas. |
| `hardware_sync` | `save_and_disable_interrupts` / `restore_interrupts` | Operaciones atomicas sobre el registro PRIMASK. |
| `hardware_irq` | Configuracion de IRQ GPIO | Configuracion segura de la NVIC. |
| `hardware_gpio` | Configuracion I2C pin function | El SDK maneja la multiplexacion de funciones de pin. |

### Ventajas del SDK vs. Implementacion Propia

**Ventajas del SDK:**
- Secuencias de inicializacion validadas (clocks, PLL, USB).
- Operaciones Flash relocadas a RAM (imposible desde Flash).
- Compatibilidad con futuras revisiones del chip.

**Desventajas del SDK:**
- Abstraccion oculta detalles del hardware.
- Mayor tamano de binario.
- Dependencia externa.

### Codigo Nativo (Sin SDK)

Los siguientes modulos acceden directamente a registros sin usar el SDK:
- **GPIO driver** (`kernel_drivers.c`): Acceso directo a SIO, IO_BANK0, PADS_BANK0.
- **Scheduler** (`scheduler.c`): Manipulacion directa de SysTick, CONTROL, PSP.
- **PendSV/SVC handlers** (`pendsv.s`, `svc_handler.s`): Ensamblador ARM puro.
- **MPU** (`mpu.c`): Configuracion directa de registros de la Memory Protection Unit.
- **Syscalls** (`syscalls.s`): Instrucciones SVC en ensamblador.

---

## 5. Estructura del Proyecto

```
Sistema-de-Riego-Automatico-sobre-PicoOS/
├── CMakeLists.txt              # Configuracion de build
├── pico_sdk_import.cmake       # Integracion del Pico SDK
├── include/
│   ├── kernel/
│   │   ├── mpu.h              # Memory Protection Unit
│   │   ├── display_manager.h  # Driver LCD 2004A (I2C)
│   │   ├── irrigation_manager.h # Driver de riego (bomba/sensor)
│   │   ├── scheduler.h        # TCB, estados, scheduler por core
│   │   ├── semaphore.h        # Semaforos con cola FIFO
│   │   ├── syscalls.h         # Interfaz de syscalls
│   │   ├── kernel_drivers.h   # Drivers GPIO/ADC bare-metal
│   │   ├── kernel_events.h    # Eventos GPIO por IRQ
│   │   ├── message_queue.h    # Colas de mensajes IPC
│   │   └── watchdog_supervisor.h # Heartbeat watchdog
│   ├── kernel_hw_config.h     # Mapa de pines de hardware
│   ├── filesystem.h           # PicoFS API
│   ├── log_memory.h           # Page cache LRU
│   ├── logger.h               # Logger persistente
│   └── user_app.h             # Recursos compartidos de usuario
├── src/
│   ├── main.c                 # Kernel boot, dual-core init
│   ├── arch/
│   │   ├── pendsv.s           # Context switch (PendSV handler)
│   │   ├── svc_handler.s      # SVC dispatcher (ASM)
│   │   └── syscalls.s         # Syscall stubs (ASM)
│   ├── kernel/
│   │   ├── mpu.c             # Configuracion de regiones MPU
│   │   ├── display_manager.c # Driver LCD: write+flush, I2C batch
│   │   ├── irrigation_manager.c # Maquina de estados bomba
│   │   ├── scheduler.c       # Planificador Round-Robin
│   │   ├── semaphore.c       # Implementacion de semaforos
│   │   ├── kernel_service.c  # Despachador de syscalls (C)
│   │   ├── kernel_drivers.c  # Drivers bare-metal GPIO/ADC
│   │   ├── kernel_events.c   # Sistema de eventos GPIO
│   │   ├── message_queue.c   # Colas de mensajes
│   │   ├── watchdog_supervisor.c # Heartbeat
│   │   ├── filesystem.c      # PicoFS (Flash filesystem)
│   │   ├── log_memory.c      # Page cache con LRU
│   │   └── logger.c          # Persistencia de logs
│   └── user/
│       ├── user_app.c         # Recursos globales (sems, colas)
│       └── tasks/
│           ├── irrigation_task.c # Consumidor de cola de riego
│           ├── sensor_task.c     # Lectura periodica ADC
│           ├── logger_task.c     # Consumidor de cola de logs
│           ├── display_task.c    # Consumidor de cola display
│           └── mpu_test_task.c   # Demo violacion MPU + recovery
```

---

## 6. Guia de Compilacion y Carga

### Requisitos

- **Pico SDK** (v1.5+): [github.com/raspberrypi/pico-sdk](https://github.com/raspberrypi/pico-sdk)
- **ARM GCC Toolchain** (`arm-none-eabi-gcc` 10+)
- **CMake** (3.13+)
- **Python 3** (para herramientas del SDK)

### Compilacion

```bash
# 1. Clonar el repositorio
git clone <url-del-repo> && cd Sistema-de-Riego-Automatico-sobre-PicoOS

# 2. Configurar variable de entorno del SDK
export PICO_SDK_PATH=/ruta/al/pico-sdk

# 3. Crear directorio de build y compilar
mkdir build && cd build
cmake ..
make -j4

# 4. El binario resultante es:
#    build/ProyectoFinal.uf2
```

### Carga en la Raspberry Pi Pico

1. **Conectar la Pico en modo BOOTSEL:**
   - Mantener presionado el boton BOOTSEL de la Pico.
   - Conectar el cable USB al computador.
   - Soltar el boton. Aparecera como unidad USB `RPI-RP2`.

2. **Copiar el firmware:**
   ```bash
   cp build/ProyectoFinal.uf2 /Volumes/RPI-RP2/    # macOS
   # o
   cp build/ProyectoFinal.uf2 /media/$USER/RPI-RP2/ # Linux
   ```

3. **La Pico se reinicia automaticamente** y comienza a ejecutar PicoOS.

### Conexion Serial (Debug)

```bash
# macOS
screen /dev/tty.usbmodem* 115200

# Linux
minicom -D /dev/ttyACM0 -b 115200
```

### Conexionado Fisico

| Componente | Pin Pico | GPIO | Notas |
|-----------|----------|------|-------|
| Bomba (rele) | Pin 9 | GPIO 6 | Salida, pull-down, activa en LOW |
| Sensor humedad | Pin 31 | GPIO 26 (ADC0) | Entrada analogica |
| Boton manual | Pin 19 | GPIO 14 | Entrada, pull-down, rising edge |
| LCD SDA | Pin 6 | GPIO 4 | I2C0 Data (LCD 2004A) |
| LCD SCL | Pin 7 | GPIO 5 | I2C0 Clock (LCD 2004A) |

---

## 7. Conceptos de SO Implementados

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
| I/O bufferizado | Write+flush, diff buffer, batch I2C por fila | `display_manager.c` |
| Memoria virtual (sim.) | Page cache con reemplazo LRU | `log_memory.c` |
| Sistema de archivos | Filesystem plano sobre Flash | `filesystem.c` |
| Tolerancia a fallos | Watchdog heartbeat + HardFault handler | `watchdog_supervisor.c`, `main.c` |
| Deteccion de deadlock | Timeout en tareas BLOCKED sin wake_tick | `scheduler.c:isr_systick()` |

---

## 8. Diagrama de Arquitectura de Procesos

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              HARDWARE (RP2040)                           │
├─────────────────────────────────┬───────────────────────────────────────┤
│          CORE 0                 │              CORE 1                    │
│   (Gestion / UI / Logs)        │     (Control Critico / RT Duro)        │
├─────────────────────────────────┼───────────────────────────────────────┤
│                                 │                                        │
│  ┌──────────────────────────┐   │   ┌──────────────────────────┐        │
│  │    SysTick (10ms)        │   │   │    SysTick (10ms)        │        │
│  │    Scheduler Core 0      │   │   │    Scheduler Core 1      │        │
│  │    Watchdog Check        │   │   │    Watchdog Check        │        │
│  └──────────┬───────────────┘   │   └──────────┬───────────────┘        │
│             │                   │              │                         │
│  ┌──────────▼───────────────┐   │   ┌──────────▼───────────────┐        │
│  │ logger_task              │   │   │ irrigation_task           │        │
│  │ - Consume log_queue      │   │   │ - Consume irrigation_queue│        │
│  │ - Page cache LRU         │   │   │ - Despacha riego         │        │
│  │ - Flush a Flash          │   │   │ - Envia a log/display    │        │
│  └──────────────────────────┘   │   └──────────────────────────┘        │
│                                 │                                        │
│  ┌──────────────────────────┐   │   ┌──────────────────────────┐        │
│  │ display_task             │   │   │ sensor_task              │        │
│  │ - Consume display_queue  │   │   │ - Lee ADC cada 5s        │        │
│  │ - write+flush al LCD     │   │   │ - Envia DRY/WET a cola   │        │
│  └──────────────────────────┘   │   └──────────────────────────┘        │
│                                 │                                        │
│                                 │   ┌──────────────────────────┐        │
│                                 │   │ irrigation_update_task   │        │
│                                 │   │ - Maquina de estados     │        │
│                                 │   │   bomba ON/OFF (syscall) │        │
│                                 │   └──────────────────────────┘        │
├─────────────────────────────────┴───────────────────────────────────────┤
│                          KERNEL (Privilegiado)                           │
├─────────────────────────────────────────────────────────────────────────┤
│  MPU: IO_BANK0 + PADS_BANK0 + I2C0 protegidos (solo kernel)             │
│  SVC Handler -> kernel_service() -> GPIO, ADC, I2C, Semaforos, Sleep    │
│  HardFault Handler -> mata tarea, PendSV, continua                      │
│  Flash ops: multicore_lockout + disable interrupts                      │
├─────────────────────────────────────────────────────────────────────────┤
│                         COMUNICACION (IPC)                               │
├─────────────────────────────────────────────────────────────────────────┤
│  irrigation_queue: sensor/boton -> irrigation_task (inter-core)         │
│  log_queue:        irrigation_task -> logger_task                        │
│  display_queue:    sensor/irrigation_task -> display_task                │
│  Semaforos:        irrigation_pump_sem, logger_sem, display_sem          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Zonas de Memoria Protegidas por MPU

```
0x00000000 ┬─────────────────────────────┐
           │ Flash (codigo + XIP FS)     │ Region 0: Full Access
0x10000000 │ XIP Base                    │
           ├─────────────────────────────┤
0x20000000 │ SRAM (stacks, variables)    │ Region 0: Full Access
           ├─────────────────────────────┤
0x40014000 │ IO_BANK0 (GPIO ctrl)       │ Region 1: SOLO KERNEL
0x40018000 ├─────────────────────────────┤
0x4001C000 │ PADS_BANK0 (pad config)    │ Region 2: SOLO KERNEL
0x4001D000 ├─────────────────────────────┤
0x40044000 │ I2C0 (bus display LCD)     │ Region 3: SOLO KERNEL
0x40045000 ├─────────────────────────────┤
           │ Otros perifericos           │ Region 0: Full Access
0xD0000000 │ SIO (acceso directo GPIO)   │ Region 0: Full Access
           └─────────────────────────────┘
```

---

## 9. Mapeo de Practicas del Curso al Proyecto

El proyecto integra los conceptos de las 10 practicas del laboratorio:

| Practica | Concepto SO | Implementacion en el proyecto |
|----------|------------|-------------------------------|
| P2: Syscalls | Interfaz kernel/usuario via SVC | `syscalls.s` + `kernel_service.c`: 17 servicios activos |
| P3: Context Switch | PendSV + interrupciones GPIO | `pendsv.s` (save/restore R4-R11) + `kernel_events.c` (boton) |
| P4: Scheduler | Round-Robin preemptivo con SysTick | `scheduler.c`: dual-core, quantum 100ms, estados de tarea |
| P5: Semaforos | Sincronizacion con cola de espera | `semaphore.c`: 3 mutex (bomba, logger, display) via syscall |
| P6: Memoria Virtual | Page cache LRU, page faults | `log_memory.c`: 6 paginas en RAM, reemplazo LRU, flush a Flash |
| P7: MPU + Recovery | Proteccion de perifericos, HardFault | `mpu.c`: 4 regiones HW + reinicio automatico de tareas |
| P8: Filesystem | PicoFS sobre Flash | `filesystem.c`: create/write/read/delete/compact sobre Flash |
| P9: UART + I/O | Polling, Interrupts, DMA, analisis | ADC=polling, boton=IRQ, LCD=polling+timeout, UART=sys_print |
| P10: Dual-Core | Multiprocesamiento asimetrico | Core 0 (UI/logs) + Core 1 (sensor/bomba), colas inter-core |

### Justificacion de sys_print vs printf

Las tareas de usuario no pueden llamar `printf()` directamente porque corren en modo no privilegiado. El hardware UART esta mapeado en el espacio de perifericos, y aunque no tiene una region MPU explicita asignada, el modelo arquitectonico exige que todo acceso a hardware pase por el kernel via syscall. `sys_print()` genera un SVC trap que eleva el privilegio, y el kernel ejecuta `printf()` en modo privilegiado. Codigo kernel (boot, ISRs, managers) si usa `printf()` directo.

---

## 10. Conclusiones

El sistema implementa un RTOS funcional con separacion de privilegios, aislamiento de memoria, comunicacion por mensajes, y tolerancia a fallos, todo sobre un microcontrolador de $4 USD. Los conceptos de sistemas operativos (scheduling, IPC, proteccion de memoria, filesystem, manejo de interrupciones) se aplican de forma tangible sobre hardware real, demostrando que los principios teoricos del curso tienen impacto directo en la confiabilidad y seguridad de sistemas embebidos.
