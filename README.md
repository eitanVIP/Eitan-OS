# EitanOS

A 64-bit hobby operating system kernel for x86-64, built from scratch in C and NASM assembly. Boots via the [Limine](https://github.com/limine-bootloader/limine) bootloader (UEFI/BIOS) and runs in QEMU.

> **Note:** Some subsystems are still being migrated from the original 32-bit version and may not be fully functional.

---

## Images

<img height="500" alt="EitanOS kernel screenshot" src="https://github.com/user-attachments/assets/62456b29-135a-404b-bacf-dd9d38907ef1" />

---

## Features

**Memory Management**
- Bitmap-based Physical Memory Manager (PMM)
- 4-level paging Virtual Memory Manager (VMM), and per process page table loading
- Kernel mapped at `0xffffffff80000000`, HHDM at `0xffff800000000000`
- Heap allocator built on top of the VMM

**Interrupts and Syscalls**
- Custom 64-bit IDT with handlers for all 32 CPU exceptions and 16 hardware IRQs
- Syscall interface via `int 0x80`, dispatched inside the IRQ/exception handler

**Process Scheduler**
- Preemptive round-robin scheduler driven by the PIT timer (IRQ0)
- Per-process virtual address spaces, each with its own PML4 cloned from the kernel
- Unix-style signal definitions (`SIG_KILL`, `SIG_TERM`, `SIG_SEGV`, etc.)

**Program Loader**
- ELF32 and ELF64 loader: Loads an ELF executable and maps memory for it, then passes the program to the scheduler
- Programs are stored on disk and launched at runtime via the `run` syscall

**Filesystem**
- Custom filesystem with sector-level disk I/O
- Based on a file table
- Supports directories

**Shell**
- Userspace shell compiled separately and loaded as an ELF64 process
- Supports many bash commands

**Display**
- Limine framebuffer-based screen driver with scrollable output
- PSF font rendering (Zap font, pre-compiled into the kernel image)

---

## Project Structure

```
src/
├── memory/                   # Memory management
├── process/                  # Process scheduling, loading, etc...
├── util/                     # Utility files like stdint, string, etc...
└── compiled_programs/        # Pre-compiled userspace programs
└── compiled_fonts/           # Pre-compiled userspace programs

programs/                     # Userspace programs that will be compiled into binary data in a C file and injected into the kernel

fonts/                        # Fonts that will be loaded

linker.ld                     # Kernel linker script
build_run.sh                  # One-shot build + QEMU launch script
```

---

## Building and Running

**Dependencies**

- `gcc` (x86-64)
- `nasm`
- `cmake`
- `ld` (binutils)
- `xorriso`
- `qemu-system-x86_64`
- OVMF firmware (`/usr/share/ovmf/OVMF.fd`)

**Build and run**

```bash
./build_run.sh
```

This will compile userspace programs and fonts, build all kernel sources via CMake, link the kernel ELF, assemble a bootable ISO with Limine, and launch QEMU with UEFI firmware.

QEMU is started with `-S -s` (paused, GDB server on port 1234). To attach GDB:

```bash
gdb -ex "target remote :1234" build/kernel.elf
```

---

## License

Apache 2.0 — see [LICENSE](LICENSE).
