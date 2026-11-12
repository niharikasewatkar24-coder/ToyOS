param(
    [switch]$Run,
    [ValidateSet(0, 1)]
    [int]$PI = 1
)

$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')

function Resolve-Tool([string]$Name) {
    $tool = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $tool) {
        throw "Required tool '$Name' was not found on PATH. Install the Windows tools listed in README.md, then open a new terminal."
    }
    return $tool.Source
}

$clang = Resolve-Tool 'clang'
$linker = Resolve-Tool 'ld.lld'
$nasm = Resolve-Tool 'nasm'
$cSources = @(
    'src/c/entry.c',
    'src/c/kernel/idt.c',
    'src/c/kernel/gdt.c',
    'src/c/kernel/exception_handler.c',
    'src/c/kernel/interrupt_handler.c',
    'src/c/kernel/syscall.c',
    'src/c/drivers/keyboard/keyboard.c',
    'src/c/drivers/timer/timer.c',
    'src/c/drivers/serial_port/serial_port.c',
    'src/c/drivers/ide/ide.c',
    'src/c/memory/pmm.c',
    'src/c/memory/paging.c',
    'src/c/scheduler/scheduler.c',
    'src/c/sync/mutex.c'
)

$asmSources = @(
    'src/asm/boot.asm',
    'src/asm/low_level_functions.asm',
    'src/asm/ex_handlers.asm',
    'src/asm/int_handlers.asm',
    'src/asm/syscall.asm',
    'src/asm/user_program.asm'
)

$objects = @()

New-Item -ItemType Directory -Force -Path 'build' | Out-Null
foreach ($source in $asmSources) {
    $object = Join-Path 'build/asm' ([IO.Path]::GetFileNameWithoutExtension($source) + '.o')
    New-Item -ItemType Directory -Force -Path (Split-Path $object) | Out-Null
    Write-Host "Assembling $source"
    & $nasm -f elf32 $source -o $object
    if ($LASTEXITCODE -ne 0) { throw "NASM failed for $source" }
    $objects += $object
}

foreach ($source in $cSources) {
    $object = Join-Path 'build/kernel' (($source -replace '\.c$', '.o') -replace '/', [IO.Path]::DirectorySeparatorChar)
    New-Item -ItemType Directory -Force -Path (Split-Path $object) | Out-Null
    Write-Host "Compiling $source"
    & $clang '--target=i386-unknown-none-elf' -march=i386 -mno-sse -mno-sse2 -std=gnu11 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -Wall -Wextra "-DENABLE_PRIORITY_INHERITANCE=$PI" -c $source -o $object
    if ($LASTEXITCODE -ne 0) { throw "Clang failed for $source" }
    $objects += $object
}

Write-Host 'Linking build/kernel.bin'
& $linker -m elf_i386 -T link.ld @objects -o build/kernel.bin
if ($LASTEXITCODE -ne 0) { throw 'ELF link failed' }
Write-Host 'Build complete: build/kernel.bin'

if ($Run) {
    $qemu = Resolve-Tool 'qemu-system-i386'
    Write-Host 'Starting ToyOS in QEMU. Serial logs will be saved to performance.jsonl.'
    & $qemu -kernel build/kernel.bin -serial 'file:performance.jsonl'
    if ($LASTEXITCODE -ne 0) { throw 'QEMU exited with an error' }
}
