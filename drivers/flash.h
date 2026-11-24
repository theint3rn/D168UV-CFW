#ifndef CFW_FLASH_H
#define CFW_FLASH_H
#include <stdint.h>
#include <stdbool.h>
#define FLASH_SETTINGS_BASE 0x08080000u
#define FLASH_SETTINGS_SIZE 0x2000u
bool flash_erase_range(uint32_t addr, uint32_t len);                       /* 2 KB-aligned, own regions only */
bool flash_program_words(uint32_t addr, const uint32_t *w, uint32_t nwords);
#endif
