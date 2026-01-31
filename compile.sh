gcc -m32 -ffreestanding -fno-pic -fno-stack-protector     -fno-asynchronous-unwind-tables     -c kernel.c -o build/kernel.o
ld -m elf_i386 -nostdlib -T linker.ld   build/multiboot.o build/start.o build/kernel.o   -o build/kernel.bin
cp build/kernel.bin iso/boot/kernel.bin
grub-mkrescue -o SakuOS.iso iso
qemu-system-i386 -cdrom SakuOS.iso
