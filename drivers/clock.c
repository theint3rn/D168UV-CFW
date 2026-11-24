/* Clocks as the stock firmware sets them (FUN_0801a864): HEXT 12 MHz -> PLL x10 = 120 MHz SCLK = HCLK,
 * APB1 = APB2 = 60 MHz (timers 120 MHz), ADC clock PCLK2/4 = 15 MHz. PLLMULT low field 8 = x10, PLLRANGE set
 * (> 72 MHz), switch through the auto-step mode (CRM_MISC2 [5:4], stock FUN_080a0138). */
#include "at32f403a.h"
#include "hal.h"

static bool wait_bit(uint32_t addr, uint32_t mask, bool set)
{
    for(uint32_t n = 0; n < 1000000u; n++) { if(((REG32(addr) & mask) != 0) == set) { return true; } }
    return false;
}

static bool wait_sclk(uint32_t sel)
{
    for(uint32_t n = 0; n < 1000000u; n++) { if(((CRM_CFG >> 2) & 3u) == sel) { return true; } }
    return false;
}

static void sclk_select(uint32_t sel)                           /* through the auto-step mode, as stock */
{
    CRM_MISC2 |= 3u << 4;
    CRM_CFG = (CRM_CFG & ~3u) | sel;
    wait_sclk(sel);
    CRM_MISC2 &= ~(3u << 4);
}

/* Pre-flash audit F4 / F9: the bootloader may hand over with the PLL running as SCLK, so SCLK goes to HICK (8 MHz)
 * first (the PLL must not be reconfigured while it is the system clock), then PLL off -> configured -> on -> SCLK.
 * If HEXT or the PLL does not come up, SCLK stays on HICK, g_clk_err = 1: the UI shows "CLK ERR" and TX is refused
 * (hal/safety.c); SysTick-based times then run 15x slow, UART baud rates are wrong. */
void clock_init(void)
{
    g_clk_err = 0;
    CRM_CTRL |= 1u << 0;                                        /* HICKEN */
    wait_bit((uint32_t)&CRM_CTRL, 1u << 1, true);               /* HICKSTBL */
    if(((CRM_CFG >> 2) & 3u) != 0u) { sclk_select(0u); }       /* SCLK = HICK */
    CRM_CTRL &= ~(1u << 24);                                    /* PLL off while configuring */
    bool ok = wait_bit((uint32_t)&CRM_CTRL, 1u << 25, false);

    CRM_CTRL |= 1u << 16;                                       /* HEXTEN */
    ok = ok && wait_bit((uint32_t)&CRM_CTRL, 1u << 17, true);   /* HEXTSTBL */
    uint32_t cfg = CRM_CFG;
    cfg &= ~((0xFu << 4) | (7u << 8) | (7u << 11) | (3u << 14) | (1u << 16) | (1u << 17) | (0xFu << 18) |
             (1u << 28) | (3u << 29) | (1u << 31));
    cfg |= (4u << 8) | (4u << 11);                              /* APB1 /2, APB2 /2, AHB /1 */
    cfg |= 1u << 14;                                            /* ADC /4 */
    if(ok) { cfg |= (1u << 16) | (8u << 18) | (1u << 31); }    /* PLL from HEXT, x10, range > 72 MHz */
    CRM_CFG = cfg;
    if(ok) {
        CRM_CTRL |= 1u << 24;                                   /* PLLEN */
        ok = wait_bit((uint32_t)&CRM_CTRL, 1u << 25, true);     /* PLLSTBL */
    }
    if(ok) {
        sclk_select(2u);                                        /* SCLK = PLL */
        ok = ((CRM_CFG >> 2) & 3u) == 2u;
        if(!ok) { sclk_select(0u); }
    }
    if(!ok) { g_clk_err = 1; }
}
