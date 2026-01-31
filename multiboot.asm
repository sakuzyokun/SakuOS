section .multiboot
align 4
    dd 0x1BADB002        ; magic
    dd 0x00000000        ; flags
    dd -(0x1BADB002)     ; checksum

