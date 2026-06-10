/*
 * STM32F0 mini flashloader for option-byte (RDP) programming.
 *
 * This code is uploaded by the STM32G0 SWD host to the target STM32F0's
 * SRAM and executed there. Because the writes to KEYR / OPTKEYR / OB happen
 * from the F0's own CPU (native bus master), the flash controller accepts
 * them even under RDP Level 1 protection — which is the trick ST-Link /
 * STM32CubeProgrammer uses.
 *
 * The STM32F0 uses the older FPEC flash controller (the same IP as STM32F1):
 *   - FLASH_CR.LOCK gates register access, FLASH_CR.OPTWRE gates OB writes
 *   - the option bytes live at 0x1FFFF800, each as a 16-bit halfword whose
 *     low byte is the value and high byte is its complement (the FPEC
 *     computes and programs the complement automatically)
 *   - FLASH_SR.BSY is bit 0 (NOT bit 16 like the G0/L4 controller)
 *   - OB programming is: OPTER+STRT erase -> OPTPG + halfword write
 *   - unlike STM32F1, the F0 FPEC HAS an FLASH_CR.OBL_LAUNCH bit (bit 13).
 *     A plain NRST / system reset does NOT reload the option bytes on F0
 *     (only a power-on reset or OBL_LAUNCH does), so the loader sets
 *     OBL_LAUNCH itself to force the reload + self-reset — exactly like the
 *     G0/U0/L4 loaders.
 *
 * Unlike the G0/L4 controller, the F0 OB erase wipes ALL option bytes, so
 * only RDP is reprogrammed here (USER/WRP/DATA fall back to their erased
 * 0xFF defaults) — identical to the host-side STM32F1 RDP path.
 *
 * The RDP byte values are the modern 3-level scheme: 0xAA = Level 0,
 * 0xCC = Level 2, anything else (0xBB here) = Level 1.
 *
 * Memory layout on the target (linker script enforces these):
 *   0x20000000 : code  (this binary, ~80 bytes)
 *   0x20000100 : param[0] = desired RDP byte in bits [7:0] (uint32_t)
 *   0x20002000 : initial MSP (top of SRAM region we use)
 *
 * Entry point: 0x20000000 (PC must be set to 0x20000001 — Thumb bit).
 *
 * On entry: SP set to 0x20002000 by the host.
 * On exit:  OBL_LAUNCH resets the chip and reloads the option bytes; if the
 *           reset is deferred we hit BKPT so the host can detect S_HALT
 *           instead of timing out.
 */

#include <stdint.h>

#define FLASH_KEYR     (*(volatile uint32_t *)0x40022004UL)
#define FLASH_OPTKEYR  (*(volatile uint32_t *)0x40022008UL)
#define FLASH_SR       (*(volatile uint32_t *)0x4002200CUL)
#define FLASH_CR       (*(volatile uint32_t *)0x40022010UL)

#define FLASH_KEY1     0x45670123UL
#define FLASH_KEY2     0xCDEF89ABUL
#define FLASH_OPTKEY1  0x45670123UL
#define FLASH_OPTKEY2  0xCDEF89ABUL

#define CR_OPTPG       (1UL << 4)
#define CR_OPTER       (1UL << 5)
#define CR_STRT        (1UL << 6)
#define CR_LOCK        (1UL << 7)
#define CR_OPTWRE      (1UL << 9)
#define CR_OBL_LAUNCH  (1UL << 13)
#define SR_BSY         (1UL << 0)

#define OB_RDP_HALFWORD (*(volatile uint16_t *)0x1FFFF800UL)
#define PARAM_RDP_WORD  (*(volatile uint32_t *)0x20000100UL)

__attribute__((noreturn, used, section(".loader_entry")))
void f0_ob_loader_entry(void)
{
  /* Wait for any prior flash activity to finish. */
  while ((FLASH_SR & SR_BSY) != 0UL) { }

  /* Unlock the flash control register (clears CR.LOCK). The two keys must
     be consecutive — running on the CPU means there are no foreign bus
     accesses between them. */
  FLASH_KEYR = FLASH_KEY1;
  FLASH_KEYR = FLASH_KEY2;

  /* Unlock option-byte access (sets CR.OPTWRE). */
  FLASH_OPTKEYR = FLASH_OPTKEY1;
  FLASH_OPTKEYR = FLASH_OPTKEY2;

  /*
   * Erase all option bytes: set OPTER then STRT. On an RDP Level 1 -> Level 0
   * regression the FPEC also mass-erases the user flash here, so the BSY wait
   * below can take significantly longer.
   */
  FLASH_CR = FLASH_CR | CR_OPTER;
  FLASH_CR = FLASH_CR | CR_STRT;
  while ((FLASH_SR & SR_BSY) != 0UL) { }
  FLASH_CR = FLASH_CR & ~CR_OPTER;

  /*
   * Program the RDP option byte. A single 16-bit halfword write to the OB
   * address programs the low byte; the FPEC computes and programs the
   * complement (high byte) automatically — exactly like ST HAL's
   * WRITE_REG(OB->RDP, level). The CPU write bypasses the debug MEM-AP and
   * is accepted under RDP Level 1.
   */
  FLASH_CR = FLASH_CR | CR_OPTPG;
  OB_RDP_HALFWORD = (uint16_t)(PARAM_RDP_WORD & 0xFFUL);
  while ((FLASH_SR & SR_BSY) != 0UL) { }
  FLASH_CR = FLASH_CR & ~CR_OPTPG;

  /*
   * Trigger the option-byte reload. On F0 a plain NRST / system reset does NOT
   * reload the option bytes (only a power-on reset or OBL_LAUNCH does), so set
   * OBL_LAUNCH here — it forces the reload and generates an internal chip
   * reset. Execution effectively ends here.
   */
  FLASH_CR = FLASH_CR | CR_OBL_LAUNCH;

  /* Belt-and-braces: if for some reason the reset is deferred, hit BKPT
     so the SWD host can detect S_HALT instead of timing out. */
  for (;;)
  {
    __asm volatile ("bkpt #0");
  }
}
