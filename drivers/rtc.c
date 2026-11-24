/* D168UV RTC (AT32F403A, STM32F1 style: 32-bit seconds counter CNTH/CNTL @0x40002800, battery backed, set up by the
 * stock firmware) + backlight (TMR8_CH2N PB0) + TX power level. The counter holds local time in seconds since 1970 (as the stock
 * firmware's clock). Write: PWR CTRL.DBP, wait RTOFF, CNF, CNT, leave CNF. Not verified on a real radio
 * (emulator: emu/models/Rtc168.cs). */
#include "at32f403a.h"
#include "board.h"
#include "hal.h"

#define RTC_BASE  0x40002800u
#define RTC_CRL   REG32(RTC_BASE + 0x04)
#define RTC_CNTH  REG32(RTC_BASE + 0x18)
#define RTC_CNTL  REG32(RTC_BASE + 0x1C)
#define PWR_CTRL  REG32(0x40007000u)

uint32_t rtc_now(void)
{
    uint32_t h = RTC_CNTH & 0xFFFFu, l = RTC_CNTL & 0xFFFFu;
    if((RTC_CNTH & 0xFFFFu) != h) { h = RTC_CNTH & 0xFFFFu; l = RTC_CNTL & 0xFFFFu; }
    return h << 16 | l;
}

void rtc_set(uint32_t t)
{
    CRM_APB1EN |= (1u << 28) | (1u << 27);                 /* PWR, BKP */
    PWR_CTRL |= 1u << 8;                                   /* DBP */
    for(int n = 0; n < 100000 && !(RTC_CRL & 0x20u); n++) { }
    RTC_CRL |= 0x10u;                                      /* CNF */
    RTC_CNTH = t >> 16;
    RTC_CNTL = t & 0xFFFFu;
    RTC_CRL &= ~0x10u;
    for(int n = 0; n < 100000 && !(RTC_CRL & 0x20u); n++) { }
}

/* Backlight: PB0 = TMR8_CH2N (complementary output), period 0xFFF, divider 3, duty per level as stock V1.07
 * (FUN_0803fb58 init, FUN_0808d0a4 on, table 0x080E2D82 = 1100 / 1200 / 1300 / 1400 / 4095, 0 = off). The emulator
 * showed C2DT 0xFFF at boot and 0 after the stock backlight timeout. */
#define TMR8_BASE 0x40013400u
#define CRM_APB2_TMR8 (1u << 13)
static const uint16_t k_bl[5] = { 1100, 1200, 1300, 1400, 4095 };
static bool s_bl_init;

void backlight_set(uint8_t level)
{
    if(!s_bl_init) {
        CRM_APB2EN |= CRM_APB2_TMR8 | (1u << 3);                   /* TMR8, GPIOB */
        gpio_mode(GPIOB, 0, GPIO_AF, PULL_NONE);
        TMR_DIV(TMR8_BASE) = 3;
        TMR_PR(TMR8_BASE) = 0xFFF;
        REG32(TMR8_BASE + 0x18) = (REG32(TMR8_BASE + 0x18) & ~0xFF00u) | 0x6800u;   /* CM1: ch2 PWM mode A + preload */
        TMR_CCTRL(TMR8_BASE) |= 1u << 6;                           /* C2CEN: complementary output on PB0 */
        REG32(TMR8_BASE + 0x44) |= 1u << 15;                       /* BRK.OEN (advanced timer outputs on) */
        TMR_SWEVT(TMR8_BASE) = 1u;
        TMR_CTRL1(TMR8_BASE) |= (1u << 7) | 1u;
        s_bl_init = true;
    }
    REG32(TMR8_BASE + 0x38) = level ? k_bl[(level - 1) % 5] : 0;  /* C2DT */
}

/* TX power level 0 low .. 3 (turbo): dac_apc() takes the stock APC value for it from the calibration block */
static uint8_t s_power;
void dac_set_power(uint8_t level) { s_power = level & 3u; }
uint8_t dac_power(void) { return s_power; }
