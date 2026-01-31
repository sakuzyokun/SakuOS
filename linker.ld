ENTRY(_start)

SECTIONS {
    . = 0;

    .multiboot ALIGN(4) : {
        KEEP(*(.multiboot))
    }

    . = 1M;

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(.bss*)
    }
}

