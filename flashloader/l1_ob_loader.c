/*
 * STM32L1 mini flashloader for option-byte (RDP) programming.
 *
 * This code is uploaded by the STM32G0 SWD host to the target STM32L1's
 * SRAM and executed there. Because the writes to PEKEYR / OPTKEYR / OB
 * happen from the L1's own CPU (native bus master), the flash controller
 * accepts them even under RDP Level 1 protection — which is the trick
 * ST-Link / STM32CubeProgrammer uses.
 *
 * The STM32L1 flash IP is the same EEPROM/PECR controller as the STM32L0
 * (same keys, same PECR/SR bits, same OB layout). The ONLY difference is the
 * flash register base address: L0 = 0x40022000, L1 = 0x40023C00. The option
 * byte block stays at 0x1FF80000 and uses the 16-bit halfword-complement
 * layout, identical to L0 (ST HAL FLASH_OB_RDPConfig writes (~RDP << 16)|RDP).
 *
 * Memory layout on the target (linker script enforces these):
 *   0x20000000 : code  (this binary, ~80 bytes)
 *   0x20000100 : param[0] = OB option word value to program (uint32_t)
 *   0x20002000 : initial MSP (top of SRAM region we use)
 *
 * Entry point: 0x20000000 (PC must be set to 0x20000001 — Thumb bit).
 *
 * On entry: SP set to 0x20002000 by the host.
 * On exit:  BKPT — the host sees DHCSR.S_HALT or the OBL_LAUNCH-induced
 *           reset (whichever happens first).
 */

#include <stdint.h>

#define FLASH_PECR     (*(volatile uint32_t *)0x40023C04UL)
#define FLASH_PEKEYR   (*(volatile uint32_t *)0x40023C0CUL)
#define FLASH_OPTKEYR  (*(volatile uint32_t *)0x40023C14UL)
#define FLASH_SR       (*(volatile uint32_t *)0x40023C18UL)

#define FLASH_PEKEY1   0x89ABCDEFUL
#define FLASH_PEKEY2   0x02030405UL
#define FLASH_OPTKEY1  0xFBEAD9C8UL
#define FLASH_OPTKEY2  0x24252627UL

#define PECR_OBL_LAUNCH (1UL << 18)
#define PECR_ERASE      (1UL << 9)
#define SR_BSY          (1UL << 0)

#define OB_RDP_ADDRESS  (*(volatile uint32_t *)0x1FF80000UL)
#define PARAM_OB_WORD   (*(volatile uint32_t *)0x20000100UL)

__attribute__((noreturn, used, section(".loader_entry")))
void l1_ob_loader_entry(void)
{
  /* Unlock PECR (PELOCK off). Two keys must be consecutive — running on
     the CPU means there are no foreign bus accesses between them. */
  FLASH_PEKEYR = FLASH_PEKEY1;
  FLASH_PEKEYR = FLASH_PEKEY2;

  /* Unlock option byte access (OPTLOCK off). */
  FLASH_OPTKEYR = FLASH_OPTKEY1;
  FLASH_OPTKEYR = FLASH_OPTKEY2;

  /* Wait for any prior flash activity to finish. */
  while ((FLASH_SR & SR_BSY) != 0UL) { }

  /*
   * Erase the option byte word first.
   * The STM32L0/L1 OB programming path is most reliable with an explicit
   * erase before program:
   *   set PECR.ERASE -> write 0 to OB address -> wait BSY -> clear PECR.ERASE
   * Skipping this step can make the subsequent OB write fail silently and the
   * hardware complement-verify then rejects the word at OBL_LAUNCH.
   */
  FLASH_PECR = FLASH_PECR | PECR_ERASE;
  OB_RDP_ADDRESS = 0UL;
  while ((FLASH_SR & SR_BSY) != 0UL) { }
  FLASH_PECR = FLASH_PECR & ~PECR_ERASE;

  /* Program the option byte word at 0x1FF80000.
     The CPU write bypasses MEM-AP and is accepted under RDP L1. */
  OB_RDP_ADDRESS = PARAM_OB_WORD;

  /* Wait for the write (and any implicit mass-erase on RDP regression)
     to complete. */
  while ((FLASH_SR & SR_BSY) != 0UL) { }

  /* Trigger OB reload — chip resets itself, asserts NRST_OUT, and reloads
     option bytes. Execution effectively ends here. */
  FLASH_PECR = FLASH_PECR | PECR_OBL_LAUNCH;

  /* Belt-and-braces: if for some reason the reset is deferred, hit BKPT
     so the SWD host can detect S_HALT instead of timing out. */
  for (;;)
  {
    __asm volatile ("bkpt #0");
  }
}
