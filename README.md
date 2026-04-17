# STM32G0 SWD Host for STM32F103

This repository contains an STM32G0-based SWD host example that talks to an STM32F103 target through bit-banged SWD.

The project was tuned against real ST-LINK logic captures and now supports:

- Stable SWD connect and memory read
- Reading 64 bytes from `0x08000000`
- STM32F103 RDP Level `0 -> 1`
- STM32F103 RDP Level `1 -> 0`

## Project Layout

- `STM32G0_SWD_Host/`
  Main STM32CubeIDE project

Important source files:

- `STM32G0_SWD_Host/Core/Src/swd_host.c`
- `STM32G0_SWD_Host/Core/Inc/swd_host.h`
- `STM32G0_SWD_Host/Core/Src/main.c`

## Hardware Setup

Host MCU:

- STM32G0

Target MCU:

- STM32F103

Main signal mapping:

- `PA0` -> target `SWDIO`
- `PA1` -> target `SWCLK`
- `PA4` -> target `nRESET`
- Common `GND`

## Current Example Behavior

The example in `main.c` does the following:

1. Connects to the STM32F103 target over SWD
2. Waits for the target to stabilize
3. Reads 64 bytes starting at `0x08000000`
4. Optionally changes the RDP level

The read buffer is:

- `flash_data[64]`

## RDP Control

RDP behavior is controlled in `STM32G0_SWD_Host/Core/Src/main.c` with:

```c
#define TARGET_RDP_ACTION_NONE      (0U)
#define TARGET_RDP_ACTION_SET_L1    (1U)
#define TARGET_RDP_ACTION_SET_L0    (2U)
#define TARGET_RDP_ACTION           TARGET_RDP_ACTION_NONE
```

Use:

- `TARGET_RDP_ACTION_NONE`
  No RDP change
- `TARGET_RDP_ACTION_SET_L1`
  Change STM32F103 from RDP Level 0 to Level 1
- `TARGET_RDP_ACTION_SET_L0`
  Change STM32F103 from RDP Level 1 to Level 0

Notes:

- `Level 1 -> Level 0` triggers a target flash mass erase on STM32F103
- `Level 1 -> Level 0` requires a more conservative reconnect sequence than normal memory access

## Implementation Notes

The working implementation depends on a few important behaviors in `swd_host.c`:

- SWD transactions use explicit idle handling after transfers
- Connect timing was adjusted to better match successful ST-LINK behavior
- `RDP Level 1` handling does not rely on option-byte reads being available
- `RDP Level 1 -> Level 0` uses:
  - reconnect under reset
  - very low SWD speed
  - longer timeout while programming the RDP option byte
  - longer reset/reload delay after regression to Level 0

## Why the RDP Level 1 -> 0 Path Needed Special Handling

Normal SWD memory access can succeed while flash register access still fails under RDP Level 1.

The final working solution was to reconnect using:

- `SWD_HOST_CONNECT_UNDER_RESET`
- `SWD_HOST_SPEED_VERY_LOW`

before unlocking the STM32F103 flash interface for the RDP regression sequence.

## Default State

The repository is left in a safe default configuration:

- 64-byte flash read enabled
- RDP change disabled by default

To test RDP operations, change `TARGET_RDP_ACTION` explicitly before building.

## Status

Validated behaviors:

- SWD connect
- Flash read from `0x08000000`
- RDP Level 0 to Level 1
- RDP Level 1 to Level 0

