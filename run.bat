@echo off

:: First check if docker is running
docker ps >NUL 2>NUL
if %errorlevel% neq 0 (
  echo.
  echo Docker engine is not running! Run Docker Desktop application
  echo.
  exit /b
)

:: Check if local console-build image exists, build if not
docker image inspect console-build:latest >NUL 2>NUL
if %errorlevel% neq 0 (
  echo Building local console-build image from Dockerfile...
  docker build -t console-build:latest .
  if %errorlevel% neq 0 (
    echo.
    echo Failed to build Docker image
    echo.
    exit /b
  )
)

:: Check if console-build container exists and whether one should be created or started
docker container inspect console-build >NUL 2>NUL
if %errorlevel% neq 0 (
  echo Creating a new container from local image. Dir %CD% will be mounted to /src
  docker run -i -d --name console-build -v %CD%:/src console-build:latest
) else (
  docker start console-build >NUL
)

:: Building kernel
echo Building kernel in the console-build container
docker exec console-build bash -l -c "cd /src && make"
if %errorlevel% neq 0 (
  echo.
  echo Compilation failed
  echo.
  exit /b
)

:: Check if qemu is available in PATH
where qemu-system-i386 >NUL 2>NUL
if %errorlevel% == 0 (
    :: Running kernel is qemu emulator
    qemu-system-i386 -kernel build/kernel.bin
    exit /b 0
)

:: Try to look into program files to see if it's in its default location
SET qemu_path_candidate=C:\Program Files\qemu\qemu-system-i386.exe

:: qemu-system-i386 -kernel build/kernel.bin
if exist "%qemu_path_candidate%" (
    :: Running kernel is qemu emulator
    "%qemu_path_candidate%" -kernel build/kernel.bin
) else (
    echo .
    echo "Cannot find qemu. Have you installed it?"
    echo .
    exit /b
)
