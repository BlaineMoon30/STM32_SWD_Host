# STM32 Multi-Target SWD Host

This repository contains an STM32G0-based SWD host that talks to a range of
STM32 targets through bit-banged SWD. It started as an STM32F103-only example
and now drives the Read-Out Protection (RDP) option byte on five families.

The project was tuned against real ST-LINK logic captures and supports:

- Stable SWD connect and memory read
- Reading 64 bytes from `0x08000000`
- STM32F1 RDP Level `0 -> 1` / `1 -> 0` (direct over the debug MEM-AP)
- STM32L0 RDP Level `0 -> 1` / `1 -> 0` (via on-target option-byte flashloader)
- STM32L1 RDP Level `0 -> 1` / `1 -> 0` (via on-target option-byte flashloader)
- STM32G0 RDP Level `0 -> 1` / `1 -> 0` (via on-target option-byte flashloader)
- STM32G4 RDP Level `0 -> 1` / `1 -> 0` (via on-target option-byte flashloader)
- STM32F0 RDP Level `0 -> 1` / `1 -> 0` (via on-target option-byte flashloader)
- STM32U0 RDP Level `0 -> 1` / `1 -> 0` (via on-target option-byte flashloader)
- STM32L4 RDP Level `0 -> 1` / `1 -> 0` (via on-target option-byte flashloader)

## Project Layout

- `STM32_SWD_Host/`
  Main STM32CubeIDE project (this directory)
- `flashloader/`
  Sources, linker scripts and build tooling for the on-target option-byte
  loaders, plus the generated `build/` artifacts

Important source files:

- `Core/Src/swd_host.c`  — SWD engine + per-family RDP logic
- `Core/Inc/swd_host.h`  — public API
- `Core/Src/main.c`      — per-family example entry points
- `Core/Inc/*_ob_loader_blob.h` — generated loader blobs consumed by `swd_host.c`

## Hardware Setup

Host MCU:

- STM32G0

Target MCU:

- STM32F1 / STM32L0 / STM32L1 / STM32G0 / STM32G4 / STM32F0 / STM32U0 / STM32L4

Main signal mapping:

- `PA0` -> target `SWDIO`
- `PA1` -> target `SWCLK`
- `PA4` -> target `nRESET`
- Common `GND`

## Public API

All functions are declared in `Core/Inc/swd_host.h` and return
`swd_host_status_t` (`SWD_HOST_OK` on success).

Host / transport:

```c
swd_host_status_t swd_host_init(void);
swd_host_status_t swd_host_init_with_config(const swd_host_config_t *config);
swd_host_status_t swd_host_configure(const swd_host_config_t *config);
void              swd_host_get_config(swd_host_config_t *config);
swd_host_status_t swd_host_set_connect_mode(swd_host_connect_mode_t connect_mode);
swd_host_status_t swd_host_set_speed(swd_host_speed_t speed);
swd_host_status_t swd_host_connect(void);
void              swd_host_disconnect(void);

swd_host_status_t swd_host_read_u8 (uint32_t address, uint8_t  *value);
swd_host_status_t swd_host_read_u16(uint32_t address, uint16_t *value);
swd_host_status_t swd_host_read_u32(uint32_t address, uint32_t *value);
swd_host_status_t swd_host_write_u8 (uint32_t address, uint8_t  value);
swd_host_status_t swd_host_write_u16(uint32_t address, uint16_t value);
swd_host_status_t swd_host_write_u32(uint32_t address, uint32_t value);
```

Per-family helpers (one set each for `stm32f1` / `stm32l0` / `stm32l1` /
`stm32g0` / `stm32g4` / `stm32f0` / `stm32u0` / `stm32l4`):

```c
swd_host_status_t stm32XX_read_u32 (uint32_t address, uint32_t *value);
swd_host_status_t stm32XX_write_u32(uint32_t address, uint32_t value);
swd_host_status_t stm32XX_get_rdp_level(stm32XX_rdp_level_t *level);
swd_host_status_t stm32XX_set_rdp_level(stm32XX_rdp_level_t  level);
```

RDP levels are `STM32XX_RDP_LEVEL_0 / _1 / _2`.

## Connect Mode and Speed

The host is configured with `swd_host_config_t`:

```c
swd_host_config_t swd_config =
{
  SWD_HOST_CONNECT_NORMAL,   /* or SWD_HOST_CONNECT_UNDER_RESET */
  SWD_HOST_SPEED_VERY_LOW
};
swd_host_init_with_config(&swd_config);
```

Connect modes:

- `SWD_HOST_CONNECT_NORMAL`
- `SWD_HOST_CONNECT_UNDER_RESET`

Speed presets:

- `SWD_HOST_SPEED_VERY_LOW`
- `SWD_HOST_SPEED_LOW`
- `SWD_HOST_SPEED_MEDIUM`
- `SWD_HOST_SPEED_HIGH`
- `SWD_HOST_SPEED_VERY_HIGH`

Mode and speed can also be changed at runtime with
`swd_host_set_connect_mode()` / `swd_host_set_speed()` before a (re)connect.

## Example Behavior

`main.c` selects one per-family test routine at compile time and runs it once
after init:

| Family | Macro | Routine |
|--------|-------|---------|
| STM32F1 | `TARGET_FAMILY_STM32F1` | `SWD_Test()` |
| STM32L0 | `TARGET_FAMILY_STM32L0` | `SWD_Test_L0()` |
| STM32G0 | `TARGET_FAMILY_STM32G0` | `SWD_Test_G0()` |
| STM32L1 | `TARGET_FAMILY_STM32L1` | `SWD_Test_L1()` |
| STM32G4 | `TARGET_FAMILY_STM32G4` | `SWD_Test_G4()` |
| STM32F0 | `TARGET_FAMILY_STM32F0` | `SWD_Test_F0()` |
| STM32U0 | `TARGET_FAMILY_STM32U0` | `SWD_Test_U0()` |
| STM32L4 | `TARGET_FAMILY_STM32L4` | `SWD_Test_L4()` |

Each routine:

1. Connects to the target over SWD
2. Waits `TARGET_CONNECT_STABILIZE_MS` for the target to stabilize
3. Reads the current RDP level
4. Reads 64 bytes from `0x08000000` into `flash_data[64]`
   (STM32F1 path; the read is left `#if 0`-disabled in the L0/L1/G0/G4 paths so
   the loader sequence can be exercised on its own — re-enable it if you want a
   read on those families)
5. Optionally changes the RDP level per `TARGET_RDP_ACTION`

## Selecting Family and RDP Action

Both selections live in `Core/Src/main.c`.

Target family:

```c
#define TARGET_FAMILY_STM32F1       (0U)
#define TARGET_FAMILY_STM32L0       (1U)
#define TARGET_FAMILY_STM32G0       (2U)
#define TARGET_FAMILY_STM32L1       (3U)
#define TARGET_FAMILY_STM32G4       (4U)
#define TARGET_FAMILY_STM32F0       (5U)
#define TARGET_FAMILY_STM32U0       (6U)
#define TARGET_FAMILY_STM32L4       (7U)
#define TARGET_FAMILY               TARGET_FAMILY_STM32L1
```

RDP action:

```c
#define TARGET_RDP_ACTION_NONE      (0U)
#define TARGET_RDP_ACTION_SET_L1    (1U)
#define TARGET_RDP_ACTION_SET_L0    (2U)
#define TARGET_RDP_ACTION           TARGET_RDP_ACTION_SET_L0
```

- `TARGET_RDP_ACTION_NONE`   — no RDP change (read only)
- `TARGET_RDP_ACTION_SET_L1` — Level 0 -> Level 1 (lock)
- `TARGET_RDP_ACTION_SET_L0` — Level 1 -> Level 0 (unlock, triggers mass erase)

Each `..._ChangeRdpLevel()` first reads the current level and refuses the action
if the target is not in the expected starting level.

Notes:

- `Level 1 -> Level 0` triggers a target flash mass erase on every family
- `Level 1 -> Level 0` requires a more conservative reconnect sequence than
  normal memory access

## STM32L0 / L1 / G0 / G4 / F0 / U0 / L4 RDP Control

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
- `f0_ob_loader.c` -> `Core/Inc/f0_ob_loader_blob.h`
- `u0_ob_loader.c` -> `Core/Inc/u0_ob_loader_blob.h`
- `l4_ob_loader.c` -> `Core/Inc/l4_ob_loader_blob.h`

Rebuild all seven blobs with `flashloader/build.bat` (needs `arm-none-eabi-gcc`
from STM32CubeCLT on PATH or the path set at the top of the script). The script
compiles each loader, objcopies to `.bin`, dumps a `.dis`, and runs
`emit_blob_header.py` to regenerate the matching `Core/Inc/*_ob_loader_blob.h`.

The STM32F1 path is the exception: it programs the option bytes directly over
the MEM-AP and needs no loader blob.

### Three flash-controller generations

The loader families split into three flash IP generations, which is why there
are three loader styles:

| Family | Flash IP | Register base | RDP location | `SR.BSY` | Commit / reload |
|--------|----------|---------------|--------------|----------|-----------------|
| L0 | EEPROM / `PECR` | `0x40022000` | OB word @ `0x1FF80000` | bit 0 | write OB word, `PECR.OBL_LAUNCH` |
| L1 | EEPROM / `PECR` | `0x40023C00` | OB word @ `0x1FF80000` | bit 0 | write OB word, `PECR.OBL_LAUNCH` |
| G0 | G4/L4/WB-style | `0x40022000` | `FLASH_OPTR[7:0]` | bit 16 | `CR.OPTSTRT`, `CR.OBL_LAUNCH` |
| G4 | G4/L4/WB-style | `0x40022000` | `FLASH_OPTR[7:0]` | bit 16 | `CR.OPTSTRT`, `CR.OBL_LAUNCH` |
| U0 | G4/L4/WB-style | `0x40022000` | `FLASH_OPTR[7:0]` | bit 16 | `CR.OPTSTRT`, `CR.OBL_LAUNCH` |
| L4 | G4/L4/WB-style | `0x40022000` | `FLASH_OPTR[7:0]` | bit 16 | `CR.OPTSTRT`, `CR.OBL_LAUNCH` |
| F0 | FPEC (F1-style) | `0x40022000` | OB halfword @ `0x1FFFF800` | bit 0 | program OB, `CR.OBL_LAUNCH` |

- **L1 == L0** except the flash register base (`0x40023C00` vs `0x40022000`).
  Keys, `PECR`/`SR` bits, the option-byte block at `0x1FF80000`, and the 16-bit
  halfword-complement RDP word format (`(~RDP << 16) | RDP`) are all identical.
- **G4 == G0 == U0 == L4** — the flash IP (base, register offsets, keys, `OPTR`
  RDP byte, `OBL_LAUNCH`) is identical; the G4/U0/L4 loaders are byte-for-byte
  the same blob as G0. The loader read-modify-writes only the RDP byte in
  `OPTR`, so all other user option bits (BOR, watchdog, nBOOT, dual-bank, etc.)
  are preserved.
- **F0 == F1 flash IP** (the older FPEC controller) but uses the modern
  `0xAA`/`0xBB`/`0xCC` 3-level RDP byte scheme. Unlike F1, the F0 RDP byte is
  flipped from a flashloader (more robust under Level 1). The loader erases the
  option bytes and programs the RDP halfword at `0x1FFFF800` (the FPEC computes
  the complement automatically). **Unlike F1, the F0 FPEC has an
  `FLASH_CR.OBL_LAUNCH` bit (bit 13)** — and it is required: on F0 a plain
  `nRST` / system reset does **not** reload the option bytes (only a power-on
  reset or `OBL_LAUNCH` does), so the loader sets `OBL_LAUNCH` itself to reload
  + self-reset. The active level is read back from `FLASH_OBR` `RDPRT1`/`RDPRT2`.

RDP byte values are common to the modern families: `0xAA` = Level 0,
`0xCC` = Level 2, any other value (`0xBB` used here) = Level 1. In every case
`Level 1 -> Level 0` triggers a target flash mass erase and uses a
reconnect-under-reset at very low SWD speed before running the loader.

> Loader SRAM usage: code @ `0x20000000`, parameter @ `0x20000100`, stack top
> `0x20002000`. The target therefore needs at least ~8 KB of SRAM (true for all
> G0/G4/U0/L4/L1/L0 parts that support RDP unlock). The F0 loaders are
> stackless straight-line code, but they still place the parameter at
> `0x20000100`; on very small STM32F0 parts (4 KB SRAM) lower the stack top in
> the matching `*_ob_loader.ld` / blob if you adapt them.

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

Normal SWD memory access can succeed while flash register access still fails
under RDP Level 1.

The final working solution was to reconnect using:

- `SWD_HOST_CONNECT_UNDER_RESET`
- `SWD_HOST_SPEED_VERY_LOW`

before unlocking the target's flash interface for the RDP regression sequence.

## Default / Safe Configuration

`TARGET_RDP_ACTION` and `TARGET_FAMILY` are set in `main.c` for whatever target
is currently being exercised. For a non-destructive run, set:

```c
#define TARGET_RDP_ACTION           TARGET_RDP_ACTION_NONE
```

so the example only connects and reads, with no option-byte write or mass
erase. Choose `SET_L1` / `SET_L0` explicitly when you intend to change RDP.

## Status

Validated behaviors:

- SWD connect
- Flash read from `0x08000000`
- RDP Level 0 -> Level 1 (F1 / L0 / L1 / G0 / G4 / F0 / U0 / L4)
- RDP Level 1 -> Level 0 (F1 / L0 / L1 / G0 / G4 / F0 / U0 / L4)
