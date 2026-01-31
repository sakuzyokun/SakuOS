global _start
extern kernel_main

section .text
_start:
    cli

    mov esp, stack_top   ; スタック初期化
    call kernel_main

.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384           ; 16KB stack
stack_top:

global outb
outb:
    mov dx, [esp+4]
    mov al, [esp+8]
    out dx, al
    ret
