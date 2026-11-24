/* D168UV custom firmware, AT32F403A (1 MB flash in two 512 KB banks, 2 KB sectors; 224 KB SRAM).
 *
 * Flash map:
 *   0x08000000-0x08003FFF  stock bootloader            NOT OURS (never write)
 *   0x08004000-0x0807FFFF  APP: this image (vectors first, the bootloader jumps here)   bank 1
 *   0x08080000-0x08081FFF  cfw settings page (8 KB = 4 sectors, drivers/settings.c)     bank 2
 *   0x08082000-0x0809FFFF  unused
 *   0x080A0000-0x080FFFFF  cfw codeplug written by the CPS link (384 KB, shared app/cfwcp.c + drivers/flash.c)
 * SRAM: 224 KB = the extended-SRAM option the stock firmware also needs (its stack top is 0x2001F670, > 96 KB).
 */
ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08004000, LENGTH = 0x08080000 - 0x08004000
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 224K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
_settings_page = 0x08080000;

SECTIONS
{
  .isr_vector ORIGIN(FLASH) :
  {
    KEEP(*(.isr_vector))
  } > FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text .text.*)
    *(.rodata .rodata.*)
    . = ALIGN(4);
    __proto_table_start = .;
    KEEP(*(.cfw_proto))
    __proto_table_end = .;
    . = ALIGN(4);
  } > FLASH

  .ARM.exidx : { *(.ARM.exidx*) } > FLASH

  _sidata = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data .data.*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH

  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  /* not zeroed at reset: hal/safety.c fault record (survives the fault reset) */
  .noinit (NOLOAD) :
  {
    . = ALIGN(4);
    *(.noinit .noinit.*)
    . = ALIGN(4);
    _enoinit = .;
  } > RAM

  _image_end = LOADADDR(.data) + SIZEOF(.data);
  ASSERT(_image_end <= 0x08080000, "image runs into the settings page")
  ASSERT(ORIGIN(FLASH) == 0x08004000, "app must start at 0x08004000 (bootloader below)")
  ASSERT(_enoinit <= _estack - 0x2000, "less than 8 KB left for the stack")

  /DISCARD/ : { *(.ARM.attributes) *(.comment) }
}
