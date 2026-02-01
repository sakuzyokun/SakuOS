# SakuOS
Delete? No — Reborn.  
A lightweight operating system that gives your old PC one last chance before it goes to the trash.

SakuOS is a minimal and experimental OS project built around the concept of  
**“an operating system that starts from what you don’t need.”**  
Currently **CLI-only**, with a GUI under development.

We’re looking for contributors who want to build this OS together.

---

## 🌱 Concept
- A lightweight OS designed to revive old or unused PCs  
- The desktop’s main character is the **Trash Bin**  
- The system aims to learn from deleted files and uninstalled apps  
  to highlight “what you truly need”  
- Still highly experimental — specifications may change significantly

---

## 🧪 Current Status (Pre-alpha 0.1 Preview)
- Boots into CLI  
- Bootloader → Kernel startup works  
- GUI not implemented yet  
- Installation not supported (live boot only)

---

## 📦 Build Instructions

### Requirements
- `gcc` / `ld`
- `nasm`
- `grub-mkrescue`
- `xorriso`
- `qemu-system-x86_64`

### Build
```bash
./compile.sh
```

### Run with QEMU
```bash
qemu-system-x86_64 -cdrom SakuOS.iso
```

---

## 📁 Directory Structure
```text
SakuOS/
├── src/
│   ├── kernel.c
│   ├── start.asm
│   ├── multiboot.asm
│   ├── compile.sh
│   └── linker.ld
├── LICENSE
└── README.md
```

---

## 💾 Download
ISO images are available on GitHub Releases.

---

## 🛠 Development Status
- Implementing memory management  
- Preparing interrupt handling (IDT / PIC)  
- Designing the foundation of the GUI (drawing, window management)  
- Prototyping the “Trash Bin–centric UI” concept  

---

## 🤝 Contributing
Bug reports, feature suggestions, documentation fixes — everything is welcome.  
Feel free to open Issues or Pull Requests.

---

## 📜 License
This project is released under the **Saku-kun Software License 1.1**.

---

## 🌐 Official Website
https://sakuzyo.net/os/SakuOS/en
