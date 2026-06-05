/*
 * Linker script for the STM32L1 mini flashloader.
 * Forces the entry function to sit at the very start of the binary so the
 * host can upload the .bin verbatim to SRAM 0x20000000 and set PC to
 * 0x20000001 (Thumb bit).
 *
 * No data / bss / heap sections — the loader is self-contained code that
 * reads its single parameter from a fixed address (0x20000100).
 */

ENTRY(l1_ob_loader_entry)

MEMORY
{
  CODE (rx) : ORIGIN = 0x20000000, LENGTH = 256
}

SECTIONS
{
  .loader 0x20000000 :
  {
    KEEP(*(.loader_entry))
    *(.text*)
    *(.rodata*)
  } > CODE

  /DISCARD/ :
  {
    *(.ARM.exidx*)
    *(.ARM.extab*)
    *(.eh_frame*)
    *(.comment)
    *(.note*)
  }
}
