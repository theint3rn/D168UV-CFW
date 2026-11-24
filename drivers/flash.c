/* Internal flash programming for the cfw's own two regions only (bank 2):
 *   settings page 0x08080000-0x08081FFF (drivers/settings.c), codeplug 0x080A0000-0x080FFFFF (shared app/cfwcp.c).
 * Everything else is refused here, so a bug cannot touch the bootloader (0x08000000-0x08003FFF) or the image.
 * AT32F403A bank-2 registers (FLASH +0x40): sector erase = SECERS + ADDR + ERSTR (2 KB sectors), programming =
 * FPRGM + 32-bit word writes. make HW=1 (CFW_HW_NO_SAVE) compiles the writes out (no flash writes on a real radio
 * without the owner's OK). */
#include "at32f403a.h"
#include "hal.h"
#include "flash.h"

#define BANK2 1

#ifndef CFW_HW_NO_SAVE
static bool allowed(uint32_t a, uint32_t n)
{
    bool set = a >= FLASH_SETTINGS_BASE && a + n <= FLASH_SETTINGS_BASE + FLASH_SETTINGS_SIZE;
    bool cp = a >= CFWCP_BASE && a + n <= CFWCP_BASE + CFWCP_SIZE;
    return n && (set || cp) && a >= 0x08080000u && a + n <= 0x08100000u;
}

static void wait_idle(void)
{
    for(uint32_t n = 0; n < 1000000u && (FLASH_STS(BANK2) & FLASH_STS_OBF); n++) { wdg_refresh(); }   /* F3 */
}
#endif

void cfw_flash_unlock(void)
{
#ifndef CFW_HW_NO_SAVE
    if(FLASH_CTRL(BANK2) & FLASH_CTRL_OPLK) {
        FLASH_UNLOCK(BANK2) = FLASH_KEY1;
        FLASH_UNLOCK(BANK2) = FLASH_KEY2;
    }
#endif
}

void cfw_flash_lock(void) { FLASH_CTRL(BANK2) |= FLASH_CTRL_OPLK; }

bool flash_erase_range(uint32_t addr, uint32_t len)
{
#ifdef CFW_HW_NO_SAVE
    (void)addr; (void)len;
    return false;
#else
    if((addr & (FLASH_SECTOR - 1u)) || (len & (FLASH_SECTOR - 1u)) || !allowed(addr, len)) { return false; }
    for(uint32_t a = addr; a < addr + len; a += FLASH_SECTOR) {
        wait_idle();
        FLASH_STS(BANK2) = 0x34u;                       /* clear PRGMERR / EPPERR / ODF */
        FLASH_CTRL(BANK2) |= FLASH_CTRL_SECERS;
        FLASH_ADDR(BANK2) = a;
        FLASH_CTRL(BANK2) |= FLASH_CTRL_ERSTR;
        wait_idle();
        FLASH_CTRL(BANK2) &= ~(FLASH_CTRL_SECERS | FLASH_CTRL_ERSTR);
    }
    return true;
#endif
}

bool flash_program_words(uint32_t addr, const uint32_t *w, uint32_t nwords)
{
#ifdef CFW_HW_NO_SAVE
    (void)addr; (void)w; (void)nwords;
    return false;
#else
    if((addr & 3u) || !allowed(addr, nwords * 4u)) { return false; }
    FLASH_STS(BANK2) = 0x34u;
    FLASH_CTRL(BANK2) |= FLASH_CTRL_FPRGM;
    for(uint32_t i = 0; i < nwords; i++) {
        *(volatile uint32_t *)(uintptr_t)(addr + 4u * i) = w[i];
        wait_idle();
    }
    FLASH_CTRL(BANK2) &= ~FLASH_CTRL_FPRGM;
    return true;
#endif
}

void cfw_flash_erase(uint32_t addr) { flash_erase_range(addr, CFWCP_PAGE); }
void cfw_flash_program16(uint32_t addr, const uint32_t w[4]) { flash_program_words(addr, w, 4); }
