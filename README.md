# Sistema de Riego Automatico sobre PicoOS

Sistema operativo minimalista (PicoOS) que gestiona riego automatico sobre Raspberry Pi Pico (RP2040). Implementa planificacion Round-Robin, proteccion de memoria (MPU), syscalls, IPC por colas, page cache LRU con write-back diferido a Flash, y tolerancia a fallos.

## Arquitectura

```
Core 0 (Gestion)              Core 1 (Control Critico)
├── logger_task                ├── irrigation_task
├── display_task               ├── sensor_task
└── mpu_test_task              └── irrigation_update_task

Kernel: MPU (5 regiones) | Scheduler RR | 17 Syscalls | Flash Queue
IPC: irrigation_queue | log_queue | display_queue | 3 Semaforos
```

## Conexionado

| Componente | GPIO | Pin Pico | Notas |
|-----------|------|----------|-------|
| Bomba (rele) | 6 | Pin 9 | Activa en LOW, pull-down |
| Sensor humedad | 26 (ADC0) | Pin 31 | Analogico |
| Boton manual | 14 | Pin 19 | Pull-down, rising edge |
| LCD SDA | 8 | Pin 11 | I2C0, LCD 2004A (PCF8574) |
| LCD SCL | 9 | Pin 12 | I2C0, 400kHz |

## Compilacion

### Requisitos

- [Pico SDK](https://github.com/raspberrypi/pico-sdk) (v1.5+)
- ARM GCC Toolchain (`arm-none-eabi-gcc` 10+)
- CMake (3.13+)

### Build

```bash
export PICO_SDK_PATH=/ruta/al/pico-sdk
mkdir build && cd build
cmake ..
make -j4
```

El binario resultante: `build/ProyectoFinal.uf2`

## Carga en la Pico

1. Mantener BOOTSEL presionado
2. Conectar USB → aparece como unidad `RPI-RP2`
3. Copiar el `.uf2`:

```bash
# macOS
cp build/ProyectoFinal.uf2 /Volumes/RPI-RP2/

# Linux
cp build/ProyectoFinal.uf2 /media/$USER/RPI-RP2/

# Windows
# Arrastrar ProyectoFinal.uf2 a la unidad RPI-RP2
```

4. La Pico se reinicia automaticamente

## Monitor Serial (Debug)

La Pico envia logs por USB CDC (serial virtual). Opciones:

### macOS

```bash
screen /dev/tty.usbmodem* 115200
```

### Linux

```bash
minicom -D /dev/ttyACM0 -b 115200
# o
screen /dev/ttyACM0 115200
```

### Windows (PuTTY)

1. Abrir PuTTY
2. Connection type: **Serial**
3. Serial line: `COMx` (revisar en Administrador de Dispositivos → Puertos COM)
4. Speed: `115200`
5. Click **Open**

Para encontrar el puerto COM: Administrador de Dispositivos → Puertos (COM y LPT) → "USB Serial Device (COMx)"

### Windows (Terminal)

```powershell
# PowerShell con Windows Terminal
# Instalar: winget install Microsoft.WindowsTerminal
mode COMx: baud=115200
type COMx
```

## Uso

El sistema arranca automaticamente al conectar. Comportamiento:

- **Sensor seco** (ADC > 2500): activa bomba, apaga cuando detecta humedad
- **Boton manual** (GPIO 14): activa un ciclo de riego de ~4s
- **Display LCD**: muestra humedad, estado bomba, ultimo evento
- **Logs**: se almacenan en page cache (6 frames LRU) y se persisten a Flash

## Estructura del Proyecto

```
├── include/
│   ├── kernel/          # Headers del kernel (scheduler, mpu, syscalls, etc.)
│   ├── filesystem.h     # PicoFS API
│   ├── flash_queue.h    # Cola write-back diferido
│   ├── log_memory.h     # Page cache LRU
│   └── logger.h         # Logger persistente
├── src/
│   ├── main.c           # Boot dual-core, idle loop (flash worker)
│   ├── arch/            # Assembly: PendSV, SVC handler, syscall stubs
│   ├── kernel/          # Scheduler, MPU, drivers, filesystem, flash queue
│   └── user/tasks/      # Tareas de usuario (irrigation, sensor, logger, display)
├── diagrams.py          # Generador de diagramas (matplotlib)
├── REPORTE.md           # Reporte academico completo
└── CMakeLists.txt
```

## Generar Diagramas

```bash
pip install matplotlib
python3 diagrams.py
```

Genera: `diagrama_arquitectura.png`, `diagrama_mpu.png`, `diagrama_writeback.png`, `diagrama_ipc.png`
