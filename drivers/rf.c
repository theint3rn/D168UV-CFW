/* RF transceiver of the D168UV: the same chip as the D890UV rf0, on a 3-wire bus SCK PB3, SDA PB4
 * (bidirectional), CS# PB5. One chip only (chip argument 1 = absent). Frame: CS low, 8 bits (reg << 1 | R),
 * 16 data bits, MSB first (stock V1.07 FUN_0807bc60 write, FUN_0807bca8 read).
 * Register values: D168UV stock V1.07 (decompile + emulator write trace), see the tables below.
 *
 * Emulator: rf_read_reg() also has the symbol FUN_0807bca8 (the stock read, reg in r0, value in r0). emu/run.py
 * hooks that name and returns the RF model's register (Renode cannot pass bit-level SDA reads back on GPIOB). */
#include "at32f403a.h"
#include "board.h"
#include "hal.h"
#include "radio.h"

#define SCK_LO() gpio_clr(RF_SCK_PORT, RF_SCK)
#define SCK_HI() gpio_set(RF_SCK_PORT, RF_SCK)

static uint32_t s_freq;

static inline void bitdelay(void) { for(volatile int i = 0; i < 5; i++) { } }

static void shift_out8(uint8_t b)
{
    for(int i = 7; i >= 0; i--) {
        gpio_write(RF_SDA_PORT, RF_SDA, (b >> i) & 1);
        SCK_HI();
        bitdelay();
        SCK_LO();
    }
}

static uint16_t shift_in16(void)
{
    uint16_t v = 0;
    SCK_LO();
    for(int i = 0; i < 16; i++) {
        v = (uint16_t)(v << 1);
        SCK_HI();
        bitdelay();
        if(gpio_read(RF_SDA_PORT, RF_SDA)) { v |= 1; }
        SCK_LO();
    }
    return v;
}

static void write_reg(uint8_t reg, uint16_t val)
{
    uint32_t p = irq_save();
    SCK_LO();
    bitdelay();
    gpio_clr(RF_CS0_PORT, RF_CS0);
    bitdelay();
    shift_out8((uint8_t)(reg << 1));
    shift_out8((uint8_t)(val >> 8));
    shift_out8((uint8_t)val);
    gpio_set(RF_CS0_PORT, RF_CS0);
    bitdelay();
    gpio_clr(RF_SDA_PORT, RF_SDA);
    irq_restore(p);
}

/* Forced TX stop (hal/safety.h board_tx_kill, from a fault handler or the SysTick TX cap): CS# high first (a fault
 * may have cut a transfer), SDA back to output, then reg 0x56 = 1 (receive, stock FUN_0807c090) and 0x30 = idle. */
void rf_tx_kill(void)
{
    gpio_set(RF_CS0_PORT, RF_CS0);
    gpio_mode(RF_SDA_PORT, RF_SDA, GPIO_OUT, PULL_NONE);
    write_reg(0x56, 0x0001);
    write_reg(0x30, 0x5F00);
}

__attribute__((noinline, used)) uint16_t rf_read_reg(uint32_t reg)
{
    uint32_t p = irq_save();
    SCK_LO();
    gpio_clr(RF_CS0_PORT, RF_CS0);
    shift_out8((uint8_t)((reg << 1) | 1));
    gpio_set(RF_SDA_PORT, RF_SDA);
    gpio_mode(RF_SDA_PORT, RF_SDA, GPIO_IN, PULL_NONE);
    uint16_t v = shift_in16();
    gpio_mode(RF_SDA_PORT, RF_SDA, GPIO_OUT, PULL_NONE);
    gpio_set(RF_CS0_PORT, RF_CS0);
    gpio_clr(RF_SDA_PORT, RF_SDA);
    irq_restore(p);
    return v;
}
uint16_t FUN_0807bca8(uint32_t reg) __attribute__((alias("rf_read_reg")));

void rf_write(int chip, uint8_t reg, uint16_t val) { if(chip == 0) { write_reg(reg, val); } }
uint16_t rf_read(int chip, uint8_t reg) { return chip == 0 ? rf_read_reg(reg) : 0; }

struct rv { uint8_t r; uint16_t v; };
static void write_list(int chip, const struct rv *l, int n)
{
    for(int i = 0; i < n; i++) { rf_write(chip, l[i].r, l[i].v); }
}
#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* ---- D168UV stock V1.07 register values (checked against a register-write trace of the stock firmware in the
 * emulator: hook on FUN_0807bc60, boot + dual-band receive). ----
 * Reg 0x52 = eight switch bits (bits 15:12 = index, bit 0 = value, 0x40 always set), stock FUN_0808f968(idx, v) with
 * a shadow byte: idx 3 = 1 after init (FUN_0803e838), idx 7 / idx 4 = UHF / VHF receive path (FUN_0809096a),
 * idx 5 = UHF transmit path (FUN_080929a4: TX frequency > 300 MHz). These are the band switches of the D168UV.
 * Band split for the RX tables: 300 MHz (stock DAT_0809204c = 30,000,000 x 10 Hz). */

/* FUN_0803e88c power-on init (part before the band table) */
static const struct rv k_init_a[] = {
    {0x50, 0x0001}, {0x58, 0xF1F0},
    {0x40, 0x70E6}, {0x40, 0x60FF}, {0x40, 0x50F4}, {0x40, 0x40E8}, {0x40, 0x30DC}, {0x40, 0x20CD},
    {0x40, 0x10C3}, {0x40, 0x00B9},
};
/* FUN_0808f9bc(band): 0 = UHF, 1 = VHF */
static const struct rv k_band_uhf[] = {
    {0x53, 0x773F}, {0x53, 0x673F}, {0x53, 0x56FF}, {0x53, 0x467D}, {0x53, 0x367A}, {0x53, 0x2677},
    {0x53, 0x1475}, {0x53, 0x00F0}, {0x42, 0x56A1}, {0x43, 0x1F4F}, {0x49, 0x7066},
};
static const struct rv k_band_vhf[] = {
    {0x53, 0x773F}, {0x53, 0x673F}, {0x53, 0x567F}, {0x53, 0x463D}, {0x53, 0x363A}, {0x53, 0x263A},
    {0x53, 0x1634}, {0x53, 0x00F0}, {0x42, 0x46A1}, {0x43, 0x1F4F}, {0x49, 0x706A},
};
/* FUN_0803e88c continued (FUN_0807bc2c(0xd) = reg 0x5B 0xB085 in the trace) */
static const struct rv k_init_b[] = {
    {0x52, 0x0040}, {0x5B, 0xB085},
    {0x41, 0x710F}, {0x44, 0x1490}, {0x45, 0x8874}, {0x46, 0x00F1}, {0x47, 0x27FE},
    {0x49, 0x0050}, {0x49, 0x1078}, {0x49, 0x706A}, {0x49, 0xB0CC}, {0x4D, 0x0000}, {0x57, 0xA008},
    {0x5C, 0x0001}, {0x5D, 0x8611}, {0x5E, 0x0768}, {0x5F, 0x0768}, {0x60, 0x0300}, {0x2A, 0x1081},
    {0x30, 0x5F6B},
};
/* FUN_0808cf2c receive branch + FUN_08070dca(0) + FUN_08070b88(0) */
static const struct rv k_rx[] = {
    {0x1A, 0x64C1}, {0x1B, 0x1258}, {0x2B, 0x708D}, {0x53, 0x773F}, {0x26, 0x6468}, {0x27, 0x6080},
    {0x18, 0x006E}, {0x24, 0xF4AF}, {0x25, 0x0B56}, {0x32, 0x0622},
    {0x20, 0x0000}, {0x21, 0x0000}, {0x22, 0x0000}, {0x23, 0x0000},
    {0x1C, 0x0000}, {0x1D, 0x0000}, {0x1E, 0x0000}, {0x1F, 0x0000},
};
/* FUN_0803e88c tail, after the frequency */
static const struct rv k_init_c[] = {
    {0x59, 0x2204}, {0x29, 0x1090}, {0x56, 0x0000}, {0x3C, 0x00FC},
};
/* channel receive set-up as traced (stock ~0x08090800 path, before / after the band + RX tables) */
static const struct rv k_ch_pre[] = {
    {0x2D, 0x3205}, {0x2F, 0x0A0F}, {0x16, 0x0000}, {0x3F, 0x3232},
};
static const struct rv k_ch_post[] = {
    {0x12, 0x2010}, {0x13, 0x0018}, {0x10, 0xA3E0}, {0x11, 0x0000},
    {0x2E, 0x153B}, {0x2E, 0x2485}, {0x2E, 0x558B}, {0x2E, 0x74EB}, {0x2C, 0x3850}, {0x2D, 0x2215}, {0x2E, 0xE9D9},
};
/* FUN_0808cf2c transmit branch (FUN_0800b9f8() != 0) + FUN_08070dca(1) + FUN_08070b88(0xf): same values as the
 * D890UV stock TX set */
static const struct rv k_tx[] = {
    {0x19, 0x0381}, {0x24, 0xA192}, {0x25, 0x2590}, {0x26, 0x0010}, {0x32, 0x0600}, {0x53, 0x7000},
    {0x2B, 0xF08D}, {0x18, 0x006E},
    {0x20, 0x8D90}, {0x21, 0x3B15}, {0x22, 0x8D15}, {0x23, 0x3B99},
    {0x1C, 0x80D8}, {0x1D, 0x3F3F}, {0x1E, 0x80F0}, {0x1F, 0x3F27},
};

static uint8_t s_sw = 0x08;                     /* reg 0x52 shadow (stock DAT_0808f9b8): idx 3 set by init */
static void sw_set(int chip, int idx, bool on)  /* stock FUN_0808f968: write only on change */
{
    uint8_t m = (uint8_t)(1u << idx);
    if(on == ((s_sw & m) != 0)) { return; }
    s_sw = on ? (uint8_t)(s_sw | m) : (uint8_t)(s_sw & ~m);
    rf_write(chip, 0x52, (uint16_t)((idx << 12) | 0x40 | (on ? 1 : 0)));
}
static bool uhf(uint32_t hz) { return hz > 300000000u; }

static uint8_t s_vox;                           /* VOX level 0..3 (vox_hw_config below) */
static void vox_regs(void)                      /* stock FUN_0807c0e0 + switch 3; k_ch_post resets 0x12 / 0x13 */
{
    static const uint16_t k12[4] = { 0x10, 0x20, 0x5C, 0x9C }, k13[4] = { 0x18, 0x31, 0x6F, 0xAF };
    rf_write(0, 0x12, (uint16_t)(k12[s_vox] | 0x2000));
    rf_write(0, 0x13, k13[s_vox]);
    sw_set(0, 3, s_vox == 0);
}

bool rf_init(int chip)
{
    if(chip != 0) { return false; }             /* one transceiver on the D168UV */
    /* boot self-test as stock (trace: 50 0001, 58 B110, 58 F1F0, 57 A888, then the init list) */
    rf_write(chip, 0x50, 0x0001);
    rf_write(chip, 0x58, 0xB110);
    rf_write(chip, 0x58, 0xF1F0);
    rf_write(chip, 0x57, 0xA888);
    bool ok = rf_read(chip, 0x57) == 0xA888;
    write_list(chip, k_init_a, N(k_init_a));
    write_list(chip, k_band_uhf, N(k_band_uhf));
    write_list(chip, k_init_b, N(k_init_b));
    write_list(chip, k_rx, N(k_rx));
    rf_write(chip, 0x19, 0x0381);
    rf_write(chip, 0x48, 0x9826);                      /* FUN_0808fd14(1) */
    rf_set_freq(chip, 435125000u);
    write_list(chip, k_init_c, N(k_init_c));
    for(int i = 1; i < 8; i++) {                       /* FUN_0803e838 */
        rf_write(chip, 0x52, (uint16_t)((i << 12) | 0x40 | (i == 3 ? 1 : 0)));
    }
    s_sw = 0x08;
    return ok;
}

/* stock FUN_0808fd50: f [10 Hz] -> reg 0x69 = f*5 >> 15 (= Hz >> 16), reg 0x68 = f*10 & 0xFFFF (= Hz low half) */
uint32_t rf_cur_freq(void) { return s_freq; }      /* last main-chip frequency (dac_apc) */

/* squelch threshold (stock FUN_0807bf6c -> FUN_0807bf54: reg 0x49 = threshold): from the RF calibration for the
 * frequency and the squelch level, rewritten on every frequency change */
static int s_sql = 3;
static uint16_t s_thr;

static void sql_write(int chip)
{
    s_thr = dac_sql_threshold(s_freq, s_sql);
    rf_write(chip, 0x49, s_thr);
}

void rf_set_sql(int level)
{
    s_sql = level;
    sql_write(0);
}

uint16_t rf_sql_thr(void) { return s_thr; }

void rf_set_freq(int chip, uint32_t hz)
{
    s_freq = hz;
    rf_write(chip, 0x69, (uint16_t)(hz >> 16));
    rf_write(chip, 0x68, (uint16_t)hz);
    sql_write(chip);
}

/* Receive set-up, stock order from the trace: 56 = 0, 52 idx0 = 0, frequency, channel regs, band table, RX filters,
 * 48 (FUN_0808fd14(0): 0x082E analog, 0x09FE digital), 49 squelch level, 30 (FUN_0807c304: 0x5F75 / 0x5F65),
 * 5C 0, 58, 5F / 60 by band, 56 = 1 (receive), then the band switches (52 idx 7 / 4).
 * Squelch level reg 0x49 (0x0097 UHF / 0x00A7 VHF in the trace) = stock FUN_0807bf6c threshold (sql_write). The analog /
 * digital choice of reg 0x48 and the 0x30 wide / narrow mapping are not confirmed; the cfw uses 0x082E for analog
 * FM (channel mode from radio_current()) and 0x5F75 = wide. PA5 tune voltage (drivers/dac.c) follows the frequency. */
void rf_rx_fm(int chip, bool wide)
{
    bool u = uhf(s_freq);
    rf_write(chip, 0x56, 0x0000);
    rf_write(chip, 0x52, 0x0040);
    rf_set_freq(chip, s_freq);
    write_list(chip, k_ch_pre, N(k_ch_pre));
    write_list(chip, u ? k_band_uhf : k_band_vhf, u ? N(k_band_uhf) : N(k_band_vhf));
    rf_write(chip, 0x2A, 0x108C);
    write_list(chip, k_rx, N(k_rx));
    write_list(chip, k_ch_post, N(k_ch_post));
    if(s_vox) { vox_regs(); }
    bool fm = radio_current()->mode == CFW_MODE_FM;
    rf_write(chip, 0x48, fm ? 0x082E : 0x09FE);
    sql_write(chip);                                    /* stock: 0x0097 / 0x00A7 on the trace's default channel */
    rf_write(chip, 0x30, wide ? 0x5F75 : 0x5F65);
    rf_write(chip, 0x5C, 0x0000);
    rf_write(chip, 0x58, 0xF1F0);
    rf_write(chip, 0x5F, u ? 0x0569 : 0x0468);
    rf_write(chip, 0x60, 0x0309);
    rf_write(chip, 0x56, 0x0001);
    sw_set(chip, 5, false);
    sw_set(chip, 7, u);
    sw_set(chip, 4, !u);
    dac_rx_tune(s_freq);
    audio_route(fm ? AUDIO_RF : AUDIO_BB);
}

/* Transmit set-up, stock FUN_08091de8 (analog PTT): audio muted (PE0 / PC12 / PA7 low), TX register set
 * (FUN_0808f7b4 -> FUN_0808cf2c), 5C = 0 (FUN_0808fd04), FUN_0807c090: 5E / 60 by frequency (< 390 MHz:
 * 0x0468 / 0x0306, else 0x0569 / 0x0309), 56 = 2 (transmit), 52 idx 5 = TX frequency > 300 MHz (FUN_080929a4).
 * The PA itself is switched by PC4 / PE6 + the APC level (dac_apc, as the stock EXTI5 / PD5 handler).
 * Not ported: the stock spur list FUN_0809ca00 (409.5 / 422.5 / ... / 487.5 MHz +-150 kHz: reg 60 = 0x000F, 56
 * toggled). Unverified on hardware: test into a dummy load. */
void rf_tx_fm(int chip, bool on)
{
    if(on) {
        audio_route(AUDIO_MUTE);
        rf_set_freq(chip, s_freq);
        rf_write(chip, 0x48, 0x082E);
        write_list(chip, k_tx, N(k_tx));
        rf_write(chip, 0x5C, 0x0000);
        bool lo = s_freq < 390000000u;
        rf_write(chip, 0x5E, lo ? 0x0468 : 0x0569);
        rf_write(chip, 0x60, lo ? 0x0306 : 0x0309);
        rf_write(chip, 0x56, 0x0002);
        sw_set(chip, 5, uhf(s_freq));
    } else {
        rf_rx_fm(chip, true);
    }
}

/* stock FUN_080e3c20: reg 0x31 = tone | 0x1000 (0 = off), reg 0x2E = x | 0xC000 */
void rf_set_ctcss(int chip, uint16_t tone_dhz)
{
    rf_write(chip, 0x31, tone_dhz ? (uint16_t)(tone_dhz | 0x1000) : 0);
}

/* Receive status as the stock firmware reads it (V1.05):
 *   reg 0x02 [9:0] = RSSI: FUN_08049e3c, squelch FUN_080d22f0 (open when RSSI >= the threshold written to
 *                    reg 0x49 by FUN_080e36f4, close below threshold - 12)
 *   reg 0x01 [15:8] = noise (> 0x50 closes the squelch), [3:0] = glitch count
 *   reg 0x0E bit 14 = carrier / squelch detect (DAT_2000406c, FUN_080df38c)
 * The RSSI -> dBm scale is not in the stock code (it compares raw values against calibration data);
 * rf_rssi_dbm() assumes 0.5 dB steps from -160 dBm, which is also what the emulator model uses. */
uint16_t rf_rssi(int chip) { return rf_read(chip, 0x02) & 0x3FF; }
uint16_t rf_noise(int chip) { return rf_read(chip, 0x01); }
bool rf_carrier(int chip) { return (rf_read(chip, 0x0E) & 0x4000) != 0; }
int rf_rssi_dbm(uint16_t raw) { return (int)raw / 2 - 160; }

void rf_idle(int chip)
{
    rf_write(chip, 0x30, 0x5F00);                      /* stock FUN_080929b4(1) */
    audio_route(AUDIO_MUTE);
}

/* ---- VOX (stock V1.07) ----
 * The RF chip measures the microphone level itself: FUN_0807c0e0 writes the thresholds of the menu VOX level
 * (settings +0x0C) to reg 0x12 = {0x10, 0x20, 0x5C, 0x9C}[level] | 0x2000 and reg 0x13 = {0x18, 0x31, 0x6F, 0xAF}[level]
 * (tables 0x080C9AD8 / 0x080C9AEC), and reg 0x52 switch 3 is cleared while VOX is on (set when off: FUN_08092cf4 ->
 * FUN_0808f968(3, v)). The detector output is PD3: the stock VOX poll (0x08099B4C, literal 0x08099BF0 = VOX flag
 * 0x20001055) reads GPIOD IDR bit 3 (FUN_08014548(GPIOD, 8)) and keys TX (key event 0x16) after 3 polls with PD3 low;
 * the hold time is VOX delay (settings +0x0D) + 5 ticks. PD3 is an input with pull-down (CRL nibble 8, ODR bit 0). */
void vox_hw_config(uint8_t level)
{
    s_vox = level & 3;
    gpio_mode(GPIOD, 3, GPIO_IN, PULL_DOWN);
    vox_regs();
}
bool vox_hw_active(void) { return s_vox && !gpio_read(GPIOD, 3); }
