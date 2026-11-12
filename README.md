# ToyOS — x86-32 Operating System

ToyOS is a small 32-bit x86 operating system written in C and NASM assembly. It includes kernel initialization, protected-mode structures, memory management, task scheduling, synchronization, Ring 3 user mode, system calls, and IDE-backed persistent filesystem storage.

It supports native Windows and Linux development. WSL and Docker are not required.

## Features

- **Kernel and CPU setup:** Global Descriptor Table (GDT), Interrupt Descriptor Table (IDT), exception handling, interrupt handling, and low-level assembly routines.
- **Memory management:** Physical Memory Manager (PMM) and paging.
- **Scheduling:** Task creation, context switching, idle task handling, and Multilevel Feedback Queue (MLFQ) scheduling.
- **Synchronization:** Mutexes, task blocking and wake-up, and priority inheritance.
- **User mode:** Ring 3 execution and system-call support.
- **Drivers:** Keyboard, PIT timer, serial port, and IDE.
- **Persistent storage:** IDE-backed filesystem operations and persistent file content.
- **Diagnostics:** Structured JSON serial logs and periodic performance samples.

## Requirements

### Windows

Install LLVM, NASM, and QEMU from PowerShell:

```powershell
winget install -e --id LLVM.LLVM
winget install -e --id NASM.NASM
winget install -e --id SoftwareFreedomConservancy.QEMU
```

Restart PowerShell after installation so the tools are available on PATH.

The Windows build uses:

- Clang (`clang`)
- LLVM linker (`ld.lld`)
- NASM (`nasm`)
- QEMU (`qemu-system-i386`) for running the kernel

### Linux

On Ubuntu or Debian, install the build and emulator tools:

```bash
sudo apt update
sudo apt install clang lld nasm qemu-system-x86 make
```

## Build and Run

### Native Windows

Open PowerShell in the project directory.

Build the kernel without launching QEMU:

```powershell
.\rebuild_iso.bat
```

Despite its legacy name, this script builds `build\kernel.bin`; it does not create an ISO.

Build and launch ToyOS in QEMU:

```powershell
.\run.bat
```

Serial output is saved to `performance.jsonl` in the project directory.

Priority Inheritance (PI) can be configured in the Windows build script. PI is enabled by default (PI=1).

Build with PI disabled:

```powershell
.\tools\build.ps1 -PI 0
```

Build explicitly with PI enabled:

```powershell
.\tools\build.ps1 -PI 1
```

Run the PI-enabled configuration in QEMU:

```powershell
.\tools\build.ps1 -PI 1 -Run
```

Run the PI-disabled configuration in QEMU:

```powershell
.\tools\build.ps1 -PI 0 -Run
```

The legacy run.bat and rebuild_iso.bat wrappers use the default PI-enabled configuration.

### Native Linux

Build the kernel:

```bash
make
```

Run ToyOS in QEMU:

```bash
make run
```

Serial output is saved to `performance.jsonl`.

## Optional GRUB ISO

The GRUB ISO workflow is available through the Linux Makefile.

On Ubuntu or Debian, install the additional tools:

```bash
sudo apt install grub-pc-bin xorriso
```

Build the ISO:

```bash
make iso
```

Boot it in QEMU:

```bash
make boot_iso
```

The generated ISO is located at:

```text
build/toyos.iso
```

The GRUB ISO workflow is separate from the native Windows kernel build.

## Performance Logs and Charts

ToyOS writes newline-delimited JSON records to the serial port (COM1).

Normal kernel log records include:

- PIT tick
- Uptime in milliseconds
- Log level and event message

Periodic `performance.sample` records include:

- Uptime
- Cumulative task switches
- Currently running task
- Physical pages used and total pages

### Install Python dependencies

On Windows:

```powershell
py -m pip install -r tools/requirements.txt
```

On Ubuntu or Debian:

```bash
sudo apt install python3-matplotlib
```

### Generate a performance chart

After running ToyOS and closing QEMU:

Windows:

```powershell
py tools\plot_performance.py performance.jsonl -o performance.png
```

Linux:

```bash
python3 tools/plot_performance.py performance.jsonl -o performance.png
```

### Compare two runs

```bash
python3 tools/compare_performance.py baseline.jsonl updated.jsonl -o comparison.png
```

The charts show scheduler switch rate and physical page usage over uptime.

See [BASELINE_COMPARISON.md](BASELINE_COMPARISON.md) for the existing instrumentation comparison and its limitations. Improved monitoring resolution does not, by itself, demonstrate improved kernel performance.

## Project Structure

```text
src/
├── asm/              Boot, interrupt, syscall, and user-program assembly
└── c/
    ├── drivers/      IDE, keyboard, serial, and timer drivers
    ├── kernel/       GDT, IDT, exceptions, interrupts, and syscalls
    ├── memory/       Physical memory management and paging
    ├── scheduler/   Task scheduling and context switching
    ├── sync/         Mutexes and synchronization
    └── tests/        Scheduler test sources

tools/                Windows build and Python analysis scripts
iso/                  GRUB configuration for the optional ISO
Makefile              Linux build, run, and ISO targets
run.bat               Windows build and QEMU launch
rebuild_iso.bat       Windows kernel build
```

## Verification and Testing

The following development scenarios have been exercised in QEMU:

- Successful kernel compilation and diskless boot.
- Kernel initialization through memory management and paging.
- Ring 3 user-program execution and return to the kernel.
- Scheduler task creation, blocking, wake-up, and termination.
- An observed priority-inheritance and priority-restoration scenario.
- MLFQ queue demotion and aging behavior.
### Filesystem Verification

Filesystem checks are separated by the level of behavior they demonstrate.

**Boot and initialization checks**

Earlier filesystem tests verified that ToyOS could boot with an IDE-backed image and report `FILESYSTEM LOADED FROM DISK`. Other tests exercised reading file content already present in an image. A successful initialization message alone does not prove that a new file or edited content was saved successfully.

**Runtime persistence test — verified 12 November 2026**

A separate interactive QEMU test used `build/filesystem-runtime-test.img`, an isolated copy of an existing test image. The following sequence was completed:

1. Booted ToyOS with the IDE disk image attached.
2. Listed existing files and read previously stored content.
3. Created `runtime-write-check` and confirmed it appeared in the file listing.
4. Edited the file with the two-line text `toyos persistent write verification 2026` followed by `abcd`, and received the `File saved` confirmation.
5. Inspected the disk image directly and verified that both the filename and exact content were present in the expected file-table slot.
6. Rebooted ToyOS using the same disk image.
7. Listed the file and successfully read the saved content after reboot.

The fourth-boot serial log also records `FILESYSTEM LOADED FROM DISK` at tick 0. The interactive read-back was observed in the QEMU shell; it is not claimed to be recorded as a structured serial event.

This verifies the tested create, edit, disk-save, reboot, and content-recovery path. It is a targeted runtime test, not proof of exhaustive filesystem correctness, crash consistency, or reliability under all disk and failure conditions.

These results validate specific development scenarios. They do not establish exhaustive correctness of every kernel subsystem or every possible scheduling, synchronization, and filesystem condition.

Verification note: The GRUB ISO was successfully built, passed GRUB Multiboot validation, and booted in QEMU using the Linux/WSL Makefile workflow. The native Windows build currently produces a kernel binary; it does not create the GRUB ISO.
