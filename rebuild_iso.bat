@echo off

:: First check if docker is running
docker ps >NUL 2>NUL
if %errorlevel% neq 0 (
  echo.
  echo Docker engine is not running! Run Docker Desktop application
  echo.
  exit /b
)

:: Check if console-build container exists, create if not
docker container inspect console-build >NUL 2>NUL
if %errorlevel% neq 0 (
  echo Container console-build not found. Building local image first...
  
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
  
  echo Creating console-build container. Dir %CD% will be mounted to /src
  docker run -i -d --name console-build -v %CD%:/src console-build:latest
  if %errorlevel% neq 0 (
    echo.
    echo Failed to create container
    echo.
    exit /b
  )
) else (
  docker start console-build >NUL
)

:: Recompile code and build ISO inside docker container
echo Recompiling and building ISO in the console-build container...
docker exec console-build bash -c "cd /src && make boot_iso"
if %errorlevel% neq 0 (
  echo.
  echo Build failed
  echo.
  exit /b
)

echo.
echo Build completed successfully!
echo ISO created at: build\kernel.iso
echo.

