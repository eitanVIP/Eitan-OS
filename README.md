# EitanOS

A 64-bit hobby operating system kernel for x86-64, built from scratch in C and NASM assembly. Boots via the [Limine](https://github.com/limine-bootloader/limine) bootloader (UEFI/BIOS) and runs in QEMU.

> **Note:** Some subsystems are still being migrated from the original 32-bit version and may not be fully functional.

---

## Features

**Memory Management**
- Bitmap-based Physical Memory Manager (PMM) backed by the Limine memory map
- 4-level paging Virtual Memory Manager (VMM) with `vmm_map_page`, `vmm_alloc`, `vmm_free`, `vmm_virt_to_phys`, and per-CPU page table loading
- Kernel mapped at `0xffffffff80000000`, HHDM at `0xffff800000000000`
- Heap allocator built on top of the VMM

**Interrupts and Syscalls**
- Custom 64-bit IDT with handlers for all 32 CPU exceptions and 16 hardware IRQs
- Syscall interface via `int 0x80`, dispatched inside the IRQ/exception handler
- Supported syscalls: `exit`, `run`, `kill`, `malloc`, `free`, `read_keyboard`, `print`, `clear_screen`, `read_file`, `write_file`

**Process Scheduler**
- Preemptive round-robin scheduler driven by the PIT timer (IRQ0)
- Per-process virtual address spaces, each with its own PML4 cloned from the kernel
- Unix-style signal definitions (`SIG_KILL`, `SIG_TERM`, `SIG_SEGV`, etc.)

**Program Loader**
- ELF32 loader: parses the ELF header and program headers, maps loadable segments into the process address space, and hands off to the scheduler
- Programs are stored on disk and launched at runtime via the `run` syscall

**Filesystem**
- Custom filesystem with sector-level disk I/O
- Supports: `read_file`, `write_file`, `delete_file`, `list_files`, `list_dirs`

**Shell**
- Userspace shell compiled separately and loaded as an ELF32 process
- Supports: `ls`, `cd`, `cat`, `touch`, `write`, `rm`, `echo`, `clear`, `man`
- Can launch other ELF programs from disk using the `run` syscall
- Uses its own heap (via `malloc`/`free` syscalls) and reads keyboard input scancode by scancode

**Display**
- Limine framebuffer-based screen driver with scrollable output
- PSF font rendering (Zap font, pre-compiled into the kernel image)

---

## Project Structure

```
src/
├── kernel.c                  # Kernel entry point, init sequence
├── main_asm.S                # ISR/IRQ stubs, GDT helpers, SSE setup
├── screen.{c,h}              # Framebuffer display + scroll
├── VGA_screen.{c,h}          # (Legacy) VGA text mode driver
├── gdt.{c,h}                 # GDT + TSS setup
├── filesystem.{c,h}          # Custom disk filesystem
├── memory/
│   ├── pmm.{c,h}             # Physical Memory Manager
│   ├── vmm.{c,h}             # Virtual Memory Manager (4-level paging)
│   └── allocator.{c,h}       # Heap allocator
├── process/
│   ├── interrupts.{c,h}      # IDT init, exception/IRQ dispatch, syscall handler
│   ├── process_scheduler.{c,h}
│   └── program_loader.{c,h}  # ELF32 loader
├── util/
│   ├── io.{c,h}              # Port I/O, keyboard
│   ├── string.{c,h}          # String utilities
│   ├── panic.{c,h}           # Kernel panic
│   ├── limine.h              # Limine protocol types
│   ├── stdint.h              # Type definitions
│   └── util.{c,h}            # Misc helpers
└── compiled_programs/        # Pre-compiled userspace programs (shell, test)

programs/
├── shell/                    # Shell source (compiled separately, loaded as ELF32)
└── test/                     # Test program source

fonts/
└── zap.psf                   # PSF bitmap font (compiled into the kernel)

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

## Memory Layout

| Region | Address |
|---|---|
| Kernel ELF | `0xffffffff80000000` |
| HHDM (physical memory view) | `0xffff800000000000` |
| User processes | Lower half (per-process PML4) |
| Process stack | `0x00007fffffffffff` (grows down) |

---

## License

Apache 2.0 — see [LICENSE](LICENSE).
