/* Power: PC3 latch + PA0 volume-knob switch.
 * Stock V1.07: PA0 high = knob in the off position. High at boot -> charge-only loop (FUN_08005444); high while
 * running -> FUN_0800c48c saves, clears PA7 (audio amp), PE0, PC3 and waits for power to go. The cfw: armed
 * once PA0 has been seen low; then ~100 ms of PA0 high -> save hook, LCD off, PC3 low, AIRCR reset (in case
 * USB / charger keeps it powered). PA0 still high 3 s after reset -> off again. */
#include "at32f403a.h"
#include "board.h"
#include "hal.h"

static void (*s_save)(void);
static bool s_armed;
static uint8_t s_high;

void power_hold(void) { gpio_set(PIN_PWR_LATCH_PORT, PIN_PWR_LATCH); }

void power_set_save_hook(void (*fn)(void)) { s_save = fn; }

static bool key_high(void) { return gpio_read(PIN_PWR_KEY_PORT, PIN_PWR_KEY); }

void power_off(void)
{
    if(s_save) { s_save(); }
    lcd_on(false);
    gpio_clr(PIN_PWR_LATCH_PORT, PIN_PWR_LATCH);
    delay_ms(200);
    dsb();
    SCB_AIRCR = (0x5FAu << 16) | (1u << 2);            /* SYSRESETREQ */
    for(;;) { }
}

#define BOOT_OFF_MAX_MS 3000u

void power_poll(void)
{
    if(!key_high()) { s_armed = true; s_high = 0; return; }
    if(!s_armed) {
        if(hal_millis() > BOOT_OFF_MAX_MS) { power_off(); }
        return;
    }
    if(++s_high >= 5) { power_off(); }
}

bool power_armed(void) { return s_armed; }
