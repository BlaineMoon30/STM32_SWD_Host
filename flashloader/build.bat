@echo off
REM Build the STM32 mini flashloaders and emit their Core/Inc/*_blob.h headers.
REM
REM Usage:  build.bat
REM Toolchain: STM32CubeCLT (or any arm-none-eabi-gcc on PATH)
REM
REM Outputs:
REM   build\l0_ob_loader.elf / .bin / .dis   + ..\Core\Inc\l0_ob_loader_blob.h
REM   build\l1_ob_loader.elf / .bin / .dis   + ..\Core\Inc\l1_ob_loader_blob.h
REM   build\g0_ob_loader.elf / .bin / .dis   + ..\Core\Inc\g0_ob_loader_blob.h
REM   build\g4_ob_loader.elf / .bin / .dis   + ..\Core\Inc\g4_ob_loader_blob.h
REM   build\f0_ob_loader.elf / .bin / .dis   + ..\Core\Inc\f0_ob_loader_blob.h
REM   build\u0_ob_loader.elf / .bin / .dis   + ..\Core\Inc\u0_ob_loader_blob.h
REM   build\l4_ob_loader.elf / .bin / .dis   + ..\Core\Inc\l4_ob_loader_blob.h
REM   (all *_blob.h are consumed by swd_host.c)

setlocal

set "GCC=C:\ST\STM32CubeCLT_1.21.0\GNU-tools-for-STM32\bin\arm-none-eabi-gcc.exe"
set "OBJCOPY=C:\ST\STM32CubeCLT_1.21.0\GNU-tools-for-STM32\bin\arm-none-eabi-objcopy.exe"
set "OBJDUMP=C:\ST\STM32CubeCLT_1.21.0\GNU-tools-for-STM32\bin\arm-none-eabi-objdump.exe"

if not exist "%GCC%" (
  echo Toolchain not found at "%GCC%". Edit GCC/OBJCOPY in this script.
  exit /b 1
)

if not exist build mkdir build

REM ----- STM32L0 loader (Cortex-M0+) -----
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

py emit_blob_header.py build\l0_ob_loader.bin ..\Core\Inc\l0_ob_loader_blob.h l0_ob_loader_blob L0_OB_LOADER
if errorlevel 1 exit /b 1

REM ----- STM32L1 loader (Cortex-M3) — built as M0+ subset, runs on M3 -----
"%GCC%" ^
  -mcpu=cortex-m0plus -mthumb ^
  -nostdlib -nostartfiles -ffreestanding ^
  -fno-builtin -fno-common ^
  -Os -fno-toplevel-reorder ^
  -Wall -Wextra ^
  -T l1_ob_loader.ld ^
  -Wl,--build-id=none -Wl,-Map=build\l1_ob_loader.map ^
  -o build\l1_ob_loader.elf l1_ob_loader.c
if errorlevel 1 exit /b 1

"%OBJCOPY%" -O binary build\l1_ob_loader.elf build\l1_ob_loader.bin
if errorlevel 1 exit /b 1

"%OBJDUMP%" -d build\l1_ob_loader.elf > build\l1_ob_loader.dis
if errorlevel 1 exit /b 1

py emit_blob_header.py build\l1_ob_loader.bin ..\Core\Inc\l1_ob_loader_blob.h l1_ob_loader_blob L1_OB_LOADER
if errorlevel 1 exit /b 1

REM ----- STM32G0 loader (Cortex-M0+) -----
"%GCC%" ^
  -mcpu=cortex-m0plus -mthumb ^
  -nostdlib -nostartfiles -ffreestanding ^
  -fno-builtin -fno-common ^
  -Os -fno-toplevel-reorder ^
  -Wall -Wextra ^
  -T g0_ob_loader.ld ^
  -Wl,--build-id=none -Wl,-Map=build\g0_ob_loader.map ^
  -o build\g0_ob_loader.elf g0_ob_loader.c
if errorlevel 1 exit /b 1

"%OBJCOPY%" -O binary build\g0_ob_loader.elf build\g0_ob_loader.bin
if errorlevel 1 exit /b 1

"%OBJDUMP%" -d build\g0_ob_loader.elf > build\g0_ob_loader.dis
if errorlevel 1 exit /b 1

py emit_blob_header.py build\g0_ob_loader.bin ..\Core\Inc\g0_ob_loader_blob.h g0_ob_loader_blob G0_OB_LOADER
if errorlevel 1 exit /b 1

REM ----- STM32G4 loader (Cortex-M4) — built as M0+ subset, runs on M4 -----
"%GCC%" ^
  -mcpu=cortex-m0plus -mthumb ^
  -nostdlib -nostartfiles -ffreestanding ^
  -fno-builtin -fno-common ^
  -Os -fno-toplevel-reorder ^
  -Wall -Wextra ^
  -T g4_ob_loader.ld ^
  -Wl,--build-id=none -Wl,-Map=build\g4_ob_loader.map ^
  -o build\g4_ob_loader.elf g4_ob_loader.c
if errorlevel 1 exit /b 1

"%OBJCOPY%" -O binary build\g4_ob_loader.elf build\g4_ob_loader.bin
if errorlevel 1 exit /b 1

"%OBJDUMP%" -d build\g4_ob_loader.elf > build\g4_ob_loader.dis
if errorlevel 1 exit /b 1

py emit_blob_header.py build\g4_ob_loader.bin ..\Core\Inc\g4_ob_loader_blob.h g4_ob_loader_blob G4_OB_LOADER
if errorlevel 1 exit /b 1

REM ----- STM32F0 loader (Cortex-M0) — FPEC controller, like STM32F1 -----
"%GCC%" ^
  -mcpu=cortex-m0 -mthumb ^
  -nostdlib -nostartfiles -ffreestanding ^
  -fno-builtin -fno-common ^
  -Os -fno-toplevel-reorder ^
  -Wall -Wextra ^
  -T f0_ob_loader.ld ^
  -Wl,--build-id=none -Wl,-Map=build\f0_ob_loader.map ^
  -o build\f0_ob_loader.elf f0_ob_loader.c
if errorlevel 1 exit /b 1

"%OBJCOPY%" -O binary build\f0_ob_loader.elf build\f0_ob_loader.bin
if errorlevel 1 exit /b 1

"%OBJDUMP%" -d build\f0_ob_loader.elf > build\f0_ob_loader.dis
if errorlevel 1 exit /b 1

py emit_blob_header.py build\f0_ob_loader.bin ..\Core\Inc\f0_ob_loader_blob.h f0_ob_loader_blob F0_OB_LOADER
if errorlevel 1 exit /b 1

REM ----- STM32U0 loader (Cortex-M0+) — G0/G4/L4-style controller -----
"%GCC%" ^
  -mcpu=cortex-m0plus -mthumb ^
  -nostdlib -nostartfiles -ffreestanding ^
  -fno-builtin -fno-common ^
  -Os -fno-toplevel-reorder ^
  -Wall -Wextra ^
  -T u0_ob_loader.ld ^
  -Wl,--build-id=none -Wl,-Map=build\u0_ob_loader.map ^
  -o build\u0_ob_loader.elf u0_ob_loader.c
if errorlevel 1 exit /b 1

"%OBJCOPY%" -O binary build\u0_ob_loader.elf build\u0_ob_loader.bin
if errorlevel 1 exit /b 1

"%OBJDUMP%" -d build\u0_ob_loader.elf > build\u0_ob_loader.dis
if errorlevel 1 exit /b 1

py emit_blob_header.py build\u0_ob_loader.bin ..\Core\Inc\u0_ob_loader_blob.h u0_ob_loader_blob U0_OB_LOADER
if errorlevel 1 exit /b 1

REM ----- STM32L4 loader (Cortex-M4) — built as M0+ subset, runs on M4 -----
"%GCC%" ^
  -mcpu=cortex-m0plus -mthumb ^
  -nostdlib -nostartfiles -ffreestanding ^
  -fno-builtin -fno-common ^
  -Os -fno-toplevel-reorder ^
  -Wall -Wextra ^
  -T l4_ob_loader.ld ^
  -Wl,--build-id=none -Wl,-Map=build\l4_ob_loader.map ^
  -o build\l4_ob_loader.elf l4_ob_loader.c
if errorlevel 1 exit /b 1

"%OBJCOPY%" -O binary build\l4_ob_loader.elf build\l4_ob_loader.bin
if errorlevel 1 exit /b 1

"%OBJDUMP%" -d build\l4_ob_loader.elf > build\l4_ob_loader.dis
if errorlevel 1 exit /b 1

py emit_blob_header.py build\l4_ob_loader.bin ..\Core\Inc\l4_ob_loader_blob.h l4_ob_loader_blob L4_OB_LOADER
if errorlevel 1 exit /b 1

echo.
echo === Built loaders ===
for %%I in (build\l0_ob_loader.bin) do echo L0 size: %%~zI bytes
for %%I in (build\l1_ob_loader.bin) do echo L1 size: %%~zI bytes
for %%I in (build\g0_ob_loader.bin) do echo G0 size: %%~zI bytes
for %%I in (build\g4_ob_loader.bin) do echo G4 size: %%~zI bytes
for %%I in (build\f0_ob_loader.bin) do echo F0 size: %%~zI bytes
for %%I in (build\u0_ob_loader.bin) do echo U0 size: %%~zI bytes
for %%I in (build\l4_ob_loader.bin) do echo L4 size: %%~zI bytes
echo Headers in ..\Core\Inc\: l0_ob_loader_blob.h l1_ob_loader_blob.h g0_ob_loader_blob.h g4_ob_loader_blob.h f0_ob_loader_blob.h u0_ob_loader_blob.h l4_ob_loader_blob.h

endlocal
