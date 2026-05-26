# Sistema de Riego Automático sobre PicoOS

**Sistemas Operativos 2026-2 (0713)**
**Plataforma:** Raspberry Pi Pico (RP2040 - Dual-core ARM Cortex-M0+ @ 125MHz)
**Repositorio:** https://github.com/jzarcoo/Sistema-de-Riego-Automatico-sobre-PicoOS
**Video de funcionamiento:** https://canva.link/1fd4zm14b8c9oei

---

## 1. Introducción

El riego manual de plantas es una tarea que se olvida fácilmente y que, cuando se hace de forma ineficiente, desperdicia agua. Un sistema automático que mida la humedad del suelo y active el riego solo cuando es necesario resuelve ambos problemas. Sin embargo, construirlo sobre un microcontrolador sin sistema operativo resulta en código monolítico difícil de mantener y sin garantías de seguridad.

Este proyecto aborda ese problema implementando un sistema operativo minimalista (PicoOS) sobre la Raspberry Pi Pico que gestiona el riego automático. El sistema lee un sensor de humedad, activa una bomba cuando el suelo está seco, permite riego manual por botón, muestra el estado en un LCD, y registra eventos en Flash. Todo esto corre sobre un kernel con separación de privilegios, protección de memoria por hardware, y planificación preemptiva en dos núcleos, demostrando que los conceptos de sistemas operativos tienen aplicación directa en sistemas embebidos reales.

---

## 2. Objetivos

- Implementar un kernel con planificación Round-Robin preemptiva sobre dos cores independientes.
- Aplicar protección de memoria vía MPU para aislar tareas de usuario del hardware.
- Demostrar IPC por paso de mensajes (productor-consumidor) sin variables compartidas.
- Implementar page cache con reemplazo LRU y write-back diferido a Flash.
- Construir un sistema de archivos minimalista sobre Flash (PicoFS).
- Garantizar tolerancia a fallos: watchdog, HardFault handler, reinicio automático de tareas.
- Demostrar la integración de múltiples subsistemas de SO en una aplicación funcional sobre hardware real.

---

## 3. Marco Teórico

### 3.1 Planificación de Procesos

Un scheduler Round-Robin asigna un quantum fijo a cada tarea. Cuando el quantum expira, el scheduler preempta la tarea y selecciona la siguiente en estado READY. Se eligió Round-Robin porque las tareas del sistema (sensor, bomba, display, logger) tienen importancia similar y no requieren planificación por prioridad. En un sistema dual-core como el RP2040, cada core ejecuta su propio scheduler de forma independiente.

### 3.2 Protección de Memoria

La MPU (Memory Protection Unit) del ARM Cortex-M0+ permite definir regiones de memoria con permisos diferenciados entre modo privilegiado (kernel) y no privilegiado (tareas). Si una tarea viola los permisos, el hardware genera un HardFault. En este proyecto, la MPU es esencial para evitar que una tarea de usuario acceda directamente a periféricos críticos como la bomba o el bus I2C del display.

### 3.3 Comunicación entre Procesos (IPC)

El modelo productor-consumidor usa colas de mensajes tipados. Las tareas productoras envían mensajes a una cola y las consumidoras los extraen. Este esquema desacopla las tareas y evita condiciones de carrera sin necesidad de memoria compartida explícita. En nuestro caso, es la única forma segura de comunicar tareas que corren en cores distintos, ya que no comparten scheduler ni stack.

### 3.4 Semáforos y Sincronización

Los semáforos de conteo implementan una cola de espera FIFO: si el recurso no está disponible, la tarea se bloquea y se encola; al liberar el recurso, se despierta la primera tarea en espera. En este proyecto se usan como mutex (inicializados a 1) para garantizar exclusión mutua en el acceso a la bomba, el logger y el display, recursos que no deben ser manipulados por dos tareas simultáneamente.

### 3.5 Page Cache y Políticas de Escritura

Existen dos políticas principales de escritura en cache:

- **Write-Through:** Escribe a cache y a disco simultáneamente. Seguro pero lento.
- **Write-Back:** Escribe solo a cache. El disco se actualiza después (cuando la página es desalojada o hay flush). Rápido pero se pierden datos ante corte de energía.

**Política elegida: Write-Back.** Se eligió Write-Back porque las operaciones de escritura a Flash en el RP2040 requieren pausar ambos cores (multicore lockout), lo cual es costoso. Con Write-Back, las escrituras se acumulan en RAM y se persisten de forma diferida desde el idle loop, minimizando las pausas del sistema. El riesgo de pérdida de datos ante corte de energía es aceptable porque los logs de riego no son críticos.

**Algoritmo de reemplazo: LRU (Least Recently Used).** Cuando la cache está llena, se desaloja la página que no se ha usado por más tiempo.

### 3.6 Sistemas de Archivos

Un filesystem minimalista sobre Flash requiere considerar que Flash solo puede borrarse por sectores completos y programarse por páginas. Las operaciones de escritura necesitan read-modify-write. En un sistema multicore, las operaciones Flash deben pausar el otro core para evitar que ejecute código desde Flash mientras esta se reprograma.

### 3.7 Tolerancia a Fallos

El watchdog detecta tareas que dejaron de reportar actividad y las reinicia automáticamente. El HardFault handler atrapa violaciones de memoria y mata la tarea ofensora sin afectar al resto del sistema. La combinación de ambos mecanismos permite que el kernel sobreviva a fallos de tareas individuales, algo crítico en un sistema que controla hardware físico como una bomba de agua.

---

## 4. Desarrollo

### 4.1 Arquitectura Dual-Core

| Core | Rol | Tareas |
|------|-----|--------|
| Core 0 | Gestión, UI, logs | `logger_task`, `display_task`, `mpu_test_task` |
| Core 1 | Control crítico, bomba, sensor | `irrigation_task`, `sensor_task`, `irrigation_update_task` |

Cada core tiene su scheduler independiente con SysTick a 10ms. La separación no es arbitraria: el control de la bomba y la lectura del sensor están en Core 1 para garantizar que la latencia de I/O del display o del logger en Core 0 no afecte la respuesta del sistema de riego.

**Comunicación inter-core:** Exclusivamente por colas de mensajes con deshabilitación de interrupciones para atomicidad.

```
[sensor_task] --MSG_SOIL_DRY/WET----> [irrigation_queue] --> [irrigation_task]
[sensor_task] --MSG_DISPLAY_TEXT----> [display_queue]    --> [display_task]
[botón IRQ]   --MSG_MANUAL_TRIGGER--> [irrigation_queue] --> [irrigation_task]
[irrigation_task] --MSG_LOG_TEXT-----> [log_queue]        --> [logger_task]
[irrigation_task] --MSG_DISPLAY_TEXT-> [display_queue]    --> [display_task]
```

### 4.2 Memory Protection Unit (MPU)

Se configuraron 5 regiones:

| Región | Dirección | Tamaño | Acceso | Protege |
|--------|-----------|--------|--------|---------|
| 0 | 0x00000000 | 4GB | Full | Background (ROM, RAM, SIO) |
| 1 | 0x40014000 | 16KB | Solo kernel | IO_BANK0: función de pin |
| 2 | 0x4001C000 | 4KB | Solo kernel | PADS_BANK0: config eléctrica |
| 3 | 0x40044000 | 4KB | Solo kernel | I2C0: bus LCD |
| 4 | 0x40034000 | 4KB | Solo kernel | UART0: serial output |

Si una tarea de usuario intenta acceder a estos periféricos directamente, la MPU genera HardFault. El handler mata la tarea y el sistema continúa operando.

**Nota sobre SIO (0xD0000000):** No se protege porque contiene el registro CPUID que el Pico SDK consulta desde `get_core_num()` en muchas funciones internas. La MPU del M0+ no permite proteger parte de un bloque. La bomba se protege indirectamente con IO_BANK0: sin acceso a IO_BANK0, una tarea no puede reconfigurar el pin de la bomba.

**PRIVDEFENA=1:** El modo privilegiado tiene acceso total. Las tareas acceden a periféricos únicamente vía syscalls.

### 4.3 Planificador

- Round-Robin preemptivo con quantum de 10 ticks (100ms).
- Conmutación de contexto vía PendSV.
- Estados posibles: DORMANT, READY, RUNNING, BLOCKED.
- Stack de 512 words (2KB) por tarea.
- El scheduler guarda R4-R11 manualmente porque el Cortex-M0+ no tiene instrucción STMDB.

### 4.4 Syscalls

El kernel expone 17 servicios vía instrucción SVC:

```
Tarea (unprivileged, PSP)
    | SVC #0 (r7 = id, r0-r3 = args)
    v
wrapper_svc [ASM] -> extrae args del stack frame
    v
kernel_service() [C] -> despacha (GPIO, ADC, semáforos, sleep, print, etc.)
    v
Retorno a tarea (resultado en r0)
```

### 4.5 Subsistema de I/O

La jerarquía de acceso a hardware sigue el patrón clásico de SO:

```
User Task --> syscall --> Device Manager --> Device Driver --> Hardware
```

| Periférico | Técnica | Justificación |
|-----------|---------|---------------|
| ADC (sensor) | Polling cada 5s | La conversión tarda ~2μs, no justifica interrupt |
| GPIO botón | IRQ + triple sampling + debounce 500ms | Evento asincrónico, filtra EMI del relay |
| GPIO bomba | Write directo (SIO) | Un ciclo de reloj, no requiere buffering |
| I2C LCD | Double buffer + diff + batch por fila | Solo envía filas modificadas, ahorra bus |
| UART | sys_print (MPU protege UART0) | Fuerza uso de syscall para serializar acceso |

Para el **Display LCD 2004A** se implementó un driver HD44780 vía PCF8574T a 400kHz con double buffer (front/back), diff por fila con memcmp, y batch I2C. El driver es tolerante a desconexión: si el timeout I2C se dispara, se desactiva sin afectar al resto del sistema.

### 4.6 Semáforos

Se implementaron 3 mutex (semáforos inicializados a 1):
- `irrigation_pump_sem`: Exclusión mutua para la bomba.
- `logger_sem`: Exclusión mutua para operaciones de Flash.
- `display_sem`: Exclusión mutua para el LCD.

Cada semáforo tiene una cola de espera FIFO. El timeout por watchdog detecta posibles deadlocks: si una tarea permanece bloqueada más de 5 segundos, se considera colgada.

### 4.7 Page Cache LRU + Write-Back Diferido

La cache consta de 6 frames de 64 bytes en RAM con política Write-Back:

1. **Hit:** Se escribe en el frame correspondiente y se marca como dirty.
2. **Miss (cache llena):** LRU selecciona la víctima. Si está dirty, se encola en `flash_work_queue`.
3. **Flush periódico (cada 50 mensajes):** Se encolan todas las páginas dirty.
4. **Write-Back:** El idle loop del kernel drena la cola y escribe a Flash en modo privilegiado (thread mode).

El flush diferido evita deadlock inter-core: las operaciones de Flash necesitan `multicore_lockout`, que requiere que Core 1 responda a una IRQ FIFO. Si Core 1 está en SVC handler, no puede responder. El idle loop corre en thread mode donde Core 1 siempre puede atender el lockout.

### 4.8 Sistema de Archivos (PicoFS)

Se implementó un filesystem plano sobre Flash:
- Tabla de metadatos en un sector dedicado, con datos contiguos a continuación.
- Operaciones soportadas: create, read, write, append, delete, compact, format.
- Seguridad multicore: cada operación Flash usa `multicore_lockout` + deshabilitación de interrupciones.
- Las lecturas se realizan vía XIP (Execute-In-Place), accediendo directamente al mapa de memoria.

### 4.9 Tolerancia a Fallos

**Watchdog:** Cada tarea reporta `sys_heartbeat()` periódicamente. Si no reporta en 500 ticks (5 segundos), el supervisor la reinicia con un stack fresco.

**HardFault Handler:** Ante una violación de MPU, identifica la tarea y el PC ofensor, marca la tarea como DORMANT, y dispara PendSV para que el scheduler continúe con la siguiente tarea.

**Protección de la bomba:** Se implementó un timeout máximo de 25 segundos, un tiempo mínimo de 3 segundos entre activaciones, y un pull-down en el pin GPIO para que un pin flotante siempre signifique bomba apagada.

### 4.10 Manejo de Flash Multicore

El RP2040 comparte la Flash entre ambos cores. Durante una operación de escritura o borrado, el XIP se desactiva y ningún core puede ejecutar código desde Flash. Esto requiere pausar Core 1 vía `multicore_lockout`.

**Problema:** Si Core 1 está dentro del SVC handler cuando Core 0 solicita el lockout, la FIFO IRQ no puede interrumpirlo (misma prioridad en Cortex-M0+), causando un deadlock.

**Solución implementada:** Write-back diferido con timeout.
1. Las páginas dirty del page cache se encolan en `flash_work_queue` (RAM).
2. El idle loop del kernel (thread mode, Core 0) drena la cola.
3. `multicore_lockout_start_timeout_us(50ms)` reemplaza la versión blocking.
4. Si el timeout expira (Core 1 en SVC), se reintenta en la siguiente iteración.

Esto garantiza que el sistema nunca se congela por una operación de Flash.

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

El código nativo (sin SDK) incluye: GPIO driver, scheduler, PendSV/SVC handlers, MPU y syscalls.

---

## 5. Resultados

### 5.1 Funcionamiento del Sistema

El sistema opera correctamente en todas las condiciones probadas:
- El sensor lee humedad cada 5 segundos y envía MSG_SOIL_DRY o MSG_SOIL_WET a la cola de irrigación.
- La bomba se activa cuando el suelo está seco (ADC > 2500) y se apaga automáticamente al detectar humedad suficiente.
- El botón manual dispara un ciclo de riego de ~4 segundos, independiente del sensor.
- El display LCD muestra en tiempo real la humedad, el estado de la bomba y el último evento registrado.
- Los logs se persisten a Flash vía page cache LRU sin bloquear el sistema.
- La demo de MPU genera un HardFault controlado y el sistema sobrevive sin reiniciarse.
- El watchdog detecta y reinicia tareas colgadas de forma transparente.

### 5.2 Problemas Encontrados y Soluciones

**1. EMI del relay en GPIO del botón:**
El relay genera ruido electromagnético al conmutar que activa GPIO 14 falsamente. Solución: triple sampling en ISR (3 lecturas con ~1ms entre cada una) + debounce de 500ms + drain de mensajes post-riego.

**2. Deadlock inter-core por Flash lockout:**
`multicore_lockout_start_blocking()` espera que Core 1 responda vía FIFO IRQ. Si Core 1 está en SVC handler, la IRQ no puede interrumpirlo (misma prioridad en M0+). Solución: write-back diferido (las escrituras a Flash se encolan y se ejecutan desde el idle loop) + `multicore_lockout_start_timeout_us(50ms)` que reintenta sin bloquear el sistema.

**3. SIO no protegible por MPU:**
El bloque SIO (0xD0000000) contiene CPUID que el SDK usa internamente. Proteger SIO completo causa HardFault en funciones básicas del SDK. La MPU del M0+ no permite proteger sub-bloques. Solución: proteger IO_BANK0 (que controla la función del pin) en vez de SIO directamente.

**4. printf en ISR/SVC handler:**
`printf` usa UART que es bloqueante. Dentro de un ISR o SVC handler, bloquea el sistema entero. Solución: las tareas usan `sys_print` (syscall) y el ISR no imprime nada.

---

## 6. Conclusiones

Este proyecto demostró que los conceptos teóricos de sistemas operativos no son abstracciones académicas: se aplican directamente sobre hardware real con restricciones concretas. La planificación Round-Robin, la protección de memoria por MPU, la comunicación por colas de mensajes, y la persistencia con page cache LRU operan sobre el microcontrolador cumpliendo requisitos de tiempo real.

Las restricciones del hardware (M0+ sin MMU, prioridades fijas de excepciones, Flash compartida entre cores) obligaron a tomar decisiones de diseño que no aparecen en los libros de texto: el write-back diferido para evitar deadlock, la imposibilidad de proteger SIO, y el filtrado de EMI por software. Estos son problemas que solo se descubren al implementar sobre hardware físico y que enriquecieron significativamente el aprendizaje.

Lo más desafiante fue el deadlock inter-core por Flash lockout, porque no era reproducible de forma determinista y requirió entender a fondo el modelo de interrupciones del Cortex-M0+. La solución (flush diferido desde el idle loop) es un patrón que se usa en kernels reales y fue satisfactorio llegar a ella por necesidad.

---

## 7. Conexionado Físico

| Componente | GPIO | Pin Pico | Notas |
|-----------|------|----------|-------|
| Bomba (relé) | 6 | Pin 9 | Activa en LOW, pull-down |
| Sensor humedad | 26 (ADC0) | Pin 31 | Analógico |
| Botón manual | 14 | Pin 19 | Pull-down, rising edge |
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
└── CMakeLists.txt
```

---

## 9. Referencias

- Silberschatz, A., Galvin, P., Gagne, G. *Operating System Concepts* (10th Edition). Wiley, 2018.
- Raspberry Pi Ltd. *RP2040 Datasheet*. 2021.
- Raspberry Pi Ltd. *Raspberry Pi Pico C/C++ SDK Documentation*. 2021.
- ARM Ltd. *ARMv6-M Architecture Reference Manual*. 2017.
