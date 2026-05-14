@echo off
REM Build the STM32L0 mini flashloader and emit Core/Inc/l0_ob_loader_blob.h
REM
REM Usage:  build.bat
REM Toolchain: STM32CubeCLT (or any arm-none-eabi-gcc on PATH)
REM
REM Outputs:
REM   build\l0_ob_loader.elf
REM   build\l0_ob_loader.bin
REM   ..\Core\Inc\l0_ob_loader_blob.h    (consumed by swd_host.c)

setlocal

set "GCC=C:\ST\STM32CubeCLT\GNU-tools-for-STM32\bin\arm-none-eabi-gcc.exe"
set "OBJCOPY=C:\ST\STM32CubeCLT\GNU-tools-for-STM32\bin\arm-none-eabi-objcopy.exe"
set "OBJDUMP=C:\ST\STM32CubeCLT\GNU-tools-for-STM32\bin\arm-none-eabi-objdump.exe"

if not exist "%GCC%" (
  echo Toolchain not found at "%GCC%". Edit GCC/OBJCOPY in this script.
  exit /b 1
)

if not exist build mkdir build

"%GCC%" ^
  -mcpu=cortex-m0plus -mthumb ^
  -nostdlib -nostartfiles -ffreestanding ^
  -fno-builtin -fno-common ^
  -Os -fno-toplevel-reorder ^
  -Wall -Wextra ^
  -T l0_ob_loader.ld ^
  -Wl,--build-id=none -Wl,-Map=build\l0_ob_loader.map ^
  -o build\l0_ob_loader.elf l0_ob_loader.c
if errorlevel 1 exit /b 1

"%OBJCOPY%" -O binary build\l0_ob_loader.elf build\l0_ob_loader.bin
if errorlevel 1 exit /b 1

"%OBJDUMP%" -d build\l0_ob_loader.elf > build\l0_ob_loader.dis
if errorlevel 1 exit /b 1

REM Emit the C header from the binary
python emit_blob_header.py build\l0_ob_loader.bin ..\Core\Inc\l0_ob_loader_blob.h
if errorlevel 1 exit /b 1

echo.
echo === Built loader ===
for %%I in (build\l0_ob_loader.bin) do echo Size: %%~zI bytes
echo Header: ..\Core\Inc\l0_ob_loader_blob.h
echo Disasm: build\l0_ob_loader.dis

endlocal
