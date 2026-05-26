#!/usr/bin/env python3
"""
Genera diagramas de arquitectura de PicoOS.
Requiere: pip install matplotlib
Uso: python3 diagrams.py
"""

import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

def diagram_architecture():
    fig, ax = plt.subplots(1, 1, figsize=(14, 10))
    ax.set_xlim(0, 14)
    ax.set_ylim(0, 10)
    ax.axis('off')
    ax.set_title('PicoOS - Arquitectura Dual-Core Asimetrica', fontsize=14, fontweight='bold')

    # Core 0
    ax.add_patch(FancyBboxPatch((0.5, 4.5), 6, 5, boxstyle="round,pad=0.1",
                                facecolor='#E3F2FD', edgecolor='#1565C0', linewidth=2))
    ax.text(3.5, 9.2, 'CORE 0 - Gestion / UI / Logs', ha='center', fontsize=11, fontweight='bold', color='#1565C0')

    # Core 1
    ax.add_patch(FancyBboxPatch((7.5, 4.5), 6, 5, boxstyle="round,pad=0.1",
                                facecolor='#E8F5E9', edgecolor='#2E7D32', linewidth=2))
    ax.text(10.5, 9.2, 'CORE 1 - Control Critico / RT', ha='center', fontsize=11, fontweight='bold', color='#2E7D32')

    # Tareas Core 0
    tasks_c0 = [
        ('logger_task', 'Page cache LRU\nFlush diferido'),
        ('display_task', 'LCD 2004A\nWrite+Flush'),
        ('mpu_test_task', 'Demo MPU\n+ Recovery'),
    ]
    for i, (name, desc) in enumerate(tasks_c0):
        y = 8.3 - i * 1.4
        ax.add_patch(FancyBboxPatch((1, y), 5.2, 1.1, boxstyle="round,pad=0.05",
                                    facecolor='white', edgecolor='#1565C0'))
        ax.text(1.3, y + 0.7, name, fontsize=9, fontweight='bold')
        ax.text(1.3, y + 0.2, desc, fontsize=7, color='gray')

    # Tareas Core 1
    tasks_c1 = [
        ('irrigation_task', 'Consume cola riego\nDispatch bomba'),
        ('sensor_task', 'ADC cada 5s\nEnvia DRY/WET'),
        ('irrigation_update_task', 'Syscall: estado\nbomba ON/OFF'),
    ]
    for i, (name, desc) in enumerate(tasks_c1):
        y = 8.3 - i * 1.4
        ax.add_patch(FancyBboxPatch((8, y), 5.2, 1.1, boxstyle="round,pad=0.05",
                                    facecolor='white', edgecolor='#2E7D32'))
        ax.text(8.3, y + 0.7, name, fontsize=9, fontweight='bold')
        ax.text(8.3, y + 0.2, desc, fontsize=7, color='gray')

    # Kernel
    ax.add_patch(FancyBboxPatch((0.5, 1.5), 13, 2.7, boxstyle="round,pad=0.1",
                                facecolor='#FFF3E0', edgecolor='#E65100', linewidth=2))
    ax.text(7, 3.9, 'KERNEL (Modo Privilegiado)', ha='center', fontsize=11, fontweight='bold', color='#E65100')

    kernel_items = [
        'SVC Handler (17 syscalls)',
        'MPU: IO_BANK0 + PADS + I2C',
        'Scheduler RR (10ms quantum)',
        'Flash Queue (Write-Back diferido)',
        'HardFault Handler (mata tarea, continua)',
    ]
    for i, item in enumerate(kernel_items):
        col = i % 3
        row = i // 3
        ax.text(1.5 + col * 4.5, 3.3 - row * 0.6, '• ' + item, fontsize=7.5)

    # IPC
    ax.add_patch(FancyBboxPatch((0.5, 0.3), 13, 1, boxstyle="round,pad=0.1",
                                facecolor='#F3E5F5', edgecolor='#6A1B9A', linewidth=1.5))
    ax.text(7, 1.05, 'IPC: irrigation_queue | log_queue | display_queue | Semaforos (3)',
            ha='center', fontsize=9, color='#6A1B9A')
    ax.text(7, 0.55, 'Colas thread-safe (disable IRQ) + flash_work_queue (write-back)',
            ha='center', fontsize=8, color='#6A1B9A')

    plt.tight_layout()
    plt.savefig('diagrama_arquitectura.png', dpi=150, bbox_inches='tight')
    plt.close()
    print("Generado: diagrama_arquitectura.png")


def diagram_mpu():
    fig, ax = plt.subplots(1, 1, figsize=(10, 7))
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 10)
    ax.axis('off')
    ax.set_title('MPU - Mapa de Memoria Protegida', fontsize=13, fontweight='bold')

    regions = [
        ('0x00000000', '4GB', 'Full Access (Background)', '#C8E6C9', 'Region 0: ROM, Flash, RAM, SIO'),
        ('0x40014000', '16KB', 'SOLO KERNEL', '#FFCDD2', 'Region 1: IO_BANK0 (GPIO control)'),
        ('0x4001C000', '4KB', 'SOLO KERNEL', '#FFCDD2', 'Region 2: PADS_BANK0 (config pines)'),
        ('0x40044000', '4KB', 'SOLO KERNEL', '#FFCDD2', 'Region 3: I2C0 (bus LCD)'),
    ]

    y = 8.5
    for addr, size, access, color, desc in regions:
        ax.add_patch(FancyBboxPatch((1, y), 8, 1.2, boxstyle="round,pad=0.05",
                                    facecolor=color, edgecolor='black', linewidth=1.5))
        ax.text(1.3, y + 0.8, f'{addr} ({size})', fontsize=9, fontweight='bold')
        ax.text(1.3, y + 0.3, f'{access} — {desc}', fontsize=8)
        y -= 1.6

    # Nota
    ax.text(5, 1.5, 'PRIVDEFENA=1: Modo privilegiado tiene acceso total.\n'
                     'Tareas (unprivileged) acceden perifericos protegidos via SVC.\n'
                     'Violacion → HardFault → tarea muere → sistema continua.',
            ha='center', fontsize=9, style='italic',
            bbox=dict(boxstyle='round', facecolor='#FFFDE7', edgecolor='#F57F17'))

    plt.tight_layout()
    plt.savefig('diagrama_mpu.png', dpi=150, bbox_inches='tight')
    plt.close()
    print("Generado: diagrama_mpu.png")


def diagram_flash_writeback():
    fig, ax = plt.subplots(1, 1, figsize=(12, 6))
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 6)
    ax.axis('off')
    ax.set_title('Write-Back Diferido: Page Cache → Flash Queue → Flash', fontsize=12, fontweight='bold')

    # Page Cache
    ax.add_patch(FancyBboxPatch((0.5, 3), 3, 2.5, boxstyle="round,pad=0.1",
                                facecolor='#E3F2FD', edgecolor='#1565C0', linewidth=2))
    ax.text(2, 5.2, 'Page Cache (RAM)', ha='center', fontsize=10, fontweight='bold')
    ax.text(2, 4.6, '6 frames × 64 bytes', ha='center', fontsize=8)
    ax.text(2, 4.1, 'LRU replacement', ha='center', fontsize=8)
    ax.text(2, 3.6, 'dirty bit por frame', ha='center', fontsize=8)
    ax.text(2, 3.2, '[SVC handler context]', ha='center', fontsize=7, color='red')

    # Flash Queue
    ax.add_patch(FancyBboxPatch((4.5, 3), 3, 2.5, boxstyle="round,pad=0.1",
                                facecolor='#FFF3E0', edgecolor='#E65100', linewidth=2))
    ax.text(6, 5.2, 'Flash Queue (RAM)', ha='center', fontsize=10, fontweight='bold')
    ax.text(6, 4.6, '8 entries × 64 bytes', ha='center', fontsize=8)
    ax.text(6, 4.1, 'Ring buffer FIFO', ha='center', fontsize=8)
    ax.text(6, 3.6, 'Productor: SVC', ha='center', fontsize=8)
    ax.text(6, 3.2, 'Consumidor: idle loop', ha='center', fontsize=7, color='green')

    # Flash
    ax.add_patch(FancyBboxPatch((8.5, 3), 3, 2.5, boxstyle="round,pad=0.1",
                                facecolor='#E8F5E9', edgecolor='#2E7D32', linewidth=2))
    ax.text(10, 5.2, 'Flash (PicoFS)', ha='center', fontsize=10, fontweight='bold')
    ax.text(10, 4.6, 'Persistente', ha='center', fontsize=8)
    ax.text(10, 4.1, 'multicore_lockout', ha='center', fontsize=8)
    ax.text(10, 3.6, 'disable interrupts', ha='center', fontsize=8)
    ax.text(10, 3.2, '[thread mode privilegiado]', ha='center', fontsize=7, color='green')

    # Arrows
    ax.annotate('', xy=(4.4, 4.2), xytext=(3.6, 4.2),
                arrowprops=dict(arrowstyle='->', lw=2, color='#1565C0'))
    ax.text(4, 4.5, 'evict/\nflush', ha='center', fontsize=7)

    ax.annotate('', xy=(8.4, 4.2), xytext=(7.6, 4.2),
                arrowprops=dict(arrowstyle='->', lw=2, color='#E65100'))
    ax.text(8, 4.5, 'idle loop\nprocess', ha='center', fontsize=7)

    # Explanation
    ax.text(6, 1.5, 'Evict y flush solo encolan datos (rapido, sin I/O).\n'
                     'El idle loop de main() drena la cola y escribe a Flash.\n'
                     'Idle loop corre en thread mode → multicore_lockout funciona → sin deadlock.',
            ha='center', fontsize=9, style='italic',
            bbox=dict(boxstyle='round', facecolor='#FFFDE7', edgecolor='#F57F17'))

    plt.tight_layout()
    plt.savefig('diagrama_writeback.png', dpi=150, bbox_inches='tight')
    plt.close()
    print("Generado: diagrama_writeback.png")


def diagram_ipc_flow():
    fig, ax = plt.subplots(1, 1, figsize=(12, 5))
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 5)
    ax.axis('off')
    ax.set_title('Flujo IPC: Productor-Consumidor por Colas de Mensajes', fontsize=12, fontweight='bold')

    # Productores
    producers = [
        ('sensor_task', 1, 4, '#E8F5E9'),
        ('boton IRQ', 1, 2.8, '#FFF3E0'),
        ('irrigation_task', 1, 1.6, '#E3F2FD'),
    ]
    for name, x, y, color in producers:
        ax.add_patch(FancyBboxPatch((x, y), 2.2, 0.8, boxstyle="round,pad=0.05",
                                    facecolor=color, edgecolor='black'))
        ax.text(x + 1.1, y + 0.4, name, ha='center', fontsize=8, fontweight='bold')

    # Colas
    queues = [
        ('irrigation_queue', 4.5, 3.4, '#FFECB3'),
        ('display_queue', 4.5, 2.2, '#FFECB3'),
        ('log_queue', 4.5, 1.0, '#FFECB3'),
    ]
    for name, x, y, color in queues:
        ax.add_patch(FancyBboxPatch((x, y), 2.5, 0.8, boxstyle="round,pad=0.05",
                                    facecolor=color, edgecolor='#F57F17', linewidth=1.5))
        ax.text(x + 1.25, y + 0.4, name, ha='center', fontsize=8)

    # Consumidores
    consumers = [
        ('irrigation_task', 8.5, 3.4, '#E3F2FD'),
        ('display_task', 8.5, 2.2, '#E3F2FD'),
        ('logger_task', 8.5, 1.0, '#E3F2FD'),
    ]
    for name, x, y, color in consumers:
        ax.add_patch(FancyBboxPatch((x, y), 2.2, 0.8, boxstyle="round,pad=0.05",
                                    facecolor=color, edgecolor='black'))
        ax.text(x + 1.1, y + 0.4, name, ha='center', fontsize=8, fontweight='bold')

    # Arrows productores -> colas
    arrows_prod = [
        (3.2, 4.4, 4.5, 3.8),
        (3.2, 3.2, 4.5, 3.8),
        (3.2, 2.0, 4.5, 2.6),
        (3.2, 2.0, 4.5, 1.4),
    ]
    for x1, y1, x2, y2 in arrows_prod:
        ax.annotate('', xy=(x2, y2), xytext=(x1, y1),
                    arrowprops=dict(arrowstyle='->', lw=1.2, color='gray'))

    # Arrows colas -> consumidores
    for _, x, y, _ in queues:
        ax.annotate('', xy=(8.5, y + 0.4), xytext=(x + 2.5, y + 0.4),
                    arrowprops=dict(arrowstyle='->', lw=1.5, color='#F57F17'))

    # Labels
    ax.text(3.5, 4.6, 'DRY/WET', fontsize=7, color='gray')
    ax.text(3.5, 3.4, 'MANUAL_TRIGGER', fontsize=7, color='gray')
    ax.text(3.5, 2.3, 'DISPLAY_TEXT', fontsize=7, color='gray')
    ax.text(3.5, 1.2, 'LOG_TEXT', fontsize=7, color='gray')

    plt.tight_layout()
    plt.savefig('diagrama_ipc.png', dpi=150, bbox_inches='tight')
    plt.close()
    print("Generado: diagrama_ipc.png")


if __name__ == '__main__':
    diagram_architecture()
    diagram_mpu()
    diagram_flash_writeback()
    diagram_ipc_flow()
    print("\nTodos los diagramas generados.")
