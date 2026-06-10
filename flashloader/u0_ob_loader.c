/*
 * STM32U0 mini flashloader for option-byte (RDP) programming.
 *
 * This code is uploaded by the STM32G0 SWD host to the target STM32U0's
 * SRAM and executed there. Because the writes to KEYR / OPTKEYR / OPTR
 * happen from the U0's own CPU (native bus master), the flash controller
 * accepts them even under RDP Level 1 protection — which is the trick
 * ST-Link / STM32CubeProgrammer uses.
 *
 * The STM32U0 flash IP is the newer controller shared with G0/G4/L4/C0:
 *   - FLASH_CR.LOCK / OPTLOCK gate register / option-byte writes
 *   - the RDP level lives in FLASH_OPTR[7:0]
 *   - FLASH_SR.BSY is bit 16 (NOT bit 0 like the L0 controller)
 *   - programming is committed with FLASH_CR.OPTSTRT, reloaded with OBL_LAUNCH
 *
 * The register base (0x40022000), the offsets, the keys, the OPTR RDP byte
 * encoding and OBL_LAUNCH are byte-for-byte identical to the STM32G0, so this
 * loader is functionally the same as g0_ob_loader.c.
 *
 * The loader does a read-modify-write of only the RDP byte in OPTR so that
 * every other user option bit (BOR, watchdog, nBOOT, etc.) is preserved.
 *
 * Memory layout on the target (linker script enforces these):
 *   0x20000000 : code  (this binary, ~100 bytes)
 *   0x20000100 : param[0] = desired RDP byte in bits [7:0] (uint32_t)
 *   0x20002000 : initial MSP (top of SRAM region we use)
 *
 * Entry point: 0x20000000 (PC must be set to 0x20000001 — Thumb bit).
 *
 * On entry: SP set to 0x20002000 by the host.
 * On exit:  OBL_LAUNCH resets the chip; if the reset is deferred we hit BKPT
 *           so the SWD host can detect S_HALT instead of timing out.
 */

#include <stdint.h>

#define FLASH_KEYR     (*(volatile uint32_t *)0x40022008UL)
#define FLASH_OPTKEYR  (*(volatile uint32_t *)0x4002200CUL)
#define FLASH_SR       (*(volatile uint32_t *)0x40022010UL)
#define FLASH_CR       (*(volatile uint32_t *)0x40022014UL)
#define FLASH_OPTR     (*(volatile uint32_t *)0x40022020UL)

#define FLASH_KEY1     0x45670123UL
#define FLASH_KEY2     0xCDEF89ABUL
#define FLASH_OPTKEY1  0x08192A3BUL
#define FLASH_OPTKEY2  0x4C5D6E7FUL

#define CR_OPTSTRT     (1UL << 17)
#define CR_OBL_LAUNCH  (1UL << 27)
#define CR_OPTLOCK     (1UL << 30)
#define CR_LOCK        (1UL << 31)
#define SR_BSY         (1UL << 16)

#define OPTR_RDP_MASK  0x000000FFUL
#define PARAM_RDP_WORD (*(volatile uint32_t *)0x20000100UL)

__attribute__((noreturn, used, section(".loader_entry")))
void u0_ob_loader_entry(void)
{
  uint32_t optr;

  /* Wait for any prior flash activity to finish. */
  while ((FLASH_SR & SR_BSY) != 0UL) { }

  /* Unlock the flash control register (clears CR.LOCK). The two keys must
     be consecutive — running on the CPU means there are no foreign bus
     accesses between them. */
  FLASH_KEYR = FLASH_KEY1;
  FLASH_KEYR = FLASH_KEY2;

  /* Unlock option-byte access (clears CR.OPTLOCK). */
  FLASH_OPTKEYR = FLASH_OPTKEY1;
  FLASH_OPTKEYR = FLASH_OPTKEY2;

  while ((FLASH_SR & SR_BSY) != 0UL) { }

  /* Read-modify-write only the RDP byte so all other option bits survive.
     The CPU register access bypasses the debug MEM-AP and is accepted
     under RDP Level 1. */
  optr = FLASH_OPTR;
  optr = (optr & ~OPTR_RDP_MASK) | (PARAM_RDP_WORD & OPTR_RDP_MASK);
  FLASH_OPTR = optr;

  /* Commit the option-byte programming. */
  FLASH_CR = FLASH_CR | CR_OPTSTRT;

  /* Wait for the program (and any implicit mass-erase on RDP regression)
     to complete. */
  while ((FLASH_SR & SR_BSY) != 0UL) { }

  /* Trigger OB reload — chip resets itself, asserts NRST_OUT, and reloads
     option bytes. Execution effectively ends here. */
  FLASH_CR = FLASH_CR | CR_OBL_LAUNCH;

  /* Belt-and-braces: if for some reason the reset is deferred, hit BKPT
     so the SWD host can detect S_HALT instead of timing out. */
  for (;;)
  {
    __asm volatile ("bkpt #0");
  }
}
