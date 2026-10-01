# OS x86-32 Console

A simple x86-32 console operating system built with assembly and C.

## Prerequisites

- **WSL (Windows Subsystem for Linux)** - For building the kernel
- **Docker Desktop** - For running the OS
- **QEMU** - Installed on Windows

## Building (via WSL)

1. Open WSL and navigate to the project directory.

2. Install build dependencies:

```bash
sudo apt update
sudo apt install build-essential nasm grub-pc-bin xorriso qemu-system-i386 gcc-multilib make
```

3. Build the kernel:

```bash
make boot_iso
```

## Running (with Docker)

1. Ensure Docker Desktop is running

2. Run the batch script:

```batch
run.bat
```

3. Update code inside docker container:

docker exec -ti console-build bash
cd /src
make boot_iso

The script builds using Docker and runs QEMU automatically.

Or run QEMU manually:

```batch
qemu-system-i386 -kernel build/kernel.bin
```

## Project Structure

```
os-x86_32-console/
├── src/
│   ├── asm/          # Assembly source files
│   └── c/            # C source files
│       ├── drivers/
│       │   ├── keyboard/
│       │   ├── serial_port/
│       │   └── timer/
│       └── kernel/
├── iso/              # ISO boot files
├── build/            # Build output
├── Makefile          # Build configuration
├── link.ld           # Linker script
├── run.bat           # Windows build/run script
└── rebuild_iso.bat   # Compile code
```
