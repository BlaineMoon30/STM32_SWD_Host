# STM32G0 SWD Host for STM32F103

This repository contains an STM32G0-based SWD host example that talks to an STM32F103 target through bit-banged SWD.

The project was tuned against real ST-LINK logic captures and now supports:

- Stable SWD connect and memory read
- Reading 64 bytes from `0x08000000`
- STM32F103 RDP Level `0 -> 1`
- STM32F103 RDP Level `1 -> 0`
- STM32L0 RDP Level `0 -> 1` / `1 -> 0` (via on-target option-byte flashloader)
- STM32L1 RDP Level `0 -> 1` / `1 -> 0` (via on-target option-byte flashloader)
- STM32G0 RDP Level `0 -> 1` / `1 -> 0` (via on-target option-byte flashloader)
- STM32G4 RDP Level `0 -> 1` / `1 -> 0` (via on-target option-byte flashloader)

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

## STM32L0 / L1 / G0 / G4 RDP Control

These families do not allow the RDP option byte to be flipped over the debug
MEM-AP while the target is under RDP Level 1. Instead, the host uploads a tiny
option-byte flashloader into the target's SRAM and runs it on the target CPU —
writes issued by the target's own core (native bus master) are accepted even
under Level 1. This is the same trick ST-Link / STM32CubeProgrammer use.

Loader sources live in `flashloader/`:

- `l0_ob_loader.c` -> `Core/Inc/l0_ob_loader_blob.h`
- `l1_ob_loader.c` -> `Core/Inc/l1_ob_loader_blob.h`
- `g0_ob_loader.c` -> `Core/Inc/g0_ob_loader_blob.h`
- `g4_ob_loader.c` -> `Core/Inc/g4_ob_loader_blob.h`

Rebuild the blobs with `flashloader/build.bat` (needs `arm-none-eabi-gcc` from
STM32CubeCLT on PATH or the path set in the script).

Select the target family in `Core/Src/main.c`:

```c
#define TARGET_FAMILY_STM32F1       (0U)
#define TARGET_FAMILY_STM32L0       (1U)
#define TARGET_FAMILY_STM32G0       (2U)
#define TARGET_FAMILY_STM32L1       (3U)
#define TARGET_FAMILY_STM32G4       (4U)
#define TARGET_FAMILY               TARGET_FAMILY_STM32G0
```

Then drive RDP with `TARGET_RDP_ACTION` (`NONE` / `SET_L1` / `SET_L0`), as for
the STM32F1 path.

### Two flash-controller generations

The four families split into two flash IP generations, which is why there are
two loader styles:

| Family | Flash IP | Register base | RDP location | `SR.BSY` | Commit / reload |
|--------|----------|---------------|--------------|----------|-----------------|
| L0 | EEPROM / `PECR` | `0x40022000` | OB word @ `0x1FF80000` | bit 0 | write OB word, `PECR.OBL_LAUNCH` |
| L1 | EEPROM / `PECR` | `0x40023C00` | OB word @ `0x1FF80000` | bit 0 | write OB word, `PECR.OBL_LAUNCH` |
| G0 | G4/L4/WB-style | `0x40022000` | `FLASH_OPTR[7:0]` | bit 16 | `CR.OPTSTRT`, `CR.OBL_LAUNCH` |
| G4 | G4/L4/WB-style | `0x40022000` | `FLASH_OPTR[7:0]` | bit 16 | `CR.OPTSTRT`, `CR.OBL_LAUNCH` |

- **L1 == L0** except the flash register base (`0x40023C00` vs `0x40022000`).
  Keys, `PECR`/`SR` bits, the option-byte block at `0x1FF80000`, and the 16-bit
  halfword-complement RDP word format (`(~RDP << 16) | RDP`) are all identical.
- **G4 == G0** — the flash IP (base, register offsets, keys, `OPTR` RDP byte,
  `OBL_LAUNCH`) is identical; the G4 loader is byte-for-byte the same blob as
  G0. The G0/G4 loader read-modify-writes only the RDP byte in `OPTR`, so all
  other user option bits (BOR, watchdog, nBOOT, dual-bank, etc.) are preserved.

RDP byte values are common to all four: `0xAA` = Level 0, `0xCC` = Level 2,
any other value (`0xBB` used here) = Level 1. In every case `Level 1 -> Level 0`
triggers a target flash mass erase and uses a reconnect-under-reset at very low
SWD speed before running the loader.

> Loader SRAM usage: code @ `0x20000000`, parameter @ `0x20000100`, stack top
> `0x20002000`. The target therefore needs at least ~8 KB of SRAM (true for all
> G0/G4/L1/L0 parts that support RDP unlock).

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

