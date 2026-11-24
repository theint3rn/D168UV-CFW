/* D168UV analog outputs (stock V1.07, see docs/AUDIO_PATHS.md):
 *   PA4 = DAC1 = TX power control (APC). Stock FUN_08004364(v): D1DTH12R = (v & 0xFFF) << 4, set by the EXTI5
 *         handler (IRQ23_Handler 0x0800679c) on the baseband TX line PD5: high -> PC4 high, PE6 high, DAC1 = level;
 *         low -> PE6 low, PC4 low, DAC1 = 0. dac_apc() does the same from the PTT path; the level is the stock
 *         FUN_0808ca20 value for the channel's power (dac_apc_value).
 *   PA5 = DAC2 = RX front-end tune voltage. Stock FUN_0809099c / FUN_0800cdf8: per-band tables built from three
 *         calibration points each (FUN_08039030 UHF, FUN_080390e0 VHF), interpolated by frequency. Ported below:
 *         dac_tune_load_cal() reads the stock calibration block from NAND, dac_rx_tune(hz) sets PA5.
 *   PB9 = TMR4_CH4 PWM = MCU audio into the TX modulator (stock FUN_08091bd4 sets PB9 AF, FUN_08095c70 writes CCR4,
 *         TMR5 IRQ paces the samples; PD1 low while the MCU plays = mic path off, FUN_08009430 / FUN_0800940c).
 *         cfw: TMR4 PWM at 48 kHz (PR 2499), CCR4 reloaded every period by DMA1 ch7 (TMR4 overflow request) from the
 *         shared TX ring; the ring holds CCR values (board_cfg.h DAC_CODE scales the 12-bit modulator samples).
 *         One PWM period = one sample, so the read position is DMA1 CNDTR7.
 *         Analog FM test tone (CFW_MCU_FM_TX) is allowed on every build; digital-mode TX needs CFW_DIGITAL_TX
 *         (HW=1 builds: only with HW_DTX=1, dummy load only - PB9 coupling / pre-emphasis unknown). */
#include <string.h>
#include "at32f403a.h"
#include "board.h"
#include "hal.h"
#include "cal_default.h"

static volatile uint16_t s_ring[DAC_RING] __attribute__((aligned(4)));
static bool s_clk, s_streaming;

static void clocks(void)
{
    if(s_clk) { return; }
    CRM_APB1EN |= CRM_APB1_DAC | CRM_APB1_TMR4;
    CRM_AHBEN |= CRM_AHB_DMA1;
    (void)CRM_APB1EN;
    gpio_mode(GPIOA, 4, GPIO_ANALOG, PULL_NONE);
    gpio_mode(GPIOA, 5, GPIO_ANALOG, PULL_NONE);
    DAC_CTRL |= (1u << 0) | (1u << 16);                /* EN1, EN2: no trigger, DTH -> output */
    DAC_D2DTH12R = 0x800u;                             /* until dac_rx_tune() */
    gpio_write(TX_MIC_PORT, TX_MIC, 1);                /* PD1 high = mic path on (stock idle level) */
    gpio_mode(TX_MIC_PORT, TX_MIC, GPIO_OUT, PULL_NONE);
    gpio_mode(TX_PWM_PORT, TX_PWM, GPIO_IN, PULL_NONE); /* PB9 hi-Z while the MCU does not drive TX audio */
    s_clk = true;
}

/* PA level and the TX enables PE6 / PC4 (stock EXTI5 handler). Level = stock FUN_0808ca20 for the channel power
 * (dac_set_power), from the calibration tables loaded by dac_tune_load_cal(); 0 without them (TX refused, F5). */
uint32_t rf_cur_freq(void);
uint16_t dac_apc_value(uint32_t hz);
void dac_apc(bool tx)
{
    clocks();
    if(tx && safety_tx_block(rf_cur_freq())) { tx = false; }   /* backstop: a refused TX never keys the PA */
    DAC_D1DTH12R = tx ? ((uint32_t)dac_apc_value(rf_cur_freq()) & 0xFFFu) << 4 : 0u;
    gpio_write(TX_EN1_PORT, TX_EN1, tx);
    gpio_write(TX_EN0_PORT, TX_EN0, tx);
    safety_tx_on(tx);                                      /* SysTick TX cap / stall watch (shared hal/safety.c) */
}

volatile uint16_t *dac_ring(void) { return s_ring; }

void dac_stream_start(void)
{
    clocks();
    for(uint32_t i = 0; i < DAC_RING; i++) { s_ring[i] = DAC_CODE(2048u); }
    TMR_CTRL1(TMR4_BASE) = 0;
    DMA_CCR(DMA1_BASE, 7) = 0;
    DMA_CPAR(DMA1_BASE, 7) = TMR_C4DT_ADDR(TMR4_BASE);
    DMA_CMAR(DMA1_BASE, 7) = (uint32_t)(uintptr_t)s_ring;
    DMA_CNDTR(DMA1_BASE, 7) = DAC_RING;
    DMA_CCR(DMA1_BASE, 7) = (2u << 12) | (1u << 10) | (1u << 8) | (1u << 7) | (1u << 5) | (1u << 4) | 1u;  /* mem->CCR4 */
    TMR_DIV(TMR4_BASE) = 0;
    TMR_PR(TMR4_BASE) = TX_PWM_TOP - 1u;               /* 120 MHz / 2500 = 48 kHz = ADC_FS */
    TMR_C4DT(TMR4_BASE) = DAC_CODE(2048u);
    TMR_CM2(TMR4_BASE) = (TMR_CM2(TMR4_BASE) & 0x00FFu) | (6u << 12) | (1u << 11);   /* ch4 PWM mode 1, preload */
    TMR_CCTRL(TMR4_BASE) |= 1u << 12;                  /* C4EN, active high */
    TMR_IDEN(TMR4_BASE) = 1u << 8;                     /* overflow -> DMA request (DMA1 ch7) */
    TMR_SWEVT(TMR4_BASE) = 1u;
    TMR_CTRL1(TMR4_BASE) = (1u << 7) | 1u;             /* PR preload, count */
    gpio_mode(TX_PWM_PORT, TX_PWM, GPIO_AF, PULL_NONE);
    gpio_clr(TX_MIC_PORT, TX_MIC);                     /* PD1 low: MCU drives TX audio, mic path off */
    s_streaming = true;
}

void dac_stream_stop(void)
{
    if(!s_streaming) { return; }
    s_streaming = false;
    TMR_IDEN(TMR4_BASE) = 0;
    DMA_CCR(DMA1_BASE, 7) = 0;
    TMR_C4DT(TMR4_BASE) = DAC_CODE(2048u);
    TMR_CTRL1(TMR4_BASE) = 0;
    TMR_CCTRL(TMR4_BASE) &= ~(1u << 12);
    gpio_mode(TX_PWM_PORT, TX_PWM, GPIO_IN, PULL_NONE);
    gpio_set(TX_MIC_PORT, TX_MIC);                     /* mic path back on (stock FUN_0800940c) */
}

uint32_t dac_read_pos(void)
{
    uint32_t left = DMA_CNDTR(DMA1_BASE, 7) & 0xFFFFu;
    if(left == 0 || left > DAC_RING) { return 0; }
    return (DAC_RING - left) % DAC_RING;
}

/* ---------------- PA5 RX tune voltage (stock FUN_08039030 / FUN_080390e0 / FUN_0809099c) ----------------
 * Calibration block: NAND 0x02F60000 (A) / 0x02F80000 (B), A used when its block marker is valid (stock
 * FUN_0807a64c + FUN_08029e28: 55 55 AA AA at +0x1FFFC and not AA AA 55 55 at +0x1FFF8), else B. Offset 0x00 = UHF
 * calibration struct (RAM 0x2000cc28), 0x40 = VHF struct (0x2000cc48); tune points = u16 at struct +0x12/+0x14/+0x16:
 *   UHF 400 / 440 / 480 MHz, table 13 points every 10 MHz 400..520 MHz (last = 0x1000), linear between the points;
 *   VHF 136 / 155 / 174 MHz, table 39 points every 1 MHz 136..174 MHz.
 * Lookup in stock units (f in 10 Hz): below 300 MHz VHF, index = (f - 136 MHz) / 1 MHz, tenth-step interpolation by
 * the 100 kHz digit; UHF index = (f - 400 MHz) / 10 MHz, by the 1 MHz digit. Unsigned arithmetic as stock.
 * The stock special cases (DAT_2000156b + channel index 0x10..0x12 / 0x28..0x2A = fixed table points) are not used.
 * Without a valid block (or a point outside 1..0xFFF) PA5 stays at mid-scale 0x800. */
#define CAL_A        0x02F60000u
#define CAL_B        0x02F80000u
static uint16_t s_uhf[13], s_vhf[39];
static bool s_cal_ok, s_apc_ok;
/* APC (stock FUN_0807a64c copies, FUN_0808ca20 uses): UHF struct +0x00 (levels 0..3 at +2..+5), VHF struct +0x40
 * (levels at +2..+5, +0x1E = 200..300 MHz value); per-level tables one byte per MHz: UHF 400..520 MHz (0x79 bytes)
 * at +0x300 + 0x80 * level, reference point index 0x28 (440 MHz); VHF 136..174 MHz (0x27 bytes) at +0x500 +
 * 0x40 * level, reference index 9 (145 MHz). */
static uint8_t s_cu[0x20], s_cv[0x20], s_apc_u[4][0x79], s_apc_v[4][0x27];

/* stock FUN_0808ca20, f in 10 Hz units; the table index is clamped (stock: unbounded). Pre-flash audit F5: the stock
 * u16 sum "cal + table - reference" wraps to ~0xFFFF (12-bit mask -> near full scale) when the table point is below
 * the reference by more than cal; here it is computed signed and clamped to 0..max, max = the highest value the stock
 * formula gives anywhere in that band at any power level. No valid calibration -> 0 (TX is refused, board_apc_ok). */
static uint8_t s_apc_max_u, s_apc_max_v;
static bool s_apc_ok_u, s_apc_ok_v;

/* squelch (stock FUN_0807bf6c): bases u16 UHF +0x10, VHF +0x50; per-frequency correction tables VHF +0x600 (20 bytes,
 * 2 MHz steps from 136 MHz, ref index 10), UHF +0x620 (25 bytes, 5 MHz steps from 400 MHz, ref index 8). Default
 * calibration (cal_default.c) until a NAND block is read. */
static uint16_t s_sqb[2];
static uint8_t s_sqt_v[0x14], s_sqt_u[0x19];
static bool s_sq;

static void sq_set(const uint8_t *u, const uint8_t *v, const uint8_t *tv, const uint8_t *tu)
{
    s_sqb[0] = (uint16_t)(u[0x10] | u[0x11] << 8);
    s_sqb[1] = (uint16_t)(v[0x10] | v[0x11] << 8);
    memcpy(s_sqt_v, tv, sizeof s_sqt_v);
    memcpy(s_sqt_u, tu, sizeof s_sqt_u);
    s_sq = true;
}

static void sq_defaults(void)
{
    sq_set(cal_default, cal_default + 0x40u, cal_default + 0x600u, cal_default + 0x620u);
}

/* stock FUN_0807bf6c, f in 10 Hz: reg 0x49 = base + 4 * (level - 1) (level 0 = 0) + table[reference] - table[f], u16
 * arithmetic as stock. The table index is clamped (stock: unbounded). Not ported: the stock temperature term
 * FUN_08039694 and the two special-mode branches that use a base alone. */
uint16_t dac_sql_threshold(uint32_t hz, int level)
{
    if(!s_sq) { sq_defaults(); }
    uint32_t f = hz / 10u;
    int li = level < 1 ? 0 : level > 5 ? 4 : level - 1;
    if(f > 30000000u) {
        uint32_t i = f < 40000000u ? 0 : (f - 40000000u) / 500000u;
        if(i > 0x18u) { i = 0x18u; }
        return (uint16_t)(s_sqb[0] + 4 * li + s_sqt_u[8] - s_sqt_u[i]);
    }
    uint32_t i = f < 13600000u ? 0 : (f - 13600000u) / 200000u;
    if(i > 0x13u) { i = 0x13u; }
    return (uint16_t)(s_sqb[1] + 4 * li + s_sqt_v[10] - s_sqt_v[i]);
}

static int clampi(int v, int mx) { return v < 0 ? 0 : v > mx ? mx : v; }

uint16_t dac_apc_value(uint32_t hz)
{
    if(!s_apc_ok) { return 0; }
    uint32_t f = hz / 10u;
    int l = dac_power() & 3;
    if(f < 20000000u) {
        uint32_t i = (f - 13600000u) / 100000u & 0xFFu;
        return (uint16_t)clampi((int)s_cv[2 + l] + s_apc_v[l][i < 0x27u ? i : 0x26u] - s_apc_v[l][9], s_apc_max_v);
    }
    if(f > 30000000u) {
        uint32_t i = (f - 40000000u) / 100000u & 0xFFu;
        return (uint16_t)clampi((int)s_cu[2 + l] + s_apc_u[l][i < 0x79u ? i : 0x78u] - s_apc_u[l][0x28], s_apc_max_u);
    }
    return (uint16_t)clampi(s_cv[0x1E], s_apc_max_v);
}

/* band usable: tables read, not erased (all 0xFF) or blank, and some level gives a non-zero drive */
static bool apc_band(const uint8_t *cal, const uint8_t *t, int stride, int n, int ref, uint8_t *mx)
{
    int m = 0;
    bool ff = true, zero = true;
    for(int l = 0; l < 4; l++) {
        const uint8_t *tl = t + stride * l;
        for(int i = 0; i < n; i++) {
            if(tl[i] != 0xFFu) { ff = false; }
            if(tl[i] != 0u) { zero = false; }
            int v = (int)cal[2 + l] + tl[i] - tl[ref];
            if(v > m) { m = v; }
        }
    }
    *mx = (uint8_t)(m > 0xFF ? 0xFF : m);
    return !ff && !zero && m > 0;
}

bool board_apc_ok(uint32_t hz)
{
    if(!s_apc_ok) { return false; }
    uint32_t f = hz / 10u;
    if(f < 20000000u) { return s_apc_ok_v; }
    if(f > 30000000u) { return s_apc_ok_u; }
    return s_apc_ok_v && s_cv[0x1E] != 0xFFu && s_cv[0x1E] != 0u;
}

/* Fault / SysTick TX stop (hal/safety.h): register writes only. APC 0, PE6 + PC4 low, TMR4 PWM off and PB9 hi-Z,
 * PD1 high (mic path back on, stock idle level); rf: RF chip back to receive (drivers/rf.c). */
void rf_tx_kill(void);
void board_tx_kill(bool rf)
{
    DAC_D1DTH12R = 0;
    GPIO_BRR(TX_EN0_PORT) = 1u << TX_EN0;
    GPIO_BRR(TX_EN1_PORT) = 1u << TX_EN1;
    TMR_IDEN(TMR4_BASE) = 0;
    TMR_CTRL1(TMR4_BASE) = 0;
    TMR_CCTRL(TMR4_BASE) &= ~(1u << 12);
    GPIO_CFGHR(TX_PWM_PORT) = (GPIO_CFGHR(TX_PWM_PORT) & ~(0xFu << ((TX_PWM - 8) * 4))) | (0x4u << ((TX_PWM - 8) * 4));
    GPIO_BSRR(TX_MIC_PORT) = 1u << TX_MIC;
    if(rf) { rf_tx_kill(); }
}

static bool block_valid(uint32_t base)
{
    static const uint8_t ok[4] = { 0x55, 0x55, 0xAA, 0xAA }, bad[4] = { 0xAA, 0xAA, 0x55, 0x55 };
    uint8_t m[4];
    if(!nand_read(base + 0x1FFFCu, m, 4) || memcmp(m, ok, 4) != 0) { return false; }
    if(!nand_read(base + 0x1FFF8u, m, 4) || memcmp(m, bad, 4) == 0) { return false; }
    return true;
}

static int16_t quarter(int d) { return (int16_t)((d + (int)((uint32_t)(d >> 31) >> 30)) * 0x4000 >> 16); }

_Static_assert(CAL_DEFAULT_LEN >= 0x620u + 0x19u, "cal_default too short");
bool dac_tune_load_cal(void)
{
    uint8_t u[0x20], v[0x20];
    bool a = block_valid(CAL_A), b = !a && block_valid(CAL_B);
    uint32_t base = a ? CAL_A : CAL_B;
    s_cal_ok = false;
    s_apc_ok = false;
    bool rd = nand_read(base, u, sizeof u) && nand_read(base + 0x40u, v, sizeof v);
    if(rd) { memcpy(s_cu, u, sizeof s_cu); memcpy(s_cv, v, sizeof s_cv); }
    bool ok = rd && (a || b);                          /* F5: no APC without a valid block marker */
    uint8_t tv[sizeof s_sqt_v], tu[sizeof s_sqt_u];
    if(ok && nand_read(base + 0x600u, tv, sizeof tv) && nand_read(base + 0x620u, tu, sizeof tu)) { sq_set(u, v, tv, tu); }
    else { sq_defaults(); }
    if(!ok) {                                          /* RX tune from the default RF calibration; APC stays off */
        memcpy(u, cal_default, sizeof u);
        memcpy(v, cal_default + 0x40u, sizeof v);
    }
    for(int l = 0; l < 4 && ok; l++) {
        ok = nand_read(base + 0x300u + 0x80u * (uint32_t)l, s_apc_u[l], sizeof s_apc_u[l]) &&
             nand_read(base + 0x500u + 0x40u * (uint32_t)l, s_apc_v[l], sizeof s_apc_v[l]);
    }
    s_apc_ok_u = ok && apc_band(s_cu, &s_apc_u[0][0], 0x79, 0x79, 0x28, &s_apc_max_u);
    s_apc_ok_v = ok && apc_band(s_cv, &s_apc_v[0][0], 0x27, 0x27, 9, &s_apc_max_v);
    s_apc_ok = s_apc_ok_u || s_apc_ok_v;
    cal_report(ok ? (a ? 'A' : 'B') : 0, base, 0x800u, s_apc_ok_u, s_apc_ok_v);
    uint16_t up[3], vp[3];
    for(int i = 0; i < 3; i++) {
        up[i] = (uint16_t)(u[0x12 + 2 * i] | u[0x13 + 2 * i] << 8);
        vp[i] = (uint16_t)(v[0x12 + 2 * i] | v[0x13 + 2 * i] << 8);
        if(up[i] == 0 || up[i] > 0xFFF || vp[i] == 0 || vp[i] > 0xFFF) { return false; }
    }
    /* FUN_08039030 */
    s_uhf[0] = up[0]; s_uhf[4] = up[1]; s_uhf[8] = up[2];
    for(int i = 1; i < 4; i++) { s_uhf[i] = (uint16_t)(s_uhf[i - 1] + quarter((int)up[1] - (int)up[0])); }
    for(int i = 5; i < 8; i++) { s_uhf[i] = (uint16_t)(s_uhf[i - 1] + quarter((int)up[2] - (int)up[1])); }
    for(int i = 9; i < 12; i++) { s_uhf[i] = (uint16_t)(s_uhf[i - 1] + quarter(0x1000 - (int)up[2])); }
    s_uhf[12] = 0x1000;
    /* FUN_080390e0 */
    s_vhf[0] = vp[0]; s_vhf[19] = vp[1]; s_vhf[38] = vp[2];
    for(int i = 1; i < 19; i++) { s_vhf[i] = (uint16_t)(s_vhf[i - 1] + (int16_t)(((int)vp[1] - (int)vp[0]) / 19)); }
    for(int i = 20; i < 38; i++) { s_vhf[i] = (uint16_t)(s_vhf[i - 1] + (int16_t)(((int)vp[2] - (int)vp[1]) / 19)); }
    s_cal_ok = true;
    return true;
}

bool dac_tune_cal_ok(void) { return s_cal_ok; }

uint16_t dac_tune_value(uint32_t hz)
{
    if(!s_cal_ok) { return 0x800u; }
    uint32_t f = hz / 10u, r;                          /* stock unit: 10 Hz */
    if(f < 30000000u) {
        uint32_t idx = (f - 13600000u) / 100000u & 0xFFu;
        if(idx >= 0x26u) { r = s_vhf[0x26]; }
        else {
            uint32_t frac = ((f - 13600000u) / 10000u & 0xFFu) % 10u;
            r = (s_vhf[idx] + ((((uint32_t)s_vhf[idx + 1] - s_vhf[idx]) & 0xFFFFu) * frac & 0xFFFFu) / 10u) & 0xFFFFu;
        }
    } else {
        uint32_t idx = (f - 40000000u) / 1000000u & 0xFFu;
        if(idx >= 0xCu) { r = s_uhf[0xC]; }
        else {
            uint32_t frac = ((f - 40000000u) / 100000u & 0xFFu) % 10u;
            r = (s_uhf[idx] + ((((uint32_t)s_uhf[idx + 1] - s_uhf[idx]) & 0xFFFFu) * frac & 0xFFFFu) / 10u) & 0xFFFFu;
        }
    }
    return (uint16_t)(r > 0xFFFu ? 0xFFFu : r);        /* stock writes 0x1000 into the 12-bit register (= 0) */
}

void dac_rx_tune(uint32_t hz)
{
    clocks();
    DAC_D2DTH12R = dac_tune_value(hz);
}
