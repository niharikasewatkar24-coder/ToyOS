# ToyOS x86-32 Console

A small 32-bit x86 operating system written in C and NASM assembly, with Ring 3 user mode, system calls, a scheduler, and IDE-backed persistent storage. It builds and runs natively on Windows or Linux; WSL and Docker are not required.

## Native Windows

Install LLVM, NASM, and QEMU from PowerShell:

```powershell
winget install -e --id LLVM.LLVM
winget install -e --id NASM.NASM
winget install -e --id SoftwareFreedomConservancy.QEMU
```

Restart PowerShell so the tools are on PATH. From the project folder, build and run ToyOS:

```powershell
.\run.bat
```

This builds `build\kernel.bin`, starts QEMU, and saves serial output to `performance.jsonl`. To build without starting QEMU, run .\rebuild_iso.bat. Despite its legacy name, the native Windows build creates a kernel binary, not an ISO. The Windows build uses Clang, NASM, and LLVM's `ld.lld` linker.

## Native Linux

On Ubuntu or Debian, install the build and run tools:

```bash
sudo apt update
sudo apt install clang lld nasm qemu-system-x86 make
```

From the project folder, build and run with serial capture:

```bash
make clean
make run
```

This writes `performance.jsonl` in the project folder. To build and boot the optional GRUB ISO, install `grub-pc-bin` and `xorriso`, then run `make boot_iso`.

## Performance logs and charts

The kernel writes newline-delimited JSON records to COM1. Normal log records include a PIT tick and uptime in milliseconds. Periodic `performance.sample` records include cumulative task switches, the running task, and physical pages used and total.

Install Matplotlib for the Python you will use. On Ubuntu or Debian:

```bash
sudo apt install python3-matplotlib
```

On Windows, install Python with the `py` launcher, then run:

```powershell
py -m pip install -r tools/requirements.txt
```

After running ToyOS and closing QEMU, create a chart:

```bash
python3 tools/plot_performance.py performance.jsonl -o performance.png
```

On Windows, use:

```powershell
py tools\plot_performance.py performance.jsonl -o performance.png
```

Compare two runs with:

```bash
python3 tools/compare_performance.py baseline.jsonl updated.jsonl -o comparison.png
```

The charts show scheduler switch rate and physical page usage over uptime. See [BASELINE_COMPARISON.md](BASELINE_COMPARISON.md) for the limits of the available before-and-after baseline.

## Project structure

`src/` contains kernel C and NASM sources. `tools/` contains the Windows build script and Python analysis scripts. `Makefile` provides native Linux build and run targets; `run.bat` builds and launches ToyOS on Windows.
