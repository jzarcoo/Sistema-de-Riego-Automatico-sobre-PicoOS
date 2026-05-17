.syntax unified
.cpu cortex-m0plus
.thumb

@ Smart wrapper for system calls
.global wrapper_svc
.type wrapper_svc, %function

wrapper_svc:
    @ The processor stores a special code in LR (EXC_RETURN)
    @ If bit 2 of LR is 1, we return to PSP. If 0, to MSP.
    @ We use this to know where the arguments are.

    mov r0, lr          @ Move LR to R0 for testing
    movs r1, #4         @ Mask for bit 2 (value 4)
    tst r0, r1          @ Test bit 2 of LR
    beq use_msp         @ If 0 (Z-flag set), use MSP

use_psp:
    mrs r0, psp         @ Load PSP address into R0 (Arg0 for C)
    b call_kernel       @ Jump to the call

use_msp:
    mrs r0, msp         @ Load MSP address into R0 (Arg0 for C)

call_kernel:
    mov r1, r7          @ Arg1 for C: Syscall ID (in r7)
    
    push {r4, lr}       @ Save r4 and lr (stack alignment)
    bl kernel_service   @ Jump to C kernel function
    pop {r4, pc}        @ Return
